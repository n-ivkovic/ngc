#include "str.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

char* str_copy(char* dst, const char* src, const size_t len)
{
	if (!dst || !src)
		return NULL;

	*dst = '\0';
	return strncat(dst, src, len);
}

char* str_to(char* dst, const char* src, const size_t len, int (*to)(const int))
{
	if (!dst || !src)
		return NULL;

	size_t ind = 0;

	for (; ind < len && src[ind] != '\0'; ind++)
		dst[ind] = to(src[ind]);

	dst[ind] = '\0';
	return dst;
}

char* str_rm(char* dst, const char* src, const size_t len, int (*is)(const int))
{
	if (!dst || !src)
		return NULL;

	size_t dst_ind = 0;

	for (size_t src_ind = 0; src_ind < len && src[src_ind] != '\0'; src_ind++) {
		if (is(src[src_ind]))
			continue;

		dst[dst_ind] = src[src_ind];
		dst_ind++;
	}

	dst[dst_ind] = '\0';
	return dst;
}

char* str_trim(char* dst, const char* src, const size_t len)
{
	if (!dst || !src)
		return NULL;

	size_t src_ind = 0, dst_ind = 0;

	for (; src_ind < len && src[src_ind] != '\0'; src_ind++) {
		if (src_ind == dst_ind && isspace(src[dst_ind]))
			dst_ind++;
	}

	size_t dst_len = src_ind;

	while (dst_len > dst_ind && isspace(src[dst_len - 1])) {
		dst_len--;
	}

	char* result = str_copy(dst, &src[dst_ind], dst_len - dst_ind);
	return result;
}

int str_comp(const char* s1, const char* s2, const size_t len, int (*to)(const int))
{
	bool s1_ended = false, s2_ended = false;

	for (size_t ind = 0; ind < len && !s1_ended && !s2_ended; ind++) {
		char c1 = s1_ended ? '\0' : s1[ind];
		char c2 = s2_ended ? '\0' : s2[ind];

		// Compare chars - return if inequal
		int result = to(c1) - to(c2);
		if (result != 0)
			return result;

		// End of s1 reached
		if (c1 == '\0')
			s1_ended = true;

		// End of s2 reached
		if (c2 == '\0')
			s2_ended = true;
	}

	// Strings equal
	return 0;
}

long long str_split(struct llist* list, const char* src, const size_t len, const char* delim)
{
	if (!src)
		return -1;

	long long result = 0;
	size_t token_ind = strspn(src, delim); // Start after any prefixed delim
	size_t token_len;

	do {
		// Get length of token
		token_len = strcspn(&src[token_ind], delim);
		if (token_len == 0)
			break;

		// Limit length to max total chars
		if (token_ind + token_len > len)
			token_len -= (token_ind + token_len) - len;

		// Ensure token is null-delmited
		char token[STR_CHARS(token_len)];
		if (!str_copy(token, &src[token_ind], token_len))
			return -1;

		// Push copy of token to list
		if (!llist_push(list, token, sizeof(token)))
			return -1;

		// Add token length + any proceeding delim to get index of next token
		token_ind += token_len;
		token_ind += strspn(&src[token_ind], delim);

		result++;
	} while (token_len > 0);

	return result;
}

unsigned long long str_ull(const char* str)
{
	unsigned long long result = 0;

	if (!str)
		return result;

	size_t str_len = strlen(str);
	size_t len = (str_len > STRULL_MAX_LEN) ? STRULL_MAX_LEN : str_len;

	for (size_t ind = 0; ind < len; ind++) {
		result |= (unsigned long long)str[ind] << (8 * sizeof(char) * (len - ind - 1));
	}

	return result;
}
