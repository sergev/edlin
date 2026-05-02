#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "edlin.h"
#include "parser.h"

int main(void)
{
    Editor ed;
    editor_init(&ed);
    ed.current = 3;
    ed.count   = 10;

    char line1[] = "5,10L";
    char *p      = line1;
    Cmd cmd;
    assert(parse_command(&ed, &p, &cmd) == PARSE_OK);
    assert(cmd.code == 'L');
    assert(cmd.param[0] == 5 && cmd.param[1] == 10);

    char line2[] = "E";
    p            = line2;
    memset(&cmd, 0, sizeof cmd);
    assert(parse_command(&ed, &p, &cmd) == PARSE_OK);
    assert(cmd.code == 'E');
    assert(cmd.nparam == 1 && cmd.param[0] == 0);

    char line3[] = ".D";
    ed.current   = 4;
    p            = line3;
    memset(&cmd, 0, sizeof cmd);
    assert(parse_command(&ed, &p, &cmd) == PARSE_OK);
    assert(cmd.code == 'D');
    assert(cmd.param[0] == 4);

    char line4[] = "#L";
    p            = line4;
    memset(&cmd, 0, sizeof cmd);
    assert(parse_command(&ed, &p, &cmd) == PARSE_OK);
    assert(cmd.code == 'L');
    assert(cmd.param[0] == 11); /* count+1 */

    puts("test_parser: ok");
    editor_free(&ed);
    return 0;
}
