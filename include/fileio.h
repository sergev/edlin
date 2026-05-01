#ifndef FILEIO_H
#define FILEIO_H

#include "edlin.h"

/* Startup: open paths, load file into editor (new file if missing). Returns 0 ok. */
int fileio_startup(Editor *ed, const char *path, int binary_mode);

/* Append next chunk from input file (A command). */
int fileio_append(Editor *ed, unsigned nlines_param);

/* Write first part / n lines (W). */
int fileio_write(Editor *ed, unsigned param1);

/* Save all and exit (E): write temp, rename to .bak chain. */
int fileio_end(Editor *ed);

/* Quit: close temp and delete scratch; exit process. */
void fileio_quit_abort(Editor *ed);

/* Merge file at param1 line (T). path_rest is remainder of command line. */
int fileio_merge(Editor *ed, unsigned dest_line, const char *merge_path);

#endif /* FILEIO_H */
