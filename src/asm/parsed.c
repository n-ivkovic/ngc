#include "parsed.h"

void parsed_ref_macro_empty(struct parsed_ref_macro* ref_macro)
{
    if (!ref_macro) return;

    llist_empty(&ref_macro->params);
}

void parsed_base_empty(struct parsed_base* base)
{
    if (!base) return;

    llist_empty(&base->lines);
    llist_empty(&base->refs_data);
    llist_delegate_empty(&base->refs_macros, (void (*)(void *))parsed_ref_macro_empty);
    llist_empty(&base->defs_data);
}

void parsed_def_macro_empty(struct parsed_def_macro* def_macro)
{
    if (!def_macro) return;

    parsed_base_empty(&def_macro->base);
    llist_empty(&def_macro->params);
}

void parsed_file_empty(struct parsed_file* file)
{
    if (!file) return;

    parsed_base_empty(&file->base);
    llist_delegate_empty(&file->defs_macros, (void (*)(void *))parsed_def_macro_empty);
}
