#ifndef ASSEMBLE_H
#define ASSEMBLE_H

#include "err.h"
#include "parsed.h"

/**
 * Assemble parsed file to NGC instructions.
 *
 * @param error Struct to store error.
 * @param instructions Dynamic array to push NGC instructions.
 * @param file Parsed file.
 * @returns 0 if successfully assembled. >0 line number if error.
 */
size_t assemble_file(struct error* err, struct dynarr* instructions, const struct parsed_file file);

#endif
