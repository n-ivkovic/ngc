# NGC Assembler Tests

End-to-end approval tests which are run against a compiled `ngc-asm` executable.

## Test structure

Approval tests are divided into positive and negative tests.

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

Error outputs from `ngc-asm` are formatted as `<input-path>[:<line-number>]: <message>`.

Expected error outputs in **.err** files do not include `<input-path>`, so are formatted as `[:<line-number>]: <message>`.

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

Please read [CONTRIBUTING.md](../../CONTRIBUTING.md) before making any contributions.
