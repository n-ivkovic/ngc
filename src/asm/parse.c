#include "parse.h"

#include "../ngc.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define FILE_LINES_MAX 0x100000
#define FILE_COLS_MAX (0xFF - 2)

enum parsed_scope {
	SCOPE_FILE_E,
	SCOPE_MACRO_E
};

enum parse_inst_alu_result {
	ALU_INST_FAILURE_E,
	ALU_INST_SUCCESS_E,
	ALU_INST_HINT_INST_DATA_E,
	ALU_INST_HINT_REF_MACRO_E
};

/**
 * Get whether token is a valid key.
 *
 * @param tok Token to validate.
 * @returns Whether token can be a valid key.
 */
static bool key_valid(const char* tok)
{
	// Handle NULL pointer
	if (!tok)
		return false;

	// Key must begin with alpha char (A-Z, a-z), or period (.)
	if (!isalpha(tok[0]) && tok[0] != '.')
		return false;

	size_t tok_len = strlen(tok);

	// Key cannot exceed max len
	if (tok_len > PARSED_KEY_LEN_MAX)
		return false;

	// Key cannot be A or D (lower case allowed)
	if (tok_len == 1 && (tok[0] == 'A' || tok[0] == 'D'))
		return false;

	// Key must only consist of alpha chars (A-Z, a-z), digit chars (0-9), underscores (_), and periods (.)
	for (size_t ind = 1; ind < tok_len; ind++) {
		if (!isalnum(tok[ind]) && tok[ind] != '_' && tok[ind] != '.')
			return false;
	}

	return true;
}

/**
 * Get index of key in linked list.
 *
 * @param key Key to search for
 * @param list Linked list to search.
 * @returns Index of key in linked list. -1 if not found.
 */
static long long keys_get(const char* key, const struct llist list)
{
	if (!key)
		return false;

	struct llist_node* node = list.head;
	for (size_t ind = 0; node && ind < list.len; node = node->next, ind++) {
		if (PARSED_KEYS_EQ((char*)node->val, key))
			return (long long)ind;
	}

	return -1;
}

/**
 * Push parsed line.
 *
 * @param err Struct to store error.
 * @param lines Linked list to store parsed result.
 * @param type Type of parsed line.
 * @param line_num Number of line in file.
 * @param val Parsed line value.
 * @returns Whether parsed line was pushed successfully.
 */
static bool lines_push(struct error* err, struct llist* lines, const enum parsed_line_type type, const size_t line_num, const size_t val)
{
	struct parsed_line result = { .type = type, .line_num = line_num, .val = val };
	if (!llist_push(lines, &result, sizeof(result))) {
		error_init(err, ERRVAL_FAILURE, "Failed to push parsed line to list");
		return false;
	}

	return true;
}

/**
 * Find or push parsed data reference.
 *
 * @param err Struct to store error.
 * @param refs_data Linked list to store parsed result.
 * @param key Parsed data key.
 * @param key_size Size of parsed data key.
 * @returns Index parsed result is located at in linked list. -1 if error occurred.
 */
static long long refs_data_push(struct error* err, struct llist* refs_data, const char* key, const size_t key_size)
{
	// Find if key already exists in data references list
	long long existing_ind = keys_get(key, *refs_data);
	if (existing_ind >= 0)
		return existing_ind;

	if (!llist_push(refs_data, key, key_size)) {
		error_init(err, ERRVAL_FAILURE, "Failed to push parsed data reference to list");
		return -1;
	}

	return (long long)refs_data->len - 1;
}

/**
 * Push parsed macro parameter reference.
 *
 * @param err Struct to store error.
 * @param refs_macro_params Linked list to store parsed result.
 * @param type Type of parsed macro parameter reference.
 * @param val Parsed macro parameter value.
 * @returns Whether parsed macro parameter was pushed successfully.
 */
static bool refs_macro_params_push(struct error* err, struct llist* refs_macro_params, const enum parsed_ref_macro_param_type type, const size_t val)
{
	struct parsed_ref_macro_param result = { .type = type, .val = val };
	if (!llist_push(refs_macro_params, &result, sizeof(result))) {
		error_init(err, ERRVAL_FAILURE, "Failed to push parsed macro parameter reference to list");
		return false;
	}

	return true;
}

static int is_uscore(int ch) { return ch == '_'; }

/**
 * Parse number, between 0 and NGC_WORD_MAX (0x7FFF) inclusive.
 *
 * @param tok Token to parse.
 * @returns Parsed number. -1 if error.
 */
static long parse_number(char* tok)
{
	#define PREFIX_LEN 2

	size_t tok_len = strlen(tok);
	if (tok_len < 1)
		return -1;

	size_t value_ind = 0;
	int value_base = 10;

	// Check if number has prefix indicating a different base
	if (tok_len >= (PREFIX_LEN + 1)) {
		// Copy prefix to new string
		char tok_prefix[STR_CHARS(PREFIX_LEN)] = { 0 };
		if (!str_copy(tok_prefix, tok, PREFIX_LEN))
			return -1;

		// Determine base from prefix
		switch (str_ull(tok_prefix)) {
			case 0x3058: // 0X
			case 0x3078: // 0x
				value_ind = PREFIX_LEN; // Numerical value begins after prefix
				value_base = 16;
				break;
			case 0x3042: // 0B
			case 0x3062: // 0b
				value_ind = PREFIX_LEN; // Numerical value begins after prefix
				value_base = 2;
				break;
		}
	}

	char* tok_in = tok;
	char tok_fmt[STR_CHARS(tok_len)];

	// Format prefixed numbers without prefix and underscores
	if (value_ind > 0) {
		if (!str_rm(tok_fmt, &tok[value_ind], tok_len, is_uscore))
			return -1;

		tok_in = tok_fmt;
	}

	char* tok_end_ptr = NULL;
	long result = strtol(tok_in, &tok_end_ptr, value_base);

	// Invalid
	if (*tok_end_ptr)
		return -1;

	// Invalid - value out of bounds
	if (result > NGC_WORD_MAX || result < 0)
		return -1;

	return (int)result;
}

/**
 * Parse key.
 *
 * @param err Struct to store error.
 * @param key String to copy key result to.
 * @param tok Token to parse.
 * @param msg Error message suffix.
 * @returns Whether key was valid and parsed successfully.
 */
static bool parse_key(struct error* err, char* key, const char* tok, const char* msg)
{
	// Validate token exists
	if (!tok) {
		error_init(err, ERRVAL_SYNTAX, "No key given in %s", msg);
		return false;
	}

	// Validate token can be a key
	if (!key_valid(tok)) {
		error_init(err, ERRVAL_SYNTAX, "Invalid key given in %s: '%s'", msg, tok);
		return false;
	}

	size_t key_len = strlen(tok);
	assert(key_len <= PARSED_KEY_LEN_MAX);

	// Copy token to key result
	if (!str_copy(key, tok, key_len)) {
		error_init(err, ERRVAL_FAILURE, "Failed to copy string");
		return false;
	}

	return true;
}

/**
 * Parse DEFINE statement.
 *
 * @param err Struct to store error.
 * @param defs_data Linked list to push parsed result to.
 * @param line_toks Linked list of tokens in file line.
 * @returns Whether DEFINE statement was valid and parsed successfully.
 */
static bool parse_def_data_define(struct error* err, struct llist* defs_data, const struct llist line_toks)
{
	#define TOKS_DEFINE_LEN 3

	assert(defs_data);

	struct parsed_def_data result = { .type = DATA_CONST_E };

	// Parse one token at a time
	struct llist_node* tok_node = line_toks.head;
	for (size_t tok_ind = 0; tok_ind <= TOKS_DEFINE_LEN; tok_node = (tok_node) ? tok_node->next : NULL, tok_ind++) {
		char* tok = tok_node ? (char*)(tok_node->val) : NULL;

		switch (tok_ind) {
			// DEFINE keyword - verified outside this function
			case 0:
				break;

			// Key
			case 1:
				if (!parse_key(err, result.key, tok, "DEFINE statement"))
					return false;

				break;

			// Value
			case 2:
				// Validate token exists
				if (!tok) {
					error_init(err, ERRVAL_SYNTAX, "No value given in DEFINE statement");
					return false;
				}

				// Parse + set token as number
				long parsed_number = parse_number(tok);
				if (parsed_number < 0) {
					error_init(err, ERRVAL_SYNTAX, "Invalid value given in DEFINE statement: '%s'", tok);
					return false;
				}

				result.val = parsed_number;
				break;

			// Invalid extranious token
			default:
				if (tok) {
					error_init(err, ERRVAL_SYNTAX, "Invalid value given in DEFINE statement: '%s'", tok);
					return false;
				}

				break;
		}
	}

	// Push parsed result
	if (!llist_push(defs_data, &result, sizeof(result))) {
		error_init(err, ERRVAL_FAILURE, "Failed to push parsed data definition to list");
		return false;
	}

	return true;
}

/**
 * Parse LABEL statement.
 *
 * @param err Struct to store error.
 * @param defs_data Linked list to push parsed result to.
 * @param toks Linked list of tokens in file line.
 * @returns Whether LABEL statement was valid and parsed successfully.
 */
static bool parse_def_data_label(struct error* err, struct llist* defs_data, const struct llist line_toks, const size_t inst_num)
{
	#define TOKS_LABEL_LEN 2

	struct parsed_def_data result = { .type = DATA_LABEL_E, .val = inst_num };

	struct llist_node* tok_node = line_toks.head;
	for (size_t tok_ind = 0; tok_ind <= TOKS_LABEL_LEN; tok_node = (tok_node) ? tok_node->next : NULL, tok_ind++) {
		char* tok = tok_node ? (char*)(tok_node->val) : NULL;

		switch (tok_ind) {
			// LABEL keyword - verified outside this function
			case 0:
				break;

			// Key
			case 1:
				if (!parse_key(err, result.key, tok, "LABEL statement"))
					return false;

				break;

			// Invalid extranious token
			default:
				if (tok) {
					error_init(err, ERRVAL_SYNTAX, "Invalid value given in LABEL statement: '%s'", tok);
					return false;
				}

				break;
		}
	}

	// Push parsed result
	if (!llist_push(defs_data, &result, sizeof(result))) {
		error_init(err, ERRVAL_FAILURE, "Failed to push parsed data definition to list");
		return false;
	}

	return true;
}

/**
 * Parse %MACRO statement.
 *
 * @param err Struct to store error.
 * @param defs_macros Linked list to push parsed result to.
 * @param line_toks Linked list of tokens in file line.
 * @returns Whether %MACRO statement was valid and parsed successfully.
 */
static bool parse_def_macro(struct error* err, struct llist* defs_macros, const struct llist line_toks)
{
	#define TOKS_DEF_MACRO_MIN 2

	assert(defs_macros);

	struct parsed_def_macro result = { 0 };

	struct llist_node* tok_node = line_toks.head;
	for (size_t tok_ind = 0; (tok_node && tok_ind < line_toks.len) || tok_ind < TOKS_DEF_MACRO_MIN; tok_node = (tok_node) ? tok_node->next : NULL, tok_ind++) {
		char* tok = tok_node ? (char*)(tok_node->val) : NULL;

		switch (tok_ind) {
			// %MACRO keyword - verified outside this function
			case 0:
				break;

			// Key
			case 1:
				if (!parse_key(err, result.key, tok, "%MACRO statement"))
					goto error;

				break;

			// Parameters
			default:
				;
				char param_key[PARSED_KEY_CHARS] = { 0 };
				if (!parse_key(err, param_key, tok, "%MACRO statement"))
					goto error;

				// Validate parameter key is not duplicate
				if (keys_get(param_key, result.params) >= 0) {
					error_init(err, ERRVAL_SYNTAX, "Duplicate parameters given in %%MACRO statement: '%s'", tok);
					goto error;
				}

				// Add macro parameter to list of parameters
				if (!llist_push(&result.params, param_key, sizeof(param_key))) {
					error_init(err, ERRVAL_FAILURE, "Failed to push parsed macro parameter definition to list");
					goto error;
				}

				break;
		}
	}

	// Push parsed result to list
	if (!llist_push(defs_macros, &result, sizeof(result))) {
		error_init(err, ERRVAL_FAILURE, "Failed to push parsed macro definition to list");
		goto error;
	}

	return true;

	error:
	parsed_def_macro_empty(&result);
	return false;
}

/**
 * Parse macro reference.
 *
 * @param err Struct to store error.
 * @param lines Linked list to push parsed result to.
 * @param refs_data Linked list to push parsed result to.
 * @param refs_macros Linked list to push parsed result to.
 * @param line_num Number of line in file.
 * @param line_toks Linked list of tokens in file line.
 * @returns Whether macro reference was valid and parsed successfully.
 */
static bool parse_ref_macro(struct error* err, struct llist* lines, struct llist* refs_data, struct llist* refs_macros, const size_t line_num, const struct llist line_toks)
{
	#define TOKS_REF_MACRO_MIN 1

	assert(refs_data);
	assert(refs_macros);

	struct parsed_ref_macro result = { 0 };

	struct llist_node* tok_node = line_toks.head;
	for (size_t tok_ind = 0; (tok_node && tok_ind < line_toks.len) || tok_ind < TOKS_REF_MACRO_MIN; tok_node = (tok_node) ? tok_node->next : NULL, tok_ind++) {
		char* tok = tok_node ? (char*)(tok_node->val) : NULL;

		switch (tok_ind) {
			// Key
			case 0:
				if (!parse_key(err, result.key, tok, "macro reference"))
					goto error;

				break;

			// Parameters
			default:
				;
				// Try parse parameter as number
				long parsed_number = parse_number(tok);
				if (parsed_number >= 0) {
					// Push parsed parameter result to list
					if (!refs_macro_params_push(err, &result.params, PARAM_CONST_E, (size_t)parsed_number))
						goto error;

					break;
				}

				// Parse parameter as data reference
				char ref_data_key[PARSED_KEY_CHARS] = { 0 };
				if (!parse_key(err, ref_data_key, tok, "macro reference"))
					goto error;

				// Push data reference result
				long long ref_data_ind = refs_data_push(err, refs_data, ref_data_key, sizeof(ref_data_key));
				if (ref_data_ind < 0)
					goto error;

				// Push parsed parameter result to list
				if (!refs_macro_params_push(err, &result.params, PARAM_REF_DATA_E, (size_t)ref_data_ind))
					goto error;

				break;
		}
	}

	// Push macro reference result
	if (!llist_push(refs_macros, &result, sizeof(result))) {
		error_init(err, ERRVAL_FAILURE, "Failed to push parsed macro reference to list");
		goto error;
	}

	// Push line result
	size_t refs_macros_ind = refs_macros->len - 1;
	if (!lines_push(err, lines, LINE_REF_MACRO_E, line_num, refs_macros_ind))
		goto error;

	return true;

	error:
	parsed_ref_macro_empty(&result);
	return false;
}

/**
 * Parse single target of ALU instruction assembly.
 *
 * @param tok Token to parse.
 * @returns NGC ALU instruction bit indicating target. -1 if error.
 */
static long parse_inst_alu_target(const char* tok)
{
	switch (str_ull(tok)) {
		case 0x0041: // A
			return NGC_IN_TARGET_A;
		case 0x0044: // D
			return NGC_IN_TARGET_D;
		case 0x2A41: // *A
			return NGC_IN_TARGET_AA;
		default: // Invalid target
			return -1;
	}
}

/**
 * Parse targets of ALU instruction assembly.
 *
 * @param tok Token to parse.
 * @returns NGC ALU instruction bits indicating targets (bits 4-6). -1 if error.
 */
static long parse_inst_alu_targets(const char* tok)
{
	// Invalid - null input
	if (!tok)
		return -1;

	#define TOK_TARGET_SEP ","
	#define TOK_TARGET_SEP_LEN strlen(TOK_TARGET_SEP)

	size_t tok_len = strlen(tok);
	char tok_copy[tok_len];
	if (!str_copy(tok_copy, tok, tok_len))
		return -1;

	int result = 0;
	size_t parsed_len = 0;
	char* target = strtok(tok_copy, TOK_TARGET_SEP);

	while (target) {
		int target_parsed = parse_inst_alu_target(target);

		// Invalid target
		if (target_parsed < 0)
			return -1;

		// Invalid - same target given multiple times
		if ((target_parsed & result) > 0)
			return -1;

		parsed_len += strlen(target);
		result |= target_parsed;

		target = strtok(NULL, TOK_TARGET_SEP);
		if (target)
			parsed_len += TOK_TARGET_SEP_LEN;
	}

	// Invalid - extranious separator
	if (parsed_len < tok_len)
		return -1;

	return result;
}

/**
 * Parse operation of ALU instruction assembly.
 *
 * @param tok Token to parse.
 * @returns NGC ALU instruction bits indicating operation (bits 7-13). -1 if error.
 */
static long parse_inst_alu_opr(const char* tok)
{
	switch (str_ull(tok)) {
		case 0x00000030: // 0
			return NGC_IN_OPR_ZX;
		case 0x00000031: // 1
			return NGC_IN_OPR_U | NGC_IN_OPR_OP0 | NGC_IN_OPR_ZX;
		case 0x00002D31: // -1
			return NGC_IN_OPR_NEG1;
		case 0x00000041: // A
			return NGC_IN_OPR_U | NGC_IN_OPR_ZX;
		case 0x00002D41: // -A
			return NGC_IN_OPR_U | NGC_IN_OPR_OP1 | NGC_IN_OPR_ZX;
		case 0x00007E41: // ~A
			return NGC_IN_OPR_OP1 | NGC_IN_OPR_OP0 | NGC_IN_OPR_SW;
		case 0x00412B31: // A+1
			return NGC_IN_OPR_U | NGC_IN_OPR_OP0 | NGC_IN_OPR_SW;
		case 0x00412D31: // A-1
			return NGC_IN_OPR_U | NGC_IN_OPR_OP1 | NGC_IN_OPR_OP0 | NGC_IN_OPR_SW;
		case 0x00412D44: // A-D
			return NGC_IN_OPR_U | NGC_IN_OPR_OP1 | NGC_IN_OPR_SW;
		case 0x00000044: // D
			return NGC_IN_OPR_U | NGC_IN_OPR_ZX | NGC_IN_OPR_SW;
		case 0x00002D44: // -D
			return NGC_IN_OPR_U | NGC_IN_OPR_OP1 | NGC_IN_OPR_ZX | NGC_IN_OPR_SW;
		case 0x00007E44: // ~D
			return NGC_IN_OPR_OP1 | NGC_IN_OPR_OP0;
		case 0x00442B31: // D+1
			return NGC_IN_OPR_U | NGC_IN_OPR_OP0;
		case 0x00442D31: // D-1
			return NGC_IN_OPR_U | NGC_IN_OPR_OP1 | NGC_IN_OPR_OP0;
		case 0x00442B41: // D+A
			return NGC_IN_OPR_U;
		case 0x442B2A41: // D+*A
			return NGC_IN_AA | NGC_IN_OPR_U;
		case 0x00442D41: // D-A
			return NGC_IN_OPR_U | NGC_IN_OPR_OP1;
		case 0x442D2A41: // D-*A
			return NGC_IN_AA | NGC_IN_OPR_U | NGC_IN_OPR_OP1;
		case 0x00442641: // D&A
			return 0x00;
		case 0x44262A41: // D&*A
			return NGC_IN_AA;
		case 0x00447C41: // D|A
			return NGC_IN_OPR_OP0;
		case 0x447C2A41: // D|*A
			return NGC_IN_AA | NGC_IN_OPR_OP0;
		case 0x00445E41: // D^A
			return NGC_IN_OPR_OP1;
		case 0x445E2A41: // D^*A
			return NGC_IN_AA | NGC_IN_OPR_OP1;
		case 0x00002A41: // *A
			return NGC_IN_AA | NGC_IN_OPR_U | NGC_IN_OPR_ZX;
		case 0x002D2A41: // -*A
			return NGC_IN_AA | NGC_IN_OPR_U | NGC_IN_OPR_OP1 | NGC_IN_OPR_ZX;
		case 0x007E2A41: // ~*A
			return NGC_IN_AA | NGC_IN_OPR_OP1 | NGC_IN_OPR_OP0 | NGC_IN_OPR_SW;
		case 0x2A412B31: // *A+1
			return NGC_IN_AA | NGC_IN_OPR_U | NGC_IN_OPR_OP0 | NGC_IN_OPR_SW;
		case 0x2A412D31: // *A-1
			return NGC_IN_AA | NGC_IN_OPR_U | NGC_IN_OPR_OP1 | NGC_IN_OPR_OP0 | NGC_IN_OPR_SW;
		case 0x2A412D44: // *A-D
			return NGC_IN_AA | NGC_IN_OPR_U | NGC_IN_OPR_OP1 | NGC_IN_OPR_SW;
		default: // Invalid operation
			return -1;
	}
}

/**
 * Parse jump condition of ALU instruction assembly.
 *
 * @param tok Token to parse.
 * @returns NGC ALU instruction bits indicating jump condition (bits 1-3). -1 if error.
 */
static long parse_inst_alu_jump(const char* tok)
{
	switch (str_ull(tok)) {
		case 0x4A4754: // JGT
			return NGC_IN_JUMP_GT;
		case 0x4A4551: // JEQ
			return NGC_IN_JUMP_EQ;
		case 0x4A4745: // JGE
			return NGC_IN_JUMP_EQ | NGC_IN_JUMP_GT;
		case 0x4A4C54: // JLT
			return NGC_IN_JUMP_LT;
		case 0x4A4E45: // JNE
			return NGC_IN_JUMP_LT | NGC_IN_JUMP_GT;
		case 0x4A4C45: // JLE
			return NGC_IN_JUMP_LT | NGC_IN_JUMP_EQ;
		case 0x4A4D50: // JMP
			return NGC_IN_JUMP_LT | NGC_IN_JUMP_EQ | NGC_IN_JUMP_GT;
		default: // Invalid condition
			return -1;
	}
}

/**
 * Parse ALU instruction assembly.
 *
 * @param err Struct to store error.
 * @param lines Linked list to push parsed result.
 * @param line_num Number of line in file.
 * @param line_st Assembly line with whitespace stripped.
 * @param line_len Length of assembly line.
 * @returns Enum value indicating whether ALU instruction was valid and parsed successfully, or hint if file line could be another kind of instruction.
 */
static enum parse_inst_alu_result parse_inst_alu(struct error* err, struct llist* lines, const size_t line_num, const char* line_st, const size_t line_len)
{
	// Original NandGame assembler sets bits 14-16 of ALU instructions to 1 despite only bit 16 being relevant
	#define NGC_IN_ALU (NGC_IN_CI | (1 << 14) | (1 << 13))

	assert(lines);

	// Special syntax case - line can be JMP only
	if (strncmp(line_st, "JMP", line_len) == 0)
	{
		// Original NandGame assembler sets operation bits of instruction to "-1"
		if (!lines_push(err, lines, LINE_INST_E, line_num, (size_t)(NGC_IN_ALU | NGC_IN_OPR_NEG1 | NGC_IN_JUMP_LT | NGC_IN_JUMP_EQ | NGC_IN_JUMP_GT)))
			return ALU_INST_FAILURE_E;

		return ALU_INST_SUCCESS_E;
	}

	// Get pointers to ALU instruction token separators
	char* line_equals_ptr = strchr(line_st, '=');
	char* line_semicol_ptr = strchr(line_st, ';');

	// Init ALU operation token ind and len
	size_t tok_opr_ind = 0;
	size_t tok_opr_len = 0;

	// Init ALU instruction parts
	long inst_target = 0, inst_opr = 0, inst_jump = 0;

	// Equals char found - syntax expected to be "[Target] = [Operation] [...]"
	// Parse ALU target
	if (line_equals_ptr) {
		tok_opr_ind = (int)(line_equals_ptr - line_st) + 1; // Char after '='
		int tok_target_len = tok_opr_ind - 1;

		// Syntax error - no chars before '='
		if (tok_target_len < 1) {
			error_init(err, ERRVAL_SYNTAX, "No target given");
			return ALU_INST_FAILURE_E;
		}

		// Copy target token to new string
		char tok_target[STR_CHARS(tok_target_len)];
		if (!str_copy(tok_target, line_st, tok_target_len)) {
			error_init(err, ERRVAL_FAILURE, "Failed to copy string");
			return ALU_INST_FAILURE_E;
		}

		// Parse target
		inst_target = parse_inst_alu_targets(tok_target);
		if (inst_target < 0) {
			error_init(err, ERRVAL_SYNTAX, "Invalid target: '%s'", tok_target);
			return ALU_INST_FAILURE_E;
		}
	}

	tok_opr_len = line_len - tok_opr_ind;

	// Semicolon char found - syntax expected to be "[...] [Operation] ; [Jump]"
	// Parse ALU jump condition
	if (line_semicol_ptr) {
		int tok_jump_ind = (int)(line_semicol_ptr - line_st) + 1; // Char after ';'
		int tok_jump_len = line_len - tok_jump_ind;

		// Syntax error - no chars after ';'
		if (tok_jump_len < 1) {
			error_init(err, ERRVAL_SYNTAX, "No jump condition given");
			return ALU_INST_FAILURE_E;
		}

		tok_opr_len = tok_opr_len - tok_jump_len - 1;

		// Copy jump condition token to new string
		char tok_jump[STR_CHARS(tok_jump_len)];
		if (!str_copy(tok_jump, &line_st[tok_jump_ind], tok_jump_len)) {
			error_init(err, ERRVAL_FAILURE, "Failed to copy string");
			return ALU_INST_FAILURE_E;
		}

		// Parse jump condition
		inst_jump = parse_inst_alu_jump(tok_jump);
		if (inst_jump < 0) {
			error_init(err, ERRVAL_SYNTAX, "Invalid jump condition: '%s'", tok_jump);
			return ALU_INST_FAILURE_E;
		}
	}

	// Copy ALU operation token to new string
	char tok_opr[STR_CHARS(tok_opr_len)];
	if (!str_copy(tok_opr, &line_st[tok_opr_ind], tok_opr_len)) {
		error_init(err, ERRVAL_FAILURE, "Failed to copy string");
		return ALU_INST_FAILURE_E;
	}

	// Parse ALU operation
	inst_opr = parse_inst_alu_opr(tok_opr);
	if (inst_opr >= 0) {
		if (!lines_push(err, lines, LINE_INST_E, line_num, (size_t)(NGC_IN_ALU | inst_opr | inst_target | inst_jump)))
			return ALU_INST_FAILURE_E;

		return ALU_INST_SUCCESS_E;
	}

	// ALU operation invalid - return hint if file line could be another kind of instruction
	if (!line_semicol_ptr && parse_inst_alu_jump(tok_opr) < 0) {
		// Line could be a data instruction - "A = [...]"
		if (line_equals_ptr && inst_target == NGC_IN_TARGET_A)
			return ALU_INST_HINT_INST_DATA_E;

		// Line could be a macro reference
		if (!line_equals_ptr)
			return ALU_INST_HINT_REF_MACRO_E;
	}

	// ALU operation invalid
	error_init(err, ERRVAL_SYNTAX, "Invalid operation: '%s'", tok_opr);
	return ALU_INST_FAILURE_E;
}

/**
 * Parse data instruction.
 *
 * @param err Struct to store error.
 * @param lines Linked list to push parsed result.
 * @param refs_data Linked list to push parsed result.
 * @param line_num Number of line in file.
 * @param line_st Assembly line with whitespace trimmed.
 * @param line_len Length of assembly line.
 * @returns Whether data instruction was valid and parsed successfully.
 */
static bool parse_inst_data(struct error* err, struct llist* lines, struct llist* refs_data, const size_t line_num, const char* line_tr, const size_t line_len)
{
	// Get pointer to '=' char
	// Whether 'A' is being targeted before '=' is checked before this function
	char* line_equals_ptr = strchr(line_tr, '=');
	if (!line_equals_ptr) {
		error_init(err, ERRVAL_SYNTAX, "Invalid instruction");
		return false;
	}

	// Get string after '=' char and trim - use as value for data instruction
	char data_str[STR_CHARS(line_len)];
	if (!str_trim(data_str, line_equals_ptr + 1, line_len)) {
		error_init(err, ERRVAL_FAILURE, "Failed to trim whitespace from string");
		return false;
	}

	// Try parse data value as number
	long parsed_number = parse_number(data_str);
	if (parsed_number >= 0) {
		// Push line result
		if (!lines_push(err, lines, LINE_INST_E, line_num, (size_t)parsed_number))
			return false;

		return true;
	}

	// Validate data value can be a key
	if (!key_valid(data_str)) {
		error_init(err, ERRVAL_SYNTAX, "Invalid operation: '%s'", data_str);
		return false;
	}

	size_t data_key_len = sizeof(data_str);

	// Ensure key is null-terminated
	char data_key[PARSED_KEY_CHARS] = { 0 };
	if (!str_copy(data_key, data_str, data_key_len)) {
		error_init(err, ERRVAL_FAILURE, "Failed to copy string");
		return false;
	}

	// Push data reference result
	long long ref_data_ind = refs_data_push(err, refs_data, data_key, sizeof(data_key));
	if (ref_data_ind < 0)
		return false;

	// Push line result
	if (!lines_push(err, lines, LINE_REF_DATA_E, line_num, (size_t)ref_data_ind))
		return false;

	return true;
}

/**
 * Parse line of assembly file.
 *
 * @param err Struct to store error.
 * @param result Struct to store parsed result.
 * @param defs_macros Linked list to push parsed result.
 * @param scope Scope assembly line is being parsed within.
 * @param line_num Number of line in file.
 * @param line Assembly line.
 * @param line_len Length of assembly line.
 * @returns Whether line was valid and parsed successfully.
 */
static bool parse_line(struct error* err, struct parsed_base* result, struct llist* defs_macros, enum parsed_scope* scope, const size_t line_num, const char* line, const size_t line_len)
{
	assert(result);
	assert(scope);

	bool success = false;

	char line_tr[STR_CHARS(line_len)];
	char line_st[STR_CHARS(line_len)];

	struct llist line_toks = { 0 };

	// Trim whitespace from line
	if (!str_trim(line_tr, line, line_len)) {
		error_init(err, ERRVAL_FAILURE, "Failed to trim whitespace from string");
		goto exit;
	}

	size_t line_tr_len = strlen(line_tr);

	// Skip line if empty or comment
	if (line_tr[0] == '#' || line_tr[0] == '\0') {
		success = true;
		goto exit;
	}

	// Build list of tokens in line
	if (!str_split(&line_toks, line_tr, line_tr_len, WHITESPACE)) {
		error_init(err, ERRVAL_FAILURE, "Failed to split string");
		goto exit;
	}

	// Copy first token
	char line_tok_first[STR_CHARS(STRULL_MAX_LEN)] = { 0 };
	if (!str_copy(line_tok_first, (char*)llist_get(line_toks, 0), STRULL_MAX_LEN)) {
		error_init(err, ERRVAL_FAILURE, "Failed to copy string");
		goto exit;
	}

	// Convert first token to uppercase
	if (!str_to(line_tok_first, line_tok_first, STRULL_MAX_LEN, toupper)) {
		error_init(err, ERRVAL_FAILURE, "Failed to convert string to uppercase");
		goto exit;
	}

	// Parse line as non-instruction definitions
	switch (str_ull(line_tok_first)) {
		// DEFINE
		case 0x444546494E45:
			success = parse_def_data_define(err, &result->defs_data, line_toks);
			goto exit;

		// LABEL
		case 0x4C4142454C:
			success = parse_def_data_label(err, &result->defs_data, line_toks, result->lines.len - result->refs_macros.len); // To avoid double-counting during assembly, macro references do not count towards instruction count
			goto exit;

		// %MACRO
		case 0x254D4143524F:
			// %MACRO statement only allowed in main scope
			if (*scope != SCOPE_FILE_E || !defs_macros) {
				error_init(err, ERRVAL_SYNTAX, "Nested %%MACRO statements not allowed");
				goto exit;
			}

			// Parse %MACRO statement
			if (!parse_def_macro(err, defs_macros, line_toks))
				goto exit;

			// Change scope to macro
			*scope = SCOPE_MACRO_E;

			success = true;
			goto exit;

		// %END
		case 0x25454E44:
			// %END statement only allowed in macro scope
			if (*scope != SCOPE_MACRO_E) {
				error_init(err, ERRVAL_SYNTAX, "%%END statement must have an accompanying %%MACRO statement");
				goto exit;
			}

			// Revert scope to file
			*scope = SCOPE_FILE_E;

			success = true;
			goto exit;
	}

	// Remove all whitespace from line
	if (!str_rm(line_st, line_tr, line_tr_len, isspace)) {
		error_init(err, ERRVAL_FAILURE, "Failed to remove whitespace from string");
		goto exit;
	}

	size_t line_st_len = strlen(line_st);
	assert(line_st_len <= line_tr_len);

	// Try parse line as ALU instruction
	enum parse_inst_alu_result parse_inst_alu_result = parse_inst_alu(err, &result->lines, line_num, line_st, line_st_len);
	switch (parse_inst_alu_result) {
		case ALU_INST_FAILURE_E:
			success = false;
			break;
		case ALU_INST_SUCCESS_E:
			success = true;
			break;
		case ALU_INST_HINT_INST_DATA_E:
			success = parse_inst_data(err, &result->lines, &result->refs_data, line_num, line_tr, line_tr_len);
			break;
		case ALU_INST_HINT_REF_MACRO_E:
			success = parse_ref_macro(err, &result->lines, &result->refs_data, &result->refs_macros, line_num, line_toks);
			break;
		default:
			error_init(err, ERRVAL_FAILURE, "Unknown ALU instruction parse result: %d", parse_inst_alu_result);
			success = false;
			break;
	}

	exit:
	llist_empty(&line_toks);
	return success;
}

size_t parse_file(struct error* err, struct parsed_file* file, FILE* fp)
{
	if (!file) {
		error_init(err, ERRVAL_FAILURE, "Result struct is null");
		return 1;
	}

	if (!fp) {
		error_init(err, ERRVAL_FAILURE, "File pointer is null");
		return 1;
	}

	char f_line[STR_CHARS(FILE_COLS_MAX + 1)] = { 0 }; // +1 to detect when max exceeded
	size_t line_num = 0;

	enum parsed_scope scope = SCOPE_FILE_E;

	// Parse each line of file
	while (fgets(f_line, sizeof(f_line), fp) != 0 && line_num <= FILE_LINES_MAX) {
		line_num++;

		// Check if too many lines in file
		if (line_num > FILE_LINES_MAX) {
			error_init(err, ERRVAL_FILE, "File contains too many lines (max %ld)", FILE_LINES_MAX);
			return line_num;
		}

		size_t f_line_len = strlen(f_line);

		// Check if too many chars in line
		if (f_line_len > FILE_COLS_MAX) {
			error_init(err, ERRVAL_FILE, "File contains too many columns (max %ld)", FILE_COLS_MAX);
			return line_num;
		}

		struct parsed_base* result_scope;
		struct llist* defs_macros = NULL;

		// Set scope of line being parsed
		switch (scope) {
			case SCOPE_FILE_E:
				result_scope = &file->base;
				defs_macros = &file->defs_macros;
				break;
			case SCOPE_MACRO_E:
				;
				// Get last macro definition
				struct llist_node* macros_node = file->defs_macros.head;
				for (size_t macros_ind = 0; macros_node && macros_node->next && macros_ind < file->defs_macros.len; macros_node = macros_node->next, macros_ind++);

				if (!macros_node) {
					error_init(err, ERRVAL_FAILURE, "Failed to find macro scope");
					return line_num;
				}

				// Set destination to last macro in list
				result_scope = (struct parsed_base*)&(((struct parsed_def_macro*)macros_node->val)->base);
				break;
			default:
				error_init(err, ERRVAL_FAILURE, "Unknown result scope: %d", scope);
				return line_num;
		}

		// Parse line
		if (!parse_line(err, result_scope, defs_macros, &scope, line_num, f_line, f_line_len))
			return line_num;
	}

	// Validate all macro definitions have been ended
	if (scope != SCOPE_FILE_E) {
		error_init(err, ERRVAL_SYNTAX, "%%MACRO statement must have an accompanying %%END statement");
		return line_num;
	}

	return 0;
}
