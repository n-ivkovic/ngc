#ifndef PARSE_H
#define PARSE_H

#include "err.h"
#include "parsed.h"

#include <stdio.h>

/**
 * Parse assembly file.
 *
 * @param err Struct to store error.
 * @param result Struct to store parsed file.
 * @param fp Assembly file pointer.
 * @returns 0 if successful, > 0 if error. Returns which line is associated with error when err->val is ERRVAL_SYNTAX.
 */
size_t parse_file(struct error* err, struct parsed_file* result, FILE* fp);

#endif
