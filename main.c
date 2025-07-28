#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "lib/parser.h"

// Constantes
#define BUFF_IN 128

// Variables Globales
char pwd[1024];
char* user = "user";
char* pcname = "userpc";
int exiting = 0;

int main() {
    char buff_in[BUFF_IN];
    while (exiting == 0) {
        // Prompt
        getcwd(pwd,1024);
        printf("%s@%s:%s$ ",user,pcname,pwd);

        // Entrada
        fgets(buff_in,BUFF_IN,stdin);

        // Analisis

        // Ejecucion

    }
}