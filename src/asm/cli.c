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
		print_err("%s:%zu: %s", f_path, f_line, err.msg);
	else
		print_file_err(f_path, err.msg);
}

int main(int argc, char* argv[])
{
	char* in_path = NULL;
	char* out_path = NULL;

	int opt;
	extern char* optarg;
	extern int optind, optopt;

	// Set vars from opts
	while ((opt = getopt(argc, argv, ":i:o:vV")) != -1) {
		switch (opt) {
			case 'o':
				out_path = optarg;
				break;
			case 'v':
			case 'V':
				printf("ngc-asm v0.6.1%s", EOL);
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

	bool in_stdin = !in_path || strncmp(in_path, PATH_STDIN, strlen(PATH_STDIN) + 1) == 0;
	char* in_name = in_stdin ? PATH_STDIN : in_path;

	// Open input file
	FILE* in_fp = in_stdin ? stdin : fopen(in_path, "r");
	if (!in_fp) {
		print_file_err(in_name, "Failed to open file");
		return ERRVAL_FILE;
	}

	struct error err = { 0 };

	// Initialise struct to store parsed input file
	struct parsed_file file = { 0 };
	parsed_file_alloc(&file);

	// Parse input file
	size_t parse_result = parse_file(&err, &file, in_fp, LANG_FEAT_ALL);
	fclose(in_fp);

	// Exit if any error occurred when parsing
	if (parse_result > 0) {
		print_err_err(in_name, parse_result, err);
		parsed_file_empty(&file);
		return err.val;
	}

	// Initialise dynamic array of assembled parsed file
	struct dynarr instructions = { 0 };
	dynarr_alloc(&instructions, 0x20, sizeof(ngc_word_t)); // Failure to pre-allocate space is non-critical - not checking return result

	// Assemble parsed file
	size_t assemble_result = assemble_file(&err, &instructions, file);
	parsed_file_empty(&file);

	// Exit if any error occurred when assembling
	if (assemble_result > 0) {
		dynarr_empty(&instructions);
		print_err_err(in_name, assemble_result, err);
		return err.val;
	}

	bool out_stdout = !out_path || strncmp(out_path, PATH_STDOUT, strlen(PATH_STDOUT) + 1) == 0;
	char* out_name = out_stdout ? PATH_STDOUT : out_path;

	// Open output file
	FILE* out_fp = out_stdout ? stdout : fopen(out_path, "wb");
	if (!out_fp) {
		dynarr_empty(&instructions);
		print_file_err(out_name, "Failed to open file");
		return ERRVAL_FILE;
	}

	// Output assembled instructions
	fwrite(instructions.vals, instructions.val_size, instructions.len, out_fp);

	fclose(out_fp);
	dynarr_empty(&instructions);

	return 0;
}
