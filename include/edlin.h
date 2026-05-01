/*
 * Portable EDLIN editor — core state (C11).
 * Behavioral reference: historical MS-DOS EDLIN assembly in this repo.
 */
#ifndef EDLIN_H
#define EDLIN_H

#include <stddef.h>
#include <stdio.h>

#define EDLIN_MAX_LINE 253 /* replacement length check in original */
#define EDLIN_COMBUF 512

typedef struct Editor {
    char **lines; /* owned; logical lines without trailing CR/LF */
    size_t count;
    size_t cap;
    size_t current; /* 1 .. count; 1 == first line */

    char *path; /* original file path */

    int binary_mode; /* /B — do not treat ^Z as EOF when loading */
    int is_new_file;
    int have_eof_read; /* input side saw EOF */

    FILE *rd_fp; /* input file for append (position tracked) */
    long rd_bytes_read;

    char *temp_path; /* .$.$$ scratch path for saving */
    FILE *wr_fp;     /* opened on first write */

    int delflg; /* first write deletes .bak */

    /* Display / pager (defaults match typical 80x25) */
    unsigned disp_rows;

    int qflg; /* query replace */

    /* Pending merge/search strings from command line (GETTEXT) */
    char txt1[256];
    char txt2[256];
} Editor;

void editor_init(Editor *ed);
void editor_free(Editor *ed);

int editor_resize(Editor *ed, size_t need);

/* Line index is 1-based; returns 0 on success */
int editor_find_line(const Editor *ed, size_t line_1b, size_t *out_idx);

const char *editor_line_get(const Editor *ed, size_t line_1b);
size_t editor_line_len(const Editor *ed, size_t line_1b);

/* Insert empty lines if needed so line_1b exists [1..] */
int editor_ensure_line(Editor *ed, size_t line_1b);

int editor_delete_range(Editor *ed, size_t first_1b, size_t last_1b);
int editor_insert_before(Editor *ed, size_t line_1b, const char *text, size_t len);
int editor_replace_line(Editor *ed, size_t line_1b, const char *text, size_t len);

/* Total logical lines (same as last line number when non-empty) */
size_t editor_last_line(const Editor *ed);

/* Move bytes-equivalent: relocate line range [p1,p2] before line p3; repeat >=1 */
int editor_blk_move(Editor *ed, unsigned p1, unsigned p2, unsigned p3, unsigned repeat,
                    int is_move);

#endif /* EDLIN_H */
