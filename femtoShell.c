#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROMPT "femto$ "

void echo(char *input)
{
    char *message = input + 5;
    printf("%s\n", message);
}
int femtoshell_main(int argc, char *argv[]) {
    // Write your code here
    // Do not write a main() function. Instead, deal with femtoshell_main() as the main function of your program.
    char *buffer = NULL;
    size_t buffer_size = 0;
    ssize_t bytes_read;
    int status = 0;
    
    setbuf(stdout, NULL);

    while (1) {

        printf(PROMPT);
        

        bytes_read = getline(&buffer, &buffer_size, stdin);
        if (bytes_read == -1) {
            free(buffer);
            return status;
        }
        
        if (bytes_read > 0 && buffer[bytes_read - 1] == '\n') {
            buffer[bytes_read - 1] = '\0';
        }

        
        if (strlen(buffer) == 0) {
            continue;
        }
        
        if (strcmp(buffer, "exit") == 0) {
            printf("Good Bye\n");
            free(buffer);
            return 0;
        } else if (strncmp(buffer, "echo ", 5) == 0) {
            echo(buffer);
            status = 0;
        } else {
            printf("Invalid command\n");
            status = -1;
        }
    }
    free(buffer);
    return 0;
}