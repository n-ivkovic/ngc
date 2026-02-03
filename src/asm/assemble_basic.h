#ifndef ASSEMBLE_BASIC_H
#define ASSEMBLE_BASIC_H

#include "err.h"
#include "llist.h"
#include "parsed.h"

/**
 * Assemble macro-less parsed file to NGC instructions.
 *
 * @param error Struct to store error.
 * @param instructions Linked list to push NGC instructions.
 * @param file Parsed file.
 * @returns 0 if successfully assembled. >0 line number if error.
 */
size_t assemble_file_basic(struct error* err, struct llist* instructions, const struct parsed_base file);

#endif
