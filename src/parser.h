#ifndef PARSER_H
#define PARSER_H

#include "edlin.h"

typedef struct Cmd {
    unsigned param[4];
    int nparam; // number of numeric params parsed (0..4)
    int query;
    char code; // command letter or '\r' for blank-line edit, ';' for no-op
} Cmd;

typedef enum {
    PARSE_OK = 0,
    PARSE_ERR,
    PARSE_LINE_DONE // consumed to end of sub-line
} ParseResult;

// Reads argv after the program name: optional /B and one file path; sets binary mode and path.
int parse_invocation(int argc, char **argv, char **out_path, int *out_binary);

// Parses one command from the input string (one physical line may hold several after ';').
// Updates *ptr_inout past the command and optional ';'. See Cmd for line refs and letter.
ParseResult parse_command(Editor *ed, char **ptr_inout, Cmd *cmd);

#endif // PARSER_H
