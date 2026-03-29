#define _XOPEN_SOURCE 600

#include "../print.h"
#include "emu.h"

#include <curses.h>
#include <inttypes.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define NGC_UWORD_DEC_STR_LEN 5 // "65536"

#define PATH_STDIN "-"
#define PATH_TTY "/dev/tty"

#ifdef CLOCK_MONOTONIC
#define SYSCLOCK CLOCK_MONOTONIC
#else
#define SYSCLOCK CLOCK_REALTIME
#endif

#define US_PER_SEC 1000000
#define NS_PER_US 1000

#define TERM_IN_PER_SEC 20
#define TERM_OUT_PER_SEC 10
#define US_PER_TERM_IN (US_PER_SEC / TERM_IN_PER_SEC)
#define US_PER_TERM_OUT (US_PER_SEC / TERM_OUT_PER_SEC)

#define CLOCK_HZ_MIN 1
#define CLOCK_HZ_MAX 10000
#define CLOCK_HZ_MULTI 10

#define WIN_ROW_GAP 0
#define WIN_COL_GAP 1
#define WIN_ROW_PADDING 1
#define WIN_COL_PADDING 2
#define WIN_ROWS(lines) (lines + (WIN_ROW_PADDING * 2))
#define WIN_COLS(cols) (cols + (WIN_COL_PADDING * 2))

#define WIN_GROUP_1_X 0
#define WIN_GROUP_1_COLS WIN_COLS(20)
#define WIN_GROUP_2_X (WIN_GROUP_1_X + WIN_GROUP_1_COLS + WIN_COL_GAP)
#define WIN_GROUP_2_COLS WIN_COLS(23)
#define WIN_GROUP_3_X (WIN_GROUP_2_X + WIN_GROUP_2_COLS + WIN_COL_GAP)
#define WIN_GROUP_3_COLS WIN_COLS(13)

#define WIN_CLOCK_Y 0
#define WIN_CLOCK_ROWS WIN_ROWS(2)
#define WIN_REG_Y (WIN_CLOCK_Y + WIN_CLOCK_ROWS + WIN_ROW_GAP)
#define WIN_REG_ROWS WIN_ROWS(3)
#define WIN_INTR_Y (WIN_REG_Y + WIN_REG_ROWS + WIN_ROW_GAP)
#define WIN_INTR_ROWS WIN_ROWS(1)
#define WIN_RAM_Y 0
#define WIN_RAM_ROWS WIN_ROWS(WIN_RAM_LINES)
#define WIN_ROM_Y 0
#define WIN_ROM_ROWS WIN_ROWS(WIN_ROM_LINES)

#define WINS_TOTAL_ROWS WIN_RAM_ROWS
#define WINS_TOTAL_COLS (WIN_GROUP_1_COLS + WIN_COL_GAP + WIN_GROUP_2_COLS + WIN_COL_GAP + WIN_GROUP_3_COLS)

#define WINS_OFFSET_Y(rows) ((rows > WINS_TOTAL_ROWS) ? (rows - WINS_TOTAL_ROWS) / 2 : 0)
#define WINS_OFFSET_X(cols) ((cols > WINS_TOTAL_COLS) ? (cols - WINS_TOTAL_COLS) / 2 : 0)

#define WIN_RAM_LINES 10
#define WIN_ROM_LINES 10

#define MEM_ADDR_INIT(addr, lines) ((addr / lines) * lines)

enum exit_val {
	SUCCESS_E,
	FAILURE_E,
	INVALID_ARGS_E
};

struct term {
	SCREEN* scr;
	WINDOW* win;
	FILE* in_fp;
	int rows;
	int cols;
};

struct display_wins {
	WINDOW* clock;
	WINDOW* registers;
	WINDOW* internal;
	WINDOW* ram;
	WINDOW* rom;
};

struct ngc_clock {
	bool enabled;
	unsigned short hz;
};

static long long get_epoch_us(void)
{
	struct timespec time;

	clock_gettime(SYSCLOCK, &time);
	return time.tv_sec * US_PER_SEC + time.tv_nsec / NS_PER_US;
}

static void sleep_us(const long long us)
{
	imaxdiv_t us_div = imaxdiv(us, US_PER_SEC);
	struct timespec time = {
		.tv_sec = us_div.quot,
		.tv_nsec = us_div.rem * NS_PER_US
	};

	nanosleep(&time, NULL);
}

static bool term_init(struct term* term, const char in_path[])
{
	if (!term)
		return false;

	term->in_fp = fopen(in_path, "r");
	if (!term->in_fp)
		goto error;

	if (!isatty(fileno(term->in_fp)))
		goto error;

	char* term_env = getenv("TERM");
	if (!term_env)
		goto error;

	term->scr = newterm(term_env, stdout, term->in_fp);
	if (!term->scr)
		goto error;

	term->win = stdscr;

	cbreak();
	noecho();
	nodelay(term->win, TRUE);
	keypad(term->win, TRUE);
	curs_set(0);

	int term_rows, term_cols;
	getmaxyx(term->win, term_rows, term_cols);

	term->rows = term_rows;
	term->cols = term_cols;

	return true;

	error:

	term->scr = NULL;
	term->win = NULL;

	if (term->in_fp) fclose(term->in_fp);
	term->in_fp = NULL;

	return false;
}

static bool term_resized(struct term* term, int* prev_rows, int* prev_cols)
{
	if (!term)
		return false;

	int term_rows, term_cols;
	getmaxyx(term->win, term_rows, term_cols);

	if (term_rows == term->rows && term_cols == term->cols)
		return false;

	if (prev_rows)
		*prev_rows = term->rows;

	if (prev_cols)
		*prev_cols = term->cols;

	term->rows = term_rows;
	term->cols = term_cols;
	return true;
}

static inline int term_get_in(const struct term* term)
{
	return wgetch(term->win);
}

static inline void term_clear(const struct term* term)
{
	wclear(term->win);
	wrefresh(term->win);
}

static void term_free(struct term* term)
{
	endwin();

	term->scr = NULL;
	term->win = NULL;

	if (term->in_fp) fclose(term->in_fp);
	term->in_fp = NULL;
}

static void window_update_start(WINDOW* win)
{
	// Init cursor position
	wmove(win, WIN_ROW_PADDING, WIN_COL_PADDING);
}

static void window_update_finish(WINDOW* win, const char label[])
{
	// Draw dim box + label around window
	wattron(win, A_DIM);
	box(win, 0, 0);
	wmove(win, 0, 1);
	wprintw(win, " %s ", label);
	wattroff(win, A_DIM);

	wnoutrefresh(win);
	doupdate();
}

static void wprint_label(WINDOW* win, const char label[], const unsigned char label_padding)
{
	unsigned char label_len = (unsigned char)strlen(label);

	wprintw(win, "%s: ", label);

	// Append whitespace
	for (char c = label_len; c < label_padding; c++) {
		wprintw(win, " ");
	}
}

static void wprint_result_val(WINDOW* win, const char label[], const unsigned char label_padding, const ngc_uword_t val)
{
	wprint_label(win, label, label_padding);
	wprintw(win, "0x%04hX", val);
}

static void wprint_result_diff(WINDOW* win, const char label[], const unsigned char label_padding, const ngc_uword_t val_in, const ngc_uword_t val_out)
{
	wprint_result_val(win, label, label_padding, val_in);

	if (val_in != val_out) {
		wprintw(win, " -> 0x%04hX", val_out);
		return;
	}

	// Clear any potential previous diffs
	wclrtoeol(win);
}

static void mvwprint_result_val(WINDOW* win, const int y, const int x, const char label[], const unsigned char label_padding, const ngc_uword_t val)
{
	wmove(win, y, x);
	wprint_result_val(win, label, label_padding, val);
}

static void mvwprint_result_diff(WINDOW* win, const int y, const int x, const char label[], const unsigned char label_padding, const ngc_uword_t val_in, const ngc_uword_t val_out)
{
	wmove(win, y, x);
	wprint_result_diff(win, label, label_padding, val_in, val_out);
}

static void window_clock_update(WINDOW* win, const struct ngc_clock clock)
{
	window_update_start(win);

	int y, x;
	getyx(win, y, x);

	wprint_label(win, "Status", 6);
	wprintw(win, clock.enabled ? "Running" : "Paused");
	wclrtoeol(win);

	y++;
	wmove(win, y, x);

	wprint_label(win, "Freq", 6);
	wprintw(win, "%hd Hz", clock.hz);
	wclrtoeol(win);

	window_update_finish(win, "Clock");
}

static void window_registers_update(WINDOW* win, const struct ngc_tick tick)
{
	window_update_start(win);

	int y, x;
	getyx(win, y, x);

	mvwprint_result_diff(win, y, x, "A", 2, tick.in.a, tick.out.a);
	y++;
	mvwprint_result_diff(win, y, x, "D", 2, tick.in.d, tick.out.d);
	y++;
	mvwprint_result_diff(win, y, x, "PC", 2, tick.in.pc, tick.out.pc);

	window_update_finish(win, "Registers");
}

static void window_internal_update(WINDOW* win, const struct ngc_tick tick)
{
	window_update_start(win);

	wprint_result_val(win, "ALU", 3, tick.alu);

	window_update_finish(win, "Internal");
}

static void window_ram_update(WINDOW* win, const struct ngc_tick tick, const struct dynarr ram)
{
	window_update_start(win);

	int y, x;
	getyx(win, y, x);

	// Print portion of RAM around address given in A register
	size_t addr_target = (size_t)(ngc_uword_t)tick.in.a;
	size_t addr_start = MEM_ADDR_INIT(addr_target, WIN_RAM_LINES);
	for (size_t addr = addr_start; addr <= addr_start + WIN_RAM_LINES; addr++, y++) {
		// Clear line if address exceeds RAM size
		if (addr > NGC_RXM_LEN) {
			wmove(win, y, x);
			wclrtoeol(win);
			continue;
		}

		// Use address as label
		char label[NGC_UWORD_DEC_STR_LEN + 1] = { 0 };
		sprintf(label, "%zu", addr);

		// Print diff of value at address between ticks
		if (addr == addr_target) {
			wattron(win, A_REVERSE);
			mvwprint_result_diff(win, y, x, label, NGC_UWORD_DEC_STR_LEN, tick.in.aa, tick.out.aa);
			wattroff(win, A_REVERSE);
		// Print value at address
		} else {
			mvwprint_result_val(win, y, x, label, NGC_UWORD_DEC_STR_LEN, ngc_rxm_get(ram, addr));
			wclrtoeol(win); // Clear any potential previous diffs
		}
	}

	window_update_finish(win, "RAM [A: *A]");
}

static void window_rom_update(WINDOW* win, const struct ngc_tick tick, const struct dynarr rom)
{
	window_update_start(win);

	int y, x;
	getyx(win, y, x);

	// Print portion of ROM around address given in PC register
	size_t addr_target = (size_t)(ngc_uword_t)tick.in.pc;
	size_t addr_start = MEM_ADDR_INIT(addr_target, WIN_ROM_LINES);
	for (size_t addr = addr_start; addr <= addr_start + WIN_ROM_LINES; addr++, y++) {
		// Clear line if address exceeds ROM size
		if (addr > NGC_RXM_LEN) {
			wmove(win, y, x);
			wclrtoeol(win);
			continue;
		}

		// Use address as label
		char label[NGC_UWORD_DEC_STR_LEN + 1] = { 0 };
		sprintf(label, "%zu", addr);

		if (addr == addr_target)
			wattron(win, A_REVERSE);

		// Print value at address
		mvwprint_result_val(win, y, x, label, NGC_UWORD_DEC_STR_LEN, ngc_rxm_get(rom, addr));

		if (addr == addr_target)
			wattroff(win, A_REVERSE);
	}

	window_update_finish(win, "ROM [PC: In]");
}

static bool windows_init(struct display_wins* wins, const struct term* term)
{
	if (!wins || !term)
		return false;

	int y_offset = WINS_OFFSET_Y(term->rows);
	int x_offset = WINS_OFFSET_X(term->cols);

	wins->clock = derwin(term->win, WIN_CLOCK_ROWS, WIN_GROUP_1_COLS, WIN_CLOCK_Y + y_offset, WIN_GROUP_1_X + x_offset);
	if (!wins->clock)
		goto error;

	wins->registers = derwin(term->win, WIN_REG_ROWS, WIN_GROUP_1_COLS, WIN_REG_Y + y_offset, WIN_GROUP_1_X + x_offset);
	if (!wins->registers)
		goto error;

	wins->internal = derwin(term->win, WIN_INTR_ROWS, WIN_GROUP_1_COLS, WIN_INTR_Y + y_offset, WIN_GROUP_1_X + x_offset);
	if (!wins->internal)
		goto error;

	wins->ram = derwin(term->win, WIN_RAM_ROWS, WIN_GROUP_2_COLS, WIN_RAM_Y + y_offset, WIN_GROUP_2_X + x_offset);
	if (!wins->ram)
		goto error;

	wins->rom = derwin(term->win, WIN_ROM_ROWS, WIN_GROUP_3_COLS, WIN_ROM_Y + y_offset, WIN_GROUP_3_X + x_offset);
	if (!wins->rom)
		goto error;

	return true;

	error:
	if (wins->clock) delwin(wins->clock);
	if (wins->registers) delwin(wins->registers);
	if (wins->internal) delwin(wins->internal);
	if (wins->ram) delwin(wins->ram);
	if (wins->rom) delwin(wins->rom);
	return false;
}

static void windows_update(const struct display_wins wins, const struct ngc_clock clock, const struct ngc_tick tick, const struct ngc_mem mem)
{
	window_clock_update(wins.clock, clock);
	window_registers_update(wins.registers, tick);
	window_internal_update(wins.internal, tick);
	window_ram_update(wins.ram, tick, mem.ram);
	window_rom_update(wins.rom, tick, mem.rom);
}

static void windows_free(struct display_wins* wins)
{
	if (!wins)
		return;

	delwin(wins->clock);
	delwin(wins->registers);
	delwin(wins->internal);
	delwin(wins->ram);
	delwin(wins->rom);
}

static bool windows_resized(struct display_wins* wins, const struct term* term, const int prev_term_rows, const int prev_term_cols)
{
	if (!wins || !term)
		return false;

	// Terminal does not fit windows - do not try update
	if (term->rows < WINS_TOTAL_ROWS || term->cols < WINS_TOTAL_COLS)
		return true;

	// Terminal fit windows before resize - move windows
	if (prev_term_rows >= WINS_TOTAL_ROWS && prev_term_cols >= WINS_TOTAL_COLS) {
		int y_offset = WINS_OFFSET_Y(term->rows);
		int x_offset = WINS_OFFSET_X(term->cols);

		mvwin(wins->clock, WIN_CLOCK_Y + y_offset, WIN_GROUP_1_X + x_offset);
		mvwin(wins->registers, WIN_REG_Y + y_offset, WIN_GROUP_1_X + x_offset);
		mvwin(wins->internal, WIN_INTR_Y + y_offset, WIN_GROUP_1_X + x_offset);
		mvwin(wins->ram, WIN_RAM_Y + y_offset, WIN_GROUP_2_X + x_offset);
		mvwin(wins->rom, WIN_ROM_Y + y_offset, WIN_GROUP_3_X + x_offset);

		return true;
	// Terminal did not fit windows before resize - re-init windows
	// Terminal shrinking also shrinks windows which cannot be resized
	} else {
		windows_free(wins);
		return windows_init(wins, term);
	}
}

/**
 * Set values of RAM/ROM in NandGame computer memory to values of read file.
 */
static bool ngc_rxm_set_fp(struct dynarr* rxm, const ngc_uword_t addr, FILE* fp)
{
	if (!rxm || !fp)
		return false;

	// Read file bytes into buffer array
	uint8_t buffer[NGC_RXM_SIZE + 1] = { 0 }; // +1 to detect when max exceeded
	size_t buffer_n = fread(buffer, sizeof(buffer[0]), sizeof(buffer) / sizeof(buffer[0]), fp);

	// Invalid number of file bytes
	if (buffer_n == 0 || buffer_n > NGC_RXM_SIZE)
		return false;

	// Get number of words in file bytes
	imaxdiv_t buffer_words = imaxdiv(buffer_n, sizeof(ngc_word_t));
	if (buffer_words.rem > 0)
		return false;

	// Copy file buffer into NGC data
	if (!ngc_rxm_set(rxm, addr, (ngc_word_t*)buffer, (size_t)buffer_words.quot))
		return false;

	return true;
}

static unsigned short parse_clock_hz_opt(char* optarg)
{
	if (!optarg || optarg[0] != '1')
		return 0;

	unsigned short result = 1;

	// Parse argument and validate it's a power of 10 in a single loop
	size_t optarg_len = strlen(optarg);
	for (size_t ind = 1; ind < optarg_len; ind++) {
		if (optarg[ind] != '0')
			return 0;

		result *= 10;
	}

	if (result < CLOCK_HZ_MIN || result > CLOCK_HZ_MAX)
		return 0;

	return result;
}

// Data to manage in signal handlers
bool term_set = false, windows_set = false;
struct term term = { 0 };
struct display_wins windows = { 0 };
struct ngc_mem mem = { 0 };

/**
 * Free data required to be managed in signal handlers.
 */
static void main_free(void)
{
	ngc_mem_empty(&mem);
	if (windows_set) windows_free(&windows);
	if (term_set) term_free(&term);
}

/**
 * Handler for exit signals.
 */
static void exit_sig(int signal)
{
	main_free();
	exit(signal);
}

int main(int argc, char* argv[])
{
	#define ERR_LEN_MAX 254

	enum exit_val exit_val = FAILURE_E;
	char exit_err[ERR_LEN_MAX + 1] = { 0 };

	int opt;
	extern char* optarg;
	extern int optind, optopt;

	char* rom_path = NULL;
	struct ngc_clock clock = { .enabled = true, .hz = 10 };

	// Set vars from opts
	while ((opt = getopt(argc, argv, ":pc:vV")) != -1) {
		switch (opt) {
			case 'p':
				clock.enabled = false;
				break;
			case 'c':
				;
				clock.hz = parse_clock_hz_opt(optarg);
				if (clock.hz == 0) {
					snprintf(exit_err, ERR_LEN_MAX, "Invalid NGC clock Hz: %s", optarg);
					exit_val = INVALID_ARGS_E;
					goto exit;
				}
				break;
			case 'v':
			case 'V':
				printf("ngc-emu v0.5.0%s", EOL);
				exit_val = SUCCESS_E;
				goto exit;
			case ':':
				snprintf(exit_err, ERR_LEN_MAX, "Option -%c requires an argument", optopt);
				exit_val = INVALID_ARGS_E;
				goto exit;
			case '?':
				snprintf(exit_err, ERR_LEN_MAX, "Unknown option: -%c", optopt);
				exit_val = INVALID_ARGS_E;
				goto exit;
		}
	}

	// Set input file path from arg
	for (; optind < argc; optind++) {
		if (rom_path) {
			snprintf(exit_err, ERR_LEN_MAX, "Multiple ROM files given");
			exit_val = INVALID_ARGS_E;
			goto exit;
		}

		rom_path = argv[optind];
	}

	// Pre-allocate space for NGC memory
	ngc_mem_alloc(&mem);

	// Open ROM file
	bool rom_stdin = !rom_path || strncmp(rom_path, PATH_STDIN, strlen(PATH_STDIN) + 1) == 0;
	FILE* rom_fp = rom_stdin ? stdin : fopen(rom_path, "rb");
	if (!rom_fp) {
		snprintf(exit_err, ERR_LEN_MAX, "Failed to open ROM file: '%s'", rom_stdin ? "stdin" : rom_path);
		goto exit;
	}

	// Load ROM file into NGC memory
	bool rom_loaded = ngc_rxm_set_fp(&mem.rom, 0, rom_fp);
	fclose(rom_fp);
	if (!rom_loaded) {
		snprintf(exit_err, ERR_LEN_MAX, "Failed to load ROM file into NGC memory");
		goto exit;
	}

	// Signal handlers
	signal(SIGQUIT, exit_sig);
	signal(SIGINT, exit_sig);
	signal(SIGTERM, exit_sig);

	// Init terminal for curses output
	term_set = term_init(&term, PATH_TTY);
	if (!term_set) {
		snprintf(exit_err, ERR_LEN_MAX, "Failed to init terminal");
		goto exit;
	}

	// Init display windows
	windows_set = windows_init(&windows, &term);
	if (!windows_set) {
		snprintf(exit_err, ERR_LEN_MAX, "Failed to init display windows");
		goto exit;
	}

	long long last_term_in_epoch_us = 0, last_tick_epoch_us = 0, last_term_out_epoch_us = 0;
	struct ngc_tick tick = { 0 };

	// Calculate first processor tick result
	ngc_tick_calc(&tick, mem);
	last_tick_epoch_us = get_epoch_us();

	// Update emulation until end of ROM reached
	while (mem.pc < mem.rom.len) {
		bool reset = false, step = false;

		// Read keyboard input if due
		if (get_epoch_us() - last_term_in_epoch_us >= US_PER_TERM_IN) {
			switch (term_get_in(&term)) {
				case 'q':
				case 'Q':
				case 27: // Esc
					exit_val = SUCCESS_E;
					goto exit;
				case 'r':
				case 'R':
					reset = true;
					break;
				case 'p':
				case 'P':
					clock.enabled = !clock.enabled;
					break;
				case 's':
				case 'S':
					step = !clock.enabled;
					break;
				case '[':
					if (clock.hz > CLOCK_HZ_MIN)
						clock.hz /= CLOCK_HZ_MULTI;
					break;
				case ']':
					if (clock.hz < CLOCK_HZ_MAX)
						clock.hz *= CLOCK_HZ_MULTI;
					break;
			}

			last_term_in_epoch_us = get_epoch_us();
		}

		long long us_per_tick = US_PER_SEC / clock.hz;

		// Reset processor
		if (reset) {
			ngc_mem_reset(&mem);

			// Calculate first processor tick result
			ngc_tick_calc(&tick, mem);
			last_tick_epoch_us = get_epoch_us();
		}

		// Tick processor if due
		if (step || (clock.enabled && get_epoch_us() - last_tick_epoch_us >= us_per_tick)) {
			// Set NandGame computer memory to processor tick result
			if (!ngc_tick_set(&mem, tick)) {
				snprintf(exit_err, ERR_LEN_MAX, "Failed to set memory to processor tick result");
				goto exit;
			}

			// Calculate next processor tick result
			ngc_tick_calc(&tick, mem);
			last_tick_epoch_us = get_epoch_us();
		}

		// Draw display windows if due
		if (get_epoch_us() - last_term_out_epoch_us >= US_PER_TERM_OUT) {
			// Update display windows on terminal resize
			int prev_term_rows, prev_term_cols;
			if (term_resized(&term, &prev_term_rows, &prev_term_cols)) {
				if (!windows_resized(&windows, &term, prev_term_rows, prev_term_cols)) {
					snprintf(exit_err, ERR_LEN_MAX, "Failed to resize display windows");
					goto exit;
				}

				term_clear(&term);
			}

			windows_update(windows, clock, tick, mem);
			last_term_out_epoch_us = get_epoch_us();
		}

		// Get times events are next due
		long long event_epochs_us[3] = {
			last_term_in_epoch_us + US_PER_TERM_IN,
			last_tick_epoch_us + us_per_tick,
			last_term_out_epoch_us + US_PER_TERM_OUT
		};

		// Get next time an event is due
		long long next_event_epoch_us = event_epochs_us[0];
		for (size_t ind = 0; ind < (sizeof(event_epochs_us) / sizeof(next_event_epoch_us)); ind++) {
			if (event_epochs_us[ind] < next_event_epoch_us)
				next_event_epoch_us = event_epochs_us[ind];
		}

		// Sleep until next event is due
		long long sleep_time_us = next_event_epoch_us - get_epoch_us();
		if (sleep_time_us > 0)
			sleep_us(sleep_time_us);
	}

	exit_val = SUCCESS_E;

	exit:
	main_free();

	if (exit_val != SUCCESS_E && exit_err[0])
		print_err(exit_err);

	return exit_val;
}
