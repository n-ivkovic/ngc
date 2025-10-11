#ifndef PARSED_H
#define PARSED_H

#include "llist.h"
#include "str.h"

#include <ctype.h>

#define PARSED_KEY_LEN_MAX 0x3F
#define PARSED_KEY_CHARS (STR_CHARS(PARSED_KEY_LEN_MAX))
#define PARSED_KEY_SIZE (STR_SIZE(PARSED_KEY_LEN_MAX))
#define PARSED_KEYS_EQ(k1, k2) (str_comp(k1, k2, PARSED_KEY_LEN_MAX, tolower) == 0)

/**
 * Type of parsed assembly line.
 */
enum parsed_line_type {
	LINE_INST_E,
	LINE_REF_DATA_E,
	LINE_REF_MACRO_E
};

/**
 * Parsed assembly line.
 */
struct parsed_line {
	enum parsed_line_type type;
	size_t line_num;
	size_t val; // Parsed instruction if type is LINE_INST_E, index of parsed_base.refs_* if type is LINE_REF_*_E
};

/**
 * Type of parsed parameter of macro reference.
 */
enum parsed_ref_macro_param_type {
	PARAM_CONST_E,
	PARAM_REF_DATA_E
};

/**
 * Parsed parameter of macro reference.
 */
struct parsed_ref_macro_param {
	enum parsed_ref_macro_param_type type;
	size_t val; // Parsed number if type is PARAM_CONST_E, index of parsed_base.refs_data if type is PARAM_REF_DATA_E
};

/**
 * Parsed macro reference.
 */
struct parsed_ref_macro {
	char key[PARSED_KEY_CHARS];
	struct llist params; // llist of parsed_ref_macro_param
};

/**
 * Type of parsed data definition.
 */
enum parsed_def_data_type {
	DATA_CONST_E, // DEFINE statement
	DATA_LABEL_E // LABEL statement
};

/**
 * Parsed data definition (DEFINE or LABEL statement).
 */
struct parsed_def_data {
	enum parsed_def_data_type type;
	char key[PARSED_KEY_CHARS];
	size_t val; // Parsed number if type is DATA_CONST_E, instruction count if type is DATA_LABEL_E
};

/**
 * Base result of parsed assembly.
 */
struct parsed_base {
	struct llist lines; // llist of parsed_line
	struct llist refs_data; // llist of char[PARSED_KEY_CHARS]
	struct llist refs_macros; // llist of parsed_ref_macro
	struct llist defs_data; // llist of parsed_def_data
};

/**
 * Parsed macro definition.
 */
struct parsed_def_macro {
	struct parsed_base base;
	char key[PARSED_KEY_CHARS];
	struct llist params; // llist of char[PARSED_KEY_CHARS]
};

/**
 * Parsed assembly file.
 */
struct parsed_file {
	struct parsed_base base;
	struct llist defs_macros; // llist of parsed_def_macro
};

void parsed_ref_macro_empty(struct parsed_ref_macro* ref_macro);

void parsed_def_macro_empty(struct parsed_def_macro* def_macro);

void parsed_base_empty(struct parsed_base* base);

void parsed_file_empty(struct parsed_file* file);

#endif
