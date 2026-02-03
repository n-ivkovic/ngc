#include "assemble_common.h"

#include <assert.h>

bool inst_push(struct error* err, struct llist* instructions, const ngc_word_t inst)
{
	if (!llist_push(instructions, &inst, sizeof(inst))) {
		error_init(err, ERRVAL_FAILURE, "Failed to push instruction to list");
		return false;
	}

	return true;
}

struct parsed_def_data* def_data_get(const struct llist defs_data, const char* key)
{
	assert(key);

	struct llist_node* data_node = defs_data.head;
	for (size_t data_ind = 0; data_node && data_ind < defs_data.len; data_node = data_node->next, data_ind++) {
		struct parsed_def_data* data = (struct parsed_def_data*)data_node->val;
		if (PARSED_KEYS_EQ(data->key, key))
			return data;
	}

	return NULL;
}

struct parsed_def_data* assemble_ref_data(struct error* err, const struct llist defs_data, const struct llist refs_data, const size_t refs_data_ind)
{
	// Get referenced data key at given index
	char* data_key = (char*)llist_get(refs_data, refs_data_ind);
	if (!data_key) {
		error_init(err, ERRVAL_FAILURE, "Data reference index not in list: %zu", refs_data_ind);
		return NULL;
	}

	// Get data definition using referenced data key
	struct parsed_def_data* def_data = def_data_get(defs_data, data_key);
	if (!def_data) {
		error_init(err, ERRVAL_SYNTAX, "Data reference not defined: '%s'", data_key);
		return NULL;
	}

	return def_data;
}
