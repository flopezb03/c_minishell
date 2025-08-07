#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include "lib/parser.h"
#include "command_process.h"
#include "builtin_commands.h"



// Constantes
#define BUFF_IN 128
#define PWD_SIZE 1024

// Variables Globales

char *user = "user";
char *pcname = "userpc";

CommandProcess fg;
struct CPList bg;

//  Funciones
void handler_sigint(int sig);


int main() {

    char pwd[PWD_SIZE];
    int exiting = 0;
    char buff_in[BUFF_IN];
    tline *command_line = NULL;
    int error = 0;
    int builtin = -1;


    signal(SIGINT, handler_sigint);
    getcwd(pwd,PWD_SIZE);

    while (exiting == 0) {
        error = 0;
        builtin = -1;
        buff_in[0] = '\0';
        command_line = NULL;
        init_cp(&fg);
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
        if (builtin == -1){ // Comando externo
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
        if (builtin != -1){     // Comandos internos
            int res;

            if (builtin == 0) {     // exit
                exiting = 1;
            }else if (builtin == 1) {   // cd
                res = builtin_cd(command_line->commands[0].argv,command_line->commands[0].argc,pwd,PWD_SIZE);
                if (res == -1)
                    fprintf(stderr,"Numero incorrecto de parametros en comando cd\n");
                else if (res == -2)
                    perror("chdir");
            }else if (builtin == 2) {   // jobs
                if (builtin_jobs(command_line->commands[0].argc,&bg) == -1)
                    fprintf(stderr,"Numero incorrecto de parametros en comando jobs\n");
            }else if (builtin == 3) {   // fg
                res = builtin_fg(command_line->commands[0].argv,command_line->commands[0].argc,&fg,&bg);
                if (res == -1)
                    fprintf(stderr,"Numero incorrecto de parametros en comando fg\n");
                else if (res == -2)
                    fprintf(stderr,"Parametro invalido, no se puede convertir a int\n");
            }else if (builtin == 4) {   // umask
                res = builtin_umask(command_line->commands[0].argv,command_line->commands[0].argc);
                if (res == -1)
                    fprintf(stderr,"Numero incorrecto de parametros en comando umask\n");
                else if (res == -2)
                    fprintf(stderr,"Parametro invalido\n");
            }
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
                }
            }
            if (error == 1) {
                free_mem(&fg);
                close_pipes(&fg,fg.npipes);
                close_rfiles(&fg);
                continue;
            }

        }


        //  Cerrar Pipes y redirecciones
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


void handler_sigint(int sig) {
    if (sig != SIGINT)
        return;
    if (fg.line == NULL) {
        char pwd[PWD_SIZE];
        getcwd(pwd,PWD_SIZE);
        fprintf(stdout,"\n%s@%s:%s$ ",user,pcname,pwd);
        fflush(stdout);
        return;
    }

    printf("\n");
    for (int n_pid = 0; n_pid < fg.line->ncommands; n_pid++)
        kill(fg.pids[n_pid], SIGTERM);
}