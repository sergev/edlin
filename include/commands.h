#ifndef COMMANDS_H
#define COMMANDS_H

#include "edlin.h"
#include "parser.h"

// Runs one parsed command: updates the editor, talks to file I/O, or prints messages.
// rest_after_cmd points past the command letter; merge/search may consume a path from it.
void cmd_dispatch(Editor *ed, const Cmd *cmd, char **rest_after_cmd);

#endif // COMMANDS_H
