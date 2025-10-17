# NGC Assembler Tests

End-to-end tests which are run against the compiled `ngc-asm` executable.

## Test structure

Tests are divided into positive and negative tests.

### Positive tests

Positive tests ensure when assembling a given assembly file input, the assembler returns an expected machine code output (`stdout`).
No error output (`stderr`) is expected.

| File path           | Description |
| ---                 | ---         |
| {test_name}.**in**  | Assembly file input. |
| {test_name}.**out** | Expected machine code output. |

### Negative tests

Negative tests ensure when assembling a given assembly file input, the assembler returns an expected error output (`stderr`).
No machine code output (`stdout`) is expected.

| File path           | Description |
| ---                 | ---         |
| {test_name}.**in**  | Assembly file input. |
| {test_name}.**err** | Expected error output. |

## CLI usage

```
$ ./test.sh [-ap] <path>
```

| Option             | Description |
| ---                | ---         |
| `<path>`           | Path to `ngc-asm` executable. |
| `-a`, `--ascii`    | Print ASCII-only text, do not print Unicode text. |
| `-p`, `--no-color` | Print uncoloured text. |

### Output

On completion, the script prints the number of passed tests.

If any tests failed, the script will also print the number of failed tests and the path of each failed test.

### Exit statuses

| Value | Description |
| ---   | ---         |
| 0     | Tests passed. |
| 1     | Tests failed. |
| 2     | Invalid command options. |
| 3     | Invalid test structure. |

## Contributing

Please adhere to the following when creating a pull request:

- Ensure changes are branched from `develop` and the pull request merges back into `develop`.
- Ensure changes match the general coding style of the project. However, no specific style is prescribed.
- Ensure changes to [**./test.sh**](./test.sh):
    - Do not cause [ShellCheck](https://www.shellcheck.net/) to return any errors or warnings.
    - Conform to [POSIX.1-2001](https://pubs.opengroup.org/onlinepubs/000095399/), but do not utilise features removed from or marked as obsolete in newer standards up to [POSIX.1-2024](https://pubs.opengroup.org/onlinepubs/9799919799/).
