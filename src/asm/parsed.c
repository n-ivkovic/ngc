#include "parsed.h"

#include <stdlib.h>

#define PARSED_LINES_CAPACITY_INIT  8
#define PARSED_DATA_CAPACITY_INIT   2
#define PARSED_MACROS_CAPACITY_INIT 2

void parsed_def_macro_alloc(struct parsed_def_macro* def_macro)
{
	if (!def_macro)
		return;

	parsed_base_alloc(&def_macro->base);

	// No space pre-allocated for macro parameters
}

void parsed_base_alloc(struct parsed_base* base)
{
	if (!base)
		return;

	// Failure to pre-allocate space is non-critical - not checking return results
	dynarr_alloc(&base->lines, PARSED_LINES_CAPACITY_INIT, sizeof(struct parsed_line));
	dynarr_alloc(&base->refs_data, PARSED_DATA_CAPACITY_INIT, sizeof(char[PARSED_KEY_CHARS]));
	dynarr_alloc(&base->refs_macros, PARSED_MACROS_CAPACITY_INIT, sizeof(struct parsed_ref_macro));
	dynarr_alloc(&base->defs_data, PARSED_DATA_CAPACITY_INIT, sizeof(struct parsed_def_data));

	// No space pre-allocated for macro parameters
}

void parsed_file_alloc(struct parsed_file* file)
{
	if (!file)
		return;

	parsed_base_alloc(&file->base);
	dynarr_alloc(&file->defs_macros, PARSED_MACROS_CAPACITY_INIT, sizeof(struct parsed_def_macro)); // Failure to pre-allocate space is non-critical - not checking return result
}

struct parsed_def_data* parsed_def_data_get(const struct dynarr defs_data, const char* key)
{
	if (!key)
		return NULL;

	for (size_t data_ind = 0; data_ind < defs_data.len; data_ind++) {
		struct parsed_def_data* data = dynarr_get(defs_data, data_ind);
		if (!data)
			continue;

		if (PARSED_KEYS_EQ(data->key, key))
			return data;
	}

	return NULL;
}

struct parsed_def_macro* parsed_def_macro_get(const struct dynarr defs_macros, const char* key)
{
	if (!key)
		return NULL;

	for (size_t macro_ind = 0; macro_ind < defs_macros.len; macro_ind++) {
		struct parsed_def_macro* macro = dynarr_get(defs_macros, macro_ind);
		if (!macro)
			continue;

		if (PARSED_KEYS_EQ(macro->key, key))
			return macro;
	}

	return NULL;
}

void parsed_ref_macro_empty(struct parsed_ref_macro* ref_macro)
{
	if (!ref_macro)
		return;

	dynarr_empty(&ref_macro->params);
}

void parsed_base_empty(struct parsed_base* base)
{
	if (!base)
		return;

	dynarr_empty(&base->lines);
	dynarr_empty(&base->refs_data);
	dynarr_delegate_empty(&base->refs_macros, (void (*)(void *))parsed_ref_macro_empty);
	dynarr_empty(&base->defs_data);
}

void parsed_def_macro_empty(struct parsed_def_macro* def_macro)
{
	if (!def_macro)
		return;

	parsed_base_empty(&def_macro->base);
	dynarr_empty(&def_macro->params);
}

void parsed_file_empty(struct parsed_file* file)
{
	if (!file)
		return;

	parsed_base_empty(&file->base);
	dynarr_delegate_empty(&file->defs_macros, (void (*)(void *))parsed_def_macro_empty);
}
