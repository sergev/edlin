#include "fileio.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "edlin.h"
#include "messages.h"

//
// Returns 1 if string s ends with suffix suf, ignoring ASCII letter case.
//
static int ends_with_ci(const char *s, const char *suf)
{
    size_t ls = strlen(s);
    size_t lu = strlen(suf);
    if (lu > ls)
        return 0;
    for (size_t i = 0; i < lu; ++i) {
        char a = (char)(s[ls - lu + i]);
        char b = suf[i];
        if (a >= 'A' && a <= 'Z')
            a += 32;
        if (b >= 'A' && b <= 'Z')
            b += 32;
        if (a != b)
            return 0;
    }
    return 1;
}

//
// Builds a scratch filename like.basename$$$ next to the real path for temp saves.
//
static int build_temp_path(const char *src, char **out)
{
    size_t n  = strlen(src);
    char *buf = malloc(n + 16);
    if (!buf)
        return -1;
    memcpy(buf, src, n + 1);
    char *dot   = strrchr(buf, '.');
    char *slash = strrchr(buf, '/');
#ifdef _WIN32
    char *bs = strrchr(buf, '\\');
    if (!slash || (bs && bs > slash))
        slash = bs;
#endif
    if (dot && (!slash || dot > slash))
        *dot = '\0';
    strcat(buf, ".$$$");
    *out = buf;
    return 0;
}

//
// Splits loaded file bytes into lines (newlines, optional CR, optional ^Z in text mode).
// Appends each segment as a new line at the end of the editor buffer.
//
static int append_loaded_lines(Editor *ed, const unsigned char *buf, size_t len, int binary_mode)
{
    size_t i          = 0;
    size_t line_start = 0;

    while (i < len) {
        if (!binary_mode && buf[i] == 0x1a)
            break;
        if (buf[i] == '\n') {
            size_t seglen = i - line_start;
            if (seglen > 0 && buf[line_start + seglen - 1] == '\r')
                seglen--;
            if (editor_insert_before(ed, ed->count + 1, (const char *)(buf + line_start), seglen) !=
                0)
                return -1;
            ++i;
            line_start = i;
            continue;
        }
        ++i;
    }
    if (line_start < len) {
        size_t seglen = len - line_start;
        if (!binary_mode && seglen > 0 && buf[line_start + seglen - 1] == 0x1a)
            seglen--;
        if (seglen > 0) {
            if (editor_insert_before(ed, ed->count + 1, (const char *)(buf + line_start), seglen) !=
                0)
                return -1;
        }
    }
    return 0;
}

//
// Opens the file, loads it into memory as lines, and opens a temp file for later W/E.
// New files get an empty buffer; .bak names are rejected; read errors are reported.
//
int fileio_startup(Editor *ed, const char *path, int binary_mode)
{
    ed->binary_mode = binary_mode;
    ed->path        = strdup(path);
    if (!ed->path)
        return -1;

    if (ends_with_ci(path, ".bak")) {
        msg_nobak();
        return -1;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        if (errno == ENOENT) {
            ed->is_new_file = 1;
            msg_new_file();
            if (build_temp_path(path, &ed->temp_path) != 0)
                return -1;
            ed->wr_fp = fopen(ed->temp_path, "w+b");
            if (!ed->wr_fp) {
                msg_bad_drive();
                return -1;
            }
            return 0;
        }
        msg_bad_drive();
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        msg_read_error(path);
        return -1;
    }
    long sz = ftell(fp);
    if (sz < 0) {
        fclose(fp);
        msg_read_error(path);
        return -1;
    }
    rewind(fp);
    unsigned char *buf = calloc(1, (size_t)sz + 1);
    if (!buf) {
        fclose(fp);
        msg_mem_full();
        return -1;
    }
    size_t rd = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);

    size_t effective = rd;
    if (!binary_mode) {
        for (size_t i = 0; i < rd; ++i) {
            if (buf[i] == 0x1a) {
                effective = i;
                break;
            }
        }
    }

    if (append_loaded_lines(ed, buf, effective, binary_mode) != 0) {
        free(buf);
        msg_mem_full();
        return -1;
    }
    free(buf);

    ed->rd_fp = fopen(path, "rb");
    if (!ed->rd_fp) {
        msg_read_error(path);
        return -1;
    }
    if (fseek(ed->rd_fp, (long)rd, SEEK_SET) != 0) {
        fclose(ed->rd_fp);
        ed->rd_fp = NULL;
        msg_read_error(path);
        return -1;
    }

    if (build_temp_path(path, &ed->temp_path) != 0)
        return -1;
    ed->wr_fp = fopen(ed->temp_path, "w+b");
    if (!ed->wr_fp) {
        msg_bad_drive();
        return -1;
    }

    if (ed->count > 0)
        ed->current = 1;
    return 0;
}

//
// Reads any remaining bytes from the original file on disk and appends them as new lines.
//
int fileio_append(Editor *ed, unsigned nlines_param)
{
    (void)nlines_param;
    if (!ed->rd_fp || ed->have_eof_read) {
        msg_eof();
        return 0;
    }

    long pos = ftell(ed->rd_fp);
    if (pos < 0 || fseek(ed->rd_fp, 0, SEEK_END) != 0) {
        msg_read_error(ed->path);
        return -1;
    }
    long end = ftell(ed->rd_fp);
    if (end < pos)
        end = pos;
    size_t len = (size_t)(end - pos);
    if (fseek(ed->rd_fp, pos, SEEK_SET) != 0) {
        msg_read_error(ed->path);
        return -1;
    }

    unsigned char *chunk = malloc(len + 1);
    if (!chunk) {
        msg_mem_full();
        return -1;
    }
    size_t n = fread(chunk, 1, len, ed->rd_fp);
    if (ferror(ed->rd_fp)) {
        free(chunk);
        msg_read_error(ed->path);
        return -1;
    }

    size_t take = n;
    if (!ed->binary_mode) {
        for (size_t i = 0; i < n; ++i) {
            if (chunk[i] == 0x1a) {
                take = i;
                break;
            }
        }
    }

    if (append_loaded_lines(ed, chunk, take, ed->binary_mode) != 0) {
        free(chunk);
        msg_mem_full();
        return -1;
    }
    free(chunk);
    ed->have_eof_read = 1;
    msg_eof();
    return 0;
}

//
// Opens the writable temp file on first use so W/E can flush lines to disk.
//
static int ensure_wr(Editor *ed)
{
    if (ed->wr_fp)
        return 0;
    if (!ed->temp_path && ed->path && build_temp_path(ed->path, &ed->temp_path) != 0)
        return -1;
    ed->wr_fp = fopen(ed->temp_path, "a+b");
    if (!ed->wr_fp)
        return -1;
    return 0;
}

//
// Writes the first chunk of lines to the scratch file and removes them from memory.
// If param1 is 0, writes about one quarter of all lines (classic EDLIN heuristic).
//
int fileio_write(Editor *ed, unsigned param1)
{
    if (ensure_wr(ed) != 0) {
        msg_disk_full();
        return -1;
    }

    size_t until = ed->count;
    if (param1 != 0) {
        if (param1 <= ed->count)
            until = param1 - 1;
    } else {
        // Quarter heuristic on line count
        until = ed->count ? (ed->count + 3) / 4 : 0;
    }

    for (size_t L = 1; L <= until && L <= ed->count; ++L) {
        const char *ln = editor_line_get(ed, L);
        if (!ln)
            continue;
        if (fprintf(ed->wr_fp, "%s\n", ln) < 0) {
            msg_disk_full();
            return -1;
        }
    }
    if (until > 0)
        editor_delete_range(ed, 1, until);
    return 0;
}

//
// Saves every remaining line to the temp file, applies .bak rename, replaces the real file, exits.
//
int fileio_end(Editor *ed)
{
    if (ensure_wr(ed) != 0) {
        msg_disk_full();
        return -1;
    }

    fclose(ed->wr_fp);
    ed->wr_fp = fopen(ed->temp_path, "wb");
    if (!ed->wr_fp) {
        msg_disk_full();
        return -1;
    }

    for (size_t L = 1; L <= ed->count; ++L) {
        const char *ln = editor_line_get(ed, L);
        if (ln && fprintf(ed->wr_fp, "%s\n", ln) < 0) {
            msg_disk_full();
            return -1;
        }
    }
    if (!ed->binary_mode)
        fputc(0x1a, ed->wr_fp);
    fclose(ed->wr_fp);
    ed->wr_fp = NULL;

    char *bak = malloc(strlen(ed->path) + 8);
    if (!bak)
        return -1;
    strcpy(bak, ed->path);
    char *dot   = strrchr(bak, '.');
    char *slash = strrchr(bak, '/');
#ifdef _WIN32
    char *bs = strrchr(bak, '\\');
    if (!slash || (bs && bs > slash))
        slash = bs;
#endif
    if (dot && (!slash || dot > slash))
        strcpy(dot, ".bak");
    else
        strcat(bak, ".bak");

    remove(bak);
    if (rename(ed->path, bak) != 0 && errno != ENOENT) {
        free(bak);
        msg_disk_full();
        return -1;
    }
    free(bak);

    if (rename(ed->temp_path, ed->path) != 0) {
        msg_disk_full();
        return -1;
    }

    if (ed->rd_fp) {
        fclose(ed->rd_fp);
        ed->rd_fp = NULL;
    }
    exit(0);
}

//
// Discards the scratch file and exits immediately without writing the main file (quit confirmed).
//
void fileio_quit_abort(Editor *ed)
{
    if (ed->wr_fp) {
        fclose(ed->wr_fp);
        ed->wr_fp = NULL;
    }
    if (ed->temp_path)
        remove(ed->temp_path);
    if (ed->rd_fp) {
        fclose(ed->rd_fp);
        ed->rd_fp = NULL;
    }
    exit(0);
}

//
// Loads merge_path as its own buffer of lines, then inserts those lines before dest_line.
//
int fileio_merge(Editor *ed, unsigned dest_line, const char *merge_path)
{
    FILE *m = fopen(merge_path, "rb");
    if (!m) {
        msg_bad_drive();
        return -1;
    }
    if (fseek(m, 0, SEEK_END) != 0) {
        fclose(m);
        return -1;
    }
    long msz = ftell(m);
    if (msz < 0) {
        fclose(m);
        return -1;
    }
    rewind(m);
    unsigned char *buf = malloc((size_t)msz + 1);
    if (!buf) {
        fclose(m);
        msg_merge_err();
        return -1;
    }
    size_t rd = fread(buf, 1, (size_t)msz, m);
    fclose(m);

    unsigned ins = dest_line;
    if (ins == 0)
        ins = (unsigned)ed->current;

    Editor tmp;
    editor_init(&tmp);
    if (append_loaded_lines(&tmp, buf, rd, ed->binary_mode) != 0) {
        editor_free(&tmp);
        free(buf);
        msg_merge_err();
        return -1;
    }
    free(buf);

    for (size_t k = 0; k < tmp.count; ++k) {
        const char *ln = editor_line_get(&tmp, k + 1);
        if (!ln || editor_insert_before(ed, ins + (unsigned)k, ln, strlen(ln)) != 0) {
            editor_free(&tmp);
            msg_merge_err();
            return -1;
        }
    }
    editor_free(&tmp);
    return 0;
}
