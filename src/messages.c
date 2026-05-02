#include "messages.h"

#include <stdio.h>

//
// Prints the asterisk prompt so the user knows EDLIN is waiting for a command.
//
void msg_prompt(void)
{
    fputs("*", stdout);
}

//
// Tells the user no file existed yet, so editing starts with an empty buffer.
//
void msg_new_file(void)
{
    fputs("New file\n", stdout);
}

//
// Tells the user their input did not match a valid command or line reference.
//
void msg_entry_error(void)
{
    fputs("Entry error\n", stdout);
}

//
// Tells the user a search found no matching text in the chosen range.
//
void msg_not_found(void)
{
    fputs("Not found\n", stdout);
}

//
// Tells the user a write failed (disk full or similar); edits may be lost.
//
void msg_disk_full(void)
{
    fputs("Disk full. Edits lost.\n", stdout);
}

//
// Prints which path failed when reading from disk (three lines like classic DOS).
//
void msg_read_error(const char *path)
{
    fputs("Read error in:\n", stdout);
    fputs(path, stdout);
    fputc('\n', stdout);
}

//
// Tells the user the append source file has no more data to read.
//
void msg_eof(void)
{
    fputs("End of input file\n", stdout);
}

//
// Asks for confirmation before throwing away edits (quit path).
//
void msg_abort_edit(void)
{
    fputs("Abort edit (Y/N)? ", stdout);
}

//
// Asks “O.K.?” before applying a replace when query mode is on.
//
void msg_ok_prompt(void)
{
    fputs("O.K.? ", stdout);
}

//
// Asks whether to print the next screen of lines when paging output.
//
void msg_continue(void)
{
    fputs("Continue (Y/N)?\n", stdout);
}

//
// Reminds the user that copy/move needs a destination line number.
//
void msg_dest_required(void)
{
    fputs("Must specify destination line number\n", stdout);
}

//
// Says the merge could not finish, usually because memory ran out.
//
void msg_merge_err(void)
{
    fputs("Not enough room to merge the entire file\n", stdout);
}

//
// Would be used for OEM/code-page mismatch when merging; kept for classic messages.
//
void msg_cp_err(void)
{
    fputs("Cannot merge - Code page mismatch\n", stdout);
}

//
// Says the target file cannot be written because it is read-only.
//
void msg_file_ro(void)
{
    fputs("File is READ-ONLY\n", stdout);
}

//
// Says .bak backup files must be renamed before EDLIN will edit them.
//
void msg_nobak(void)
{
    fputs("Cannot edit .BAK file--rename file\n", stdout);
}

//
// Generic “bad path or drive” style error when fopen or rename fails.
//
void msg_bad_drive(void)
{
    fputs("Invalid drive or file name\n", stdout);
}

//
// Says the user forgot to pass a file name on the command line.
//
void msg_ndname(void)
{
    fputs("File name must be specified\n", stdout);
}

//
// Duplicate prompt text for abort (same string as msg_abort_edit in this build).
//
void msg_q_edit(void)
{
    fputs("Abort edit (Y/N)? ", stdout);
}

//
// Says malloc failed so the editor cannot hold more text.
//
void msg_mem_full(void)
{
    fputs("Insufficient memory\n", stdout);
}

//
// Says an inserted or replaced line would exceed the maximum allowed length.
//
void msg_toolong(void)
{
    fputs("Line too long\n", stdout);
}

//
// Prints a fixed multi-line table of EDLIN commands (H command).
//
void msg_help(void)
{
    static const char *const lines[] = {
        "Commands:\n",
        "  (number only)  Edit that line (blank-line edit)\n",
        "  I              Insert lines before a line; end with . or Ctrl-Z\n",
        "  L              List lines (optional range)\n",
        "  D              Delete line(s)\n",
        "  Stext          Search for substring\n",
        "  R              Replace first occurrence (Rold;new)\n",
        "  C              Copy block\n",
        "  M              Move block\n",
        "  T              Merge file\n",
        "  P              Page / list with prompts\n",
        "  W              Write beginning to disk / shrink buffer\n",
        "  A              Append rest of original disk file (needs one number, e.g. 1A)\n",
        "  E              End: save and exit\n",
        "  Q              Quit without saving (confirms)\n",
        "  ;              No-op; also separates commands on one line\n",
        "  H              Print this help\n",
    };
    for (size_t i = 0; i < sizeof lines / sizeof lines[0]; ++i)
        fputs(lines[i], stdout);
}

//
// Prints the line number and '*' marker before asking for insert text (DOS-style prompt).
// Same prefix idea as msg_line_out with current line marker; based on edlin.skl "%1:%2".
//
void msg_line_prompt(size_t line_1b)
{
    char prefix[16];
    int n = snprintf(prefix, sizeof prefix, "%6zu:*", line_1b);
    if (n > 0)
        fwrite(prefix, 1, (size_t)n, stdout);
    fflush(stdout);
}

//
// Prints one line with a line number, optional '*' current marker, and caret notation for controls.
// DOS DISPLAY uses "%1:%2" — line number, colon, marker (* or space).
//
void msg_line_out(const char *content, size_t line_1b, int current_star)
{
    char prefix[16];
    int n = snprintf(prefix, sizeof prefix, "%6zu:%c", line_1b, current_star ? '*' : ' ');
    if (n > 0)
        fwrite(prefix, 1, (size_t)n, stdout);
    // Echo content with control-char display like DISPLAY (subset)
    for (const unsigned char *p = (const unsigned char *)content;; ++p) {
        unsigned char c = *p;
        if (c == '\0')
            break;
        if (c >= ' ' && c != 0x7f) {
            fputc((int)c, stdout);
        } else if (c == '\t' || c == '\n' || c == '\r') {
            fputc((int)c, stdout);
        } else {
            fputc('^', stdout);
            fputc((char)(c | 0x40), stdout);
        }
    }
    fputc('\n', stdout);
}
