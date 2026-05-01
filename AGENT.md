# AGENT.md — EDLIN (MS-DOS line editor) — agent context

This file summarizes the historic **EDLIN** sources in this workspace so assistants can navigate, explain, or extend work related to this codebase without re-reading all assembly.

## What this tree is

- **EDLIN** is a **line-oriented** text editor for DOS: one **current line**, commands typed on a **prompt**, text held in a memory buffer ending with **Ctrl+Z (0x1Ah)** as EOF marker.
- These files match **MS-DOS EDLIN utility version 4.00** (1988) sources: module banners reference **SYSPARSE**, **message retriever**, **DBCS**, enhanced video, extended opens, etc. (`edlin.asm`, `edlequ.asm` headers).
- The repo **`README.md`** describes the broader **Microsoft MS-DOS** source publication (MIT license). **`LICENSE`** is MIT, Copyright IBM and Microsoft Corporation.
- **`makefile`** assumes a **parent DOS build tree** (`..\..\inc`, `..\..\messages`, `..\..\dos`, etc.). This checkout contains only the **EDLIN subset**; a full build requires those includes and tools (MASM/SALUT, LINK, `convert` to `.COM`, message build for `edlin.ctl`).

## Module map and link order

Link command (from headers): **`EDLIN+EDLCMD1+EDLCMD2+EDLMES+EDLPARSE`** → `EDLIN.EXE` (see `edlin.lnk`).

| Module | Role |
|--------|------|
| **`edlin.asm`** | Entry **`EDLIN`**, main **command loop** (`COMMAND`), **`GETNUM`** / parameter parsing, **`COMTAB`** dispatch table, **MOVE/COPY** (`BLKMOVE`), video save/restore (**`EDLIN_DISP_GET`**), bridge **`EDLIN_COMMAND`** from external parser. |
| **`edlcmd1.asm`** | Core editing helpers: **`APPEND`**, **`DELETE`**, listing/paging hooks, errors, etc. (exports include **`append`**, **`delete`**, **`pager`**, **`list`**, **`ewrite`**, **`wrt`**, …). |
| **`edlcmd2.asm`** | Additional command implementation and **display/pagination** helpers (**`EDLIN_DISP_COUNT`**, **`EDLIN_PG_COUNT`**, **`EDLIN_PG_PROMPT`** — “Continue (Y/N)?”). |
| **`edlmes.asm`** | **Message retriever** integration: **`PRE_LOAD_MESSAGE`**, **`printf`**, **`disp_fatal`**, `SYSLOADMSG` / `SYSDISPMSG` style display. |
| **`edlparse.asm`** | External **`PARSER_COMMAND`**: DOS command line → **required filespec**, optional **`/B`** switch (see below). |
| **`edlequ.asm`** | Shared **equates** (stack size, video IOCTL, extended open flags, parse exit codes, **`Display_Buffer_Struc`**, Y/N validation). |
| **`edlstdsw.inc`** | Build/personality switches (e.g. **IBM**, **WANG**, **Rainbow**, escape/cancel keys). **`DOSSYM.INC`** / **`EDLSTDSW.INC`** are pulled via `edlequ.asm` in a full tree. |
| **`edlin.skl`** | **Message skeleton** for building localized/packed messages (`FASTBLD` / country `.msg` in full build). Defines prompt **`*`**, fatals, “Invalid drive…”, “New file”, merge/codepage strings, etc. |

## External invocation (`EDLPARSE`)

- **Required:** a **filespec** (path to file to create or edit).
- **Optional:** **`/B`** — binary mode: documented in parser as switching whether **Ctrl+Z is treated as end-of-file** vs literal (`loadmod` / “viceversa” in `edlin.asm` data).
- Parser uses **system parser (SYSPARSE)**; control blocks in `edlparse.asm` describe **one filespec** and **one switch**.

## Interactive commands (from `COMTAB` / `TABLE` in `edlin.asm`)

The command letter is looked up in **`COMTAB`** after optional **line arguments** and optional **`?`** (sets **`QFLG`** for **query** on applicable commands).

**`COMTAB`** bytes (order matters for dispatch): **CR (13)**, **`;`**, then **`A C D E I L M P Q R S T W`**.

| Letter | Label / behavior (from `TABLE`) |
|--------|-----------------------------------|
| *(CR)* | **`NOCOM`** — blank line |
| **`;`** | **`NOCOM`** — no-op / remark-style line |
| **`A`** | **`APPEND`** — read more lines from input file into buffer |
| **`C`** | **`COPY`** — block copy (needs **3** parameters; see `COPY`/`MOVE`) |
| **`D`** | **`DELETE`** — delete line range |
| **`E`** | **`ENDED`** — end edit (save & exit in classic EDLIN semantics) |
| **`I`** | **`INSERT`** — insert lines before current/specified line |
| **`L`** | **`LIST`** — list lines |
| **`M`** | **`MOVE`** — block move (**`MOVFLG`** vs copy) |
| **`P`** | **`PAGER`** — page through text |
| **`Q`** | **`QUIT`** — quit (typically abandons; prompts in messages) |
| **`R`** | **`replac_from_curr`** — replace text |
| **`S`** | **`search_from_curr`** — search (`srchmod` controls scope in data) |
| **`T`** | **`MERGE`** — merge (**Transfer**) from another file |
| **`W`** | **`EWRITE`** — write lines to disk |

**Multiple commands** on one input line are separated by **`;`** after the first command (parser resumes at `PARSE`).

## Line argument syntax (`GETNUM` in `edlin.asm`)

- **`nnn`** — absolute line number (non-zero; zero is invalid).
- **`.`** — **current line** (`CURRENT`).
- **`#`** — **number of lines** in buffer (counts LF in range `START`..`ENDTXT`).
- **`+n`** / **`-n`** — relative to **current line** (clamped to at least line 1 for `-`).
- Parameters can be **comma-separated**; up to **four** numeric parameters with special handling for the **fourth** (`.`, `#`, `+`, `-` disallowed there — errors go to **`COMERR`**).
- Optional **`?`** before the command letter enables **query** mode (**`QFLG`**).

## Important buffer / state (from `edlin.asm` data definitions)

- **`START`** — beginning of in-memory file; byte **before** `START` must be **< 40H** (constraint for DBCS back-scan).
- **`ENDTXT`** — pointer to **Ctrl+Z** EOF in buffer.
- **`CURRENT`** / **`POINTER`** — 1-based current line index and offset to current line.
- **`LAST`** — end of available memory for buffer.
- **`THREE4TH`** — threshold (~75% full) used with **`APPEND`** to avoid overfilling.
- **`COMBUF`** — DOS buffered input for command line (**80h** max buffer byte).
- **`EDITBUF`** — line edit buffer (**258** bytes).
- **`path_name`**, **`rd_handle`**, **`wrt_handle`** — active files.

## Messages (`edlin.skl`)

Skeleton defines utility classes: drive/name errors, read-only, disk full, **“Entry error”** (`BADCOM`), **“New file”**, **“O.K.? ”**, **“Abort edit (Y/N)? ”**, **“Continue (Y/N)? ”**, merge errors, **code page mismatch** on merge, etc. Message numbers tie into **`edlmes.asm`** tables.

## Guidance for coding agents

1. **Respect linkage and table order** — **`COMTAB`** and **`TABLE`** must stay in sync (`edlin.asm` warns: *“Careful changing the order…”*).
2. **Full builds** need the parent **`inc`**, **`parse.asm`**, **`sysmsg.inc`**, **`dossym.inc`**, and country **`.msg`** files referenced in **`makefile`**; do not assume **`make`** works from this folder alone.
3. **`include` paths** in `makefile` point **outside** this directory; grep or edits should account for **missing headers** in a sparse checkout.
4. **Historical accuracy:** README on upstream MS-DOS repo says sources are **reference** and PRs against original files are discouraged; follow repo policy if this tree inherits it.
5. **Reimplementation / ports:** preserve **command letters**, **parameter rules**, **`/B`**, and **EOF (^Z)** semantics if claiming EDLIN compatibility.

## Quick file index

- **Main logic & dispatch:** `edlin.asm`
- **Commands / I-O:** `edlcmd1.asm`, `edlcmd2.asm`
- **CLI parse:** `edlparse.asm`
- **Strings / SYSMSG:** `edlmes.asm`, `edlin.skl`
- **Shared constants:** `edlequ.asm`, `edlstdsw.inc`
- **Build:** `makefile`, `edlin.lnk`

---

*Generated from the sources in this repository (`edlin.asm` `COMTAB`/`GETNUM`, `edlparse.asm`, `edlequ.asm`, `edlin.skl`, `makefile`).*
