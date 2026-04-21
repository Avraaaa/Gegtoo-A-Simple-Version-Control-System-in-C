#ifndef COMMANDS_H
#define COMMANDS_H

void geg_init(const char *path);
void geg_add(int argc, char *argv[]);
void geg_remove(int argc, char *argv[]);
void geg_status(void);
void geg_commit(const char* message);
void geg_cat(const char *id);
void geg_log(void);
void geg_checkout(const char *target_id);
void geg_diff(int argc, char *argv[]);
void geg_branch(int argc, char *argv[]);

#endif
