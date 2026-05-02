#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands.h"
#include "fileio.h"
#include "messages.h"

//
// Skips spaces and tabs at *p (same idea as the parser’s skip_ws).
//
static void skip_ws(char **p)
{
    while (**p == ' ' || **p == '\t')
        (*p)++;
}

//
// Copies one field from R/S command text: stops at ';' or end of line.
// Ctrl-V (0x16) quotes the next character so ';' can appear inside a field.
//
static int gettext_field(char **pp, char *buf, size_t bufsz)
{
    size_t n = 0;
    char *s = *pp;
    while (*s && *s != '\r' && *s != '\n') {
        if (*s == ';')
            break;
        if ((unsigned char)*s == 0x16 && s[1]) {
            ++s;
            if (n + 1 >= bufsz)
                return -1;
            buf[n++] = *s++;
            continue;
        }
        if (n + 1 >= bufsz)
            return -1;
        buf[n++] = *s++;
    }
    buf[n] = '\0';
    *pp = s;
    return 0;
}

//
// Reads one line from stdin and removes a trailing CR/LF so the rest is plain text.
//
static int read_line_stdin(char *buf, size_t sz)
{
    if (!fgets(buf, (int)sz, stdin))
        return -1;
    size_t L = strlen(buf);
    while (L > 0 && (buf[L - 1] == '\n' || buf[L - 1] == '\r'))
        buf[--L] = '\0';
    return 0;
}

//
// Asks Y/N and returns 1 for yes, 0 for no; empty line counts as yes (DOS style).
//
static int prompt_yn(void)
{
    char b[32];
    if (read_line_stdin(b, sizeof b) != 0)
        return 0;
    if (b[0] == '\0' || b[0] == '\r')
        return 1;
    char c = (char)tolower((unsigned char)b[0]);
    if (c == 'y')
        return 1;
    if (c == 'n')
        return 0;
    return prompt_yn();
}

//
// L command: prints a window of lines with line numbers and * on the current line.
//
static void cmd_list(Editor *ed, const Cmd *cmd)
{
    unsigned start = cmd->param[0];
    unsigned endp = cmd->param[1];
    if (ed->count == 0)
        return;
    if (start == 0) {
        if (ed->current > 11)
            start = (unsigned)ed->current - 11;
        else
            start = 1;
    }
    if (endp == 0) {
        unsigned window = ed->disp_rows > 2 ? ed->disp_rows - 2u : 1u;
        endp = start + window - 1;
    }
    if (endp < start) {
        msg_entry_error();
        return;
    }
    unsigned last = (unsigned)ed->count;
    if (start > last) {
        msg_entry_error();
        return;
    }
    if (endp > last)
        endp = last;
    for (unsigned L = start; L <= endp; ++L) {
        const char *ln = editor_line_get(ed, L);
        if (!ln)
            break;
        msg_line_out(ln, L, L == ed->current ? 1 : 0);
    }
}

//
// P command: like list but pauses every screenful and updates current line as it goes.
//
static void cmd_pager(Editor *ed, const Cmd *cmd)
{
    unsigned last = ed->count ? (unsigned)ed->count : 1u;
    unsigned start = cmd->param[0];
    if (start == 0) {
        start = (unsigned)ed->current;
        if (start != 1)
            ++start;
    }
    if (start > ed->count)
        return;
    unsigned endp = cmd->param[1];
    if (endp == 0) {
        unsigned w = ed->disp_rows > 2 ? ed->disp_rows - 2u : 1u;
        endp = start + w;
    }
    ++endp;
    if (endp > last + 1)
        endp = last + 1;
    if (endp <= start) {
        msg_entry_error();
        return;
    }
    unsigned pg = ed->disp_rows > 1 ? ed->disp_rows - 1u : 1u;
    unsigned shown = 0;
    for (unsigned L = start; L < endp; ++L) {
        const char *ln = editor_line_get(ed, L);
        if (!ln)
            break;
        msg_line_out(ln, L, L == ed->current ? 1 : 0);
        ++shown;
        ed->current = L;
        if (shown % pg == 0 && L + 1 < endp) {
            msg_continue();
            if (!prompt_yn())
                break;
        }
    }
}

//
// D command: deletes a range of lines; omitted params default to the current line only.
//
static void cmd_delete(Editor *ed, const Cmd *cmd)
{
    unsigned p1 = cmd->param[0];
    unsigned p2 = cmd->param[1];
    if (p1 == 0)
        p1 = (unsigned)ed->current;
    if (p2 == 0)
        p2 = p1;
    if (p2 < p1 || p1 < 1 || p2 > ed->count) {
        msg_entry_error();
        return;
    }
    editor_delete_range(ed, p1, p2);
    ed->current = p1 <= ed->count ? p1 : (ed->count ? ed->count : 1u);
}

//
// I command: repeatedly reads lines from the user and inserts before line n until "." or ^Z.
//
static void cmd_insert(Editor *ed, const Cmd *cmd)
{
    unsigned n = cmd->param[0];
    if (n == 0)
        n = (unsigned)ed->current; // insert before current line
    if (n < 1) {
        msg_entry_error();
        return;
    }
    for (;;) {
        char line[300];
        msg_line_prompt(n);
        if (read_line_stdin(line, sizeof line) != 0)
            break;
        if (line[0] == '\x1a')
            break;
        // Single dot ends insert (modern alternative to Ctrl-Z); ^V. inserts a literal dot
        if (strcmp(line, ".") == 0)
            break;
        // unquote ^V
        char out[300];
        size_t o = 0;
        for (size_t i = 0; line[i] && o + 1 < sizeof out; ++i) {
            if ((unsigned char)line[i] == 0x16 && line[i + 1])
                out[o++] = line[++i];
            else
                out[o++] = line[i];
        }
        out[o] = '\0';
        if (o > EDLIN_MAX_LINE) {
            msg_toolong();
            continue;
        }
        editor_insert_before(ed, n, out, o);
        ++n;
    }
}

//
// Blank-line edit: only a line number — show line, then replace it or append after last line.
//
static void cmd_nocom(Editor *ed, const Cmd *cmd)
{
    if (cmd->nparam > 1) {
        msg_entry_error();
        return;
    }
    unsigned n = cmd->param[0];
    if (n == 0)
        n = (unsigned)ed->current + 1u;
    if (n < 1 || n > ed->count + 1) {
        msg_entry_error();
        return;
    }
    if (n <= ed->count) {
        const char *cur = editor_line_get(ed, n);
        char buf[300];
        if (cur) {
            msg_line_out(cur, n, 1);
        }
        msg_line_prompt(n);
        if (read_line_stdin(buf, sizeof buf) != 0)
            return;
        char out[300];
        size_t o = 0;
        for (size_t i = 0; buf[i] && o + 1 < sizeof out; ++i) {
            if ((unsigned char)buf[i] == 0x16 && buf[i + 1])
                out[o++] = buf[++i];
            else
                out[o++] = buf[i];
        }
        out[o] = '\0';
        if (o > EDLIN_MAX_LINE) {
            msg_toolong();
            return;
        }
        editor_replace_line(ed, n, out, o);
        ed->current = n;
    } else {
        char buf[300];
        msg_line_prompt(n);
        if (read_line_stdin(buf, sizeof buf) != 0)
            return;
        editor_insert_before(ed, n, buf, strlen(buf));
        ed->current = n;
    }
}

//
// S command (and shared logic): finds old text in a line range; optional query before stopping.
//
static void cmd_search(Editor *ed, const Cmd *cmd, char **rio, int from_current)
{
    if (cmd->nparam > 2) {
        msg_entry_error();
        return;
    }
    char oldp[256];
    char *r = *rio;
    if (gettext_field(&r, oldp, sizeof oldp) != 0) {
        msg_entry_error();
        return;
    }
    *rio = r;
    size_t oldlen = strlen(oldp);
    if (oldlen == 0) {
        msg_entry_error();
        return;
    }

    unsigned start_line = cmd->param[0];
    unsigned end_line = cmd->param[1];
    if (start_line == 0) {
        if (from_current)
            start_line = (unsigned)ed->current + 1u;
        else
            start_line = 1;
    }
    if (end_line == 0)
        end_line = (unsigned)ed->count;
    if (end_line < start_line || start_line < 1) {
        msg_entry_error();
        return;
    }

    for (unsigned L = start_line; L <= end_line && L <= ed->count; ++L) {
        const char *ln = editor_line_get(ed, L);
        if (!ln)
            continue;
        if (strstr(ln, oldp)) {
            ed->current = L;
            msg_line_out(ln, L, 1);
            if (cmd->query) {
                msg_ok_prompt();
                if (!prompt_yn())
                    continue;
            }
            return;
        }
    }
    msg_not_found();
}

//
// R command: parses old;new fields, finds first hit in range, optionally asks, then replaces once.
//
static void cmd_replace(Editor *ed, const Cmd *cmd, char **rio, int from_current)
{
    if (cmd->nparam > 2) {
        msg_entry_error();
        return;
    }
    char oldp[256], newp[256];
    char *r = *rio;
    if (gettext_field(&r, oldp, sizeof oldp) != 0) {
        msg_entry_error();
        return;
    }
    skip_ws(&r);
    if (*r == ';')
        ++r;
    if (gettext_field(&r, newp, sizeof newp) != 0) {
        msg_entry_error();
        return;
    }
    *rio = r;
    size_t oldlen = strlen(oldp);
    size_t newlen = strlen(newp);
    if (oldlen == 0) {
        msg_entry_error();
        return;
    }
    unsigned start_line = cmd->param[0];
    unsigned end_line = cmd->param[1];
    if (start_line == 0) {
        if (from_current)
            start_line = (unsigned)ed->current + 1u;
        else
            start_line = 1;
    }
    if (end_line == 0)
        end_line = (unsigned)ed->count;

    for (unsigned L = start_line; L <= end_line && L <= ed->count; ++L) {
        const char *ln = editor_line_get(ed, L);
        if (!ln)
            continue;
        char *hit = strstr(ln, oldp);
        if (!hit)
            continue;
        size_t prefix = (size_t)(hit - ln);
        size_t suffix_len = strlen(hit + oldlen);
        if (prefix + newlen + suffix_len > EDLIN_MAX_LINE) {
            msg_toolong();
            return;
        }
        char buf[300];
        memcpy(buf, ln, prefix);
        memcpy(buf + prefix, newp, newlen);
        memcpy(buf + prefix + newlen, hit + oldlen, suffix_len + 1);
        msg_line_out(buf, L, 1);
        if (cmd->query) {
            msg_ok_prompt();
            if (!prompt_yn())
                continue;
        }
        editor_replace_line(ed, L, buf, prefix + newlen + suffix_len);
        ed->current = L;
        return;
    }
    msg_not_found();
}

//
// Central switch: runs the right handler for the parsed command letter and trailing merge path.
//
void cmd_dispatch(Editor *ed, const Cmd *cmd, char **rest_after_cmd)
{
    char *rest = rest_after_cmd ? *rest_after_cmd : NULL;
    if (!rest)
        rest = (char *)"";

    switch (cmd->code) {
    case '\r':
        cmd_nocom(ed, cmd);
        break;
    case ';':
        break;
    case 'A':
        if (cmd->nparam != 1) {
            msg_entry_error();
            break;
        }
        fileio_append(ed, cmd->param[0]);
        break;
    case 'C':
        if (cmd->nparam < 3 || cmd->nparam > 4) {
            msg_entry_error();
            break;
        }
        if (editor_blk_move(ed, cmd->param[0], cmd->param[1], cmd->param[2], cmd->param[3], 0) !=
            0) {
            if (cmd->param[2] == 0)
                msg_dest_required();
            else
                msg_entry_error();
        }
        break;
    case 'D':
        if (cmd->nparam > 2)
            msg_entry_error();
        else
            cmd_delete(ed, cmd);
        break;
    case 'E':
        if (cmd->nparam != 1 || cmd->param[0] != 0)
            msg_entry_error();
        else
            fileio_end(ed);
        break;
    case 'H':
        if (cmd->nparam != 1 || cmd->param[0] != 0)
            msg_entry_error();
        else
            msg_help();
        break;
    case 'I':
        if (cmd->nparam > 1)
            msg_entry_error();
        else
            cmd_insert(ed, cmd);
        break;
    case 'L':
        if (cmd->nparam > 2)
            msg_entry_error();
        else
            cmd_list(ed, cmd);
        break;
    case 'M':
        if (cmd->nparam != 3) {
            msg_entry_error();
            break;
        }
        if (editor_blk_move(ed, cmd->param[0], cmd->param[1], cmd->param[2], 0, 1) != 0) {
            msg_entry_error();
        }
        break;
    case 'P':
        if (cmd->nparam > 2)
            msg_entry_error();
        else
            cmd_pager(ed, cmd);
        break;
    case 'Q':
        if (cmd->nparam != 1 || cmd->param[0] != 0) {
            msg_entry_error();
            break;
        }
        msg_abort_edit();
        if (prompt_yn())
            fileio_quit_abort(ed);
        break;
    case 'R':
        cmd_replace(ed, cmd, &rest, 1);
        break;
    case 'S':
        cmd_search(ed, cmd, &rest, 1);
        break;
    case 'T': {
        if (cmd->nparam != 1) {
            msg_entry_error();
            break;
        }
        char path[256];
        skip_ws(&rest);
        size_t i = 0;
        while (*rest && *rest != ';' && *rest != ' ' && *rest != '\t' && i + 1 < sizeof path)
            path[i++] = *rest++;
        path[i] = '\0';
        if (i == 0) {
            msg_entry_error();
            break;
        }
        fileio_merge(ed, cmd->param[0], path);
        break;
    }
    case 'W':
        if (cmd->nparam > 1)
            msg_entry_error();
        else
            fileio_write(ed, cmd->param[0]);
        break;
    default:
        msg_entry_error();
        break;
    }
    if (rest_after_cmd)
        *rest_after_cmd = rest;
}
