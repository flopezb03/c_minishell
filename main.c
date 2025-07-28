#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "lib/parser.h"
#include <sys/wait.h>

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
            execv(command_line->commands[0].filename,command_line->commands[0].argv);
        }else {
            waitpid(pid,NULL,0);
        }

    }
}