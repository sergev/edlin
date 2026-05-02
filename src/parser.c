#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "edlin.h"
#include "parser.h"

static int starts_with_ci(const char *s, const char *pfx)
{
    for (; *pfx; ++pfx, ++s) {
        unsigned char a = (unsigned char)*s;
        unsigned char b = (unsigned char)*pfx;
        if (tolower(a) != tolower(b))
            return 0;
    }
    return 1;
}

int parse_invocation(int argc, char **argv, char **out_path, int *out_binary)
{
    if (!out_path || !out_binary)
        return -1;
    *out_path = NULL;
    *out_binary = 0;

    char *path = NULL;
    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (starts_with_ci(a, "/B") || starts_with_ci(a, "-B")) {
            *out_binary = 1;
            continue;
        }
        if (!path)
            path = argv[i];
        else
            return -1; /* too many args */
    }
    if (!path)
        return -1;
    *out_path = path;
    return 0;
}

static void skip_ws(char **p)
{
    while (**p == ' ' || **p == '\t')
        (*p)++;
}

static int get_num(Editor *ed, char **p, unsigned *out, int fourth);

static int get_lineref(Editor *ed, char **p, unsigned *out, int fourth)
{
    skip_ws(p);
    if (**p == '.') {
        if (fourth)
            return -1;
        (*p)++;
        *out = (unsigned)ed->current;
        return 0;
    }
    if (**p == '#') {
        if (fourth)
            return -1;
        (*p)++;
        /* Byte-buffer semantics: “line after last” ≈ count + 1 (see GETNUM MAXLIN) */
        *out = (unsigned)ed->count + 1u;
        return 0;
    }
    if (**p == '+') {
        if (fourth)
            return -1;
        (*p)++;
        unsigned n = 0;
        if (get_num(ed, p, &n, fourth) != 0)
            return -1;
        *out = (unsigned)ed->current + n;
        return 0;
    }
    if (**p == '-') {
        if (fourth)
            return -1;
        (*p)++;
        unsigned n = 0;
        if (get_num(ed, p, &n, fourth) != 0)
            return -1;
        unsigned cur = (unsigned)ed->current;
        if (n >= cur)
            *out = 1;
        else
            *out = cur - n;
        return 0;
    }
    return get_num(ed, p, out, fourth);
}

static int get_num(Editor *ed, char **p, unsigned *out, int fourth)
{
    (void)ed;
    (void)fourth;
    skip_ws(p);
    unsigned dx = 0;
    int saw = 0;
    while (**p >= '0' && **p <= '9') {
        if (dx > 6553u)
            return -1;
        dx = dx * 10u + (unsigned)(**p - '0');
        saw = 1;
        (*p)++;
    }
    if (!saw) {
        *out = 0;
        return 0;
    }
    if (dx == 0)
        return -1;
    *out = dx;
    return 0;
}

static int dispatch_index(char cmd)
{
    static const char tab[] = {'\r', ';', 'A', 'C', 'D', 'E', 'H', 'I', 'L',
                               'M', 'P', 'Q', 'R', 'S', 'T', 'W'};
    for (size_t i = 0; i < sizeof tab; ++i) {
        if (tab[i] == cmd)
            return (int)i;
    }
    return -1;
}

ParseResult parse_command(Editor *ed, char **ptr_inout, Cmd *cmd)
{
    char *s = *ptr_inout;
    memset(cmd, 0, sizeof *cmd);

    skip_ws(&s);
    if (*s == '\0' || *s == '\n')
        return PARSE_LINE_DONE;

    memset(cmd->param, 0, sizeof cmd->param);
    cmd->nparam = 0;

    for (;;) {
        int fourth_flag = (cmd->nparam == 3);
        unsigned dx = 0;
        if (get_lineref(ed, &s, &dx, fourth_flag) != 0)
            return PARSE_ERR;
        cmd->param[cmd->nparam++] = dx;

        skip_ws(&s);
        if (*s == ',') {
            ++s;
            if (cmd->nparam >= 4)
                return PARSE_ERR;
            continue;
        }
        break;
    }

    skip_ws(&s);
    if (*s == '?') {
        cmd->query = 1;
        ++s;
        skip_ws(&s);
    }

    /* CR / newline -> blank-line edit */
    if (*s == '\0' || *s == '\n' || *s == '\r') {
        cmd->code = '\r';
        *ptr_inout = s;
        if (cmd->param[1] != 0 && cmd->param[1] < cmd->param[0])
            return PARSE_ERR;
        return PARSE_OK;
    }

    if (*s == ';') {
        cmd->code = ';';
        ++s;
        *ptr_inout = s;
        if (cmd->param[1] != 0 && cmd->param[1] < cmd->param[0])
            return PARSE_ERR;
        return PARSE_OK;
    }

    unsigned char cu = (unsigned char)*s;
    if (cu >= 'a' && cu <= 'z')
        cu = (unsigned char)(cu - 32);
    cmd->code = (char)cu;
    ++s;

    int ix = dispatch_index(cmd->code);
    if (ix < 0)
        return PARSE_ERR;

    if (cmd->param[1] != 0 && cmd->param[1] < cmd->param[0])
        return PARSE_ERR;

    *ptr_inout = s;
    return PARSE_OK;
}
