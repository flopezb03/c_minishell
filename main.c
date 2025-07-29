#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "lib/parser.h"
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>


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


int main() {
    char pwd[1024];
    int exiting = 0;
    char buff_in[BUFF_IN];
    tline *command_line = NULL;
    pid_t pid;
    CommandProcess fg;


    while (exiting == 0) {
        // Prompt
        getcwd(pwd,1024);
        printf("%s@%s:%s$ ",user,pcname,pwd);

        // Entrada
        fgets(buff_in,BUFF_IN,stdin);

        // Analisis
        command_line = tokenize(buff_in);
        if (command_line == NULL) {
            //  ERROR
            continue;
        }else {
            fg.line = command_line;
            fg.pids = calloc(command_line->ncommands,sizeof(pid_t));
            // Reservar y abrir pipes
            fg.npipes = command_line->ncommands-1;
            fg.pipes = calloc(fg.npipes,sizeof(int[2]));
            for (int n_pipe = 0; n_pipe < fg.npipes; n_pipe++)
                if (pipe(fg.pipes[n_pipe]) == -1) {
                    //ERROR
                }

            //  Comprobacion de ficheros de redirecciones
            if (fg.line->redirect_input == NULL)
                fg.rin = -1;
            else {
                fg.rin = open(fg.line->redirect_input,O_RDONLY);
                if (fg.rin == -1) {
                    //  ERROR
                }
            }
            if (fg.line->redirect_output == NULL)
                fg.rout = -1;
            else {
                fg.rout = open(fg.line->redirect_output,O_WRONLY | O_CREAT);
                if (fg.rout == -1) {
                    //  ERROR
                }
            }
            if (fg.line->redirect_error == NULL)
                fg.rerr = -1;
            else {
                fg.rerr = open(fg.line->redirect_error,O_WRONLY | O_CREAT | O_APPEND);
                if (fg.rerr == -1) {
                    //  ERROR
                }
            }
        }



        // Ejecucion
        if (fg.npipes == 0) {
            fg.pids[0] = fork();
            if (fg.pids[0] < 0) {
                // ERROR
            }else if (fg.pids[0] == 0) {
                if (fg.rin != -1)
                    dup2(fg.rin,STDIN_FILENO);
                if (fg.rout != -1)
                    dup2(fg.rout,STDOUT_FILENO);
                if (fg.rerr != -1)
                    dup2(fg.rerr,STDERR_FILENO);

                execv(fg.line->commands[0].filename,fg.line->commands[0].argv);
            }else {

            }
        }else {
            for (int n_cmd = 0; n_cmd < fg.line->ncommands; n_cmd++) {
                fg.pids[n_cmd] = fork();
                if (fg.pids[n_cmd] < 0) {
                    //ERROR
                }else if (fg.pids[n_cmd] == 0) {
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
                }else {

                }
            }
        }




        for (int n_pipe = 0; n_pipe < fg.npipes; n_pipe++) {
            close(fg.pipes[n_pipe][0]);
            close(fg.pipes[n_pipe][1]);
        }
        for (int n = 0; n < fg.line->ncommands; n++)
            waitpid(fg.pids[n], NULL, 0);


    }
}