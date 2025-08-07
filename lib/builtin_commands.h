#ifndef BUILTIN_COMMANDS_H
#define BUILTIN_COMMANDS_H
#include "command_process.h"

int builtin_cd(char **argv, int argc, char *pwd, int pwd_size);
int builtin_jobs(int argc, struct CPList *bg);
int builtin_fg(char **argv, int argc, CommandProcess *fg, struct CPList *bg);
int builtin_umask(char **argv, int argc);
#endif
