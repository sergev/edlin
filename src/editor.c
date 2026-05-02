#include <stdlib.h>
#include <string.h>

#include "edlin.h"

//
// Allocates a copy of string s on the heap (caller frees via editor_free / delete).
//
static char *dup_line(const char *s)
{
    size_t n = strlen(s);
    char *p  = malloc(n + 1);
    if (!p)
        return NULL;
    memcpy(p, s, n + 1);
    return p;
}

//
// Sets the editor to an empty document: no lines, current line 1, default screen rows.
//
void editor_init(Editor *ed)
{
    memset(ed, 0, sizeof *ed);
    ed->current   = 1;
    ed->disp_rows = 25;
}

//
// Frees every stored line and path string, closes FILE handles, and clears the struct.
//
void editor_free(Editor *ed)
{
    if (!ed)
        return;
    if (ed->lines) {
        for (size_t i = 0; i < ed->count; ++i)
            free(ed->lines[i]);
    }
    free(ed->lines);
    free(ed->path);
    free(ed->temp_path);
    if (ed->rd_fp) {
        fclose(ed->rd_fp);
        ed->rd_fp = NULL;
    }
    if (ed->wr_fp) {
        fclose(ed->wr_fp);
        ed->wr_fp = NULL;
    }
    memset(ed, 0, sizeof *ed);
}

//
// Ensures the lines array can hold at least `need` pointers (grows by doubling).
//
int editor_resize(Editor *ed, size_t need)
{
    if (need <= ed->cap)
        return 0;
    size_t ncap = ed->cap ? ed->cap : 8;
    while (ncap < need)
        ncap *= 2;
    char **nl = realloc(ed->lines, ncap * sizeof *nl);
    if (!nl)
        return -1;
    ed->lines = nl;
    ed->cap   = ncap;
    return 0;
}

//
// Returns how many lines are in the buffer (0 means an empty file).
//
size_t editor_last_line(const Editor *ed)
{
    return ed->count;
}

//
// Verifies that line_1b is between 1 and the line count; optionally outputs zero-based index.
//
int editor_find_line(const Editor *ed, size_t line_1b, size_t *out_idx)
{
    if (line_1b == 0 || line_1b > ed->count)
        return -1;
    if (out_idx)
        *out_idx = line_1b - 1;
    return 0;
}

//
// Returns the text of line line_1b, or NULL if that line number does not exist.
//
const char *editor_line_get(const Editor *ed, size_t line_1b)
{
    size_t ix;
    if (editor_find_line(ed, line_1b, &ix) != 0)
        return NULL;
    return ed->lines[ix];
}

//
// Returns strlen of the given line’s text, or 0 if the line is missing.
//
size_t editor_line_len(const Editor *ed, size_t line_1b)
{
    const char *s = editor_line_get(ed, line_1b);
    return s ? strlen(s) : 0;
}

//
// Inserts one new line at zero-based index idx0, shifting later lines up.
//
static int insert_raw(Editor *ed, size_t idx0, const char *text, size_t len)
{
    if (editor_resize(ed, ed->count + 1) != 0)
        return -1;
    memmove(ed->lines + idx0 + 1, ed->lines + idx0, (ed->count - idx0) * sizeof *ed->lines);
    ed->lines[idx0] = malloc(len + 1);
    if (!ed->lines[idx0])
        return -1;
    memcpy(ed->lines[idx0], text, len);
    ed->lines[idx0][len] = '\0';
    ed->count++;
    return 0;
}

//
// Adds blank lines at the end until line number line_1b exists (for sparse addressing).
//
int editor_ensure_line(Editor *ed, size_t line_1b)
{
    while (ed->count < line_1b) {
        if (insert_raw(ed, ed->count, "", 0) != 0)
            return -1;
    }
    return 0;
}

//
// Inserts a new line with given text immediately before line line_1b (1 .. count+1).
//
int editor_insert_before(Editor *ed, size_t line_1b, const char *text, size_t len)
{
    if (line_1b < 1)
        return -1;
    if (line_1b > ed->count + 1)
        return -1;
    size_t idx = line_1b - 1;
    return insert_raw(ed, idx, text, len);
}

//
// Replaces the entire contents of an existing line with new text (same line number).
//
int editor_replace_line(Editor *ed, size_t line_1b, const char *text, size_t len)
{
    size_t ix;
    if (editor_find_line(ed, line_1b, &ix) != 0)
        return -1;
    char *n = malloc(len + 1);
    if (!n)
        return -1;
    memcpy(n, text, len);
    n[len] = '\0';
    free(ed->lines[ix]);
    ed->lines[ix] = n;
    return 0;
}

//
// Deletes every line from first_1b through last_1b and compacts the array.
//
int editor_delete_range(Editor *ed, size_t first_1b, size_t last_1b)
{
    if (first_1b < 1 || last_1b < first_1b || last_1b > ed->count)
        return -1;
    size_t i0 = first_1b - 1;
    size_t n  = last_1b - first_1b + 1;
    for (size_t i = 0; i < n; ++i)
        free(ed->lines[i0 + i]);
    memmove(ed->lines + i0, ed->lines + i0 + n, (ed->count - (i0 + n)) * sizeof *ed->lines);
    ed->count -= n;
    if (ed->current > ed->count && ed->count > 0)
        ed->current = ed->count;
    else if (ed->count == 0)
        ed->current = 1;
    return 0;
}

//
// Copies or moves lines p1–p2 so they appear before line p3; repeat stacks multiple copies.
// For move, deletes the source block after computing where to insert (DOS-compatible rules).
//
int editor_blk_move(Editor *ed, unsigned p1, unsigned p2, unsigned p3, unsigned repeat, int is_move)
{
    if (p3 == 0)
        return -2; // dest required
    if (p1 == 0 || p2 == 0 || p2 < p1)
        return -1;
    if (p2 > ed->count || p1 > ed->count)
        return -1;
    // Destination must not lie strictly inside source line range
    if (p3 > p1 && p3 <= p2)
        return -1;

    unsigned rep = (repeat == 0 || repeat == 1) ? 1 : repeat;

    size_t nlines = (size_t)(p2 - p1 + 1);
    char **block  = calloc(nlines * rep, sizeof *block);
    if (!block)
        return -1;

    size_t bi = 0;
    for (unsigned r = 0; r < rep; ++r) {
        for (unsigned L = p1; L <= p2; ++L) {
            const char *src = editor_line_get(ed, L);
            if (!src) {
                for (size_t k = 0; k < bi; ++k)
                    free(block[k]);
                free(block);
                return -1;
            }
            block[bi] = dup_line(src);
            if (!block[bi]) {
                for (size_t k = 0; k < bi; ++k)
                    free(block[k]);
                free(block);
                return -1;
            }
            bi++;
        }
    }

    // Line to insert before (1-based), after optional delete
    unsigned dest_ins = p3;
    if (is_move) {
        if (p3 > p2)
            dest_ins = p3 - (unsigned)nlines;
        // p3 <= p1 already places before source; unchanged
        editor_delete_range(ed, p1, p2);
    }

    for (size_t k = 0; k < bi; ++k) {
        if (editor_insert_before(ed, dest_ins + (unsigned)k, block[k], strlen(block[k])) != 0) {
            for (; k < bi; ++k)
                free(block[k]);
            free(block);
            return -1;
        }
        free(block[k]);
        block[k] = NULL;
    }
    free(block);

    ed->current = dest_ins + (unsigned)bi - 1;
    if (ed->count > 0 && ed->current > ed->count)
        ed->current = ed->count;
    if (ed->current < 1)
        ed->current = 1;
    return 0;
}
