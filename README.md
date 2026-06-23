# HULK Compiler Interface Contract
# Requirments 
- A unix system with *gcc* avaible.
## Build
For building the project run:

```bash
make build
```
- It will create a `./hulk` executable **in the repo root** after running.

## Invoke

```bash
./hulk <file.hulk>
```

**On success (exit 0):**

- Produces an executable `./output` in the current directory.
- `./output` must run on **Linux x86_64**.

**On error:**

Exits with the code corresponding to the first (most fundamental) error type found:

| Exit code | Error type |
|-----------|------------|
| `1` | Lexical error |
| `2` | Syntactic error |
| `3` | Semantic error |

Prints **one line per error** to **stderr** in the format:

```
(line,col) TYPE: message
```

- `line`, `col`: 1-based position of the first token attributable to the error.
- Use `(0,0)` if no sensible source position exists.
- `TYPE` is exactly one of: `LEXICAL`, `SYNTACTIC`, `SEMANTIC`.
- If errors of multiple types exist, the exit code reflects the most fundamental
  type (LEXICAL takes priority over SYNTACTIC, SYNTACTIC over SEMANTIC).

**Note**: For more advanced features like compiling just to QBE, or using 
special flags run:

```bash
./hulk --help
```
## Architecture
For a summary of the architecture please read [REPORT.md](/REPORT.md)
