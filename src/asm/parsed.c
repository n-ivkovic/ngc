#include "parsed.h"

#include <stdlib.h>

#define PARSED_LINES_CAPACITY_INIT        0x10
#define PARSED_DATA_CAPACITY_INIT         0x4
#define PARSED_MACROS_CAPACITY_INIT       0x4
#define PARSED_MACRO_PARAMS_CAPACITY_INIT 0x1

struct parsed_ref_macro* parsed_ref_macro_alloc(struct parsed_ref_macro* ref_macro)
{
	if (!ref_macro)
		return NULL;

	if (!dynarr_alloc(&ref_macro->params, PARSED_MACRO_PARAMS_CAPACITY_INIT, sizeof(struct parsed_ref_macro_param)))
		return NULL;

	return ref_macro;
}

struct parsed_def_macro* parsed_def_macro_alloc(struct parsed_def_macro* def_macro)
{
	if (!def_macro)
		return NULL;

	if (!parsed_base_alloc(&def_macro->base))
		goto error;

	if (!dynarr_alloc(&def_macro->params, PARSED_MACRO_PARAMS_CAPACITY_INIT, sizeof(char[PARSED_KEY_CHARS])))
		goto error;

	return def_macro;

	error:
	parsed_def_macro_empty(def_macro);
	return NULL;
}

struct parsed_base* parsed_base_alloc(struct parsed_base* base)
{
	if (!base)
		return NULL;

	if (!dynarr_alloc(&base->lines, PARSED_LINES_CAPACITY_INIT, sizeof(struct parsed_line)))
		goto error;

	if (!dynarr_alloc(&base->refs_data, PARSED_DATA_CAPACITY_INIT, sizeof(char[PARSED_KEY_CHARS])))
		goto error;

	if (!dynarr_alloc(&base->refs_macros, PARSED_MACROS_CAPACITY_INIT, sizeof(struct parsed_ref_macro)))
		goto error;

	if (!dynarr_alloc(&base->defs_data, PARSED_DATA_CAPACITY_INIT, sizeof(struct parsed_def_data)))
		goto error;

	return base;

	error:
	parsed_base_empty(base);
	return NULL;
}

struct parsed_file* parsed_file_alloc(struct parsed_file* file)
{
	if (!file)
		return NULL;

	if (!parsed_base_alloc(&file->base))
		goto error;

	if (!dynarr_alloc(&file->defs_macros, PARSED_MACROS_CAPACITY_INIT, sizeof(struct parsed_def_macro)))
		goto error;

	return file;

	error:
	parsed_file_empty(file);
	return NULL;
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
