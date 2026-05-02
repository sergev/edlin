#include <stdio.h>

#include "messages.h"

void msg_prompt(void) { fputs("*", stdout); }

void msg_new_file(void) { fputs("New file\n", stdout); }

void msg_entry_error(void) { fputs("Entry error\n", stdout); }

void msg_not_found(void) { fputs("Not found\n", stdout); }

void msg_disk_full(void) { fputs("Disk full. Edits lost.\n", stdout); }

void msg_read_error(const char *path)
{
    fputs("Read error in:\n", stdout);
    fputs(path, stdout);
    fputc('\n', stdout);
}

void msg_eof(void) { fputs("End of input file\n", stdout); }

void msg_abort_edit(void) { fputs("Abort edit (Y/N)? ", stdout); }

void msg_ok_prompt(void) { fputs("O.K.? ", stdout); }

void msg_continue(void) { fputs("Continue (Y/N)?\n", stdout); }

void msg_dest_required(void) { fputs("Must specify destination line number\n", stdout); }

void msg_merge_err(void) { fputs("Not enough room to merge the entire file\n", stdout); }

void msg_cp_err(void) { fputs("Cannot merge - Code page mismatch\n", stdout); }

void msg_file_ro(void) { fputs("File is READ-ONLY\n", stdout); }

void msg_nobak(void) { fputs("Cannot edit .BAK file--rename file\n", stdout); }

void msg_bad_drive(void) { fputs("Invalid drive or file name\n", stdout); }

void msg_ndname(void) { fputs("File name must be specified\n", stdout); }

void msg_q_edit(void) { fputs("Abort edit (Y/N)? ", stdout); }

void msg_mem_full(void) { fputs("Insufficient memory\n", stdout); }

void msg_toolong(void) { fputs("Line too long\n", stdout); }

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

void msg_line_prompt(size_t line_1b)
{
    /* Same prefix as msg_line_out with current line marker; DOS uses "%1:%2" (edlin.skl). */
    char prefix[16];
    int n = snprintf(prefix, sizeof prefix, "%6zu:*", line_1b);
    if (n > 0)
        fwrite(prefix, 1, (size_t)n, stdout);
    fflush(stdout);
}

void msg_line_out(const char *content, size_t line_1b, int current_star)
{
    /* DOS DISPLAY uses message "%1:%2" — line number, colon, line marker (* or space). */
    char prefix[16];
    int n = snprintf(prefix, sizeof prefix, "%6zu:%c", line_1b, current_star ? '*' : ' ');
    if (n > 0)
        fwrite(prefix, 1, (size_t)n, stdout);
    /* Echo content with control-char display like DISPLAY (subset) */
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
