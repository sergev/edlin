# EDLIN Tutorial

This tutorial helps you use the **portable C11 EDLIN** in this repository.

It is written for people who are new to line editors. For every command, parameter, and error message in detail, see **[Manual.md](Manual.md)**.

---

## What you are looking at

EDLIN is a **line-oriented** editor:

- You work at a prompt that looks like **`*`**.
- The file lives in memory as **numbered lines** (1, 2, 3, …).
- One line is the **current line**. When you list the file, the current line is marked with **`*`** in the marker column; other lines show a space there.
- You type **short commands** (usually one letter) instead of clicking or using a full-screen cursor.

This feels old-fashioned compared to Notepad or VS Code, but it is predictable: **every edit is a command**.

---

## Build and run (first time)

You need a C compiler and `make`. From the repository root:

```bash
make
./edlin myfile.txt
```

- If **myfile.txt** does not exist, EDLIN prints **New file** and starts with an empty buffer.
- If it exists, EDLIN loads it.

### Text mode vs binary mode (`/B`)

In normal **text** mode, a Ctrl-Z (`^Z`, byte `0x1A`) in the file marks **end-of-file** when loading and when using **Append**. In **`/B`** **binary** mode, Ctrl-Z is kept as ordinary data.

```bash
./edlin /B data.bin
```

By default, **L** and **P** use your terminal height from **`ioctl(TIOCGWINSZ)`** (with a fallback of **25** rows). Set **`EDLIN_LINES`** to override that window size. Example for narrow paging:

```bash
EDLIN_LINES=6 ./edlin long.txt
```

---

## The session loop

After startup you see **`*`**. Type a **command line** and press **Enter**.

- Commands are **case-insensitive** (`L` and `l` are the same).
- You can put **several commands on one line**, separated by **`;`** or **Ctrl-Z** (`^Z`). Example: `L;L` lists twice.

---

## Tutorial 1 — Create a file and save it

Goal: create **notes.txt** with two lines and save.

**Step 1 — Start EDLIN**

```bash
./edlin notes.txt
```

You might see:

```text
New file
*
```

**Step 2 — Insert lines before line 1**

At **`*`**, type **`1I`** and press **Enter**. EDLIN enters **insert mode**: it shows numbered prompts and reads lines from you.

Type each line of text, then **Enter**. When you are done inserting, type **`.`** then **Enter** on a line by itself (that ends insert mode). **Ctrl-Z** on a line by itself still works for compatibility with classic EDLIN (`^Z` means hold **Ctrl** and press **Z**).

Example session (what you type is after **`*`** or after the insert prompt):

```text
*1I
     1:*First line of my notes
     2:*Second line
.
*
```

**Step 3 — List the buffer**

At **`*`**, type **`L`** and **Enter**. You should see your lines with six-digit line numbers.

**Step 4 — Save and exit**

Type **`E`** and **Enter**.

EDLIN writes the file (using a scratch file and rename, like classic DOS). In normal **text** mode the saved file ends with a **Ctrl-Z** end-of-file marker, like MS-DOS text files.

You are back at the shell.

---

## Reading the screen — line numbers and “current line”

Listings look roughly like:

```text
     1:*First line of my notes
     2: Second line
```

- The **first column** is a line number (padded to six digits).
- Then a **colon**, then **`*`** only for the **current line**, otherwise a space (same **`%1:%2`** header as MS-DOS **DISPLAY**; see [`asm/edlin.skl`](asm/edlin.skl) message 32).
- Then the text of the line.

To list only some lines, give a range before **`L`**:

```text
*1,2L
```

You will see lines 1 through 2.

---

## Tutorial 2 — Edit one line without a command letter (“blank-line edit”)

Goal: change line 1 by typing its number only.

1. Open the file: `./edlin notes.txt`
2. At **`*`**, type **`1`** and **Enter** (no **`L`**, no **`I`** — just the number).

EDLIN shows that line and lets you type a **replacement** line.

Example:

```text
*1
     1:*First line of my notes
completely new first line
*
```

Then **`L`** to verify.

**Important:** Pressing **Enter** immediately at **`*`** with an empty line does nothing. To use blank-line edit you must type at least a **line reference** (a number, **`.`**, or **`#`**) on that line first.

---

## Tutorial 3 — Insert in the middle; append at the end

**Insert before a specific line** — use **`I`** with a line number:

```text
*2I
     2:*(new text goes here)
.
*
```

That inserts **before** old line 2.

**Insert before the “current” line** — plain **`I`** (no number) inserts before whatever line is currently current.

**Append after the last line** — use **`#I`**. **`#`** alone means “last line plus one” (the EOF pseudo-line); classic EDLIN does **not** read a new line from you there. **`I`** inserts **before** a line number, so **`#I`** inserts before line last+1 — i.e. after the last existing line.

```text
*#I
     2:* 
another line at the end
.
*
```

You can end insert mode with **Ctrl+Z** on a line by itself instead of **`.`** (same as elsewhere).

(Exact line numbers depend on how many lines you already have.)

**Bare `#` then Enter** (blank-line edit with only **`#`**) moves the **current line** to that EOF position and returns to **`*`** — it does **not** append text. Use **`#I`** to type new lines at the end.

---

## Tutorial 4 — Delete lines

**Delete one line** — e.g. delete line 2:

```text
*2D
*
```

**Delete a range** — lines 2 through 4:

```text
*2,4D
*
```

With **no numbers**, **`D`** deletes the **current** line only (defaults described in the manual).

---

## Tutorial 5 — Search and replace

### Search (**`S`**)

**`S`** searches for a **literal substring** (case-sensitive). You type **`S`** followed immediately by the text to find (no space required).

Example file:

```text
alpha
beta
```

From **`*`** after loading:

```text
*Sbeta
```

EDLIN finds the line containing **`beta`** and shows it; that line becomes current.

### Replace (**`R`**)

**`R`** replaces the **first** occurrence of **old** with **new** on the **first line in range** that contains **old**. The syntax is:

```text
Rold;new
```

Optional spaces around **`;`** are fine. Example:

```text
*1,1Rfoo;baz
```

That searches only line **1** and turns **`foo`** into **`baz`** once on that line.

### A trap: one-line files

If the file has **only one line** and the current line is **1**, a plain **`S`** or **`R`** without a range uses defaults that start **after** line 1 — which can produce **Entry error** instead of **Not found**.

**Fix:** always give an explicit range for a single-line file, e.g. **`1,1Sfoo`** or **`1,1Rfoo;bar`**.

### Query mode (**`?`**)

For **`S`** and **`R`**, you can ask for confirmation. Put **`?`** **after** any line numbers and **before** the command letter:

```text
*1?Sfindme
```

When a match is found, EDLIN asks **O.K.?** — answer **`y`** or **`n`** (bare **Enter** counts as yes).

---

## Tutorial 6 — Copy and move blocks

### Copy (**`C`**)

Copy lines **start** through **end** so they appear **before** line **dest**:

```text
*1,2,4C
```

That copies lines 1–2 and inserts that block before line 4.

**Gotcha — commas matter.** Give **three** line numbers (start, end, destination), separated by commas, **before** **`C`**:

- **Wrong:** `1,2C4` — the **`4`** is not read as the destination.
- **Right:** `1,2,4C`

### Move (**`M`**)

Same idea, but the source lines are **removed** after copying:

```text
*1,1M3
```

Moves line 1 before line 3 (exact numbering after a move depends on your file).

---

## Tutorial 7 — Merge another file (**`T`**)

**`T`** inserts lines from **another file** **before** the line you specify. Put the **line number first**, then **`T`**, then the path (one token; stops at space, tab, or **`;`**).

Example (conceptual):

```text
*1Tother.txt
```

That merges **other.txt** before what was line **1** (line numbers shift after the insert).

If the path cannot be opened, you may see **Invalid drive or file name**. Merging needs real file access.

---

## Tutorial 8 — Page through a long file (**`P`**)

**`P`** lists like **`L`** but can pause with:

```text
Continue (Y/N)?
```

Answer **`y`** to continue or **`n`** to stop. Set **`EDLIN_LINES`** to a small number to see paging sooner on long files.

---

## Tutorial 9 — Write part to disk (**`W`**), append from original (**`A`**)

Large edits sometimes use **Write** to flush the **beginning** of the buffer to the scratch file and free memory.

- **`W`** with **no** number writes about the **first quarter** of lines (by count).
- **`W`** *n* writes lines **1** through **n − 1** in typical cases (see **Manual.md** for exact rules).

**`A`** (**Append**) reads **the rest** of the original input file from disk and appends to the buffer. You must supply **exactly one** line-number slot before **`A`** (classic EDLIN expects this parameter). Example:

```text
*1A
End of input file
*
```

When nothing is left to read from disk, you typically see **End of input file**. See **Manual.md** for details.

---

## Tutorial 10 — Quit without saving (**`Q`**)

To exit **without** saving your work as the final file:

```text
*Q
Abort edit (Y/N)?
```

Answer **`y`** to abandon (scratch file removed). Answer **`n`** to stay in the editor.

**Note:** **`E`** saves and exits; **`Q`** tries to quit without saving. Do not confuse them.

---

## Automation tip — piping commands

For scripts, a line containing only **`.`** during **`I`** ends insert mode portably (recommended). **Ctrl-Z** as the **first character of a line** still works. Example: insert one line, end insert, list, quit yes:

```text
1I
hello
.
L
Q
y
```

---

## Common mistakes (quick)

| What happened | What to do |
|---------------|------------|
| **Entry error** | Bad command letter, bad line numbers, wrong parameter count, or bad range. Check **Manual.md** for that command. |
| **Not found** | **`S`** / **`R`** found no match in range. |
| Copy/move wrong | Use **`1,2,4C`** not **`1,2C4`**. |
| Search/replace on one line | Use **`1,1S...`** or **`1,1R...`**. |
| Cannot edit **.bak** | Rename the backup file; EDLIN refuses `.bak`. |
| Line too long | Very long lines hit the internal limit (**253** characters in this port). Split the line or shorten it. |

---

## Cheat sheet

| Command | Meaning |
|---------|---------|
| *(number only)* | Edit that line (blank-line edit) |
| **`I`** | Insert lines before a line; end with **`.`** (or **Ctrl-Z** for classic behavior) |
| **`L`** | List lines (optional range) |
| **`D`** | Delete line(s) |
| **`S`**text | Search for substring |
| **`R`** | Replace first occurrence (`Rold;new` — no spaces required around `;`) |
| **`C`** | Copy block |
| **`M`** | Move block |
| **`T`** | Merge file |
| **`P`** | Page / list with prompts |
| **`W`** | Write beginning to disk / shrink buffer |
| **`A`** | Append rest of original disk file (**needs one number**, e.g. **`1A`**) |
| **`E`** | End: save and exit |
| **`Q`** | Quit without saving (confirms) |
| **`H`** | Print brief command help |
| **`;`** | No-op; also separates commands on one line |

---

## Where to go next

- **[Manual.md](Manual.md)** — full command reference, defaults, errors, and parsing rules.
- **[README.md](README.md)** — build, **`/B`**, tests, environment variables.

Happy editing.
