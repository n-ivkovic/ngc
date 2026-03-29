#ifndef EMU_H
#define EMU_H

#include "../dynarr.h"
#include "../ngc.h"

#include <stdbool.h>

#define NGC_RXM_LEN NGC_UWORD_MAX
#define NGC_RXM_SIZE (NGC_RXM_LEN * sizeof(ngc_word_t))

/**
 * NandGame computer memory.
 */
struct ngc_mem {
	ngc_word_t a;
	ngc_word_t d;
	ngc_uword_t pc;
	struct dynarr ram; // Dynamic array of ngc_word_t
	struct dynarr rom; // Dynamic array of ngc_word_t
};

/**
 * Snapshot of NandGame computer memory.
 */
struct ngc_mem_result {
	ngc_word_t a;
	ngc_word_t d;
	ngc_uword_t pc;
	ngc_word_t aa;
};

/**
 * Calculated result of ticking NandGame computer processor.
 */
struct ngc_tick
{
	ngc_word_t inst;
	ngc_word_t alu;
	struct ngc_mem_result in;
	struct ngc_mem_result out;
};

/**
 * Get value at address from RAM/ROM in NandGame computer memory.
 *
 * @param rxm RAM/ROM to get value from.
 * @param addr Address of value to get.
 * @returns Value at address.
 */
ngc_word_t ngc_rxm_get(const struct dynarr rxm, const ngc_uword_t addr);

/**
 * Set values of RAM/ROM in NandGame computer memory to copy of values given.
 *
 * @param rxm RAM/ROM to set values of.
 * @param addr Address of RAM/ROM to set values from.
 * @param words Pointer to values to copy.
 * @param len Number of values to copy.
 * @returns Values copied to RAM/ROM. NULL if error.
 */
ngc_word_t* ngc_rxm_set(struct dynarr* rxm, const ngc_uword_t addr, const ngc_word_t* words, const size_t len);

/**
 * Pre-allocate initial space for NandGame computer memory.
 *
 * @param mem NandGame computer memory to pre-allocate space for.
 */
void ngc_mem_alloc(struct ngc_mem* mem);

/**
 * Free values within NandGame computer memory.
 * NandGame computer memory will be in unallocated state once values are freed.
 *
 * @param mem NandGame computer memory to free values from.
 */
void ngc_mem_empty(struct ngc_mem* mem);

/**
 * Initialize volatile NandGame computer memory to default state, preserve ROM.
 *
 * @param mem NandGame computer memory to reset.
 */
void ngc_mem_reset(struct ngc_mem* mem);

/**
 * Calculate result of NandGame computer processor tick.
 *
 * @param tick Result of NandGame computer processor tick.
 * @param mem NandGame computer memory.
 */
void ngc_tick_calc(struct ngc_tick* tick, const struct ngc_mem mem);

/**
 * Set NandGame computer memory to result of calculated processor tick.
 *
 * @param mem NandGame computer memory.
 * @param tick Result of NandGame computer processor tick.
 * @returns Whether NandGame computer memory was updated successfully.
 */
bool ngc_tick_set(struct ngc_mem* mem, const struct ngc_tick tick);

#endif
