#include "assemble_basic.h"

#include "assemble_common.h"

size_t assemble_file_basic(struct error* err, struct llist* instructions, const struct parsed_base file)
{
	if (!instructions) {
		error_init(err, ERRVAL_FAILURE, "Result list is null");
		return 1;
	}

	struct llist_node* line_node = file.lines.head;
	for (size_t lines_ind = 0; line_node && lines_ind < file.lines.len; line_node = line_node->next, lines_ind++) {
		struct parsed_line* line = (struct parsed_line*)(line_node->val);

		switch (line->type) {
			case LINE_INST_E:
				// Push instruction
				if (!inst_push(err, instructions, (ngc_word_t)line->val))
					return line->line_num;

				break;

			case LINE_REF_DATA_E:
				;
				// Assemble referenced data instruction
				struct parsed_def_data* def_data = assemble_ref_data(err, file.defs_data, file.refs_data, line->val);
				if (!def_data)
					return line->line_num;

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
