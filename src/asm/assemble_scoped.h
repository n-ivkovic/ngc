#ifndef ASSEMBLE_SCOPED_H
#define ASSEMBLE_SCOPED_H

#include "err.h"
#include "llist.h"
#include "parsed.h"

/**
 * Assemble parsed file to NGC instructions.
 *
 * @param error Struct to store error.
 * @param instructions Linked list to push NGC instructions.
 * @param file Parsed file.
 * @returns 0 if successfully assembled. >0 line number if error.
 */
size_t assemble_file_scoped(struct error* err, struct llist* instructions, const struct parsed_file file);

#endif
