#ifndef PRINT_H
#define PRINT_H

#define EOL "\n"

/**
 * Print error to stderr.
 *
 * @param str String with formats to print.
 * @param ... Formatting values.
 */
void print_err(const char* str, ...);

/**
 * Print to stderr if debug build.
 *
 * @param str String with formats to print.
 * @param ... Formatting values.
 */
void print_dbg(const char* str, ...);

#endif
