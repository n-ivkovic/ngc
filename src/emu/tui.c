#define _XOPEN_SOURCE 600

#include "../print.h"
#include "emu.h"

#include <curses.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define NGC_DATA_SIZE (sizeof(ngc_word_t) * NGC_UWORD_MAX)

#define NGC_WORD_DEC_STR_LEN 6 // "-32767"
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

#define SCRDRAW_PER_SEC 12
#define US_PER_SCRDRAW (US_PER_SEC / SCRDRAW_PER_SEC)

#define CLOCK_HZ_MIN 1
#define CLOCK_HZ_MAX 10000
#define CLOCK_HZ_DEFAULT 10
#define CLOCK_HZ_MULTI 10

#define WIN_GROUP_1_X 0
#define WIN_GROUP_1_COLS 23
#define WIN_GROUP_2_X (WIN_GROUP_1_X + WIN_GROUP_1_COLS + 1)
#define WIN_GROUP_2_COLS 27
#define WIN_GROUP_3_X (WIN_GROUP_2_X + WIN_GROUP_2_COLS + 1)
#define WIN_GROUP_3_COLS 17

#define WIN_CLOCK_Y 0
#define WIN_CLOCK_ROWS (WIN_ROWS(2))
#define WIN_REG_Y (WIN_CLOCK_Y + WIN_CLOCK_ROWS)
#define WIN_REG_ROWS (WIN_ROWS(2))
#define WIN_INTR_Y (WIN_REG_Y + WIN_REG_ROWS)
#define WIN_INTR_ROWS (WIN_ROWS(1))
#define WIN_RAM_Y 0
#define WIN_RAM_ROWS (WIN_ROWS(WIN_RAM_ITEMS))
#define WIN_ROM_Y 0
#define WIN_ROM_ROWS (WIN_ROWS(WIN_ROM_ITEMS))

#define WIN_ROWS(items) (items + 2)

#define WINS_TOTAL_ROWS WIN_RAM_ROWS
#define WINS_TOTAL_COLS (1 + WIN_GROUP_1_COLS + 2 + WIN_GROUP_2_COLS + 2 + WIN_GROUP_3_COLS + 1)

#define WINS_OFFSET_Y(scr_rows) ((scr_rows - WINS_TOTAL_ROWS) / 2)
#define WINS_OFFSET_X(scr_cols) ((scr_cols - WINS_TOTAL_COLS) / 2)

#define WIN_RAM_ITEMS 9
#define WIN_ROM_ITEMS 9

#define MEM_OFFSET_IND(ind, size) ((ind / size) * size)

struct term {
	FILE* tty_fp;
};

struct result_wins {
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

// Data to manage in signal handlers
struct term term = { 0 };

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

static bool term_init(struct term* term, const char tty_path[])
{
	FILE* tty_fp = fopen(tty_path, "r");
	if (!tty_fp)
		goto error;

	if (!isatty(fileno(tty_fp)))
		goto error;

	char* term_env = getenv("TERM");
	if (!term_env)
		goto error;

	term->tty_fp = tty_fp;
	newterm(term_env, stdout, tty_fp);

	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	nodelay(stdscr, TRUE);
	curs_set(0);

	return true;

	error:
	if (tty_fp) fclose(tty_fp);
	return false;
}

static void term_free(struct term* term)
{
	endwin();
	if (term->tty_fp) fclose(term->tty_fp);
}

static void window_update_start(WINDOW* win)
{
	// Init cursor position
	wmove(win, 1, 2);

	wnoutrefresh(win);
	doupdate();
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

static void wprint_result_val(WINDOW* win, const int y, const int x, const char label[], const unsigned char label_padding, const ngc_word_t val)
{
	wmove(win, y, x);
	wprint_label(win, label, label_padding);

	wprintw(win, "0x%04hX", val);
}

static void wprint_result_diff(WINDOW* win, const int y, const int x, const char label[], const unsigned char label_padding, const ngc_word_t val_in, const ngc_word_t val_out)
{
	wmove(win, y, x);
	wprint_label(win, label, label_padding);

	wprintw(win, "0x%04hX", val_in);

	// Append diff
	if (val_in != val_out) {
		wprintw(win, " -> 0x%04hX", val_out);
		return;
	}

	// Clear any potential previous diffs
	wclrtoeol(win);
}

static void window_clock_update(WINDOW* win, const struct ngc_clock clock)
{
	window_update_start(win);

	int y, x;
	getyx(win, y, x);

	wmove(win, y, x);
	wprint_label(win, "Status", 6);
	wprintw(win, clock.enabled ? "Running" : "Paused");
	wclrtoeol(win);
	y++;

	wmove(win, y, x);
	wprint_label(win, "Freq", 6);
	wprintw(win, "%hd Hz", clock.hz);
	wclrtoeol(win);
	y++;

	window_update_finish(win, "Clock");
}

static void window_registers_update(WINDOW* win, const struct ngc_tick_result tick_result)
{
	window_update_start(win);

	int y, x;
	getyx(win, y, x);

	wprint_result_diff(win, y, x, "A", 1, tick_result.a_in, tick_result.a_out);
	y++;

	wprint_result_diff(win, y, x, "D", 1, tick_result.d_in, tick_result.d_out);
	y++;

	window_update_finish(win, "Registers");
}

static void window_internal_update(WINDOW* win, const struct ngc_tick_result tick_result)
{
	window_update_start(win);

	int y, x;
	getyx(win, y, x);

	wprint_result_val(win, y, x, "ALU", 3, (ngc_word_t)tick_result.alu);
	y++;

	window_update_finish(win, "Internal");
}

static void window_ram_update(WINDOW* win, const struct ngc_tick_result tick_result, const struct ngc_data* ram)
{
	window_update_start(win);

	int y, x;
	getyx(win, y, x);

	// Print addresses
	for (size_t ind = MEM_OFFSET_IND(tick_result.a_in, WIN_RAM_ITEMS); ind < (size_t)tick_result.a_in + WIN_RAM_ITEMS && ind <= NGC_UWORD_MAX; ind++, y++) {
		char label[NGC_UWORD_DEC_STR_LEN + 1] = { 0 };
		sprintf(label, "%ld", ind);

		bool highlight = ind == (size_t)tick_result.a_in;
		ngc_word_t val_in = highlight ? tick_result.aa_in : ram->data[ind];
		ngc_word_t val_out = highlight ? tick_result.aa_out : ram->data[ind];

		if (highlight)
			wattron(win, A_REVERSE);

		wprint_result_diff(win, y, x, label, NGC_WORD_DEC_STR_LEN, val_in, val_out);

		if (highlight)
			wattroff(win, A_REVERSE);
	}

	window_update_finish(win, "RAM [A: *A]");
}

static void window_rom_update(WINDOW* win, const struct ngc_tick_result tick_result, const struct ngc_data* rom)
{
	window_update_start(win);

	int y, x;
	getyx(win, y, x);

	// Print addresses
	for (size_t ind = MEM_OFFSET_IND(tick_result.pc_in, WIN_ROM_ITEMS); ind < (size_t)tick_result.pc_in + WIN_ROM_ITEMS && ind <= NGC_UWORD_MAX; ind++, y++) {
		char label[NGC_UWORD_DEC_STR_LEN + 1] = { 0 };
		sprintf(label, "%ld", ind);

		bool highlight = ind == (size_t)tick_result.pc_in;

		if (highlight)
			wattron(win, A_REVERSE);

		wprint_result_val(win, y, x, label, NGC_WORD_DEC_STR_LEN, rom->data[ind]);

		if (highlight)
			wattroff(win, A_REVERSE);
	}

	window_update_finish(win, "ROM [PC: In]");
}

static void windows_init(struct result_wins* wins, const int scr_rows, const int scr_cols)
{
	if (!wins)
		return;

	int y_offset = WINS_OFFSET_Y(scr_rows);
	int x_offset = WINS_OFFSET_X(scr_cols);

	wins->clock = newwin(WIN_CLOCK_ROWS, WIN_GROUP_1_COLS, WIN_CLOCK_Y + y_offset, WIN_GROUP_1_X + x_offset);
	wins->registers = newwin(WIN_REG_ROWS, WIN_GROUP_1_COLS, WIN_REG_Y + y_offset, WIN_GROUP_1_X + x_offset);
	wins->internal = newwin(WIN_INTR_ROWS, WIN_GROUP_1_COLS, WIN_INTR_Y + y_offset, WIN_GROUP_1_X + x_offset);
	wins->ram = newwin(WIN_RAM_ROWS, WIN_GROUP_2_COLS, WIN_RAM_Y + y_offset, WIN_GROUP_2_X + x_offset);
	wins->rom = newwin(WIN_ROM_ROWS, WIN_GROUP_3_COLS, WIN_ROM_Y + y_offset, WIN_GROUP_3_X + x_offset);
}

static void windows_mem_update(const struct result_wins wins, const struct ngc_tick_result tick_result, const struct ngc_mem* mem)
{
	if (!mem)
		return;

	window_registers_update(wins.registers, tick_result);
	window_internal_update(wins.internal, tick_result);
	window_ram_update(wins.ram, tick_result, &mem->ram);
	window_rom_update(wins.rom, tick_result, &mem->rom);
}

static void windows_recenter(struct result_wins* wins, const int scr_rows, const int scr_cols)
{
	if (!wins)
		return;

	int y_offset = WINS_OFFSET_Y(scr_rows);
	int x_offset = WINS_OFFSET_X(scr_cols);

	mvwin(wins->clock, WIN_CLOCK_Y + y_offset, WIN_GROUP_1_X + x_offset);
	mvwin(wins->registers, WIN_REG_Y + y_offset, WIN_GROUP_1_X + x_offset);
	mvwin(wins->internal, WIN_INTR_Y + y_offset, WIN_GROUP_1_X + x_offset);
	mvwin(wins->ram, WIN_RAM_Y + y_offset, WIN_GROUP_2_X + x_offset);
	mvwin(wins->rom, WIN_ROM_Y + y_offset, WIN_GROUP_3_X + x_offset);
}

static void windows_free(struct result_wins* wins)
{
	if (!wins)
		return;

	delwin(wins->clock);
	delwin(wins->registers);
	delwin(wins->internal);
	delwin(wins->ram);
	delwin(wins->rom);
}

static bool ngc_data_copy_fp(struct ngc_data* data, FILE* fp)
{
	// Invalid params
	if (!data || !fp)
		return false;

	// Read file bytes into buffer array
	uint8_t buffer[NGC_DATA_SIZE + 1]; // +1 to detect when max exceeded
	size_t buffer_n = fread(buffer, sizeof(uint8_t), sizeof(buffer), fp);

	// Invalid number of file bytes
	if (buffer_n == 0 || buffer_n > NGC_DATA_SIZE)
		return false;

	// Get number of words in file bytes
	imaxdiv_t buffer_words = imaxdiv(buffer_n, sizeof(ngc_uword_t));
	if (buffer_words.rem > 0)
		return false;

	// Copy file buffer into NGC data
	if (!ngc_data_copy(data, (ngc_word_t*)buffer, (ngc_uword_t)buffer_words.quot))
		return false;

	return true;
}

static void exit_sig(int signal)
{
	term_free(&term);
	exit(signal);
}

int main(int argc, char* argv[])
{
	bool success = false;
	bool term_set = false;
	bool windows_set = false;

	if (argc < 2) {
		print_err("No ROM file given");
		goto exit;
	} else if (argc > 2) {
		print_err("Too many arguments given");
		goto exit;
	}

	// Init NGC memory
	struct ngc_mem mem;
	if (!ngc_mem_init(&mem)) {
		print_err("Failed to intialize NGC memory");
		goto exit;
	}

	// Open ROM file
	bool rom_stdin = strncmp(argv[1], PATH_STDIN, strlen(PATH_STDIN)) == 0;
	FILE* rom_fp = rom_stdin ? stdin : fopen(argv[1], "rb");
	if (!rom_fp) {
		print_err("Failed to open ROM file: '%s'", rom_stdin ? "stdin" : argv[1]);
		goto exit;
	}

	// Copy ROM file into NGC memory
	bool rom_copy_success = ngc_data_copy_fp(&mem.rom, rom_fp);
	fclose(rom_fp);
	if (!rom_copy_success) {
		print_err("Failed to load ROM file into NGC memory");
		goto exit;
	}

	// Signal handlers
	signal(SIGQUIT, exit_sig);
	signal(SIGINT, exit_sig);
	signal(SIGTERM, exit_sig);

	// Init terminal for curses output
	term_set = term_init(&term, PATH_TTY);
	if (!term_set) {
		print_err("Failed to init terminal");
		goto exit;
	}

	// Init tick result windows
	struct result_wins windows;
	int scr_rows, scr_cols;
	getmaxyx(stdscr, scr_rows, scr_cols);
	windows_init(&windows, scr_rows, scr_cols);
	windows_set = true;

	// Init NGC clock
	struct ngc_clock clock = { .enabled = true, .hz = CLOCK_HZ_DEFAULT };
	long long last_tick_epoch_us = 0, last_scrdraw_epoch_us = 0;

	struct ngc_tick_result tick_result;

	// Tick NGC clock until end of ROM reached
	while (mem.pc < mem.rom.len) {
		long long loop_start_epoch_us = get_epoch_us();
		long long now_epoch_us;

		bool reset = false, step = false;

		// Read keyboard input
		int input = getch();
		switch (input) {
			case 'q':
			case 'Q':
			case 27: // Esc
				success = true;
				goto exit;
			case 'r':
			case 'R':
				if (!ngc_mem_reset(&mem)) {
					print_err("Failed to reset NGC memory");
					goto exit;
				}
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

		long long us_per_tick = US_PER_SEC / clock.hz;
		long long us_per_loop = (us_per_tick > US_PER_SCRDRAW ? us_per_tick : US_PER_SCRDRAW) / 2;

		// Do tick
		now_epoch_us = get_epoch_us();
		if (reset || step || (clock.enabled && now_epoch_us - last_tick_epoch_us > us_per_tick)) {
			last_tick_epoch_us = now_epoch_us;
			ngc_tick(&tick_result, &mem);
		}

		// Update display
		now_epoch_us = get_epoch_us();
		if (reset || now_epoch_us - last_scrdraw_epoch_us > US_PER_SCRDRAW) {
			last_scrdraw_epoch_us = now_epoch_us;

			// Resize result windows if needed
			int new_scr_rows, new_scr_cols;
			getmaxyx(stdscr, new_scr_rows, new_scr_cols);
			if (new_scr_rows != scr_rows || new_scr_cols != scr_cols) {
				scr_rows = new_scr_rows;
				scr_cols = new_scr_cols;
				windows_recenter(&windows, scr_rows, scr_cols);
				clear();
				refresh();
			}

			// Draw result windows
			window_clock_update(windows.clock, clock);
			windows_mem_update(windows, tick_result, &mem);
		}

		// Sleep before looping if required
		long long loop_sleep_us = us_per_loop - (get_epoch_us() - loop_start_epoch_us);
		if (loop_sleep_us > 0)
			sleep_us(loop_sleep_us);
	}

	success = true;

	exit:
	if (windows_set) windows_free(&windows);
	if (term_set) term_free(&term);
	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
