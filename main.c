#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "lib/parser.h"
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>


typedef struct CommandProcess {
    tline *line;
    pid_t *pids;
    int (*pipes)[2];
    int npipes;
    int rin;
    int rout;
    int rerr;
}CommandProcess;
// Constantes
#define BUFF_IN 128

// Variables Globales

char *user = "user";
char *pcname = "userpc";

//  Funciones

void free_mem(CommandProcess *cpp);
void close_pipes(CommandProcess *cpp, int num_pipes);
int builtin_cd(char **argv, int argc);

int main() {
    char pwd[1024];
    int exiting = 0;
    char buff_in[BUFF_IN];
    tline *command_line = NULL;
    CommandProcess fg;
    int error = 0;
    int builtin = -1;


    while (exiting == 0) {
        error = 0;
        builtin = -1;
        // Prompt
        getcwd(pwd,1024);
        printf("%s@%s:%s$ ",user,pcname,pwd);


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
                builtin = 1;
            else if (strcmp(command_line->commands[0].argv[0],"fg") == 0)
                builtin = 2;
            else if (strcmp(command_line->commands[0].argv[0],"umask") == 0)
                builtin = 3;
        }
        if (builtin == -1){
            fg.line = command_line;

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
        if (fg.npipes == 0) {
            if (fg.line->commands[0].filename == NULL) {
                fprintf(stderr,"El comando no se ha encontrado\n");
            }
            fg.pids[0] = fork();
            if (fg.pids[0] < 0) {
                perror("fork");
                close_pipes(&fg,fg.npipes);
                free_mem(&fg);
                continue;
            }

            if (fg.pids[0] == 0) {
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
                if (fg.line->commands[n_cmd].filename == NULL) {
                    error = 1;
                    fprintf(stderr,"El comando en la posicion %d no se ha encontrado\n",n_cmd);
                    break;
                }

                fg.pids[n_cmd] = fork();
                if (fg.pids[n_cmd] < 0) {
                    error = 1;
                    perror("fork");
                    break;
                }

                if (fg.pids[n_cmd] == 0) {
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
                continue;
            }

        }



        //  Cerrar Pipes
        close_pipes(&fg,fg.npipes);


        //  Esperar subprocesos
        for (int n = 0; n < fg.line->ncommands; n++)
            waitpid(fg.pids[n], NULL, 0);


        // Liberar Memoria
        free_mem(&fg);

    }
}



void free_mem(CommandProcess *cpp) {
    free(cpp->pids);
    free(cpp->pipes);
}
void close_pipes(CommandProcess *cpp, int num_pipes) {
    for (int i = 0; i < num_pipes; i++) {
        close(cpp->pipes[i][0]);
        close(cpp->pipes[i][1]);
    }
}
int builtin_cd(char **argv, int argc) {
    if (argc > 2)
        return -2;
    if (argc == 1)
        return chdir(getenv("HOME"));
    return chdir(argv[1]);
}