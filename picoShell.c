#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#define PROMPT "pico$ "
#define MAX_ARGS 128

void echo(char **args, int arg_count);
void pwd();
int cd(char **args, int arg_count);
char **parse_command(char *input, int *arg_count);
void free_args(char **args, int arg_count);
int execute_external(char **args);
int picoshell_main(int argc, char *argv[]) {
    // Write your code here
    // Do not write a main() function. Instead, deal with picoshell_main() as the main function of your program.
    
    char *buffer = NULL;
    size_t buffer_size = 0;
    ssize_t bytes_read;
    int status = 0;

    while (1) {
        char cwd[PATH_MAX];
        //turned off for Testcases
        if (getcwd(cwd, sizeof(cwd)) && false) {
            printf("%s %s", cwd, PROMPT);
        } else {
            printf(PROMPT);
        }
        
        bytes_read = getline(&buffer, &buffer_size, stdin);
        if (bytes_read == -1) {
            free(buffer);
            return status;
        }
        
        // Remove newline
        if (bytes_read > 0 && buffer[bytes_read - 1] == '\n') {
            buffer[bytes_read - 1] = '\0';
        }

        // Skip empty input
        if (strlen(buffer) == 0) {
            continue;
        }

        // Parse command
        int arg_count = 0;
        char **args = parse_command(buffer, &arg_count);

        if (arg_count == 0) {
            free_args(args, arg_count);
            continue;
        }

        if (strcmp(args[0], "exit") == 0) {
            printf("Good Bye\n");
            free_args(args, arg_count);
            free(buffer);
            return 0;
        } else if (strcmp(args[0], "echo") == 0) {
            echo(args, arg_count);
            status = 0;
        } else if (strcmp(args[0], "pwd") == 0) {
            pwd();
            status = 0;
        } else if (strcmp(args[0], "cd") == 0) {
            status = cd(args, arg_count);
            
        } else {
            status = execute_external(args);
        }

        free_args(args, arg_count);
    }

    free(buffer);
    return 0;
}

void echo(char **args, int arg_count) {
    for (int i = 1; i < arg_count; i++) {
        printf("%s", args[i]);
        if (i < arg_count - 1) {
            printf(" ");
        }
    }
    printf("\n");
}

void pwd() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) {
        printf("%s\n", cwd);
    } else {
        perror("pwd");
    }
}

int cd(char **args, int arg_count) {
    if (arg_count < 2) {
        fprintf(stderr, "cd: missing argument\n");
        return -1;
    }
    if (chdir(args[1]) != 0) {
        printf("cd: %s: No such file or directory\n", args[1]);
        //perror("cd");
        return -1;
    }    return 0;
}

char **parse_command(char *input, int *arg_count) {
    char **args = (char**)malloc(MAX_ARGS * sizeof(char *));
    if (!args) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    *arg_count = 0;
    char *token = strtok(input, " ");
    while (token != NULL && *arg_count < MAX_ARGS - 1) {
        args[*arg_count] = strdup(token);
        if (!args[*arg_count]) {
            perror("strdup");
            exit(EXIT_FAILURE);
        }
        (*arg_count)++;
        token = strtok(NULL, " ");
    }
    args[*arg_count] = NULL;

    return args;
}

void free_args(char **args, int arg_count) {
    for (int i = 0; i < arg_count; i++) {
        free(args[i]);
    }
    free(args);
}

int execute_external(char **args) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        // Child process
        execvp(args[0], args);
        printf("%s: command not found\n", args[0]);
        //perror("execvp");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        return WEXITSTATUS(status);
    }
}