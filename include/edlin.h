// Portable EDLIN editor — core state (C11).
// Behavioral reference: historical MS-DOS EDLIN assembly in this repo.
#ifndef EDLIN_H
#define EDLIN_H

#include <stddef.h>
#include <stdio.h>

// Maximum length for a line when replacing text (matches original behavior).
#define EDLIN_MAX_LINE 253
#define EDLIN_COMBUF 512

typedef struct Editor {
    char **lines; // owned; logical lines without trailing CR/LF
    size_t count;
    size_t cap;
    size_t current; // 1 .. count; 1 == first line

    char *path; // original file path

    int binary_mode; // /B — do not treat ^Z as EOF when loading
    int is_new_file;
    int have_eof_read; // input side saw EOF

    FILE *rd_fp; // input file for append (position tracked)
    long rd_bytes_read;

    char *temp_path; // .$.$$ scratch path for saving
    FILE *wr_fp;     // opened on first write

    int delflg; // first write deletes .bak

    // Display / pager (from TIOCGWINSZ / EDLIN_LINES; see main.c)
    unsigned disp_rows;

    int qflg; // query replace

    // Pending merge/search strings from command line (GETTEXT)
    char txt1[256];
    char txt2[256];
} Editor;

// Clears the editor struct and sets sensible defaults (e.g. current line 1).
void editor_init(Editor *ed);

// Frees all lines, file paths, and open FILE handles associated with the editor.
void editor_free(Editor *ed);

// Grows the internal line pointer array if there are not enough slots for new lines.
int editor_resize(Editor *ed, size_t need);

// Checks that a 1-based line number exists. On success returns 0 and optional zero-based index.
int editor_find_line(const Editor *ed, size_t line_1b, size_t *out_idx);

// Returns a pointer to the text of line line_1b, or NULL if that line does not exist.
const char *editor_line_get(const Editor *ed, size_t line_1b);

// Returns the length in bytes of the given line’s text (0 if the line is missing).
size_t editor_line_len(const Editor *ed, size_t line_1b);

// Appends blank lines until line line_1b exists, so insert/delete can target that number.
int editor_ensure_line(Editor *ed, size_t line_1b);

// Deletes lines first_1b through last_1b inclusive and fixes the current line if needed.
int editor_delete_range(Editor *ed, size_t first_1b, size_t last_1b);

// Inserts a new line with given text before line line_1b (valid range is 1 .. count+1).
int editor_insert_before(Editor *ed, size_t line_1b, const char *text, size_t len);

// Replaces the entire text of an existing line with the new string (length len).
int editor_replace_line(Editor *ed, size_t line_1b, const char *text, size_t len);

// Returns how many logical lines are in the buffer (same as last line number if non-empty).
size_t editor_last_line(const Editor *ed);

// Copies or moves lines p1–p2 so they appear before line p3; repeat copies the block multiple times.
int editor_blk_move(Editor *ed, unsigned p1, unsigned p2, unsigned p3, unsigned repeat,
                    int is_move);

#endif // EDLIN_H
