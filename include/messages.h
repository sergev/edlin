#ifndef MESSAGES_H
#define MESSAGES_H

// Prints the main “*” prompt with no newline (user types on the same line).
void msg_prompt(void);

// Tells the user the named file did not exist and a new buffer was started.
void msg_new_file(void);

// Tells the user the command or line number they typed could not be understood.
void msg_entry_error(void);

// Tells the user a search or replace did not find matching text in the range.
void msg_not_found(void);

// Tells the user the disk (or write) failed and unsaved work may be lost.
void msg_disk_full(void);

// Prints a short read error banner and the file path that failed to read.
void msg_read_error(const char *path);

// Tells the user the end of the input file was reached (append path).
void msg_eof(void);

// Asks whether to abort editing (used before quitting without save).
void msg_abort_edit(void);

// Asks “O.K.?” during query replace or similar confirmation steps.
void msg_ok_prompt(void);

// Asks whether to show the next page when listing long output (pager).
void msg_continue(void);

// Says the user must give a destination line for copy/move-style commands.
void msg_dest_required(void);

// Says merging another file failed because there was not enough memory room.
void msg_merge_err(void);

// Says two files use different code pages so merge cannot continue safely.
void msg_cp_err(void);

// Says the file cannot be written because it is read-only.
void msg_file_ro(void);

// Says .bak files cannot be edited directly; rename first.
void msg_nobak(void);

// Says the path or drive name is invalid or cannot be opened.
void msg_bad_drive(void);

// Says the user must pass a file name on the command line.
void msg_ndname(void);

// Asks about aborting the edit session (same prompt style as msg_abort_edit).
void msg_q_edit(void);

// Says the program ran out of heap memory for lines or buffers.
void msg_mem_full(void);

// Says a line the user entered is longer than the allowed maximum.
void msg_toolong(void);

// Prints a short list of EDLIN commands and what they do.
void msg_help(void);

// Prints the line number and “*” before insert/blank-line edit (no trailing newline).
void msg_line_prompt(size_t line_1b);

// Prints a line with a line number, current-line marker, and visible control characters.
void msg_line_out(const char *content, size_t line_1b, int current_star);

#endif // MESSAGES_H
