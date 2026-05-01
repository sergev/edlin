#ifndef COMMANDS_H
#define COMMANDS_H

#include "edlin.h"
#include "parser.h"

void cmd_dispatch(Editor *ed, const Cmd *cmd, char **rest_after_cmd);

#endif /* COMMANDS_H */
