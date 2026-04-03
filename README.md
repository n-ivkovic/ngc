# NGC

A project that encompasses both an assembler and a TUI emulator for the fictional [**N**and**G**ame](https://nandgame.com) **c**omputer.

Written in C99 and complies with [POSIX.1-2001](https://pubs.opengroup.org/onlinepubs/000095399/) through to [POSIX.1-2024](https://pubs.opengroup.org/onlinepubs/9799919799/).

![ngc demo gif](.img/ngc.gif)

This project is not AI-generated - all has been originally written by myself. Any commits authored by AI are from other contributors.

## Assembler

The assembler `ngc-asm` reads NandGame assembly instructions from a given file path and assembles NandGame machine code.

```
$ cat memset.asm
D = -1

LABEL loop
A, D = D + 1
*A = -1
A = loop
JMP

$ ngc-asm memset.asm -o memset.bin
```

NandGame machine code is output using the system's endianness.
The original NandGame does not indicate the endianness of the computer, so none has been prescribed.

### CLI usage

```
$ ngc-asm [-vV] [-o <path>] [<path>]
```

| Option      | Description |
| ---         | ---         |
| `<path>`    | Path to assembly file. File will be read from `stdin` if a path is not specified or path is `-`. |
| -o `<path>` | Path to output assembled machine code. Assembled machine code will be output to `stdout` if a path is not specified. |
| -v, -V      | Print version and exit. |

#### Exit statuses

| Value | Description |
| ---   | ---         |
| 0     | Success.    |
| 1     | General failure. |
| 2     | Failure due to invalid CLI arguments. |
| 4     | Failure due to invalid assembly file. |
| 8     | Failure due to assembly language syntax error. |

### Language usage

The assembler supports a superset of the original NandGame assembly language.
The main new language feature supported is macro definitions.

The assembler intends to be backwards-compatible with the original NandGame assembly language.
Any unknown differences from the original NandGame should be considered a bug.

#### Macro definitions

The assembler supports syntax for declaring macros.
A macro definition begins with the opening statement `%MACRO <name> [<parameters> ...]`, followed by the instructions within the macro, and then completed with the closing statement `%END`.

```
%MACRO push.static address
A = address
D = *A
push.D
%END
```

Some language features are either forbidden or behave differently if they are used within a macro definition.
The table below lists differences in language features between usage within regular assembly and usage within a macro definition.

| Feature               | Within regular assembly                      | Within macro definition                                  |
| ---                   | ---                                          | ---                                                      |
| `DEFINE` statements   | Value can be referenced anywhere in the file | Value can be referenced only within the macro definition |
| `LABEL` statements    | Value can be referenced anywhere in the file | Value can be referenced only within the macro definition |
| `%MACRO` definitions  | Allowed                                      | Forbidden                                                |

Like `DEFINE` and `LABEL` statements, a macro can be referenced both before and after its definition.

#### Known differences from the original NandGame

If the key used in a `DEFINE` or `LABEL` statement is also a valid number, when referenced:

- The original NandGame assembler will use the `DEFINE`/`LABEL` value rather than the numerical value.
- The `ngc-asm` assembler will use the numerical value rather than the `DEFINE`/`LABEL` value.

E.g:

```
DEFINE xABC 123
A = xABC
```

- The original NandGame assembler will set `A` to 123.
- The `ngc-asm` assembler will set `A` to 0xABC (2989).

### Wishlist

The following assembler features are being considered, but not guaranteed to be implemented:

- CLI option(s) to set the endianness of the output machine code.
- Support for combining multiple assembly files (for now you can `cat` assembly files together, e.g. `$ cat shared.asm my_program.asm | ngc-asm -`). This could be in the form of either:
    - A new language feature to import other assembly files, e.g. `%INCLUDE shared.asm`.
    - A new CLI option to prefix the given assembly file with another, e.g. `ngc-asm -i shared.asm my_program.asm`.
- Allow multiple syntax errors to be returned when assembling files, rather than returning only the first encountered error.
- Windows support.

## Emulator

The emulator `ngc-emu` loads NandGame machine code from a given file into the emulated ROM, where it will begin executing.

Once the emulated program counter reaches the end of ROM, the emulator will exit.

```
$ ngc-emu memset.bin
```

NandGame machine code is expected to use the system's endianness.
The original NandGame does not indicate the endianness of the computer, so none has been prescribed.

### CLI usage

```
$ ngc-emu [-pvV] [-c <hz>] [<path>]
```

| Option    | Description |
| ---       | ---         |
| `<path>`  | Path to ROM file. File will be read from `stdin` if a path is not specified or path is `-`. |
| -p        | Start emulation with the processor clock paused. Processor clock starts running if option is not specified. |
| -c `<hz>` | Start emulation at the given processor clock speed. Must be a power of 10 no larger than 10000. Processor clock starts at 10Hz if option is not specified. |
| -e        | Pause the processor clock when the emulator will exit on the next processor step (the emulated program counter reaches the end of ROM). |
| -v, -V    | Print version and exit. |

#### Exit statuses

| Value | Description |
| ---   | ---         |
| 0     | Success.    |
| 1     | General failure. |
| 2     | Failure due to invalid CLI arguments. |

### TUI keyboard controls

| Key        | Action |
| ---        | ---    |
| `P`        | Pause/resume processor clock. |
| `S`        | Advance processor clock one step only (when paused). |
| `[`        | Decrease processor clock speed 10x. |
| `]`        | Increase processor clock speed 10x. |
| `R`        | Reset volatile memory (RAM and registers). |
| `Q`, `Esc` | Exit. |

### TUI display windows

#### Clock

Displays the processor clock speed (`Hz`) and whether or not the processor clock is running (`Status`).

#### Registers

Displays the values of the `A`, `D`, and program counter (`PC`) registers and what their values will be on the next processor step. E.g.
- `A: 0x0000` indicates the current value of the `A` register is 0 and will remain the same on the next processor step.
- `A: 0x0000 -> 0xFFFF` indicates the current value of the `A` register is 0 and will updated be 0xFFFF on the next processor step.

#### Internal

Displays the processor instruction from ROM that has been executed (`Inst`) and the ALU output of the instruction (`ALU`).

#### RAM

Displays the values of a list of RAM addresses and what their values will be on the next processor step. E.g.
- `1234: 0x0000` indicates the current value at address 1234 is 0 and will remain the same on the next processor step.
- `1234: 0x0000 -> 0xFFFF` indicates the current value at address 1234 is 0 and will updated be 0xFFFF on the next processor step.

The addresses surrounding the address given in the `A` register will be the ones listed.

The memory at the address given in the `A` register will be highlighted.
This value indicates what is referred to as `*A` in the NandGame assembly language.

#### ROM

Displays the values of a list of ROM addresses.
The addresses surrounding the address given in the program counter (`PC`) register will be the ones listed.

The memory at the address given in the `PC` register will be highlighted.
This value indicates the instruction that has been executed.

### Wishlist

The following emulator features are being considered, but not guaranteed to be implemented:

- CLI option(s) to set the expected endianness of machine code.
- Additional TUI functionality.
	- Change displayed units per window.
		- Toggle between hex or decimal values.
		- Display ROM as de-assembled instructions.
	- Edit memory.
- Memory-mapped file support, facilitated by [mmap](https://en.wikipedia.org/wiki/Mmap). This would allow the emulator to interface with memory-mapped hardware, both emulated and physical.
- Unit tests.
- Windows support. Possibly would not include support for memory-mapped files.

## Installation

Ensure the following is available on your system:

- GCC or clang. To use clang, `CC=clang` must be passed to make commands.
- An [X/Open Curses](https://pubs.opengroup.org/onlinepubs/7908799/xcurses/curses.h.html) implementation such as ncurses or PDCurses. This is required for the emulator only.

Clone and build:
(Replace `github.com` with `gitlab.com` if using GitLab)
```
$ git clone https://github.com/n-ivkovic/ngc
$ cd ngc
$ make
```

Run without installing:
```
$ ./ngc-asm code.asm | ./ngc-emu
```

Install and run:
```
# make install
$ ngc-asm code.asm | ngc-emu
```

Update after installing:
```
$ git pull master
$ make
# make install
```

Additional options:
```
$ make help
```

## Contributing

Please adhere to the following when creating a pull request:

- Ensure changes are branched from `develop` and the pull request merges back into `develop`.
- Ensure changes do not cause the compiler to return any new warnings or errors.
- Ensure changes match the general coding style of the project. However, no specific style is prescribed.
- Ensures changes conform to standards followed by the project.
    - Unless specified otherwise, only features available in C99 are allowed.
    - POSIX features may be included only in the user interface files: [./src/asm/cli.c](./src/asm/cli.c) and [./src/emu/tui.c](./src/emu/tui.c). Limiting POSIX features to the interfaces only ensures the core logic remains as portable as possible.
        - Specifically conform to [POSIX.1-2001](https://pubs.opengroup.org/onlinepubs/000095399/), but do not utilise features removed from or marked as obsolete in newer standards up to [POSIX.1-2024](https://pubs.opengroup.org/onlinepubs/9799919799/).
        - Features available in standards newer than POSIX.1-2001 may be utilised as long as fallbacks are implemented.
- Ensure changes to `ngc-asm` do not cause any ASM tests to begin failing. Tests can be executed using `make test-asm`.

## License

Copyright &copy; 2025-2026 Nicholas Ivkovic.

Licensed under the GNU General Public License version 3 or later.
See [./LICENSE](./LICENSE), or [https://gnu.org/licenses/gpl.html](https://gnu.org/licenses/gpl.html) if more recent, for details.

This is free software: you are free to change and redistribute it. There is NO WARRANTY, to the extent permitted by law.
