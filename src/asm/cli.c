#define _XOPEN_SOURCE 600

#include "../ngc.h"
#include "../print.h"
#include "assemble.h"
#include "parse.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PATH_STDIN "-"
#define PATH_STDOUT "-"

/**
 * Print error associated with file.
 *
 * @param f_path File path.
 * @param msg Error message.
 */
static void print_file_err(const char* f_path, const char* msg)
{
	print_err("%s: %s", f_path, msg);
}

/**
 * Print error result.
 *
 * @param f_path File path.
 * @param f_line Line of file.
 * @param msg Error message.
 */
static void print_err_err(const char* f_path, const size_t f_line, const struct error err)
{
	if (err.val == ERRVAL_SYNTAX)
		print_err("%s:%lu: %s", f_path, f_line, err.msg);
	else
		print_file_err(f_path, err.msg);
}

int main(int argc, char* argv[])
{
	char* in_path = NULL;
	char* out_path = NULL;

	char opt;
	extern char* optarg;
	extern int optind, optopt;

	// Set vars from opts
	while ((opt = getopt(argc, argv, ":i:o:hvV")) != -1) {
		switch (opt) {
			case 'o':
				out_path = optarg;
				break;
			case 'v':
			case 'V':
			case 'h':
				printf("ngc-asm v0.1.0%s", EOL);
				return 0;
			case ':':
				print_err("Option -%c requires an argument", optopt);
				return ERRVAL_ARGS;
			case '?':
				print_err("Unknown option: -%c", optopt);
				return ERRVAL_ARGS;
		}
	}

	// Set input file path from arg
	for (; optind < argc; optind++) {
		if (in_path) {
			print_err("Multiple assembly files given");
			return ERRVAL_ARGS;
		}

		in_path = argv[optind];
	}

	if (!in_path) {
		print_err("No assembly file given");
		return ERRVAL_ARGS;
	}

	// Open input file
	bool in_stdin = strncmp(in_path, PATH_STDIN, strlen(PATH_STDIN)) == 0;
	FILE* in_fp = in_stdin ? stdin : fopen(in_path, "r");
	if (!in_fp) {
		print_file_err(in_stdin ? "stdin" : in_path, "Failed to open file");
		return ERRVAL_FILE;
	}

	struct error err = { 0 };
	struct parsed_file file = { 0 };

	// Parse input file
	size_t parse_result = parse_file(&err, &file, in_fp);
	fclose(in_fp);

	// Exit if any error occurred when parsing
	if (parse_result > 0) {
		print_err_err(in_path, parse_result, err);
		parsed_file_empty(&file);
		return err.val;
	}

	// Assemble parsed file
	struct llist instructions = { 0 };
	size_t assemble_result = assemble_file(&err, &instructions, file);
	parsed_file_empty(&file);

	// Exit if any error occurred when assembling
	if (assemble_result > 0) {
		llist_empty(&instructions);
		print_err_err(in_path, assemble_result, err);
		return err.val;
	}

	// Open output file
	bool out_stdout = !out_path || strncmp(out_path, PATH_STDOUT, strlen(PATH_STDOUT)) == 0;
	FILE* out_fp = out_stdout ? stdout : fopen(out_path, "wb");
	if (!out_fp) {
		llist_empty(&instructions);
		print_file_err(out_stdout ? "stdout" : out_path, "Failed to open file");
		return ERRVAL_FILE;
	}

	// Output assembled instructions
	struct llist_node* inst_node = instructions.head;
	for (size_t inst_ind = 0; inst_node && inst_ind < instructions.len; inst_node = inst_node->next, inst_ind++) {
		fwrite((ngc_word_t*)(inst_node->val), sizeof(ngc_word_t), 1, out_fp);
	}

	fclose(out_fp);
	llist_empty(&instructions);

	return 0;
}
