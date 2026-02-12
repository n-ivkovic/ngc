#include "assemble_full.h"

#include "../ngc.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/**
 * Expanded parsed assembly line.
 */
struct expanded_line {
	enum parsed_line_type type;
	size_t line_num;
	size_t val; // Parsed instruction if type is LINE_INST_E, index of expanded_base.refs_data if type is LINE_REF_DATA_E, index of expanded_base.macros if type is LINE_REF_MACRO_E.
};

/**
 * Expanded parsed assembly.
 */
struct expanded_base {
	struct expanded_base* parent;
	size_t line_num;
	size_t inst_len;
	struct llist macros; // llist of expanded_base
	struct llist lines; // llist of expanded_line
	struct llist refs_data; // llist of char[PARSED_KEY_CHARS]
	struct llist defs_data; // llist of parsed_def_data
};

/**
 * Free expanded assembly.
 */
void expanded_base_free(struct expanded_base* base);

/**
 * Free values within expanded assembly.
 */
static void expanded_base_empty(struct expanded_base* base)
{
    if (!base) return;

	base->parent = NULL;
	llist_delegate_empty(&base->macros, (void (*)(void *))expanded_base_free);
	llist_empty(&base->lines);
    llist_empty(&base->refs_data);
    llist_empty(&base->defs_data);
}

void expanded_base_free(struct expanded_base* base)
{
    if (!base) return;

    expanded_base_empty(base);
    free(base);
}

static bool list_copy(struct error* err, struct llist* dst, const struct llist src)
{
	if (llist_copy(dst, src) < src.len) {
		error_init(err, ERRVAL_FAILURE, "Failed to copy list");
		return false;
	}

	return true;
}

/**
 * Push expanded line.
 *
 * @param err Struct to store error.
 * @param lines Linked list to store expanded result.
 * @param type Type of expanded line.
 * @param line_num Number of line in file.
 * @param val Expanded line value.
 * @returns Whether expanded line was pushed successfully.
 */
static bool lines_push(struct error* err, struct llist* lines, const enum parsed_line_type type, const size_t line_num, const size_t val)
{
	struct expanded_line result = { .type = type, .line_num = line_num, .val = val };
	if (!llist_push(lines, &result, sizeof(result))) {
		error_init(err, ERRVAL_FAILURE, "Failed to push expanded line to list");
		return false;
	}

	return true;
}

/**
 * Expand/unwind macros from parsed result.
 *
 * @param err Struct to store error.
 * @param expanded Struct to store expanded/unwound result.
 * @param parsed Parsed assembly.
 * @param defs_macros Linked list of parsed macro definitions.
 * @returns 0 if successfully expanded/unwound. >0 line number if error.
 */
static size_t expand_parsed(struct error* err, struct expanded_base* expanded, const struct parsed_base parsed, const struct llist defs_macros)
{
	assert(expanded);

	if (!list_copy(err, &expanded->refs_data, parsed.refs_data))
		return (expanded->line_num > 0) ? expanded->line_num : 1;

	// Iterate through lines
	struct llist_node* line_node = parsed.lines.head;
	for (size_t lines_ind = 0; line_node && lines_ind < parsed.lines.len; line_node = line_node->next, lines_ind++) {
		struct parsed_line* line = line_node->val;

		switch (line->type) {
			case LINE_INST_E:
			case LINE_REF_DATA_E:
				// Push line as-is
				if (!lines_push(err, &expanded->lines, line->type, line->line_num, line->val))
					return line->line_num;

				expanded->inst_len++;
				break;

			case LINE_REF_MACRO_E:
				;
				// Get macro referenced by line
				struct parsed_ref_macro* ref_macro = llist_get(parsed.refs_macros, line->val);
				if (!ref_macro) {
					error_init(err, ERRVAL_FAILURE, "Macro reference not in list: %zu", line->val);
					return line->line_num;
				}

				// Get macro definition using macro reference key
				struct parsed_def_macro* def_macro = parsed_def_macro_get(defs_macros, ref_macro->key);
				if (!def_macro) {
					error_init(err, ERRVAL_SYNTAX, "Macro reference not defined: '%s'", ref_macro->key);
					return line->line_num;
				}

				if (def_macro->params.len > 0 || ref_macro->params.len > 0) {
					// TODO: Support macro parameters
				}

				// Build initial expanded macro and push to list
				struct expanded_base macro_expanded_init = { .parent = expanded, .line_num = line->line_num };
				struct expanded_base* macro_expanded = llist_push(&expanded->macros, &macro_expanded_init, sizeof(macro_expanded_init));
				if (!macro_expanded) {
					error_init(err, ERRVAL_FAILURE, "Failed to push expanded macro to list");
					return line->line_num;
				}

				// Push line referencing expanded macro
				if (!lines_push(err, &expanded->lines, line->type, line->line_num, expanded->macros.len - 1))
					return line->line_num;

				// Recusively build expanded macro
				size_t macro_expanded_result = expand_parsed(err, macro_expanded, def_macro->base, defs_macros);
				if (macro_expanded_result > 0)
					return macro_expanded_result;

				expanded->inst_len += macro_expanded->inst_len;
				break;

			default:
				error_init(err, ERRVAL_FAILURE, "Unknown line type: %d", line->type);
				return line->line_num;
		}
	}

	// Use number of existing instructions as initial program counter offset
	size_t pc_offset = 0;
	for (struct expanded_base* parent = expanded->parent; parent; parent = parent->parent) {
		pc_offset += parent->inst_len;
	}

	// Iterate through data definitions
	struct llist_node* macro_node = expanded->macros.head;
	struct llist_node* data_node = parsed.defs_data.head;
	for (size_t data_ind = 0; data_node && data_ind < parsed.defs_data.len; data_node = data_node->next, data_ind++) {
		struct parsed_def_data* data = data_node->val;

		// Add number of instructions in macros referenced beforehand to program counter offset
		if (macro_node) {
			struct expanded_base* macro = macro_node->val;
			if (data->line_num >= macro->line_num) {
				pc_offset += macro->inst_len;
				macro_node = macro_node->next;
			}
		}

		switch (data->type) {
			case DATA_CONST_E:
				// Push data definition as-is
				if (!llist_push(&expanded->defs_data, data, sizeof(*data))) {
					error_init(err, ERRVAL_FAILURE, "Failed to push data definition to list");
					return data->line_num;
				}

				break;

			case DATA_LABEL_E:
				// No program counter offset to account for - push data definition as-is
				if (pc_offset == 0) {
					if (!llist_push(&expanded->defs_data, data, sizeof(*data))) {
						error_init(err, ERRVAL_FAILURE, "Failed to push data definition to list");
						return data->line_num;
					}

					break;
				}

				// Assemble copy of data definition with program counter offset added
				struct parsed_def_data data_offset = { .type = data->type, .line_num = data->line_num, .val = data->val + pc_offset };
				if (!str_copy(data_offset.key, data->key, STR_SIZE(strlen(data->key)))) {
					error_init(err, ERRVAL_FAILURE, "Failed to copy string");
					return data->line_num;
				}

				// Push data definition
				if (!llist_push(&expanded->defs_data, &data_offset, sizeof(data_offset))) {
					error_init(err, ERRVAL_FAILURE, "Failed to push data definition to list");
					return data->line_num;
				}

				break;

			default:
				error_init(err, ERRVAL_FAILURE, "Unknown data definition type: %d", data->type);
				return data->line_num;
		}
	}

	return 0;
}

/**
 * Push NGC instruction.
 *
 * @param err Struct to store error.
 * @param instructions Linked list to push NGC instruction.
 * @param inst NGC instruction.
 * @returns Whether NGC instruction was pushed successfully.
 */
static bool inst_push(struct error* err, struct llist* instructions, const ngc_word_t inst)
{
	if (!llist_push(instructions, &inst, sizeof(inst))) {
		error_init(err, ERRVAL_FAILURE, "Failed to push instruction to list");
		return false;
	}

	return true;
}

/**
 * Assemble expanded/unwound assembly to NGC instructions.
 *
 * @param err Struct to store error.
 * @param instructions Linked list to push NGC instructions.
 * @param expanded Expanded/unwound assembly.
 * @returns 0 if successfully assembled. >0 line number if error.
 */
static size_t assemble_expanded(struct error* err, struct llist* instructions, const struct expanded_base expanded)
{
	struct llist_node* line_node = expanded.lines.head;
	for (size_t lines_ind = 0; line_node && lines_ind < expanded.lines.len; line_node = line_node->next, lines_ind++) {
		struct parsed_line* line = line_node->val;

		switch (line->type) {
			case LINE_INST_E:
				if (!inst_push(err, instructions, (ngc_word_t)line->val))
					return line->line_num;

				break;

			case LINE_REF_DATA_E:
				;
				// Get referenced data key at given index
				char* data_key = llist_get(expanded.refs_data, line->val);
				if (!data_key) {
					error_init(err, ERRVAL_FAILURE, "Data reference index not in list: %zu", line->val);
					return line->line_num;
				}

				// Try find data definition within current scope using referenced data key
				struct parsed_def_data* def_data = parsed_def_data_get(expanded.defs_data, data_key);

				// Data definition not within current scope - try find data definition within root (file) scope using referenced data key
				if (!def_data && expanded.parent) {
					struct expanded_base* root = expanded.parent;
					for (; root->parent; root = root->parent);
					def_data = parsed_def_data_get(root->defs_data, data_key);
				}

				// Data definition not within any scope
				if (!def_data) {
					error_init(err, ERRVAL_SYNTAX, "Data reference not defined: '%s'", data_key);
					return line->line_num;
				}

				// Push data value as NGC data instruction
				if (!inst_push(err, instructions, (ngc_word_t)def_data->val))
					return line->line_num;

				break;

			case LINE_REF_MACRO_E:
				;
				// Get referenced expanded macro at given index
				struct expanded_base* macro = llist_get(expanded.macros, line->val);
				if (!macro) {
					error_init(err, ERRVAL_FAILURE, "Macro index not in list: %zu", line->val);
					return line->line_num;
				}

				// Recursively assemble macro
				size_t macro_assemble_result = assemble_expanded(err, instructions, *macro);
				if (macro_assemble_result > 0)
					return macro_assemble_result;

				break;

			default:
				error_init(err, ERRVAL_FAILURE, "Unknown line type: %d", line->type);
				return line->line_num;
		}

		if (instructions->len > NGC_UWORD_MAX) {
			error_init(err, ERRVAL_FILE, "File contains too many instructions (max %ld)", NGC_UWORD_MAX);
			return line->line_num;
		}
	}

	return 0;
}

size_t assemble_file_full(struct error* err, struct llist* instructions, const struct parsed_file file)
{
	size_t result = 1;
	struct expanded_base file_expanded = { 0 };

	if (!instructions) {
		error_init(err, ERRVAL_FAILURE, "Result list is null");
		goto exit;
	}

	// Expand/unwind macros from parsed result
	result = expand_parsed(err, &file_expanded, file.base, file.defs_macros);
	if (result > 0)
		goto exit;

	// Assemble instructions from expanded/unwound result
	result = assemble_expanded(err, instructions, file_expanded);
	if (result > 0)
		goto exit;

	// Success
	result = 0;

	exit:
	expanded_base_empty(&file_expanded);
	return result;
}
