#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "commands.h"
#include "edlin.h"
#include "fileio.h"
#include "messages.h"
#include "parser.h"

//
// Skips spaces and tab characters at *p so parsing can continue at the next token.
//
static void skip_ws(char **p)
{
    while (**p == ' ' || **p == '\t')
        (*p)++;
}

//
// Reads an environment variable as a decimal number. If missing or zero, returns def.
// Used for EDLIN_LINES so users can set screen height without ioctl.
//
static unsigned env_u(const char *name, unsigned def)
{
    const char *s = getenv(name);
    if (!s || !*s)
        return def;
    unsigned v = 0;
    while (*s >= '0' && *s <= '9')
        v = v * 10u + (unsigned)(*s++ - '0');
    return v ? v : def;
}

//
// Asks the terminal how many text rows it has (for list/page window sizes).
// If stdin is not a tty or the query fails, returns 25 like classic EDLIN defaults.
//
static unsigned tty_rows(void)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0)
        return 25u;
    if (ws.ws_row <= 0)
        return 25u;
    return (unsigned)ws.ws_row;
}

//
// Handles one line the user typed: may contain several commands separated by ';'.
// Parses each command, runs it, and skips past errors so the rest of the line can run.
//
static void run_input_line(Editor *ed, char *line)
{
    char *p = line;
    for (;;) {
        skip_ws(&p);
        if (*p == '\0' || *p == '\n')
            break;

        Cmd cmd;
        char *save    = p;
        ParseResult r = parse_command(ed, &p, &cmd);
        if (r == PARSE_LINE_DONE)
            break;
        if (r == PARSE_ERR) {
            msg_entry_error();
            // Skip to next semicolon or end
            p = save;
            while (*p && *p != ';')
                ++p;
            if (*p == ';')
                ++p;
            continue;
        }

        cmd_dispatch(ed, &cmd, &p);

        skip_ws(&p);
        if (*p == ';' || *p == '\x1a')
            ++p;
        else
            break;
    }
}

//
// Program entry: reads command-line file path, loads the file, then loops reading lines.
// Each line is split into EDLIN commands; '*' is the prompt; Ctrl-Z can separate commands.
//
int main(int argc, char **argv)
{
    char *path = NULL;
    int binary = 0;
    if (parse_invocation(argc, argv, &path, &binary) != 0) {
        msg_ndname();
        return 1;
    }

    Editor ed;
    editor_init(&ed);
    ed.disp_rows = env_u("EDLIN_LINES", tty_rows());

    if (fileio_startup(&ed, path, binary) != 0) {
        editor_free(&ed);
        return 1;
    }

    char combuf[EDLIN_COMBUF];
    for (;;) {
        msg_prompt();
        fflush(stdout);
        if (!fgets(combuf, sizeof combuf, stdin))
            break;
        run_input_line(&ed, combuf);
    }

    editor_free(&ed);
    return 0;
}
