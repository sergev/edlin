# AGENT.md — EDLIN project context

This repository preserves historical MS-DOS EDLIN assembly sources and contains a runnable portable **C11** implementation. Treat `src/` as the active implementation and the `.asm` files as behavioral reference material.

## Current Project Shape

- **Runnable editor:** `./edlin`, built from the C sources with the root `Makefile`.
- **Source and headers:** all C implementation files and project headers live in `src/`.
- **Tests:** `make test` builds `edlin`, runs `tests/test_parser`, then runs the Python integration suite in `tests/test_edlin_commands.py` via `unittest`/`pexpect`.
- **User docs:** `README.md` gives build/run basics, `Manual.md` is the command reference, and `Tutorial.md` is the beginner walkthrough.
- **Historical reference:** top-level `edlin.asm`, `edlcmd1.asm`, `edlcmd2.asm`, `edlmes.asm`, `edlparse.asm`, `edlequ.asm`, `edlstdsw.inc`, `edlin.skl`, `makefile.dos`, and `edlin.lnk`.

## Build and Run

```bash
make
./edlin myfile.txt
./edlin /B binary.bin
make test
```

The root `Makefile` builds only the C port. It uses `-Isrc`, compiles `src/*.c`, and treats `src/*.h` as object dependencies. `makefile.dos` is the historical DOS build recipe and still expects the wider MS-DOS build tree.

Runtime environment:

- `EDLIN_LINES` optionally overrides the logical screen height used by `L` and `P`; otherwise the C port asks `ioctl(TIOCGWINSZ)` and falls back to 25 rows.

## C Port File Map

| File | Role |
|------|------|
| `src/main.c` | Process startup, display-row setup, command loop, multi-command line handling. |
| `src/parser.c`, `src/parser.h` | Invocation parsing (`/B`, filename) and EDLIN command parsing (`GETNUM`-style line refs, `?`, command letters). |
| `src/editor.c`, `src/edlin.h` | Editor state, line storage, current line, insert/delete/replace, copy/move helpers. |
| `src/commands.c`, `src/commands.h` | Interactive command dispatch and command behavior for blank-line edit, list/page, insert, delete, search/replace, copy/move, merge, help, quit/end handoff. |
| `src/fileio.c`, `src/fileio.h` | Startup load, append, write, end/save, quit cleanup, merge-file I/O. |
| `src/messages.c`, `src/messages.h` | User-visible messages, prompts, line display, help text. |
| `tests/test_parser.c` | Small C parser regression test. |
| `tests/test_edlin_commands.py` | Integration tests that drive `./edlin` through subprocesses and `pexpect`. |

## Behavior to Preserve

- EDLIN is line-oriented: one current line, a `*` prompt, numbered listings, and single-letter commands.
- Invocation requires a filename; `/B` or `-B` enables binary mode so Ctrl-Z bytes are kept as data during load/append.
- Valid commands in the C port are `A C D E H I L M P Q R S T W`, plus blank-line edit and `;` no-op/separator.
- Multiple commands on one physical input line are separated by `;` or Ctrl-Z (`0x1A`).
- Line references follow classic `GETNUM` ideas: decimal line numbers, `.`, `#` as last line plus one, `+n`, `-n`, comma-separated parameters, and special fourth-parameter restrictions.
- `?` after numeric parameters enables query mode for search/replace.
- Saved text-mode files end with Ctrl-Z; `/B` changes Ctrl-Z treatment on input.
- Scratch save behavior follows DOS style: write to `filename.$$$`, rename original to `.bak`, then rename scratch to the original path.

## Historical Assembly Reference

The assembly modules document the original MS-DOS EDLIN behavior:

| Module | Role |
|--------|------|
| `edlin.asm` | Entry point, main command loop, `GETNUM`, `COMTAB`, move/copy, video save/restore hooks. |
| `edlcmd1.asm`, `edlcmd2.asm` | Original command implementations and display/pagination helpers. |
| `edlmes.asm` | Message retriever / `SYSLOADMSG` / `SYSDISPMSG` style integration. |
| `edlparse.asm` | DOS command-line parser: required filespec and optional `/B`. |
| `edlequ.asm`, `edlstdsw.inc` | Shared equates and build/personality switches. |
| `edlin.skl` | Message skeleton with prompts and error strings. |
| `edlin.lnk` | Historical link order: `EDLIN+EDLCMD1+EDLCMD2+EDLMES+EDLPARSE`. |

Use these files to resolve compatibility questions, but implement changes in the C port unless the user specifically asks about the historical sources.

## Guidance for Coding Agents

1. Prefer existing C port patterns and keep behavior aligned with `Manual.md` and the integration tests.
2. When changing command parsing or command behavior, add or update focused tests in `tests/test_parser.c` and/or `tests/test_edlin_commands.py`.
3. Preserve command letters, parameter rules, `/B`, Ctrl-Z, scratch-file, `.bak`, and current-line semantics unless the user explicitly asks for a compatibility change.
4. Keep project headers in `src/`; do not reintroduce an `include/` directory without a clear project-wide reason.
5. For historical accuracy questions, compare against the assembly sources before changing the C port.
