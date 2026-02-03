#include "assemble_scoped.h"

#include "assemble_common.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define PARSED_INIT(parsed_base) { .lines = parsed_base.lines, .refs_data = parsed_base.refs_data, .refs_macros = parsed_base.refs_macros }

struct parsed {
	const struct llist lines; // llist of parsed_line
	const struct llist refs_data; // llist of char[PARSED_KEY_CHARS]
	const struct llist refs_macros; // llist of parsed_ref_macro
};

struct scope {
	struct llist defs_data; // llist of parsed_def_data
	const struct llist defs_macros; // llist of parsed_def_macro
	const size_t pc_offset;
};

static bool list_copy(struct error* err, struct llist* dst, const struct llist src)
{
	if (llist_copy(dst, src) < src.len) {
		error_init(err, ERRVAL_FAILURE, "Failed to copy values between lists");
		return false;
	}

	return true;
}

/**
 * Get macro definition.
 *
 * @param defs_macros Linked list of macro definitions.
 * @param key Key of macro to get.
 * @returns Pointer to macro definition. NULL if not found.
 */
static struct parsed_def_macro* def_macro_get(const struct llist defs_macros, const char* key)
{
	assert(key);

	struct llist_node* macro_node = defs_macros.head;
	for (size_t macro_ind = 0; macro_node && macro_ind < defs_macros.len; macro_node = macro_node->next, macro_ind++) {
		struct parsed_def_macro* macro = (struct parsed_def_macro*)macro_node->val;
		if (PARSED_KEYS_EQ(macro->key, key))
			return macro;
	}

	return NULL;
}

/**
 * Assemble data instruction.
 *
 * @param err Struct to store error.
 * @param pc_offset Program counter offset.
 * @param defs_data Linked list of data definitions.
 * @param refs_data Linked list of data references.
 * @param refs_data_ind Index of data reference to assemble.
 * @returns Value of data instruction. -1 if error.
 */
static long assemble_data(struct error* err, const size_t pc_offset, const struct llist defs_data, const struct llist refs_data, const size_t refs_data_ind)
{
	// Get referenced data definition
	struct parsed_def_data* def_data = assemble_ref_data(err, defs_data, refs_data, refs_data_ind);
	if (!def_data)
		return -1;

	// Return data value based on type
	switch (def_data->type) {
		case DATA_CONST_E:
			return def_data->val;
		case DATA_LABEL_E:
			return def_data->val + pc_offset;
		default:
			error_init(err, ERRVAL_FAILURE, "Unknown data definition type: %d", def_data->type);
			return -1;
	}
}

/**
 * Assemble data definitions given in macro parameter references.
 *
 * @param err Struct to store error.
 * @param macro_defs_data Linked list to push assembled result.
 * @param def_macro Macro definition.
 * @param ref_macro Macro reference.
 * @param pc_offset Program counter offset.
 * @param defs_data Linked list of data definitions.
 * @param refs_data Linked list of data references.
 * @returns Whether data definitions were assembled successfully.
 */
static bool assemble_macro_params_defs_data(struct error* err, struct llist* macro_defs_data, const struct parsed_def_macro def_macro, const struct parsed_ref_macro ref_macro, const size_t pc_offset, const struct llist defs_data, const struct llist refs_data)
{
	assert(macro_defs_data);

	size_t macro_defs_data_len_prev = macro_defs_data->len;

	if (ref_macro.params.len < def_macro.params.len) {
		error_init(err, ERRVAL_SYNTAX, "Macro reference has %zu parameter(s) fewer than required: '%s'", def_macro.params.len - ref_macro.params.len, def_macro.key);
		goto error;
	} else if (ref_macro.params.len > def_macro.params.len) {
		error_init(err, ERRVAL_SYNTAX, "Macro reference has %zu parameter(s) more than required: '%s'", ref_macro.params.len - def_macro.params.len, def_macro.key);
		goto error;
	}

	struct llist_node* ref_param_node = ref_macro.params.head;
	struct llist_node* def_param_node = def_macro.params.head;
	for (size_t param_ind = 0; param_ind < def_macro.params.len; ref_param_node = ref_param_node->next, def_param_node = def_param_node->next, param_ind++) {
		struct parsed_ref_macro_param* ref_param = (struct parsed_ref_macro_param*)ref_param_node->val;
		char* def_param = (char*)def_param_node->val;

		struct parsed_def_data macro_def_data = { .type = DATA_CONST_E };

		size_t def_param_len = strlen(def_param);

		// Use parameter key from macro definition as key
		if (!str_copy(macro_def_data.key, def_param, def_param_len)) {
			error_init(err, ERRVAL_FAILURE, "Failed to copy string");
			goto error;
		}

		switch (ref_param->type) {
			case PARAM_CONST_E:
				// Constant value being passed to param - set value of result
				macro_def_data.val = ref_param->val;
				break;
			case PARAM_REF_DATA_E:
				;
				// Assemble referenced data instruction
				long data = assemble_data(err, pc_offset, defs_data, refs_data, ref_param->val);
				if (data < 0)
					goto error;

				macro_def_data.val = (size_t)data;
				break;
			default:
				error_init(err, ERRVAL_FAILURE, "Unknown parameter reference type: %d", ref_param->type);
				goto error;
		}

		if (!llist_push(macro_defs_data, &macro_def_data, sizeof(macro_def_data))) {
			error_init(err, ERRVAL_FAILURE, "Failed to push data definition to list");
			goto error;
		}
	}

	return true;

	error:
	llist_part_empty(macro_defs_data, macro_defs_data_len_prev);
	return false;
}

/**
 * Assemble data definitions for macro scope.
 *
 * @param err Struct to store error.
 * @param macro_defs_data Linked list to push assembled result.
 * @param def_macro Macro definition.
 * @param ref_macro Macro reference.
 * @param pc_offset Program counter offset.
 * @param defs_data Linked list of data definitions.
 * @param refs_data Linked list of data references.
 * @returns Whether data definitions were assembled successfully.
 */
static bool assemble_macro_defs_data(struct error* err, struct llist* macro_defs_data, const struct parsed_def_macro def_macro, const struct parsed_ref_macro ref_macro, const size_t pc_offset, const struct llist defs_data, const struct llist refs_data)
{
	assert(macro_defs_data);

	size_t macro_defs_data_len_prev = macro_defs_data->len;

	// Assemble list of data definitions given in macro parameter references
	if (!assemble_macro_params_defs_data(err, macro_defs_data, def_macro, ref_macro, pc_offset, defs_data, refs_data))
		goto error;

	// Copy data definitions from macro definition to macro scope
	if (!list_copy(err, macro_defs_data, def_macro.base.defs_data))
		goto error;

	// Copy data definitions from current scope to macro scope
	if (!list_copy(err, macro_defs_data, defs_data))
		goto error;

	return true;

	error:
	llist_part_empty(macro_defs_data, macro_defs_data_len_prev);
	return false;
}

/**
 * Assemble parsed result to NGC instructions.
 *
 * @param err Struct to store error.
 * @param instructions Linked list to push NGC instructions.
 * @param parsed Parsed result.
 * @param scope Scope parsed result is assembled in.
 * @returns 0 if successfully assembled. >0 line number if error.
 */
static size_t assemble_parsed(struct error* err, struct llist* instructions, const struct parsed parsed, const struct scope scope)
{
	size_t pc_offset = scope.pc_offset;

	struct llist_node* line_node = parsed.lines.head;
	for (size_t lines_ind = 0; line_node && lines_ind < parsed.lines.len; line_node = line_node->next, lines_ind++) {
		struct parsed_line* line = (struct parsed_line*)(line_node->val);

		switch (line->type) {
			case LINE_INST_E:
				// Push instruction
				if (!inst_push(err, instructions, (ngc_word_t)line->val))
					return line->line_num;

				break;

			case LINE_REF_DATA_E:
				;
				// Assemble referenced data instruction
				long data = assemble_data(err, pc_offset, scope.defs_data, parsed.refs_data, line->val);
				if (data < 0)
					return line->line_num;

				// Push data instruction
				if (!inst_push(err, instructions, (ngc_word_t)data))
					return line->line_num;

				break;

			case LINE_REF_MACRO_E:
				;
				// Get macro referenced by line
				struct parsed_ref_macro* ref_macro = (struct parsed_ref_macro*)llist_get(parsed.refs_macros, line->val);
				if (!ref_macro) {
					error_init(err, ERRVAL_FAILURE, "Macro reference not in list: %zu", line->val);
					return line->line_num;
				}

				// Get macro definition using macro reference key
				struct parsed_def_macro* def_macro = def_macro_get(scope.defs_macros, ref_macro->key);
				if (!def_macro) {
					error_init(err, ERRVAL_SYNTAX, "Macro reference not defined: '%s'", ref_macro->key);
					return line->line_num;
				}

				// Assemble result and scope of macro
				struct parsed macro_parsed = PARSED_INIT(def_macro->base);
				struct scope macro_scope = {
					.defs_data = { 0 },
					.defs_macros = scope.defs_macros,
					.pc_offset = pc_offset
				};
				if (!assemble_macro_defs_data(err, &macro_scope.defs_data, *def_macro, *ref_macro, macro_scope.pc_offset, scope.defs_data, parsed.refs_data))
					return line->line_num;

				size_t instructions_len_prev = instructions->len;
				size_t macro_assemble_result = assemble_parsed(err, instructions, macro_parsed, macro_scope);
				llist_empty(&macro_scope.defs_data);

				if (macro_assemble_result > 0)
					return macro_assemble_result;

				// Add number of instructions assembled by macro to program counter offset
				pc_offset += instructions->len - instructions_len_prev;

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

size_t assemble_file_scoped(struct error* err, struct llist* instructions, const struct parsed_file file)
{
	if (!instructions) {
		error_init(err, ERRVAL_FAILURE, "Result list is null");
		return 1;
	}

	// Assemble result and scope of file
	struct parsed parsed = PARSED_INIT(file.base);
	struct scope scope = {
		.defs_data = file.base.defs_data,
		.defs_macros = file.defs_macros,
		.pc_offset = 0
	};

	return assemble_parsed(err, instructions, parsed, scope);
}
