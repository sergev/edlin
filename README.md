# EDLIN — MS-DOS line editor source

**EDLIN** is the classic line-oriented text editor shipped with MS-DOS: a prompt (`*`), line numbers, and single-letter commands (**L**ist, **I**nsert, **D**elete, **W**rite, etc.). This repository holds **only** the EDLIN utility sources extracted from the published MS-DOS corpus—8086 assembly for **DOS 4.00**–era behavior (system parser, message retriever, optional **`/B`** binary load, DBCS-related hooks in the headers).

## Contents

| Path | Purpose |
|------|---------|
| `edlin.asm` | Entry point, command loop, line-argument parsing (`GETNUM`), dispatch table, move/copy |
| `edlcmd1.asm`, `edlcmd2.asm` | Command implementation and helpers (append, delete, list, page, write, …) |
| `edlparse.asm` | External command line: required filespec, optional `/B` |
| `edlmes.asm` | Message retriever / printf bridge |
| `edlequ.asm`, `edlstdsw.inc` | Equates and build switches |
| `edlin.skl` | Message skeleton for building localized message tables |
| `makefile`, `edlin.lnk` | Link order: `EDLIN+EDLCMD1+EDLCMD2+EDLMES+EDLPARSE` → `edlin.com` |
| `AGENT.md` | Structured summary for tools and contributors (commands, buffers, build caveats) |

## Building

The `makefile` expects a **full DOS build tree**: parent directories for `inc` (e.g. `dossym.inc`, `parse.asm`), `messages`, and tools (assembler, linker, `convert` to produce `.COM`, message build for `edlin.ctl`). **This checkout is not self-contained**; treat the sources as the authoritative reference until those dependencies are wired up.

## License

See [`LICENSE`](LICENSE) — MIT License, copyright IBM and Microsoft Corporation.

## History

These files come from the same MS-DOS source releases [published by Microsoft](https://github.com/microsoft/MS-DOS) (early DOS versions were also [archived at the Computer History Museum](http://www.computerhistory.org/atchm/microsoft-ms-dos-early-source-code/)). They are preserved here for study, porting, and accurate documentation of **EDLIN** behavior—not as a drop-in build of the entire operating system.
