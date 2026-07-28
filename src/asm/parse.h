#ifndef PARSE_H
#define PARSE_H

#include "err.h"
#include "parsed.h"

#include <stdio.h>

#define LANG_FEAT_DEF_DATA         (1 << 0)
#define LANG_FEAT_DEF_MACROS       (1 << 1)
#define LANG_FEAT_DEF_MACRO_PARAMS (1 << 2)
#define LANG_FEAT_ALL              (LANG_FEAT_DEF_DATA | LANG_FEAT_DEF_MACROS | LANG_FEAT_DEF_MACRO_PARAMS)

/**
 * Parse assembly file.
 *
 * @param err Struct to store error.
 * @param result Struct to store parsed file.
 * @param fp Assembly file pointer.
 * @param features Enabled assembly language features.
 * @returns 0 if successful, > 0 if error. Returns which line is associated with error when err->val is ERRVAL_SYNTAX.
 */
size_t parse_file(struct error* err, struct parsed_file* result, FILE* fp, const int features);

#endif
