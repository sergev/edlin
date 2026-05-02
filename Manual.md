# EDLIN User Manual

This manual describes the **C11 EDLIN** implementation in this repository ([`src/`](src/), [`include/`](include/)). Behavior matches classic MS-DOS EDLIN where the port intends compatibility; differences are called out in **Portability and differences from DOS EDLIN** at the end.

---

## What EDLIN is

EDLIN is a **line-oriented** text editor. You edit a file held in memory at the **`*`** prompt. Each logical line has a **line number** (starting at 1). One line is the **current line** (shown with `*` in listings).

This repo also preserves the original **8086 assembly** sources (`*.asm`) for reference; the runnable editor is built with `make` and installed as `./edlin` (see [README.md](README.md)).

---

## Starting the editor

**Syntax:**

```text
edlin filename
edlin /B filename
edlin -B filename
```

- **filename** (required): Path to the file to create or edit.
- **`/B`** or **`-B`** (optional): **Binary mode**. When omitted (text mode), a Ctrl-Z (`0x1A`) byte in the input file marks end-of-file on load and during append. In binary mode, Ctrl-Z is treated as ordinary data.

Implementation: [`parse_invocation`](src/parser.c), [`fileio_startup`](src/fileio.c).

**Restrictions:**

- Paths whose final extension is **`.bak`** (case-insensitive) are rejected with **Cannot edit .BAK file--rename file**.

**Environment variables:**

- **`EDLIN_LINES`**: Optional positive integer; overrides the logical screen height used by **`L`** and **`P`**. If unset, the height is read from **`ioctl(TIOCGWINSZ)`** on standard output, or **25** if that fails or reports no rows. See [`main.c`](src/main.c).

If the file does not exist, the editor prints **New file** and starts an empty buffer.

---

## Session loop

After startup, the editor prints **`*`** and reads a **command line** from standard input ([`main.c`](src/main.c)). Each physical line may contain **several commands** separated by **`;`** or **`Ctrl+Z`** (`0x1A`). After one command runs, the editor skips whitespace and an optional delimiter, then parses the next command on the same line.

---

## Command line syntax (before the command letter)

The parser ([`parse_command`](src/parser.c)) builds a **`Cmd`**: up to **four numeric parameters**, an optional **`?`**, then a **command letter** ([`include/parser.h`](include/parser.h)).

### Numeric parameters

Parameters are **comma-separated**. Each parameter is parsed like historic **`GETNUM`**:

- **Decimal number** `1`–`65535`: digits only; **`0` alone is invalid** as a line number.
- **`.`** — the **current line** number.
- **`#`** — **last line plus one**: in this port, **`#` = (number of lines in buffer) + 1**. For an empty buffer this is **1** (the position after the nonexistent “last line”).
- **`+n`** — **current + n** (`n` is a decimal number immediately after `+`).
- **`-n`** — **current − n**, but not below line **1**.

The **fourth** parameter may not use **`.`**, **`#`**, **`+`**, or **`-`** (classic rule).

If **parameter 2** is not zero, it must be **greater than or equal to parameter 1** (range validity).

### Query flag **`?`**

If **`?`** appears **after** the numeric parameters and **before** the command letter, **`query`** is set. In this implementation it affects **`S`** (search) and **`R`** (replace): after a match, the editor asks **O.K.?** and accepts **`y`** / **`n`** (see [`cmd_search`](src/commands.c), [`cmd_replace`](src/commands.c)). A bare **Enter** on the yes/no prompt is treated as **yes**.

Example: **`1?Sfoo`**. Do **not** use **`?Sfoo`** at the start of the line — with no parameter list before **`?`**, it is not parsed as the query flag and you usually get **Entry error** (see [Parsing pitfalls (comma-separated forms)](#parsing-pitfalls-comma-separated-forms)).

### Command letter

Letters are **case-insensitive**. Valid commands are the letters in **`COMTAB`** / [`dispatch_index`](src/parser.c): **`A C D E H I L M P Q R S T W`**, plus two special forms:

- **End of line or nothing left** before a letter → **blank-line edit** (same slot as carriage return in the original **`COMTAB`**).
- **`;` alone** as the command → **no-op** (remark).

Invalid or unknown letters produce **Entry error**.

### Multiple commands on one input line

After a command runs, the remainder of the line is parsed again. The outer loop stops when the next non-whitespace character is neither **`;`** nor **`Ctrl+Z`** ([`run_input_line`](src/main.c)); otherwise that delimiter is consumed and the next command is parsed.

---

## Parsing pitfalls (comma-separated forms)

These behaviors come straight from [`parse_command`](src/parser.c); they show up often in scripts and in the Python integration tests ([`tests/test_edlin_commands.py`](tests/test_edlin_commands.py)).

### **`C` / `M` and commas**

The parser collects **comma-separated** numeric tokens, then the **command letter**. A digit run **after** the letter is **not** a fourth line number; it is **rest-of-line** for that command (e.g. path for **`T`**, or invalid noise for **`C`**).

- **Wrong:** **`1,2C4`** — only two line numbers are read (**`1`**, **`2`**), then command **`C`**. The **`4`** is not **`param3`**. This does **not** mean “copy lines 1–2 before line 4” and usually yields **Entry error** (wrong parameter count for **`C`**).
- **Right:** **`1,2,4C`** — three line numbers, then **`C`** (copy that block before line **4**).

Similarly, **`1,2C0`** is **not** “destination 0”. To have a **third numeric slot** that reads as zero with **`C`** immediately after the third comma, use a form like **`1,2,C`**, which can yield **Must specify destination line number** when **`param3`** is zero ([`editor_blk_move`](src/editor.c)).

Use the same **three commas before the letter** idea for move: **`1,2,4M`**, not **`1,2M4`**.

### **Query flag `?`**

Put **`?` after the parameters and before the command letter**. Example: **`1?Sfoo`**. Do **not** start the line with **`?Sfoo`** — with no leading parameter list, **`?`** is not the query flag and you will usually get **Entry error**.

### **`S` / `R` default range on a short buffer**

With **no range given**, defaults are **start = current + 1**, **end = last line**. If the buffer has **one line** and **current** is **1**, that implies **start 2**, **end 1** → **invalid range** → **Entry error** (not **Not found**). Use an explicit range such as **`1,1Sfoo`** or **`1,1Rold;new`** to search or replace on line 1.

---

## Text after the command letter (**rest**)

Some commands read **additional text** from the same line **after** the letter:

- **`gettext_field`** copies characters into a buffer until **`;`**, end of line, or end of string. A **`Ctrl+V`** (**`^V`**, byte **`0x16`**) quotes the **next** byte literally ([`gettext_field`](src/commands.c)).

**Replace (**`R`**)**: two fields in order: **`old_text`**, **`;`**, **`new_text`**. Optional whitespace around the semicolon. Example: **`Rfoo;bar`** replaces the first substring **`foo`** with **`bar`** on the first matching line in range.

**Search (**`S`**)**: one field only — the substring to find.

**Transfer / merge (**`T`**)**: after **`T`** and its numeric parameter, the **first token** is the path to merge: characters until **`;`**, space, or tab ([`cmd_dispatch`](src/commands.c)).

---

## Display format

[`msg_line_out`](src/messages.c) prints:

- A **six-digit** line number (no leading-zero suppression beyond width),
- a **colon** delimiter,
- a **space** or **`*`** — **`*`** marks the **current line**,
- the line **content**.

So **`2L`** can list line **2** with a space in the marker column (not **`*`**) when the **current line** is still line **1** — only the current line gets **`*`**.

ASCII control characters (except tab, CR, LF) are shown as **`^`** plus a letter (e.g. **`^C`** for byte `0x03`), similar to classic **DISPLAY**.

---

## Commands (reference)

Each subsection lists **purpose**, **parameters**, **defaults**, **behavior**, and **errors** for this C port.

---

### Blank-line edit (**no letter**, code carriage return)

**Purpose:** Edit a **single** line by typing optional line references, then **Enter** **without** a command letter — same idea as classic “no command letter” input ([`COMTAB`](edlin.asm) leading carriage return).

**Important:** If the command line is **empty** (for example you pressed Enter before any other character on that line), [`parse_command`](src/parser.c) returns **line done** and **nothing runs**. To use blank-line edit, type at least one **line-reference token** on that line (for example **`.`** for the current line, a **decimal line number**, or **`#`**) and then press Enter **without** typing a command letter.

**Parameters:** At most **one** comma-separated numeric slot before end-of-line. The parser always reads **one** `GETNUM`-style value first; if the first character is not part of **`.`**, **`#`**, **`+`**, **`-`**, or digits, that value is **`0`** ([`get_num`](src/parser.c)).

**Defaults ([`cmd_nocom`](src/commands.c)):**

- If **`param[0]`** is **`0`**: target line = **current + 1** (when that state is reachable from the parser as above).

**Behavior:**

- If the target line **exists**, its text is shown and you enter a replacement line from stdin (**`^V`** quoting supported). Length is capped ([`EDLIN_MAX_LINE`](include/edlin.h)).
- If the target is **line count + 1** (the **`#`** / “last line plus one” position — EOF pseudo-line), **nothing is read from stdin**; [`cmd_nocom`](src/commands.c) only sets **current line** to that position (matches classic **`NOCOM`** when the pointer is already at end-of-text). To append real lines after the last line, use **`#I`** (insert before line last+1) or another command that accepts that line reference.

**Errors:** **Entry error** if more than one numeric parameter is supplied, or the target line is invalid.

Implementation: [`cmd_nocom`](src/commands.c).

---

### **`;`** (semicolon as command)

**Purpose:** No operation (separator only).

**Parameters:** None required.

Implementation: [`cmd_dispatch`](src/commands.c) — empty case.

---

### **`H`** — Help

**Purpose:** Print a short summary of available commands (similar to the cheat sheet in [Tutorial.md](Tutorial.md)).

**Parameters:** **Exactly one**, value **`0`** (use **`H`** alone — same parsed form as **`E`** / **`Q`** before the letter).

**Behavior:** Prints fixed help text to standard output ([`msg_help`](src/messages.c)).

**Errors:** **Entry error** if a line number or other prefix appears before **`H`** (e.g. **`1H`**).

Implementation: [`cmd_dispatch`](src/commands.c).

---

### **`L`** — List

**Purpose:** Print a range of lines with headers.

**Parameters:** At most **two** (`param1` = start line, `param2` = end line).

**Defaults:**

- **`param1`**: If **0**, start = **`max(1, current − 11)`**.
- **`param2`**: If **0**, end = **`start + (disp_rows − 2)`**, capped at the last line.

**Behavior:** If the buffer has **no lines**, **`L`** does nothing (no message).

**Errors:** **Entry error** if **`param2` < param1**, if **start** is past the last line, or if more than two parameters are given.

Implementation: [`cmd_list`](src/commands.c).

---

### **`P`** — Page

**Purpose:** Like list, but advances **current line** as it prints and pauses every **`disp_rows − 1`** lines with **Continue (Y/N)?**.

**Parameters:** At most **two**.

**Defaults:**

- **`param1`**: If **0**, start at **`current + 1`**, unless **`current`** is **1**, then start at **1**.
- **`param2`**: If **0**, end window uses **`start + (disp_rows − 2)`**, then internal **`endp`** is adjusted (see code — effectively a window through the file).

**Behavior:** Answering **no** (or **`n`**) stops paging early; **yes** continues.

**Errors:** **Entry error** if the implied range is backward or invalid.

Implementation: [`cmd_pager`](src/commands.c).

---

### **`D`** — Delete

**Purpose:** Delete a range of lines.

**Parameters:** Zero to **two**.

**Defaults:**

- **`param1`**: If **0**, delete **current** line only as start.
- **`param2`**: If **0**, delete **only** line **`param1`** (same as start).

**Behavior:** Removes lines **`param1`** through **`param2`** inclusive. Adjusts **current line** to the first line still present after the gap (see [`cmd_delete`](src/commands.c)).

**Errors:** **Entry error** if more than two parameters, invalid range, or lines out of range.

Implementation: [`cmd_delete`](src/commands.c).

---

### **`I`** — Insert

**Purpose:** Insert new lines **before** the given line.

**Parameters:** At most **one**.

**Defaults:**

- **`param1`**: If **0**, insert **before** the **current** line.

**Behavior:** Repeatedly prints an insert prompt (same **`number`:`marker`** prefix as [`msg_line_out`](src/messages.c), **without** a newline after the prompt), reads stdin until end of insert: a line containing only **`.`**, or **Ctrl-Z** as the first byte of a line (classic MS-DOS), or EOF. **`^V`** quoting applies; long lines yield **Line too long**. To insert a line that is exactly **`.`**, use **`^V.`** so the terminator check does not apply.

**Errors:** **Entry error** if more than one numeric parameter.

Implementation: [`cmd_insert`](src/commands.c).

---

### **`A`** — Append

**Purpose:** Read **the rest** of the original input file from disk and append it to the buffer.

**Parameters:** **Exactly one** numeric slot must be parsed (**`nparam == 1`**). The value is reserved for line-count semantics in classic EDLIN; this port reads **all remaining bytes** in one step regardless.

**Behavior:** Opens from the prior read position, reads through EOF (honoring Ctrl-Z truncation in text mode), splits into lines, appends. Prints **End of input file** when done or when there was nothing left.

**Errors:** **Entry error** if **`nparam != 1`**. Read failures print **Read error in:** and the path.

Implementation: [`fileio_append`](src/fileio.c).

---

### **`W`** — Write

**Purpose:** Write the **first** part of the buffer to the **scratch output file** and remove those lines from memory.

**Parameters:** At most **one** (`param1`).

**Semantics:**

- **`param1 > 0`**: Write lines **`1`** through **`param1 − 1`** **if** **`param1 ≤ line count`**; if **`param1` > line count**, the implementation writes **all** lines (see [`fileio_write`](src/fileio.c)).
- **`param1 == 0`**: Write the first **quarter** of lines (by count): **`ceil(count / 4)`** lines, implemented as **`(count + 3) / 4`**.

**Errors:** **Entry error** if more than one parameter. Failures opening/appending the scratch file surface as **Disk full. Edits lost.**

Implementation: [`fileio_write`](src/fileio.c).

---

### **`E`** — End (save and exit)

**Purpose:** Save all remaining lines and exit the process successfully.

**Parameters:** **Exactly one**, and it must be **`0`** (forms like **`E`** or **`0`** before **`E`** — classic **`GETNUM`** yields one zero).

**Behavior:**

1. Rewrites the scratch file from scratch with every remaining line, each ending with newline.
2. In **text** mode, appends a **Ctrl-Z** end-of-file marker after the last newline.
3. Renames the original file to **`basename.bak`** (replacing extension if present, else appending **`.bak`**), then renames the scratch file (**`.$$$`**) to the original filename.
4. **`exit(0)`**.

**Errors:** **Entry error** if parameters do not match **`nparam == 1`** and **`param1 == 0`**. Rename/write failures print **Disk full. Edits lost.**

Implementation: [`fileio_end`](src/fileio.c).

---

### **`Q`** — Quit

**Purpose:** Abandon the edit **without** saving the scratch file as the real file.

**Parameters:** **Exactly one**, value **`0`**.

**Behavior:** Prints **Abort edit (Y/N)?**. If you answer **yes** (`y` or bare Enter per [`prompt_yn`](src/commands.c)), closes handles, **deletes** the scratch path, and **`exit(0)`**.

**Errors:** **Entry error** if parameters are wrong.

Implementation: [`cmd_dispatch`](src/commands.c), [`fileio_quit_abort`](src/fileio.c).

---

### **`S`** — Search

**Purpose:** Find the next line containing a **literal substring**.

**Parameters:** Zero to **two** line-range arguments (**start**, **end**).

**Defaults:**

- **`start`**: If **0**, search starts at **`current + 1`** (search-forward-from-current behavior in this port).
- **`end`**: If **0**, **`end`** = **last line**.

**Rest:** One **`gettext`** field — the substring (**case-sensitive**, uses C **`strstr`**).

**Query:** With **`?`**, prints **O.K.?** before stopping at a match.

**Errors:** **Entry error** for bad range or too many parameters. No match prints **Not found**.

**Default-range pitfall:** With **start** defaulted from **current + 1**, a **one-line** file and **current = 1** yields **start 2**, **end 1** — **invalid range** and **Entry error**, not **Not found**. To search line **1**, use an explicit range such as **`1,1Sfoo`** ([Parsing pitfalls](#parsing-pitfalls-comma-separated-forms)).

Implementation: [`cmd_search`](src/commands.c).

---

### **`R`** — Replace

**Purpose:** Replace the **first** occurrence of **`old`** with **`new`** on the **first** line (in range) that contains **`old`**.

**Parameters:** Same range rules as **`S`**.

**Rest:** **`old;new`** using **`gettext`** twice.

**Query:** With **`?`**, shows the proposed line and **O.K.?** before applying.

**Limits:** Rebuilt line length must not exceed **`EDLIN_MAX_LINE`** ([`EDLIN_MAX_LINE`](include/edlin.h)); otherwise **Line too long**.

**Errors:** **Entry error**, **Not found**, **Line too long**.

**Default-range pitfall:** Same as **`S`** — on a **one-line** buffer with **current = 1**, omitting the range can yield **Entry error** instead of **Not found**. Use e.g. **`1,1Rold;new`** to replace on line **1** ([Parsing pitfalls](#parsing-pitfalls-comma-separated-forms)).

Implementation: [`cmd_replace`](src/commands.c).

---

### **`C`** — Copy

**Purpose:** Copy lines **`param1`**–**`param2`** so they appear **before** line **`param3`**.

**Parameters:** **Three or four**:

- **`param1`, `param2`**, **`param3`** (destination line, required non-zero in the engine).
- **`param4`** (optional): repeat count for copying the block **as a whole**; **`0`** and **`1`** mean **once** ([`editor_blk_move`](src/editor.c)).

**Errors:** **Entry error** if **`nparam`** is not 3 or 4. If **`param3 == 0`**, **Must specify destination line number**. Other failures print **Entry error**.

**Note:** All source line numbers must be **non-zero** and in range; the destination must not lie **strictly inside** the source range ([`editor_blk_move`](src/editor.c)).

Implementation: [`cmd_dispatch`](src/commands.c), [`editor_blk_move`](src/editor.c).

---

### **`M`** — Move

**Purpose:** Same as copy, but **deletes** the source range after copying.

**Parameters:** **Exactly three** (`param1`, `param2`, `param3`). No repeat parameter in this port.

**Errors:** Same style as **`C`** (without **Must specify destination** split — failures aggregate as **Entry error**).

Implementation: [`cmd_dispatch`](src/commands.c), [`editor_blk_move`](src/editor.c) with **`is_move == 1`**.

---

### **`T`** — Transfer (merge file)

**Purpose:** Insert lines from another **file** before a given line.

**Parameters:** **Exactly one** — **`param1`** = line **before which** to insert. If **`param1`** is **0**, the implementation uses the **current line** ([`fileio_merge`](src/fileio.c)).

**Rest:** First path token after **`T`** (see above).

**Errors:** **Entry error** if **`nparam != 1`** or path empty. Open/read/merge failures print **Invalid drive or file name** or **Not enough room to merge the entire file**.

Implementation: [`cmd_dispatch`](src/commands.c), [`fileio_merge`](src/fileio.c).

---

## Files on disk

- **Scratch file:** **`basename.$$$`** — built by stripping the last extension before **`.`** (if any), then appending **`.$$$`** ([`build_temp_path`](src/fileio.c)).
- **`W`** appends written lines to that scratch file (opened at startup when possible).
- **`E`** replaces the user file with the scratch content via rename (original → **`.bak`**, scratch → original path).

---

## Messages (quick reference)

Strings come from [`messages.c`](src/messages.c):

| Message | Typical cause |
|--------|----------------|
| **Entry error** | Invalid command, parameters, or parse failure |
| **Not found** | **`S`** / **`R`** found no match |
| **New file** | Created session on missing file |
| **End of input file** | **`A`** finished reading |
| **Read error in:** | **`A`** / startup read failure |
| **Disk full. Edits lost.** | **`W`** / **`E`** I/O failure |
| **Insufficient memory** | Allocation failure |
| **Line too long** | **`I`** / **`R`** / blank-line edit exceeds limit |
| **Must specify destination line number** | **`C`** with **`param3 == 0`** |
| **Not enough room to merge the entire file** | **`T`** merge allocation / insert failure |
| **Invalid drive or file name** | Bad path (`**T**`, startup) |
| **Cannot edit .BAK file--rename file** | Invoked on **`.bak`** |
| **File name must be specified** | Started **`edlin`** with no file |
| **Abort edit (Y/N)?** | **`Q`** confirmation |
| **O.K.?** | **`?`** on **`S`** / **`R`** |
| **Continue (Y/N)?** | **`P`** paging |

Unused in code paths today but defined: **Cannot merge - Code page mismatch**, **File is READ-ONLY**, etc.

---

## Portability and differences from MS-DOS EDLIN

- **No DOS interrupts**: Screen height defaults from **`TIOCGWINSZ`** on stdout; optional override **`EDLIN_LINES`** (see [`main.c`](src/main.c)).
- **Line storage**: Internal **array of lines**, not a single segment buffer with **`^Z`** between lines (behavior is aligned logically).
- **`W`** without an argument: classic code used a **byte-quarters** heuristic; this port uses **line-count quarters**.
- **`A`**: Classic append could stop after **N** lines and seek the input file; this port reads **all** remaining bytes once.
- **`R` / `S` rest**: Classic **`GETTEXT`** used CR-terminated fields on one logical command line; this port documents **`;`** between **`old`** and **`new`** for **`R`**, and substring search via **`strstr`** (not DOS/Japanese-specific boundary rules).
- **Move/Copy defaults**: MS-DOS EDLIN defaults missing line numbers to **current** in **`BLKMOVE`**; this implementation **requires explicit numeric parameters** matching the **`nparam`** gates (zeros are not rewritten to **current** before [`editor_blk_move`](src/editor.c)).
- **Binary mode**: Matches the intent of **`/B`** (Ctrl-Z not EOF); exact DOS binary semantics may still differ on exotic encodings.

---

## Automation / scripting

With **piped stdin** (non-TTY), a line containing only **`.`** ends insert mode portably. **`Ctrl-Z`** (**`^Z`**) as the **first byte of a line** still works for compatibility and matches **`subprocess`**-style automation. **Interactive PTY** stacks can differ in how **EOF** and **line discipline** interact with **`^Z`**; prefer **`.`** in scripts when **`^Z`** is awkward ([`tests/test_edlin_commands.py`](tests/test_edlin_commands.py)).

---

## Source index

| Topic | Primary files |
|-------|----------------|
| Invocation | [`src/main.c`](src/main.c), [`src/parser.c`](src/parser.c) |
| Command parsing | [`src/parser.c`](src/parser.c), [`include/parser.h`](include/parser.h) |
| Commands | [`src/commands.c`](src/commands.c) |
| Buffer / move-copy | [`src/editor.c`](src/editor.c), [`include/edlin.h`](include/edlin.h) |
| File I/O | [`src/fileio.c`](src/fileio.c) |
| Messages | [`src/messages.c`](src/messages.c) |

For historic **`COMTAB`**, **`GETNUM`**, and module layout, see [AGENT.md](AGENT.md) and [edlin.asm](edlin.asm).
