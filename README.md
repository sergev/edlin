# EDLIN — portable C11 line editor

**EDLIN** is a small line-oriented editor inspired by the classic DOS editor: a `*` prompt, numbered lines, one current line, and single-letter commands such as **L**ist, **I**nsert, **D**elete, **W**rite, **E**nd, and **Q**uit.

This repository contains a modern portable C11 implementation that builds on macOS and Linux.

The original ASM source files included for reference come from the official MS-DOS source releases [published by Microsoft](https://github.com/microsoft/MS-DOS).

## Build and Run

Requirements:

- A C11 compiler
- `make`
- Python 3 plus `pexpect` for the integration tests

Build the editor:

```bash
make
```

Run it on a text file:

```bash
./edlin myfile.txt
```

If the file does not exist, EDLIN prints `New file` and starts with an empty buffer.

Use binary mode when Ctrl-Z bytes should be treated as normal data:

```bash
./edlin /B binary.bin
```

## Basic Usage

At the `*` prompt, type commands and press Enter:

- `I` inserts lines; end insert mode with `.` on a line by itself.
- `L` lists lines.
- `D` deletes lines.
- `Stext` searches for `text`.
- `Rold;new` replaces text.
- `Tfile` merges another file.
- `E` saves and exits.
- `Q` quits without saving after confirmation.
- `H` prints in-session help.

Many commands accept line numbers before the command letter, such as `1,3L` to list lines 1 through 3 or `#I` to insert after the last line. See [`Tutorial.md`](Tutorial.md) for a guided introduction and [`Manual.md`](Manual.md) for the full command reference.

## Testing

Install test dependencies once:

```bash
pip install -r requirements-dev.txt
```

Run the full test suite:

```bash
make test
```

## Project Layout

- `src/` — C source files and headers.
- `tests/test_parser.c` — parser unit test.
- `tests/test_edlin_commands.py` — integration tests that drive `./edlin`.
- `Manual.md` — full command reference.
- `Tutorial.md` — beginner walkthrough.
- `AGENT.md` — concise implementation notes for coding agents and contributors.

## License

See [`LICENSE`](LICENSE).
