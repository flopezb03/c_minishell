#include "builtin_commands.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

int builtin_cd(char **argv, int argc, char *pwd, int pwd_size) {
    if (argc > 2)
        return -1;

    int out;
    if (argc == 1)
        out = chdir(getenv("HOME"));
    else
        out = chdir(argv[1]);

    if (out == -1)
        return -2;

    getcwd(pwd,pwd_size);
    return 0;
}
int builtin_jobs(int argc, struct CPList *bg) {
    if (argc > 1)
        return -1;

    int index = 1;
    struct CPNode *node = bg->head;
    while (node != NULL) {
        if (cp_finish(node->e) == 0) {
            printf("\t[%d]+ Running\t\t%s",index,node->e->command);
            node = node->next;
        }
        else {
            printf("\t[%d]+ Done\t\t\t%s",index,node->e->command);
            struct CPNode *aux = node;
            node = node->next;
            remove_CPList(bg,aux);
        }
        index++;
    }

    return 0;
}
int builtin_fg(char **argv, int argc, CommandProcess *fg, struct CPList *bg) {
    if (argc > 2)
        return -1;

    char *endptr;
    long node_index = strtol(argv[1], &endptr, 10);
    if (endptr == argv[1] || *endptr != '\0')
        return -2;
    if (node_index <= 0 || node_index > bg->size)
        return -3;

    struct CPNode *node_fg = bg->head;
    if (node_index == 1) {
        bg->head = node_fg->next;
        bg->size--;
        if (bg->head == NULL)
            bg->tail = NULL;
    }else {
        struct CPNode *aux = bg->head->next;
        for (int i = 2; i < node_index-1; i++)
            aux = aux->next;
        node_fg = aux->next;
        bg->size--;
        aux->next = node_fg->next;
        if (aux->next == NULL)
            bg->tail = aux;
    }

    fg->command = node_fg->e->command;
    fg->line = node_fg->e->line;
    fg->pids = node_fg->e->pids;
    fg->npipes = node_fg->e->npipes;
    fg->rin = node_fg->e->rin;
    fg->rout = node_fg->e->rout;
    fg->rerr = node_fg->e->rerr;

    for (int n_cmd = 0; n_cmd < node_fg->e->line->ncommands; n_cmd++)
        waitpid(node_fg->e->pids[n_cmd], NULL, 0);
    free_mem(node_fg->e);
    free(node_fg);

    return 0;
}
int builtin_umask(char **argv, int argc) {
    if (argc > 2)
        return -1;

    if (strlen(argv[1]) > 4)
        return -2;
    for (int i = 0; i < strlen(argv[1]); i++)
        if (argv[1][i] > '7' || argv[1][i] < '0')
            return -2;

    umask(strtol(argv[1], NULL, 0));
    return 0;
}