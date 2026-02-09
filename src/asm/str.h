#ifndef STR_H
#define STR_H

#include "llist.h"

#include <stddef.h>

#define STR_CHARS(len) ((len) + 1)
#define STR_SIZE(len) (STR_CHARS(len) * sizeof(char))
#define STRULL_MAX_LEN (sizeof(unsigned long long) / sizeof(char))

// Chars which are considered whitespace, should be the same as isspace()
#define WHITESPACE " \t\n\v\f\r"

/**
 * Copy string.
 *
 * @param dst Output string.
 * @param src Input string.
 * @param len Max number of chars to copy.
 * @returns Pointer to output string. NULL if error.
 */
char* str_copy(char* dst, const char* src, const size_t len);

/**
 * Convert string using the given function.
 *
 * @param dst Output string.
 * @param src Input string.
 * @param len Max number of chars to look at.
 * @param to Function used to convert each character.
 * @returns Pointer to output string. NULL if error.
 */
char* str_to(char* dst, const char* src, const size_t len, int (*to)(const int));

/**
 * Remove all chars from string using the given function.
 *
 * @param dst Output string.
 * @param src Input string.
 * @param len Max number of chars to look at.
 * @param is Function used to test whether a char should be removed.
 * @returns Pointer to output string. NULL if error.
 */
char* str_rm(char* dst, const char* src, const size_t len, int (*is)(const int));

/**
 * Remove leading and trailing whitespace chars from string.
 *
 * @param dst Output string.
 * @param src Input string.
 * @param len Max number of chars to look at.
 * @returns Pointer to output string. NULL if error.
 */
char* str_trim(char* dst, const char* src, const size_t len);

/**
 * Compare two strings using the given function.
 *
 * @param s1 First string.
 * @param s2 Second string.
 * @param len Max number of chars to look at.
 * @param to Function used to convert each char compared.
 * @returns Whether s1 is equal (0), greater than (>0), or less than (<0) s2.
 */
int str_comp(const char* s1, const char* s2, const size_t len, int (*to)(const int));

/**
 * Split string by delimeter to linked list of strings.
 *
 * @param list Linked list to store split string.
 * @param src Input string.
 * @param len Max number of chars to look at.
 * @param delim Delimeter to split string by.
 * @returns Number of items pushed to linked list. -1 if error.
 */
long long str_split(struct llist* list, const char* src, const size_t len, const char* delim);

/**
 * Return string as unsigned long long.
 * Will only return first chars that can fit within return type,
 * i.e. will only return first 8 chars.
 *
 * @param str Input string.
 * @returns First 8 chars of string as unsigned long long.
 */
unsigned long long str_ull(const char* str);

#endif
