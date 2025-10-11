#ifndef ERR_H
#define ERR_H

#include "str.h"

#define ERRVAL_FAILURE (1 << 0) // Unknown/general failure
#define ERRVAL_ARGS    (1 << 1) // Invalid CLI arguments
#define ERRVAL_FILE    (1 << 2) // Invalid assembly file
#define ERRVAL_SYNTAX  (1 << 3) // Invalid assembly syntax

#define ERRMSG_LEN_MAX 0x180

/**
 * Error result.
 */
struct error {
	int val; // Return value
	char msg[STR_CHARS(ERRMSG_LEN_MAX)];
};

/**
 * Init error result.
 *
 * @param err Result to init.
 * @param val Return value.
 * @param msg Message.
 * @param ... Message formatting values.
 */
void error_init(struct error* err, const int val, const char* msg, ...);

#endif
