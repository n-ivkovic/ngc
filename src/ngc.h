#ifndef NGC_H
#define NGC_H

#include <stdint.h>

#define NGC_IN_CI        (1 << 15)
#define NGC_IN_AA        (1 << 12)
#define NGC_IN_OPR_U     (1 << 10)
#define NGC_IN_OPR_OP1   (1 << 9)
#define NGC_IN_OPR_OP0   (1 << 8)
#define NGC_IN_OPR_ZX    (1 << 7)
#define NGC_IN_OPR_SW    (1 << 6)
#define NGC_IN_TARGET_A  (1 << 5)
#define NGC_IN_TARGET_D  (1 << 4)
#define NGC_IN_TARGET_AA (1 << 3)
#define NGC_IN_JUMP_LT   (1 << 2)
#define NGC_IN_JUMP_EQ   (1 << 1)
#define NGC_IN_JUMP_GT   (1 << 0)

#define NGC_IN_OPR_NEG1 (NGC_IN_OPR_U | NGC_IN_OPR_OP1 | NGC_IN_OPR_OP0 | NGC_IN_OPR_ZX)

#define NGC_WORD_MIN INT16_MIN
#define NGC_WORD_MAX INT16_MAX
#define NGC_UWORD_MAX UINT16_MAX

/**
 * NandGame computer signed word (int16_t).
 */
typedef int16_t ngc_word_t;

/**
 * NandGame computer unsigned word (uint16_t).
 */
typedef uint16_t ngc_uword_t;

#endif
