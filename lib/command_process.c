#include "command_process.h"

#include <stdlib.h>
#include <sys/wait.h>

int insert_CPList(struct CPList *list, CommandProcess *e) {
    if (list == NULL || e == NULL)
        return -1;

    struct CPNode *new_node = malloc(sizeof(struct CPNode));
    new_node->next = NULL;
    new_node->e = malloc(sizeof(CommandProcess));
    new_node->e->command = e->command;
    new_node->e->line = e->line;
    new_node->e->pids = e->pids;
    new_node->e->pipes = e->pipes;
    new_node->e->npipes = e->npipes;
    new_node->e->rin = e->rin;
    new_node->e->rout = e->rout;
    new_node->e->rerr = e->rerr;

    if (list->size == 0) {
        list->head = new_node;
        list->tail = new_node;
        list->size = 1;
    }else {
        list->tail->next = new_node;
        list->tail = new_node;
        list->size++;
    }
    return 0;
}
int remove_CPList(struct CPList *list, struct CPNode *node) {
    if (list == NULL || node == NULL)
        return -1;
    if (list->size == 0)
        return -2;

    if (list->head == node) {
        list->head = node->next;
        list->size--;
        if (list->head == NULL)
            list->tail = NULL;
        free_mem(node->e);
        free(node);
        return 1;
    }
    struct CPNode *aux = list->head;
    while (aux != NULL) {
        if (aux->next == node) {
            aux->next = node->next;
            list->size--;
            if (node->next == NULL)
                list->tail = aux;
            free_mem(node->e);
            free(node);
            return 1;
        }
        aux = aux->next;
    }
    return 0;
}


void free_mem(CommandProcess *cpp) {
    free(cpp->command);
    free(cpp->pids);
    free(cpp->pipes);
}
void close_pipes(CommandProcess *cpp, int num_pipes) {
    for (int i = 0; i < num_pipes; i++) {
        close(cpp->pipes[i][0]);
        close(cpp->pipes[i][1]);
    }
}
void close_rfiles(CommandProcess *cpp) {
    if (cpp->rin != -1)
        close(cpp->rin);
    if (cpp->rout != -1)
        close(cpp->rout);
    if (cpp->rerr != -1)
        close(cpp->rerr);
}
int cp_finish(CommandProcess *cpp) {
    if (cpp == NULL)
        return -1;

    for (int n_pid = 0; n_pid <cpp->line->ncommands; n_pid++) {
        if (waitpid(cpp->pids[n_pid], NULL, WNOHANG) == 0)
            return 0;
    }
    return 1;
}
void init_cp(CommandProcess *cpp) {
    cpp->line = NULL;
    cpp->command = NULL;
    cpp->pids = NULL;
    cpp->pipes = NULL;
    cpp->npipes = 0;
    cpp->rin = 0;
    cpp->rout = 0;
    cpp->rerr = 0;
}