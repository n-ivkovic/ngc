#include "emu.h"

#define NGC_RAM_ALLOC_LEN 512
#define NGC_ROM_ALLOC_LEN 32

#define NGC_RXM_ALLOC(rxm, capacity) dynarr_alloc(rxm, capacity, sizeof(ngc_word_t))

ngc_word_t ngc_rxm_get(const struct dynarr rxm, const ngc_uword_t addr)
{
	ngc_word_t* val = dynarr_get(rxm, (size_t)addr);
	return (val) ? *val : 0;
}

ngc_word_t* ngc_rxm_set(struct dynarr* rxm, const ngc_uword_t addr, const ngc_word_t* words, const size_t len)
{
	// Ensure length of dynamic array does not exceed max RAM/ROM size
	if ((size_t)addr + len > NGC_RXM_LEN + 1)
		return NULL;

	return dynarr_set(rxm, (size_t)addr, words, len, sizeof(ngc_word_t));
}

void ngc_mem_alloc(struct ngc_mem* mem)
{
	// Failure to pre-allocate space is non-critical - not checking return results
	NGC_RXM_ALLOC(&mem->ram, NGC_RAM_ALLOC_LEN);
	NGC_RXM_ALLOC(&mem->rom, NGC_ROM_ALLOC_LEN);
}

void ngc_mem_empty(struct ngc_mem* mem)
{
	if (!mem)
		return;

	mem->a = 0;
	mem->d = 0;
	mem->pc = 0;
	dynarr_empty(&mem->ram);
	dynarr_empty(&mem->rom);
}

void ngc_mem_reset(struct ngc_mem* mem)
{
	if (!mem)
		return;

	mem->a = 0;
	mem->d = 0;
	mem->pc = 0;
	dynarr_empty(&mem->ram);

	// Failure to pre-allocate space is non-critical - not checking return results
	NGC_RXM_ALLOC(&mem->ram, NGC_RAM_ALLOC_LEN);
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
static ngc_word_t ngc_alu_calc(const ngc_word_t inst, const ngc_word_t a, const ngc_word_t d, const ngc_word_t aa)
{
	// Set X and Y values
	ngc_word_t x = (inst & NGC_IN_OPR_ZX) ? 0 : d;
	ngc_word_t y = (inst & NGC_IN_AA) ? aa : a;

	// Swap X and Y values
	if (inst & NGC_IN_OPR_SW) {
		ngc_word_t temp = x;
		x = y;
		y = temp;
	}

	// Return arithmatic or logic calculation of X and Y values
	switch (inst & (NGC_IN_OPR_U | NGC_IN_OPR_OP1 | NGC_IN_OPR_OP0)) {
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
static bool ngc_jump_calc(const ngc_word_t inst, const ngc_word_t alu)
{
	switch ((inst & (NGC_IN_JUMP_LT | NGC_IN_JUMP_EQ | NGC_IN_JUMP_GT))) {
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

void ngc_tick_calc(struct ngc_tick* tick, const struct ngc_mem mem)
{
	if (!tick)
		return;

	ngc_word_t inst = ngc_rxm_get(mem.rom, mem.pc);
	ngc_word_t mem_aa = ngc_rxm_get(mem.ram, (ngc_uword_t)mem.a);

	// Set input values
	tick->inst = inst;
	tick->in.a = mem.a;
	tick->in.d = mem.d;
	tick->in.pc = mem.pc;
	tick->in.aa = mem_aa;

	// Instruction is ALU instruction
	if (inst & NGC_IN_CI) {
		ngc_word_t alu = ngc_alu_calc(inst, mem.a, mem.d, mem_aa);
		tick->alu = alu;

		// Memory is set to ALU output if instruction targets it
		tick->out.a = (inst & NGC_IN_TARGET_A) ? alu : mem.a;
		tick->out.d = (inst & NGC_IN_TARGET_D) ? alu : mem.d;
		tick->out.aa = (inst & NGC_IN_TARGET_AA) ? alu : mem_aa;

		// Program counter is set to A register output if ALU output meets jump conditions
		tick->out.pc = ngc_jump_calc(inst, alu) ? tick->out.a : mem.pc + 1;
	// Instruction is data instruction
	} else {
		tick->alu = 0;
		tick->out.a = inst;
		tick->out.d = mem.d;
		tick->out.aa = mem_aa;
		tick->out.pc = mem.pc + 1;
	}
}

bool ngc_tick_set(struct ngc_mem* mem, const struct ngc_tick tick)
{
	if (!mem)
		return false;

	mem->a = tick.out.a;
	mem->d = tick.out.d;
	mem->pc = tick.out.pc;

	if (tick.in.aa != tick.out.aa && !ngc_rxm_set(&mem->ram, (ngc_uword_t)tick.in.a, &tick.out.aa, 1))
		return false;

	return true;
}
