#ifndef FILEIO_H
#define FILEIO_H

#include "edlin.h"

// Opens the file (or creates empty scratch state), loads lines into the editor, prepares temp file.
int fileio_startup(Editor *ed, const char *path, int binary_mode);

// Reads more bytes from the original disk file and appends them as new lines (A command).
int fileio_append(Editor *ed, unsigned nlines_param);

// Writes the first part of the buffer to the scratch file and removes those lines from memory (W).
int fileio_write(Editor *ed, unsigned param1);

// Writes all remaining lines, renames backups, replaces the real file, then exits the process (E).
int fileio_end(Editor *ed);

// Closes scratch files, deletes the temp file, and exits without saving (after user confirms Q).
void fileio_quit_abort(Editor *ed);

// Inserts all lines from merge_path before/at dest_line (T command).
int fileio_merge(Editor *ed, unsigned dest_line, const char *merge_path);

#endif // FILEIO_H
