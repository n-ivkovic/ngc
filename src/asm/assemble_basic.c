#include "assemble_basic.h"

#include "../ngc.h"

#include <assert.h>
#include <stdbool.h>

/**
 * Push NGC instruction.
 *
 * @param err Struct to store error.
 * @param instructions Linked list to push NGC instruction.
 * @param inst NGC instruction.
 * @returns Whether NGC instruction was pushed successfully.
 */
static bool inst_push(struct error* err, struct llist* instructions, const ngc_word_t inst)
{
	if (!llist_push(instructions, &inst, sizeof(inst))) {
		error_init(err, ERRVAL_FAILURE, "Failed to push instruction to list");
		return false;
	}

	return true;
}

size_t assemble_file_basic(struct error* err, struct llist* instructions, const struct parsed_base file)
{
	if (!instructions) {
		error_init(err, ERRVAL_FAILURE, "Result list is null");
		return 1;
	}

	struct llist_node* line_node = file.lines.head;
	for (size_t lines_ind = 0; line_node && lines_ind < file.lines.len; line_node = line_node->next, lines_ind++) {
		struct parsed_line* line = line_node->val;

		switch (line->type) {
			case LINE_INST_E:
				// Push instruction
				if (!inst_push(err, instructions, (ngc_word_t)line->val))
					return line->line_num;

				break;

			case LINE_REF_DATA_E:
				;
				// Get referenced data key at given index
				char* data_key = llist_get(file.refs_data, line->val);
				if (!data_key) {
					error_init(err, ERRVAL_FAILURE, "Data reference index not in list: %zu", line->val);
					return line->line_num;
				}

				// Get data definition using referenced data key
				struct parsed_def_data* def_data = parsed_def_data_get(file.defs_data, data_key);
				if (!def_data) {
					error_init(err, ERRVAL_SYNTAX, "Data reference not defined: '%s'", data_key);
					return line->line_num;
				}

				// Push data instruction
				if (!inst_push(err, instructions, (ngc_word_t)def_data->val))
					return line->line_num;

				break;

			case LINE_REF_MACRO_E:
				error_init(err, ERRVAL_FAILURE, "Macro reference found when none expected");
				return line->line_num;

			default:
				error_init(err, ERRVAL_FAILURE, "Unknown line type: %d", line->type);
				return line->line_num;
		}

		if (instructions->len > NGC_UWORD_MAX) {
			error_init(err, ERRVAL_FILE, "File contains too many instructions (max %ld)", NGC_UWORD_MAX);
			return line->line_num;
		}
	}

	return 0;
}
