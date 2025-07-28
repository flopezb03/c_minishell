#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "lib/parser.h"
#include <sys/wait.h>
#include <fcntl.h>

// Constantes
#define BUFF_IN 128

// Variables Globales

char* user = "user";
char* pcname = "userpc";


int main() {
    char pwd[1024];
    int exiting = 0;
    char buff_in[BUFF_IN];
    tline* command_line = NULL;
    pid_t pid;


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
        }else {

        }


        // Ejecucion
        pid = fork();
        if (pid < 0) {
            //ERROR
        }else if (pid == 0) {
            if (command_line->redirect_input != NULL) {
                FILE* in = fopen(command_line->redirect_input, "r");
                if (in == NULL) {
                    //ERROR
                }else {
                    dup2(fileno(in),0);
                }
            }
            if (command_line->redirect_output != NULL) {
                FILE* out = fopen(command_line->redirect_output, "w");  //  MODO?
                if (out == NULL) {
                    //  ERROR
                }else {
                    dup2(fileno(out),1);
                }
            }
            if (command_line->redirect_error != NULL) {
                FILE* err = fopen(command_line->redirect_error, "w");
                if (err == NULL) {
                    // ERROR
                }else {
                    dup2(fileno(err),2);
                }
            }

            execv(command_line->commands[0].filename,command_line->commands[0].argv);
        }else {
            waitpid(pid,NULL,0);
        }

    }
}