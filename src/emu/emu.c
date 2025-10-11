#include "emu.h"

#include <stdbool.h>
#include <string.h>

#define MEM_AA(mem) (mem->ram.data[(ngc_uword_t)mem->a])

/**
 * Initialize NandGame computer data (RAM or ROM) to default state.
 *
 * @param data NandGame computer data to initialize.
 * @returns Whether NandGame computer data was intialized successfully.
 */
static bool ngc_data_init(struct ngc_data* data)
{
	if (!data)
		return false;

	if (!memset(data->data, 0, sizeof(data->data)))
		return false;

	data->len = NGC_UWORD_MAX;
	return true;
}

bool ngc_mem_init(struct ngc_mem* mem)
{
	if (!mem)
		return false;

	mem->a = 0;
	mem->d = 0;
	mem->pc = 0;

	if (!ngc_data_init(&mem->ram))
		return false;

	if (!ngc_data_init(&mem->rom))
		return false;

	return true;
}

bool ngc_mem_reset(struct ngc_mem* mem)
{
	if (!mem)
		return false;

	mem->a = 0;
	mem->d = 0;
	mem->pc = 0;

	if (!ngc_data_init(&mem->ram))
		return false;

	return true;
}

bool ngc_data_copy(struct ngc_data* data, const ngc_word_t* src, const ngc_uword_t n)
{
	if (!data || !src)
		return false;

	if (!memcpy(&data->data, src, n * sizeof(ngc_word_t)))
		return false;

	data->len = n;
	return true;
}

/**
 * Calculate NandGame computer ALU output.
 * Replicates expected output of the original NandGame 'ALU' component.
 *
 * @param in NandGame computer ALU instruction.
 * @param a Value of A register.
 * @param d Value of D register.
 * @param aa Value of RAM at address given in A register.
 * @returns ALU output.
 */
static ngc_word_t ngc_calc_alu(const ngc_word_t in, const ngc_word_t a, const ngc_word_t d, const ngc_word_t aa)
{
	// Set X and Y values
	ngc_word_t x = (in & NGC_IN_OPR_ZX) ? 0 : d;
	ngc_word_t y = (in & NGC_IN_AA) ? aa : a;

	// Swap X and Y values
	if (in & NGC_IN_OPR_SW) {
		ngc_word_t temp = x;
		x = y;
		y = temp;
	}

	// Return arithmatic or logic calculation of X and Y values
	switch (in & (NGC_IN_OPR_U | NGC_IN_OPR_OP1 | NGC_IN_OPR_OP0)) {
		case NGC_IN_OPR_OP0:
			return x | y;
		case NGC_IN_OPR_OP1:
			return x ^ y;
		case NGC_IN_OPR_OP1 | NGC_IN_OPR_OP0:
			return ~x;
		case NGC_IN_OPR_U:
			return x + y;
		case NGC_IN_OPR_U | NGC_IN_OPR_OP0:
			return x + 1;
		case NGC_IN_OPR_U | NGC_IN_OPR_OP1:
			return x - y;
		case NGC_IN_OPR_U | NGC_IN_OPR_OP1 | NGC_IN_OPR_OP0:
			return x - 1;
		case 0:
		default:
			return x & y;
	}
}

/**
 * Calculate whether ALU output meets any jump conditions given in ALU instruction.
 * Replicates expected output of the original NandGame 'Condition' component.
 *
 * @param in NandGame computer ALU instruction.
 * @param alu ALU output.
 * @returns Whether ALU output meets any jump conditions.
 */
static bool ngc_calc_jump(const ngc_word_t in, const ngc_word_t alu)
{
	switch ((in & (NGC_IN_JUMP_LT | NGC_IN_JUMP_EQ | NGC_IN_JUMP_GT))) {
		case NGC_IN_JUMP_GT:
			return alu > 0;
		case NGC_IN_JUMP_EQ:
			return alu == 0;
		case NGC_IN_JUMP_EQ | NGC_IN_JUMP_GT:
			return alu >= 0;
		case NGC_IN_JUMP_LT:
			return alu < 0;
		case NGC_IN_JUMP_LT | NGC_IN_JUMP_GT:
			return alu != 0;
		case NGC_IN_JUMP_LT | NGC_IN_JUMP_EQ:
			return alu <= 0;
		case NGC_IN_JUMP_LT | NGC_IN_JUMP_EQ | NGC_IN_JUMP_GT:
			return true;
		case 0:
		default:
			return false;
	}
}

/**
 * Set inputs of tick result struct.
 *
 * @param result Tick result to update.
 * @param mem NandGame computer memory before it has been updated.
 * @param in NandGame computer instruction (data or ALU).
 */
static void tick_result_set_in(struct ngc_tick_result* result, const struct ngc_mem* mem, const ngc_word_t in)
{
	if (!result || !mem)
		return;

	result->in = in;
	result->a_in = mem->a;
	result->d_in = mem->d;
	result->aa_in = MEM_AA(mem);
	result->pc_in = mem->pc;
}

/**
 * Set outputs of tick result struct.
 *
 * @param result Tick result to update.
 * @param mem NandGame computer memory after it has been updated.
 * @param alu NandGame computer ALU output.
 */
static void tick_result_set_out(struct ngc_tick_result* result, const struct ngc_mem* mem, const ngc_word_t alu)
{
	if (!result || !mem)
		return;

	result->alu = alu;
	result->a_out = mem->a;
	result->d_out = mem->d;
	result->aa_out = mem->ram.data[(ngc_uword_t)result->a_in];
	result->pc_out = mem->pc;
}

void ngc_tick(struct ngc_tick_result* result, struct ngc_mem* mem)
{
	if (!mem)
		return;

	// Get instruction from ROM
	ngc_word_t in = mem->rom.data[mem->pc];

	if (result)
		tick_result_set_in(result, mem, in);

	ngc_word_t alu;

	// Instruction is ALU instruction - update memory based on ALU output
	if (in & NGC_IN_CI) {
		alu = ngc_calc_alu(in, mem->a, mem->d, MEM_AA(mem));

		// *A is target - set to ALU output
		if (in & NGC_IN_TARGET_AA)
			MEM_AA(mem) = alu;

		// D register is target - set to ALU output
		if (in & NGC_IN_TARGET_D)
			mem->d = alu;

		// A register is target - set to ALU output
		if (in & NGC_IN_TARGET_A)
			mem->a = alu;

		// ALU output meets jump condition - set program counter to value of A register
		if (ngc_calc_jump(in, alu))
			mem->pc = mem->a;
		// ALU output does not meet jump condition
		else
			mem->pc++;
	// Instruction is data instruction - set A register to instruction
	} else {
		alu = 0;
		mem->a = in;
		mem->pc++;
	}

	if (result)
		tick_result_set_out(result, mem, alu);
}
