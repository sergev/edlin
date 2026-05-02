# EDLIN — MS-DOS line editor source

**EDLIN** is the classic line-oriented text editor shipped with MS-DOS: a prompt (`*`), line numbers, and single-letter commands (**L**ist, **I**nsert, **D**elete, **W**rite, etc.). This repository holds the historical **8086 assembly** EDLIN sources plus a portable **C11** reimplementation.

## Portable C11 editor (`./edlin`)

Build and run on macOS/Linux (needs a C compiler):

```bash
make
./edlin myfile.txt
./edlin /B binary.bin    # optional: binary mode (no Ctrl-Z cut on load)
make test                  # parser unit test + Python integration tests (`pexpect`)
pip install -r requirements-dev.txt   # once: integration suite needs `pexpect`
./tests/smoke.sh           # minimal stdin script (build `edlin` first)

Optional merge integration (`tests/test_edlin_commands.py`): set `EDLIN_TEST_MERGE_IO=1` if `fopen` on merge paths should be exercised (some sandboxed environments block this).
```

Environment:

- `EDLIN_LINES` — optional override for logical screen height used by **L** / **P**. If unset, the height comes from **`ioctl(TIOCGWINSZ)`** on standard output, or **25** if that is unavailable or returns no rows.

### C port notes

- Uses the C standard library plus **`ioctl(TIOCGWINSZ)`** (`sys/ioctl`, `unistd`) for terminal height when **`EDLIN_LINES`** is not set.
- Command letters and numeric arguments follow the original **`COMTAB`** / **`GETNUM`** behavior (see [`AGENT.md`](AGENT.md)); details differ where DOS calls cannot be reproduced (PSP, IOCTL screen probe, SYSMSG, INT 23h, extended attributes).
- **Search / Replace**: patterns use `old;text` after **`R`** / **`S`** (semicolon separator); `^V` quoting is accepted as `0x16` in input lines.
- Comma-separated command forms, **`?`** placement, and default-range edge cases for **`S`** / **`R`** (and related parse traps) are documented in [`Manual.md`](Manual.md) § **Parsing pitfalls (comma-separated forms)**.
- **Save**: writes a scratch file (`filename.$$$`), then renames like the DOS utility (original → `.bak`, scratch → original).
- Historical **`makefile.dos`** builds the original `.com`; the root **`Makefile`** builds the C binary only.

## Historical assembly layout

| Path | Purpose |
|------|---------|
| `edlin.asm` | Entry point, command loop, line-argument parsing (`GETNUM`), dispatch table, move/copy |
| `edlcmd1.asm`, `edlcmd2.asm` | Command implementation and helpers (append, delete, list, page, write, …) |
| `edlparse.asm` | External command line: required filespec, optional `/B` |
| `edlmes.asm` | Message retriever / printf bridge |
| `edlequ.asm`, `edlstdsw.inc` | Equates and build switches |
| `edlin.skl` | Message skeleton for building localized message tables |
| `makefile.dos`, `edlin.lnk` | Original DOS link order: `EDLIN+EDLCMD1+EDLCMD2+EDLMES+EDLPARSE` → `edlin.com` |
| `AGENT.md` | Structured summary for tools and contributors (commands, buffers, build caveats) |

### Building the original DOS binary

The **`makefile.dos`** recipe expects a **full MS-DOS build tree** (parent `inc`, `messages`, assembler, linker, `convert` to `.COM`, etc.). **This checkout does not include that tree**; treat the `.asm` files as the behavioral reference.

## License

See [`LICENSE`](LICENSE) — MIT License, copyright IBM and Microsoft Corporation.

## History

These files come from the same MS-DOS source releases [published by Microsoft](https://github.com/microsoft/MS-DOS) (early DOS versions were also [archived at the Computer History Museum](http://www.computerhistory.org/atchm/microsoft-ms-dos-early-source-code/)). They are preserved here for study, porting, and accurate documentation of **EDLIN** behavior—not as a drop-in build of the entire operating system.
