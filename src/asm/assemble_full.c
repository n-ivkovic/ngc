#include "assemble_full.h"

#include "../ngc.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/**
 * Expanded parsed assembly line.
 * Same struct as parsed_line, but redefined due to indexes referring to different lists.
 */
struct expanded_line {
	enum parsed_line_type type;
	size_t line_num;
	size_t val; // Parsed instruction if type is LINE_INST_E, index of expanded_base.refs_data if type is LINE_REF_DATA_E, index of expanded_base.macros if type is LINE_REF_MACRO_E.
};

/**
 * Expanded parsed macro parameter.
 */
struct expanded_macro_param {
	char key[PARSED_KEY_CHARS];
	enum parsed_ref_macro_param_type type;
	size_t val; // Parsed value if type is PARAM_CONST_E, index of expanded_base.parent->refs_data if type is PARAM_REF_DATA_E
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
	struct llist params; // llist of expanded_macro_param
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
	llist_empty(&base->params);
	llist_empty(&base->refs_data);
	llist_empty(&base->defs_data);
}

void expanded_base_free(struct expanded_base* base)
{
    if (!base) return;

    expanded_base_empty(base);
    free(base);
}


/**
 * Get expanded macro parameter from list using key.
 *
 * @param defs_macros Linked list of expanded macro parameters.
 * @param key Key of macro parameter to get.
 * @returns Pointer to expanded macro parameter. NULL if not found.
 */
static struct expanded_macro_param* expanded_macro_param_get(const struct llist params, const char* key)
{
	if (!key)
        return NULL;

    struct llist_node* param_node = params.head;
	for (size_t param_ind = 0; param_node && param_ind < params.len; param_node = param_node->next, param_ind++) {
		struct expanded_macro_param* param = param_node->val;
		if (PARSED_KEYS_EQ(param->key, key))
			return param;
	}

	return NULL;
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

	// Copy data references as-is
	if (llist_copy(&expanded->refs_data, parsed.refs_data) < parsed.refs_data.len) {
		error_init(err, ERRVAL_FAILURE, "Failed to copy data references to list");
		return (expanded->line_num > 0) ? expanded->line_num : 1;
	}

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
					error_init(err, ERRVAL_FAILURE, "Macro reference index not in list: %zu", line->val);
					return line->line_num;
				}

				// Get macro definition using macro reference key
				struct parsed_def_macro* def_macro = parsed_def_macro_get(defs_macros, ref_macro->key);
				if (!def_macro) {
					error_init(err, ERRVAL_SYNTAX, "Macro reference not defined: '%s'", ref_macro->key);
					return line->line_num;
				}

				// Build initial expanded macro
				struct expanded_base macro_expanded_init = { .parent = expanded, .line_num = line->line_num };

				// Expand macro parameters
				if (ref_macro->params.len > 0 || def_macro->params.len > 0) {
					// Validate correct number of parameters are provided
					if (ref_macro->params.len < def_macro->params.len) {
						error_init(err, ERRVAL_SYNTAX, "Macro reference has %zu too few parameters", def_macro->params.len - ref_macro->params.len);
						return line->line_num;
					} else if (ref_macro->params.len > def_macro->params.len) {
						error_init(err, ERRVAL_SYNTAX, "Macro reference has %zu too many parameters", ref_macro->params.len - def_macro->params.len);
						return line->line_num;
					}

					// Iterate through macro parameter definitions and references
					struct llist_node* param_ref_node = ref_macro->params.head;
					struct llist_node* param_def_node = def_macro->params.head;
					for (size_t param_ind = 0; param_ref_node && param_def_node && param_ind < ref_macro->params.len && param_ind < def_macro->params.len; param_ref_node = param_ref_node->next, param_def_node = param_def_node->next, param_ind++) {
						struct parsed_ref_macro_param* param_ref = param_ref_node->val;
						char* param_def = param_def_node->val;

						// Assemble expanded macro parameter
						struct expanded_macro_param param = { .type = param_ref->type, .val = param_ref->val };
						if (!str_copy(param.key, param_def, STR_CHARS(strlen(param_def)))) {
							error_init(err, ERRVAL_FAILURE, "Failed to copy string");
							return line->line_num;
						}

						// Push expanded macro parameter
						if (!llist_push(&macro_expanded_init.params, &param, sizeof(param))) {
							error_init(err, ERRVAL_FAILURE, "Failed to push expanded macro parameter to list");
							return line->line_num;
						}
					}
				}

				// Push initial expended macro to list
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

		// Validate no macro parameter with same key already exists
		if (expanded->params.len > 0 && expanded_macro_param_get(expanded->params, data->key)) {
			error_init(err, ERRVAL_SYNTAX, "Conflicting key given in DEFINE or LABEL statement, first used in macro parameter: '%s'", data->key);
			return data->line_num;
		}

		// Add number of instructions in macros referenced beforehand to program counter offset
		if (macro_node) {
			struct expanded_base* macro = macro_node->val;
			while (macro_node && data->line_num >= macro->line_num) {
				pc_offset += macro->inst_len;
				macro_node = macro_node->next;
				macro = macro_node->val;
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
 * Assemble expanded/unwound data reference.
 *
 * @param err Struct to store error.
 * @param expanded Expanded/unwound assembly.
 * @param root Expanded/unwound root/file assembly. Should be NULL if `expanded` is the root/file.
 * @param key Key of data reference to get.
 * @returns NGC data instruction. -1 if error.
 */
static long long assemble_ref_data(struct error* err, const struct expanded_base expanded, const struct expanded_base* root, const char* key)
{
	assert(key);
	if (root)
		assert(!root->parent);

	// Try find macro parameter with matching key
	if (expanded.parent && expanded.params.len > 0) {
		struct expanded_macro_param* macro_param = expanded_macro_param_get(expanded.params, key);
		if (macro_param) {
			switch (macro_param->type) {
				case PARAM_CONST_E:
					return (long long)macro_param->val;

				case PARAM_REF_DATA_E:
					;
					// Get data key passed as macro parameter
					char* parent_key = llist_get(expanded.parent->refs_data, macro_param->val);
					if (!parent_key) {
						error_init(err, ERRVAL_FAILURE, "Data reference not in list: %zu", macro_param->val);
						return -1;
					}

					// Assemble data reference passed as macro parameter
					return assemble_ref_data(err, *expanded.parent, expanded.parent->parent ? root : NULL, parent_key);

				default:
					error_init(err, ERRVAL_FAILURE, "Unknown expanded macro parameter type: %d", macro_param->type);
					return -1;
			}
		}
	}

	// Key does not refer to any macro parameter - get data definition within current scope with matching key
	struct parsed_def_data* data = parsed_def_data_get(expanded.defs_data, key);

	// No other scope to check (currently within root/file scope) - return data found within current scope
	if (data && !root)
		return (long long)data->val;

	if (root) {
		// Get data definition within root/file scope with matching key
		struct parsed_def_data* data_root = parsed_def_data_get(root->defs_data, key);

		// No data found in root/file scope - return data found within current scope
		if (data && !data_root)
			return (long long)data->val;

		// No data found within current scope - return data found within root/file scope
		if (data_root && !data)
			return (long long)data_root->val;

		// Conflicting data found in current and root/file scopes
		if (data && data_root) {
			error_init(err, ERRVAL_SYNTAX, "Conflicting key given in DEFINE or LABEL statement, first used on line %zu: '%s'", data_root->line_num, data_root->key);
			return -1;
		}
	}

	error_init(err, ERRVAL_SYNTAX, "Data reference not defined: '%s'", key);
	return -1;
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
	// Find root/file scope
	struct expanded_base* root = NULL;
	if (expanded.parent)
		for (root = expanded.parent; root->parent; root = root->parent);

	// Iterate through lines
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

				// Assemble data value
				long long data_val = assemble_ref_data(err, expanded, root, data_key);
				if (data_val < 0)
					return line->line_num;

				// Push data value
				if (!inst_push(err, instructions, (ngc_word_t)data_val))
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
			error_init(err, ERRVAL_FILE, "File contains too many instructions (max %zu)", NGC_UWORD_MAX);
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
