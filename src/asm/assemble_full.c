#include "assemble_full.h"

#include "../ngc.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/**
 * Expanded parsed assembly line.
 * Same struct as parsed_line, but redefined due to indexes referring to different dynamic arrays.
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
	struct dynarr lines; // Dynamic array of expanded_line
	struct dynarr macros; // Dynamic array of expanded_base
	struct dynarr params; // Dynamic array of expanded_macro_param
	struct dynarr refs_data; // Dynamic array of char[PARSED_KEY_CHARS]
	struct dynarr defs_data; // Dynamic array of parsed_def_data
};

/**
 * Pre-allocate initial space for expanded assembly.
 *
 * @param base Expanded assembly to pre-allocate space for.
 * @param parsed Parsed assembly to determine size of space to pre-allocate.
 */
static struct expanded_base* expanded_base_alloc(struct expanded_base* base, const struct parsed_base parsed)
{
	if (!base)
		return NULL;

	// Failure to pre-allocate space for macros is critical
	// Dynamic array resizes done after pre-allocate invalidates any expanded_base.parent pointing to a macro
	// TODO: Rework expanded_base struct to handle dynamic array resizes
	if (parsed.refs_macros.len > 0 && dynarr_alloc(&base->macros, parsed.refs_macros.capacity, sizeof(struct expanded_base)) == 0)
		return NULL;

	// Failure to pre-allocate space is non-critical - not checking return results
	dynarr_alloc(&base->lines, parsed.lines.capacity, sizeof(struct expanded_line));
	dynarr_alloc(&base->refs_data, parsed.refs_data.capacity, sizeof(char[PARSED_KEY_CHARS]));
	dynarr_alloc(&base->defs_data, parsed.defs_data.capacity, sizeof(struct parsed_def_data));

	// No space pre-allocated for macro parameters

	return base;
}

/**
 * Free values within expanded assembly.
 */
static void expanded_base_empty(struct expanded_base* base)
{
	if (!base)
		return;

	base->parent = NULL;
	dynarr_empty(&base->lines);
	dynarr_delegate_empty(&base->macros, (void (*)(void *))expanded_base_empty);
	dynarr_empty(&base->params);
	dynarr_empty(&base->refs_data);
	dynarr_empty(&base->defs_data);
}

/**
 * Get expanded macro parameter from dynamic array using key.
 *
 * @param defs_macros Dynamic array of expanded macro parameters.
 * @param key Key of macro parameter to get.
 * @returns Pointer to expanded macro parameter. NULL if not found.
 */
static struct expanded_macro_param* expanded_macro_param_get(const struct dynarr params, const char* key)
{
	if (!key)
		return NULL;

	for (size_t param_ind = 0; param_ind < params.len; param_ind++) {
		struct expanded_macro_param* param = dynarr_get(params, param_ind);
		if (!param)
			continue;

		if (PARSED_KEYS_EQ(param->key, key))
			return param;
	}

	return NULL;
}

/**
 * Push expanded line.
 *
 * @param err Struct to store error.
 * @param lines Dynamic array to store expanded result.
 * @param type Type of expanded line.
 * @param line_num Number of line in file.
 * @param val Expanded line value.
 * @returns Whether expanded line was pushed successfully.
 */
static bool lines_push(struct error* err, struct dynarr* lines, const enum parsed_line_type type, const size_t line_num, const size_t val)
{
	struct expanded_line result = { .type = type, .line_num = line_num, .val = val };
	if (!dynarr_push(lines, &result, sizeof(result))) {
		error_init(err, ERRVAL_FAILURE, "Failed to push expanded line");
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
 * @param defs_macros Dynamic array of parsed macro definitions.
 * @returns 0 if successfully expanded/unwound. >0 line number if error.
 */
static size_t expand_parsed(struct error* err, struct expanded_base* expanded, const struct parsed_base parsed, const struct dynarr defs_macros)
{
	assert(expanded);

	// Copy data references as-is
	if (dynarr_copy(&expanded->refs_data, parsed.refs_data) < parsed.refs_data.len) {
		error_init(err, ERRVAL_FAILURE, "Failed to copy data references");
		return (expanded->line_num > 0) ? expanded->line_num : 1;
	}

	// Iterate through lines
	for (size_t lines_ind = 0; lines_ind < parsed.lines.len; lines_ind++) {
		struct parsed_line* line = dynarr_get(parsed.lines, lines_ind);
		if (!line)
			continue;

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
				struct parsed_ref_macro* ref_macro = dynarr_get(parsed.refs_macros, line->val);
				if (!ref_macro) {
					error_init(err, ERRVAL_FAILURE, "Macro reference index out of range: %zu", line->val);
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
				if (!expanded_base_alloc(&macro_expanded_init, def_macro->base)) {
					error_init(err, ERRVAL_FAILURE, "Failed to init expanded macro struct");
					return line->line_num;
				}

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
					for (size_t param_ind = 0; param_ind < ref_macro->params.len && param_ind < def_macro->params.len; param_ind++) {
						struct parsed_ref_macro_param* param_ref = dynarr_get(ref_macro->params, param_ind);
						char* param_def = dynarr_get(def_macro->params, param_ind);

						if (!param_ref || !param_def)
							continue;

						// Assemble expanded macro parameter
						struct expanded_macro_param param = { .type = param_ref->type, .val = param_ref->val };
						if (!str_copy(param.key, param_def, STR_CHARS(strlen(param_def)))) {
							error_init(err, ERRVAL_FAILURE, "Failed to copy string");
							return line->line_num;
						}

						// Push expanded macro parameter
						if (!dynarr_push(&macro_expanded_init.params, &param, sizeof(param))) {
							error_init(err, ERRVAL_FAILURE, "Failed to push expanded macro parameter");
							return line->line_num;
						}
					}
				}

				// Push initial expended macro to array
				struct expanded_base* macro_expanded = dynarr_push(&expanded->macros, &macro_expanded_init, sizeof(macro_expanded_init));
				if (!macro_expanded) {
					error_init(err, ERRVAL_FAILURE, "Failed to push expanded macro");
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

	// Get first expanded macro, used to update program counter offset
	size_t macro_ind = 0;
	struct expanded_base* macro = dynarr_get(expanded->macros, macro_ind);

	// Iterate through data definitions
	for (size_t data_ind = 0; data_ind < parsed.defs_data.len; data_ind++) {
		struct parsed_def_data* data = dynarr_get(parsed.defs_data, data_ind);
		if (!data)
			continue;

		// Validate no macro parameter with same key already exists
		if (expanded->params.len > 0 && expanded_macro_param_get(expanded->params, data->key)) {
			error_init(err, ERRVAL_SYNTAX, "Conflicting key given in DEFINE or LABEL statement, first used in macro parameter: '%s'", data->key);
			return data->line_num;
		}

		// Add number of instructions in macros referenced beforehand to program counter offset
		while (macro && data->line_num >= macro->line_num) {
			pc_offset += macro->inst_len;
			macro_ind++;
			macro = dynarr_get(expanded->macros, macro_ind);
		}

		switch (data->type) {
			case DATA_CONST_E:
				// Push data definition as-is
				if (!dynarr_push(&expanded->defs_data, data, sizeof(*data))) {
					error_init(err, ERRVAL_FAILURE, "Failed to push data definition");
					return data->line_num;
				}

				break;

			case DATA_LABEL_E:
				// No program counter offset to account for - push data definition as-is
				if (pc_offset == 0) {
					if (!dynarr_push(&expanded->defs_data, data, sizeof(*data))) {
						error_init(err, ERRVAL_FAILURE, "Failed to push data definition");
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
				if (!dynarr_push(&expanded->defs_data, &data_offset, sizeof(data_offset))) {
					error_init(err, ERRVAL_FAILURE, "Failed to push data definition");
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
 * @param data_val Int to store NGC data instruction.
 * @param expanded Expanded/unwound assembly.
 * @param root Expanded/unwound root/file assembly. Should be NULL if `expanded` is the root/file.
 * @param line_num Number of line in file.
 * @param key Key of data reference to get.
 * @returns 0 if successfully assembled. >0 line number if error.
 */
static size_t assemble_ref_data(struct error* err, size_t* data_val, const struct expanded_base expanded, const struct expanded_base* root, const size_t line_num, const char* key)
{
	assert(key);

	if (root) {
		assert(!root->parent);
		assert(expanded.parent);
	} else {
		assert(!expanded.parent);
	}

	// Try find macro parameter with matching key
	if (expanded.parent && expanded.params.len > 0) {
		struct expanded_macro_param* macro_param = expanded_macro_param_get(expanded.params, key);
		if (macro_param) {
			switch (macro_param->type) {
				case PARAM_CONST_E:
					*data_val = macro_param->val;
					return 0;

				case PARAM_REF_DATA_E:
					;
					// Get data key passed as macro parameter
					char* parent_key = dynarr_get(expanded.parent->refs_data, macro_param->val);
					if (!parent_key) {
						error_init(err, ERRVAL_FAILURE, "Data reference index out of range: %zu", macro_param->val);
						return line_num;
					}

					// Assemble data reference passed as macro parameter
					return assemble_ref_data(err, data_val, *expanded.parent, expanded.parent->parent ? root : NULL, line_num, parent_key);

				default:
					error_init(err, ERRVAL_FAILURE, "Unknown expanded macro parameter type: %d", macro_param->type);
					return line_num;
			}
		}
	}

	// Key does not refer to any macro parameter - get data definition within current scope with matching key
	struct parsed_def_data* data = parsed_def_data_get(expanded.defs_data, key);

	// No other scope to check (currently within root/file scope) - return data found within current scope
	if (data && !root) {
		*data_val = data->val;
		return 0;
	}

	if (root) {
		// Get data definition within root/file scope with matching key
		struct parsed_def_data* data_root = parsed_def_data_get(root->defs_data, key);

		// No data found in root/file scope - return data found within current scope
		if (data && !data_root) {
			*data_val = data->val;
			return 0;
		}

		// No data found within current scope - return data found within root/file scope
		if (data_root && !data) {
			*data_val = data_root->val;
			return 0;
		}

		// Conflicting data found in current and root/file scopes
		if (data && data_root) {
			error_init(err, ERRVAL_SYNTAX, "Conflicting key given in DEFINE or LABEL statement, first used on line %zu: '%s'", data_root->line_num, data_root->key);
			return data->line_num;
		}
	}

	error_init(err, ERRVAL_SYNTAX, "Data reference not defined: '%s'", key);
	return line_num;
}

/**
 * Push NGC instruction.
 *
 * @param err Struct to store error.
 * @param instructions Dynamic array to push NGC instruction.
 * @param inst NGC instruction.
 * @returns Whether NGC instruction was pushed successfully.
 */
static bool inst_push(struct error* err, struct dynarr* instructions, const ngc_word_t inst)
{
	if (!dynarr_push(instructions, &inst, sizeof(inst))) {
		error_init(err, ERRVAL_FAILURE, "Failed to push assembled instruction");
		return false;
	}

	return true;
}

/**
 * Assemble expanded/unwound assembly to NGC instructions.
 *
 * @param err Struct to store error.
 * @param instructions Dynamic array to push NGC instructions.
 * @param expanded Expanded/unwound assembly.
 * @returns 0 if successfully assembled. >0 line number if error.
 */
static size_t assemble_expanded(struct error* err, struct dynarr* instructions, const struct expanded_base expanded)
{
	// Find root/file scope
	struct expanded_base* root = NULL;
	if (expanded.parent)
		for (root = expanded.parent; root->parent; root = root->parent);

	// Iterate through lines
	for (size_t lines_ind = 0; lines_ind < expanded.lines.len; lines_ind++) {
		struct parsed_line* line = dynarr_get(expanded.lines, lines_ind);
		if (!line)
			continue;

		switch (line->type) {
			case LINE_INST_E:
				if (!inst_push(err, instructions, (ngc_word_t)line->val))
					return line->line_num;

				break;

			case LINE_REF_DATA_E:
				;
				// Get referenced data key at given index
				char* data_key = dynarr_get(expanded.refs_data, line->val);
				if (!data_key) {
					error_init(err, ERRVAL_FAILURE, "Data reference index out of range: %zu", line->val);
					return line->line_num;
				}

				// Assemble data value
				size_t data_val;
				size_t data_result = assemble_ref_data(err, &data_val, expanded, root, line->line_num, data_key);
				if (data_result > 0)
					return data_result;

				// Push data value
				if (!inst_push(err, instructions, (ngc_word_t)data_val))
					return line->line_num;

				break;

			case LINE_REF_MACRO_E:
				;
				// Get referenced expanded macro at given index
				struct expanded_base* macro = dynarr_get(expanded.macros, line->val);
				if (!macro) {
					error_init(err, ERRVAL_FAILURE, "Macro index out of range: %zu", line->val);
					return line->line_num;
				}

				// Recursively assemble macro
				size_t macro_result = assemble_expanded(err, instructions, *macro);
				if (macro_result > 0)
					return macro_result;

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

size_t assemble_file_full(struct error* err, struct dynarr* instructions, const struct parsed_file file)
{
	size_t result = 1;
	struct expanded_base file_expanded = { 0 };
	if (!expanded_base_alloc(&file_expanded, file.base)) {
		error_init(err, ERRVAL_FAILURE, "Failed to init expanded file struct");
		goto exit;
	}

	if (!instructions) {
		error_init(err, ERRVAL_FAILURE, "Instructions array is null");
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
