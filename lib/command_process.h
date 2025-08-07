#ifndef COMMAND_PROCESS_H
#define COMMAND_PROCESS_H

#include <unistd.h>
#include "parser.h"

typedef struct CommandProcess {
    char * command;
    tline *line;
    pid_t *pids;
    int (*pipes)[2];
    int npipes;
    int rin;
    int rout;
    int rerr;
}CommandProcess;
struct CPNode {
    CommandProcess *e;
    struct CPNode *next;
};
struct CPList {
    struct CPNode *head;
    struct CPNode *tail;
    int size;
};

int insert_CPList(struct CPList *list, CommandProcess *e);
int remove_CPList(struct CPList *list, struct CPNode *node);

void free_mem(CommandProcess *cpp);
void close_pipes(CommandProcess *cpp, int num_pipes);
void close_rfiles(CommandProcess *cpp);
int cp_finish(CommandProcess *cpp);
void init_cp(CommandProcess *cpp);

#endif
