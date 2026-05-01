#ifndef PARSER_H
#define PARSER_H

#include "edlin.h"

typedef struct Cmd {
    unsigned param[4];
    int nparam; /* number of numeric params parsed (0..4) */
    int query;
    char code; /* command letter or '\r' for blank-line edit, ';' for no-op */
} Cmd;

typedef enum {
    PARSE_OK = 0,
    PARSE_ERR,
    PARSE_LINE_DONE /* consumed to end of sub-line */
} ParseResult;

/* argv[0]=program, argv[1..] — sets path and binary_mode; returns 0 on success */
int parse_invocation(int argc, char **argv, char **out_path, int *out_binary);

/*
 * Parse one command from *ptr_inout (within one physical input line).
 * Advances past trailing ';' or to NUL / newline end.
 */
ParseResult parse_command(Editor *ed, char **ptr_inout, Cmd *cmd);

#endif /* PARSER_H */
