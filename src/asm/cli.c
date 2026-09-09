#define _XOPEN_SOURCE 600

#include "../endianness.h"
#include "../ngc.h"
#include "../print.h"
#include "assemble.h"
#include "parse.h"

#include <stdbool.h>
#include <stdint.h>
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
	enum endianness endianness = endianness_get();
	bool endianness_override = false;

	int opt;
	extern char* optarg;
	extern int optind, optopt;

	// Set vars from opts
	while ((opt = getopt(argc, argv, ":o:eEvV")) != -1) {
		switch (opt) {
			case 'o':
				if (out_path) {
					print_err("Multiple output paths given");
					return ERRVAL_ARGS;
				}
				out_path = optarg;
				break;
			case 'e':
				if (endianness_override) {
					print_err("Multiple endianness options given");
					return ERRVAL_ARGS;
				}
				endianness = LITTLE_E;
				endianness_override = true;
				break;
			case 'E':
				if (endianness_override) {
					print_err("Multiple endianness options given");
					return ERRVAL_ARGS;
				}
				endianness = BIG_E;
				endianness_override = true;
				break;
			case 'v':
			case 'V':
				printf("ngc-asm v0.12.0%s", EOL);
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

	bool in_stdin = !in_path || strncmp(in_path, PATH_STDIN, STR_CHARS(strlen(PATH_STDIN))) == 0;
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

	bool out_stdout = !out_path || strncmp(out_path, PATH_STDOUT, STR_CHARS(strlen(PATH_STDOUT))) == 0;
	char* out_name = out_stdout ? PATH_STDOUT : out_path;

	// Open output file
	FILE* out_fp = out_stdout ? stdout : fopen(out_path, "wb");
	if (!out_fp) {
		dynarr_empty(&instructions);
		print_file_err(out_name, "Failed to open file");
		return ERRVAL_FILE;
	}

	// Output assembled instructions
	if (endianness == endianness_get()) {
		// Endianness given in options matches system - instructions can be output as-is
		fwrite(instructions.vals, instructions.val_size, instructions.len, out_fp);
	} else {
		// Endianness given in options is opposite of system - reverse endianness of each word before outputting
		for (size_t word_ind = 0; word_ind < instructions.len; word_ind++) {
			ngc_word_t* word = dynarr_get(instructions, word_ind);
			if (!word) {
				print_err("Failed to get NGC instruction");
				fclose(out_fp);
				dynarr_empty(&instructions);
				return ERRVAL_FAILURE;
			}

			ngc_word_t word_reverse = 0;
			endianness_reverse(&word_reverse, word, sizeof(ngc_word_t));

			fwrite(&word_reverse, sizeof(word_reverse), 1, out_fp);
		}
	}

	fclose(out_fp);
	dynarr_empty(&instructions);

	return 0;
}
