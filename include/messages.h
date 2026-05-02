#ifndef MESSAGES_H
#define MESSAGES_H

void msg_prompt(void);
void msg_new_file(void);
void msg_entry_error(void);
void msg_not_found(void);
void msg_disk_full(void);
void msg_read_error(const char *path);
void msg_eof(void);
void msg_abort_edit(void);
void msg_ok_prompt(void);
void msg_continue(void);
void msg_dest_required(void);
void msg_merge_err(void);
void msg_cp_err(void);
void msg_file_ro(void);
void msg_nobak(void);
void msg_bad_drive(void);
void msg_ndname(void);
void msg_q_edit(void);
void msg_mem_full(void);
void msg_toolong(void);

void msg_line_prompt(size_t line_1b);
void msg_line_out(const char *content, size_t line_1b, int current_star);

#endif /* MESSAGES_H */
