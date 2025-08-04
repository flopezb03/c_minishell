#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "lib/parser.h"
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>



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
// Constantes
#define BUFF_IN 128

// Variables Globales

char *user = "user";
char *pcname = "userpc";
char pwd[1024];

CommandProcess fg;
struct CPList bg;

//  Funciones

void free_mem(CommandProcess *cpp);
void close_pipes(CommandProcess *cpp, int num_pipes);
void close_rfiles(CommandProcess *cpp);
int cp_finish(CommandProcess *cpp);
void init_fg(CommandProcess *cpp);

int builtin_cd(char **argv, int argc);
int builtin_jobs(int argc, struct CPList *bg);
int builtin_fg(char **argv, int argc, CommandProcess *fg, struct CPList *bg);
int builtin_umask(char **argv, int argc);

int insert_CPList(struct CPList *list, CommandProcess *e);
int remove_CPList(struct CPList *list, struct CPNode *node);

void handler_sigint(int sig);


int main() {

    int exiting = 0;
    char buff_in[BUFF_IN];
    tline *command_line = NULL;


    int error = 0;
    int builtin = -1;
    signal(SIGINT, handler_sigint);

    getcwd(pwd,1024);


    while (exiting == 0) {
        error = 0;
        builtin = -1;
        buff_in[0] = '\0';
        command_line = NULL;
        init_fg(&fg);
        fflush(stderr);



        // Prompt
        fprintf(stdout,"%s@%s:%s$ ",user,pcname,pwd);


        // Entrada
        fgets(buff_in,BUFF_IN,stdin);


        // Analisis
        command_line = tokenize(buff_in);
        if (command_line == NULL) {
            fprintf(stderr, "Error de sintaxis en el comando. %s\n",strerror(errno));
            continue;
        }
        if (command_line->ncommands == 0)
            continue;
        if (command_line->commands[0].filename == NULL) {
            if (strcmp(command_line->commands[0].argv[0],"exit") == 0)
                builtin = 0;
            else if (strcmp(command_line->commands[0].argv[0],"cd") == 0)
                builtin = 1;
            else if (strcmp(command_line->commands[0].argv[0],"jobs") == 0)
                builtin = 2;
            else if (strcmp(command_line->commands[0].argv[0],"fg") == 0)
                builtin = 3;
            else if (strcmp(command_line->commands[0].argv[0],"umask") == 0)
                builtin = 4;
        }
        if (builtin == -1){
            fg.line = command_line;


            // Comprobacion de comandos
            for (int n_cmd = 0; n_cmd < fg.line->ncommands; n_cmd++)
                if (fg.line->commands[n_cmd].filename == NULL) {
                    fprintf(stderr,"El comando en la posicion %d no se ha encontrado\n",n_cmd);
                    error = 1;
                }
            if (error == 1)
                continue;

            //  Comprobacion de ficheros de redirecciones
            if (fg.line->redirect_input == NULL)
                fg.rin = -1;
            else {
                fg.rin = open(fg.line->redirect_input,O_RDONLY);
                if (fg.rin == -1) {
                    perror("open");
                    continue;
                }
            }
            if (fg.line->redirect_output == NULL)
                fg.rout = -1;
            else {
                fg.rout = open(fg.line->redirect_output,O_WRONLY | O_CREAT);
                if (fg.rout == -1) {
                    perror("open");
                    continue;
                }
            }
            if (fg.line->redirect_error == NULL)
                fg.rerr = -1;
            else {
                fg.rerr = open(fg.line->redirect_error,O_WRONLY | O_CREAT | O_APPEND);
                if (fg.rerr == -1) {
                    perror("open");
                    continue;
                }
            }

            // Copiar comando
            fg.command = strdup(buff_in);

            //  Reservar pids
            fg.pids = calloc(command_line->ncommands,sizeof(pid_t));

            // Reservar y abrir pipes
            fg.npipes = command_line->ncommands-1;
            fg.pipes = calloc(fg.npipes, sizeof(int[2]));
            for (int n_pipe = 0; n_pipe < fg.npipes; n_pipe++)
                if (pipe(fg.pipes[n_pipe]) == -1) {
                    error = 1;
                    perror("pipe");
                    close_pipes(&fg,n_pipe);
                    break;
                }
            if (error == 1) {
                free_mem(&fg);
                continue;
            }
        }




        // Ejecucion
        if (builtin == 0) {
            exiting = 1;
            continue;
        }
        if (builtin == 1) {
            int res = builtin_cd(command_line->commands[0].argv,command_line->commands[0].argc);
            if (res == -1) {
                perror("chdir");
            }
            else if (res == -2) {
                fprintf(stderr,"Numero incorrecto de parametros en comando cd\n");
            }
            continue;
        }
        if (builtin == 2) {
            builtin_jobs(command_line->commands[0].argc,&bg);
            continue;
        }
        if (builtin == 3) {
            builtin_fg(command_line->commands[0].argv,command_line->commands[0].argc,&fg,&bg);
            continue;
        }
        if (builtin == 4) {
            builtin_umask(command_line->commands[0].argv,command_line->commands[0].argc);
            continue;
        }
        if (fg.npipes == 0) {
            fg.pids[0] = fork();
            if (fg.pids[0] < 0) {
                perror("fork");
                close_pipes(&fg,fg.npipes);
                close_rfiles(&fg);
                free_mem(&fg);
                continue;
            }

            if (fg.pids[0] == 0) {
                signal(SIGINT,SIG_IGN);

                if (fg.rin != -1)
                    dup2(fg.rin,STDIN_FILENO);
                if (fg.rout != -1)
                    dup2(fg.rout,STDOUT_FILENO);
                if (fg.rerr != -1)
                    dup2(fg.rerr,STDERR_FILENO);

                execv(fg.line->commands[0].filename,fg.line->commands[0].argv);
                perror("execv");
                exit(1);
            }else {

            }
        }else {
            for (int n_cmd = 0; n_cmd < fg.line->ncommands; n_cmd++) {
                fg.pids[n_cmd] = fork();
                if (fg.pids[n_cmd] < 0) {
                    error = 1;
                    perror("fork");
                    break;
                }

                if (fg.pids[n_cmd] == 0) {
                    signal(SIGINT,SIG_IGN);

                    // Preparar pipes
                    if (n_cmd == 0) {   // Primer comando
                        // Entrada
                        if (fg.rin != -1)
                            dup2(fg.rin,STDIN_FILENO);
                        // Salida
                        close(fg.pipes[0][0]);
                        dup2(fg.pipes[0][1],STDOUT_FILENO);
                        // Resto
                        for (int n_pipe = 1; n_pipe < fg.line->ncommands-1; n_pipe++) {
                            close(fg.pipes[n_pipe][0]);
                            close(fg.pipes[n_pipe][1]);
                        }
                    }else if (n_cmd == fg.line->ncommands-1) {  // Ultimo comando
                        // Entrada
                        dup2(fg.pipes[fg.line->ncommands-2][0],STDIN_FILENO);
                        close(fg.pipes[fg.line->ncommands-2][1]);
                        //Salida
                        if (fg.rout != -1)
                            dup2(fg.rout,STDOUT_FILENO);
                        //Resto
                        for (int n_pipe = 0; n_pipe < fg.line->ncommands-2; n_pipe++) {
                            close(fg.pipes[n_pipe][0]);
                            close(fg.pipes[n_pipe][1]);
                        }
                    }else { // Comandos intermedios
                        for (int n_pipe = 0; n_pipe < fg.line->ncommands-1; n_pipe++) {
                            if (n_pipe == n_cmd-1) { // Entrada
                                dup2(fg.pipes[n_pipe][0],STDIN_FILENO);
                                close(fg.pipes[n_pipe][1]);
                            }else if (n_pipe == n_cmd) { // Salida
                                close(fg.pipes[n_pipe][0]);
                                dup2(fg.pipes[n_pipe][1],STDOUT_FILENO);
                            }else { // Resto
                                close(fg.pipes[n_pipe][0]);
                                close(fg.pipes[n_pipe][1]);
                            }
                        }
                    }
                    if (fg.rerr != -1)
                        dup2(fg.rerr,STDERR_FILENO);


                    execv(fg.line->commands[n_cmd].filename,fg.line->commands[n_cmd].argv);
                    perror("execv");
                    exit(1);
                }else {

                }
            }
            if (error == 1) {
                free_mem(&fg);
                close_pipes(&fg,fg.npipes);
                close_rfiles(&fg);
                continue;
            }

        }



        //  Cerrar Pipes
        close_pipes(&fg,fg.npipes);
        close_rfiles(&fg);



        if (fg.line->background == 0) {
            //  Esperar subprocesos
            for (int n = 0; n < fg.line->ncommands; n++)
                waitpid(fg.pids[n], NULL, 0);

            // Liberar Memoria
            free_mem(&fg);
        }else {
            insert_CPList(&bg,&fg);
        }






    }
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
void init_fg(CommandProcess *cpp) {
    cpp->line = NULL;
    cpp->command = NULL;
    cpp->pids = NULL;
    cpp->pipes = NULL;
    cpp->npipes = 0;
    cpp->rin = 0;
    cpp->rout = 0;
    cpp->rerr = 0;
}


int builtin_cd(char **argv, int argc) {
    if (argc > 2)
        return -1;

    int out;
    if (argc == 1)
        out = chdir(getenv("HOME"));
    else
        out = chdir(argv[1]);

    if (out == -1)
        return -2;
    getcwd(pwd,1024);
    return 0;
}
int builtin_jobs(int argc, struct CPList *bg) {
    if (argc > 1)
        return -1;
    if (bg == NULL)
        return -2;

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
    if (bg == NULL)
        return -1;

    long node_index = strtol(argv[1], NULL, 10);
    if (node_index > bg->size)
        return -2;

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


void handler_sigint(int sig) {
    if (sig != SIGINT)
        return;
    if (fg.line == NULL) {
        fprintf(stdout,"\n%s@%s:%s$ ",user,pcname,pwd);
        fflush(stdout);
        return;
    }

    printf("\n");
    for (int n_pid = 0; n_pid < fg.line->ncommands; n_pid++)
        kill(fg.pids[n_pid], SIGTERM);

}