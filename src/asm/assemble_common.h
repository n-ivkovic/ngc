#ifndef ASSEMBLE_COMMON_H
#define ASSEMBLE_COMMON_H

#include "../ngc.h"
#include "err.h"
#include "llist.h"
#include "parsed.h"

#include <stdbool.h>

/**
 * Push NGC instruction.
 *
 * @param err Struct to store error.
 * @param instructions Linked list to push NGC instruction.
 * @param inst NGC instruction.
 * @returns Whether NGC instruction was pushed successfully.
 */
bool inst_push(struct error* err, struct llist* instructions, const ngc_word_t inst);

/**
 * Get data definition.
 *
 * @param defs_data Linked list of data definitions.
 * @param key Key of data to get.
 * @returns Pointer to data definition. NULL if not found.
 */
struct parsed_def_data* def_data_get(const struct llist defs_data, const char* key);

/**
 * Get data definition from data reference.
 *
 * @param err Struct to store error.
 * @param defs_data Linked list of data definitions.
 * @param refs_data Linked list of data references.
 * @param refs_data_ind Index of data reference to get.
 * @returns Pointer to data definition. NULL if not found.
 */
struct parsed_def_data* assemble_ref_data(struct error* err, const struct llist defs_data, const struct llist refs_data, const size_t refs_data_ind);

#endif
