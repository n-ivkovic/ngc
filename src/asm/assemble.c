#include "assemble.h"

#include "assemble_basic.h"
#include "assemble_scoped.h"

size_t assemble_file(struct error* err, struct llist* instructions, const struct parsed_file file)
{
    if (file.defs_macros.len == 0 && file.base.refs_macros.len == 0)
        return assemble_file_basic(err, instructions, file.base);

    return assemble_file_scoped(err, instructions, file);
}
