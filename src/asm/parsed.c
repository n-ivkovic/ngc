#include "parsed.h"

#include <stdlib.h>

struct parsed_def_data* parsed_def_data_get(const struct llist defs_data, const char* key)
{
	if (!key)
        return NULL;

    struct llist_node* data_node = defs_data.head;
	for (size_t data_ind = 0; data_node && data_ind < defs_data.len; data_node = data_node->next, data_ind++) {
		struct parsed_def_data* data = data_node->val;
		if (PARSED_KEYS_EQ(data->key, key))
			return data;
	}

	return NULL;
}

struct parsed_def_macro* parsed_def_macro_get(const struct llist defs_macros, const char* key)
{
	if (!key)
        return NULL;

    struct llist_node* macro_node = defs_macros.head;
	for (size_t macro_ind = 0; macro_node && macro_ind < defs_macros.len; macro_node = macro_node->next, macro_ind++) {
		struct parsed_def_macro* macro = macro_node->val;
		if (PARSED_KEYS_EQ(macro->key, key))
			return macro;
	}

	return NULL;
}

void parsed_ref_macro_empty(struct parsed_ref_macro* ref_macro)
{
    if (!ref_macro) return;

    llist_empty(&ref_macro->params);
}

void parsed_ref_macro_free(struct parsed_ref_macro* ref_macro)
{
    if (!ref_macro) return;

    parsed_ref_macro_empty(ref_macro);
    free(ref_macro);
}

void parsed_base_empty(struct parsed_base* base)
{
    if (!base) return;

    llist_empty(&base->lines);
    llist_empty(&base->refs_data);
    llist_delegate_empty(&base->refs_macros, (void (*)(void *))parsed_ref_macro_free);
    llist_empty(&base->defs_data);
}

void parsed_base_free(struct parsed_base* base)
{
    if (!base) return;

    parsed_base_empty(base);
    free(base);
}

void parsed_def_macro_empty(struct parsed_def_macro* def_macro)
{
    if (!def_macro) return;

    parsed_base_empty(&def_macro->base);
    llist_empty(&def_macro->params);
}

void parsed_def_macro_free(struct parsed_def_macro* def_macro)
{
    if (!def_macro) return;

    parsed_def_macro_empty(def_macro);
    free(def_macro);
}

void parsed_file_empty(struct parsed_file* file)
{
    if (!file) return;

    parsed_base_empty(&file->base);
    llist_delegate_empty(&file->defs_macros, (void (*)(void *))parsed_def_macro_free);
}

void parsed_file_free(struct parsed_file* file)
{
     if (!file) return;

    parsed_file_empty(file);
    free(file);
}
