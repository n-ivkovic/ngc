# NGC Contribution Guidelines

Thank you for your interest in contributing to this project.
Please read all applicable sections before contributing.

Please adhere to the following when contributing:

- When creating a pull request, ensure changes are branched from `develop` and the pull request merges back into `develop`.
- Ensure code changes match the general style of the project. However, no specific styles are prescribed at this stage.
- Where POSIX is conformed to, specifically conform to [POSIX.1-2001](https://pubs.opengroup.org/onlinepubs/000095399/).
    - Do not utilize features removed from or marked as obsolete in any newer versions of the standard ([POSIX.1-2008](https://pubs.opengroup.org/onlinepubs/9699919799/), [POSIX.1-2024](https://pubs.opengroup.org/onlinepubs/9799919799/)).
    - Features available in new versions of the standard may be utilised as long as either the functionality is optional or fallbacks conforming to POSIX.1-2001 are implemented.

## Assembler guidelines

Please adhere to the following when contributing to `ngc-asm`:

- Ensure contributions do not cause any tests to begin failing. Tests can be executed using `make test-asm`.
- Ensure end-to-end tests are updated to reflect any changes to expected output.
- Contributions introducing new functionality or error results are recommended to contribute new accompanying end-to-end tests, but are not required to. The project owner will consider what tests need to be created or updated before merging contributions.

## Code guidelines

### C

Please adhere to the following when contributing any C code:

- Ensure contributions do not cause GCC or clang to return any new errors or warnings.
- Unless specified otherwise, contributions must conform to C99 exclusively.
- POSIX features may be utilised only in files where the `_XOPEN_SOURCE` macro is defined. POSIX features are limited to user interface files to keep the core logic of the project as portable as possible.

### Shell

Please adhere to the following when contributing any shell scripts:

- Ensure contributions do not cause [ShellCheck](https://www.shellcheck.net/) to return any new errors or warnings.
- Ensure contributions conform to POSIX exclusively. Do not utilize 'bashisms'.
