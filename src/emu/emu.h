#ifndef EMU_H
#define EMU_H

#include "../ngc.h"

#include <stdbool.h>

/**
 * NandGame computer RAM or ROM.
 */
struct ngc_data {
	ngc_word_t data[NGC_UWORD_MAX];
	ngc_uword_t len;
};

/**
 * NandGame computer memory.
 */
struct ngc_mem {
	ngc_word_t a;
	ngc_word_t d;
	ngc_uword_t pc;
	struct ngc_data ram;
	struct ngc_data rom;
};

/**
 * Result of NandGame computer processor tick, showing difference in memory before and after tick.
 */
struct ngc_tick_result {
	ngc_word_t in;
	ngc_word_t alu;
	ngc_word_t a_in;
	ngc_word_t a_out;
	ngc_word_t d_in;
	ngc_word_t d_out;
	ngc_word_t aa_in;
	ngc_word_t aa_out;
	ngc_uword_t pc_in;
	ngc_uword_t pc_out;
};

/**
 * Initialize all NandGame computer memory to default state.
 *
 * @param mem NandGame computer memory to initialize.
 * @returns Whether NandGame computer memory was initialized successfully.
 */
bool ngc_mem_init(struct ngc_mem* mem);

/**
 * Initialize volatile NandGame computer memory to default state, preserve ROM.
 *
 * @param mem NandGame computer memory to reset.
 * @returns Whether NandGame computer memory was reset successfully.
 */
bool ngc_mem_reset(struct ngc_mem* mem);

/**
 * Copy words into NandGame computer data (RAM or ROM).
 *
 * @param data NandGame computer data to copy into.
 * @param src Words to copy from.
 * @param n Number of words to copy.
 * @returns Whether NandGame computer data was updated successfully.
 */
bool ngc_data_copy(struct ngc_data* data, const ngc_word_t* src, const ngc_uword_t n);

/**
 * Tick NandGame computer processor.
 *
 * @param result Difference in memory before and after tick.
 * @param mem NandGame computer memory.
 */
void ngc_tick(struct ngc_tick_result* result, struct ngc_mem* mem);

#endif
