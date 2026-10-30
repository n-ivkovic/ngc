#ifndef STR_H
#define STR_H

#include "../dynarr.h"

#define STR_CHARS(len) ((len) + 1)
#define STRULL_LEN (sizeof(unsigned long long) / sizeof(char))

// Chars which are considered whitespace, should be the same as isspace()
#define WHITESPACE " \t\n\v\f\r"

/**
 * Copy string.
 *
 * @param dst String to write output, including null-terminator.
 * @param src Input string.
 * @param len Max length of string to read from input and write to output, excluding null-terminator.
 * @returns Length of string written to output, excluding null-terminator.
 */
size_t str_copy(char* dst, const char* src, const size_t len);

/**
 * Convert all chars in string using a given function.
 *
 * @param dst String to write output, including null-terminator.
 * @param src Input string.
 * @param len Max length of string to read from input and write to output, excluding null-terminator.
 * @param to Function used to convert each char. If NULL, no char conversions will be performed.
 * @returns Length of string written to output, excluding null-terminator.
 */
size_t str_to(char* dst, const char* src, const size_t len, int (*to)(const int));

/**
 * Remove all chars in string matching a given function.
 *
 * @param dst String to write output, including null-terminator.
 * @param src Input string.
 * @param len Max length of string to read from input and write to output, excluding null-terminator.
 * @param is Function used to test whether a char should be removed. If NULL, no chars will be removed.
 * @returns Length of string written to output, excluding null-terminator.
 */
size_t str_rm(char* dst, const char* src, const size_t len, int (*is)(const int));

/**
 * Remove leading and trailing chars in string matching a given function.
 *
 * @param dst String to write output, including null-terminator.
 * @param src Input string.
 * @param len Max length of string to read from input and write to output, excluding null-terminator.
 * @param is Function to test whether a char should be removed. If NULL, no chars will be removed.
 * @returns Length of string written to output, excluding null-terminator.
 */
size_t str_trim(char* dst, const char* src, const size_t len, int (*is)(const int));

/**
 * Compare two strings using a given function.
 *
 * @param s1 First string.
 * @param s2 Second string.
 * @param len Max length of strings to compare, excluding null-terminator.
 * @param to Function used to convert each char before comparing. If NULL, no char conversions will be performed.
 * @returns Whether s1 is equal (0), greater than (>0), or less than (<0) s2.
 */
int str_comp(const char* s1, const char* s2, const size_t len, int (*to)(const int));

/**
 * Split string by delimeter to dynamic array of strings.
 *
 * @param da Dynamic array to store split string.
 * @param src Input string.
 * @param len Max number of chars to read from input, excluding null-terminator.
 * @param delim Chars to split string by.
 * @returns Number of items pushed to dynamic array. Negative value if error.
 */
long long str_split(struct dynarr* da, const char* src, const size_t len, const char* delim);

/**
 * Return string as unsigned long long.
 * Will only return first chars that can fit within return type.
 *
 * @param str Input string.
 * @param len Max number of chars to read from input, excluding null-terminator.
 * @returns Up to first 8 chars of string as unsigned long long.
 */
unsigned long long str_ull(const char* str, const size_t len);

#endif
