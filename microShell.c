#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include <fcntl.h>

#define PROMPT "pico$ "
#define MAX_ARGS 128

typedef struct {
    char *name;
    char *value;
    int exported;
} ShellVar;

ShellVar *variables = NULL;
int var_count = 0;

// Function declarations
void echo(char **args, int arg_count);
void pwd();
int cd(char **args, int arg_count);
char **parse_command(char *input, int *arg_count);
void free_args(char **args, int arg_count);
int execute_external(char **args);
void substitute_variables(char **args, int arg_count);
void add_or_update_var(const char *name, const char *value, int exported);
const char *get_var_value(const char *name);
void export_var(const char *name);

int microshell_main(int argc, char *argv[]) {
    char *buffer = NULL;
    size_t buffer_size = 0;
    ssize_t bytes_read;
    int status = 0;

    while (1) {
        char cwd[PATH_MAX];
        // Prompt display (disabled for testing)
        if (getcwd(cwd, sizeof(cwd)) && false) {
            printf("%s %s", cwd, PROMPT);
        } else {
            printf(PROMPT);
            fflush(stdout);
        }

        bytes_read = getline(&buffer, &buffer_size, stdin);
        if (bytes_read == -1) {
            break;
        }

        // Strip newline
        if (bytes_read > 0 && buffer[bytes_read - 1] == '\n') {
            buffer[bytes_read - 1] = '\0';
        }

        // Handle assignment line (x=5)
        char *eq_ptr = strchr(buffer, '=');
        if (eq_ptr && buffer[0] != '=' && strchr(buffer, ' ') == NULL) {
            *eq_ptr = '\0';
            const char *name = buffer;
            const char *value = eq_ptr + 1;
            if (*name == '\0' || *value == '\0') {
                printf("Invalid command\n");
            } else {
                add_or_update_var(name, value, 0);
            }
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
            break;
        } else if (strcmp(args[0], "echo") == 0) {
            substitute_variables(args, arg_count);
            echo(args, arg_count);
            status = 0;
        } else if (strcmp(args[0], "pwd") == 0) {
            pwd();
            status = 0;
        } else if (strcmp(args[0], "cd") == 0) {
            substitute_variables(args, arg_count);
            status = cd(args, arg_count);
        } else if (strcmp(args[0], "export") == 0) {
            if (arg_count < 2) {
                printf("export: missing argument\n");
            } else {
                export_var(args[1]);
            }
            status = 0;
        } else {
            substitute_variables(args, arg_count);
            status = execute_external(args);
        }

        free_args(args, arg_count);
    }

    free(buffer);

    // Free all variables
    for (int i = 0; i < var_count; i++) {
        free(variables[i].name);
        free(variables[i].value);
    }
    free(variables);

    return status;
}

// Built-in commands
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
        return -1;
    }
    return 0;
}

// Variable system
void add_or_update_var(const char *name, const char *value, int exported) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            free(variables[i].value);
            variables[i].value = strdup(value);
            if (exported) variables[i].exported = 1;
            return;
        }
    }

    variables = (ShellVar *)realloc(variables, sizeof(ShellVar) * (var_count + 1));
    variables[var_count].name = strdup(name);
    variables[var_count].value = strdup(value);
    variables[var_count].exported = exported;
    var_count++;
}

const char *get_var_value(const char *name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            return variables[i].value;
        }
    }
    return NULL;
}

void export_var(const char *name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            variables[i].exported = 1;
            setenv(variables[i].name, variables[i].value, 1);
            return;
        }
    }
    printf("export: %s: not found\n", name);
}

void substitute_variables(char **args, int arg_count) {
    for (int i = 0; i < arg_count; i++) {
        char *src = args[i];
        char *result = (char*)malloc(strlen(src) * 2 + 1); 
        if (!result) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        int ri = 0;
        for (int si = 0; src[si];) {
            if (src[si] == '$') {
                si++;
                char var_name[256];
                int vi = 0;

                // Read variable name (letters, digits, underscores)
                while (src[si] && (isalnum(src[si]) || src[si] == '_')) {
                    var_name[vi++] = src[si++];
                }
                var_name[vi] = '\0';

                const char *val = get_var_value(var_name);
                if (val) {
                    for (int k = 0; val[k]; k++) {
                        result[ri++] = val[k];
                    }
                }
                // If variable doesn't exist, skip (leave blank)
            } else {
                result[ri++] = src[si++];
            }
        }

        result[ri] = '\0';
        free(args[i]);
        args[i] = strdup(result);
        free(result);
    }
}

// Command parsing and execution
char **parse_command(char *input, int *arg_count) {
    char **args = (char**)malloc(MAX_ARGS * sizeof(char *));
    if (!args) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    *arg_count = 0;
    char *token = strtok(input, " ");
    while (token && *arg_count < MAX_ARGS - 1) {
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
    int stdin_fd = -1, stdout_fd = -1, stderr_fd = -1;
    char *input_file = NULL;
    char *output_file = NULL;
    char *error_file = NULL;
    
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        // Child parses redirections and redirects immediately
        int i = 0;
        while (args[i] != NULL) {
            if (strcmp(args[i], "<") == 0) {
            if (args[i+1] == NULL) {
                printf("syntax error near unexpected token `newline'\n");
                exit(EXIT_FAILURE);
            }
            input_file = args[i+1];
            stdin_fd = open(input_file, O_RDONLY);
            if (stdin_fd == -1) {
                fprintf(stderr, "cannot access %s: No such file or directory\n", input_file);
                exit(EXIT_FAILURE);
            }
            if (dup2(stdin_fd, STDIN_FILENO) == -1) {
                perror("dup2 stdin");
                exit(EXIT_FAILURE);
            }
            close(stdin_fd);
            // Remove redirection tokens from args
            for (int j = i; args[j] != NULL; j++) {
                args[j] = args[j+2];
            }
            continue;
            }
            else if (strcmp(args[i], ">") == 0) {
            if (args[i+1] == NULL) {
                printf("syntax error near unexpected token `newline'\n");
                exit(EXIT_FAILURE);
            }
            output_file = args[i+1];
            stdout_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (stdout_fd == -1) {
                perror("open output file");
                exit(EXIT_FAILURE);
            }
            if (dup2(stdout_fd, STDOUT_FILENO) == -1) {
                perror("dup2 stdout");
                exit(EXIT_FAILURE);
            }
            close(stdout_fd);
            // Remove redirection tokens from args
            for (int j = i; args[j] != NULL; j++) {
                args[j] = args[j+2];
            }
            continue;
            }
            else if (strcmp(args[i], "2>") == 0) {
            if (args[i+1] == NULL) {
                printf("syntax error near unexpected token `newline'\n");
                exit(EXIT_FAILURE);
            }
            error_file = args[i+1];
            stderr_fd = open(error_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (stderr_fd == -1) {
                perror("open error file");
                exit(EXIT_FAILURE);
            }
            if (dup2(stderr_fd, STDERR_FILENO) == -1) {
                perror("dup2 stderr");
                exit(EXIT_FAILURE);
            }
            close(stderr_fd);
            // Remove redirection tokens from args
            for (int j = i; args[j] != NULL; j++) {
                args[j] = args[j+2];
            }
            continue;
            }
            i++;
        }

        // Exported vars only
        for (int i = 0; i < var_count; i++) {
            if (variables[i].exported) {
                setenv(variables[i].name, variables[i].value, 1);
            }
        }

        execvp(args[0], args);
        printf("%s: command not found\n", args[0]);
        exit(EXIT_FAILURE);
    } else {
        int status;
        waitpid(pid, &status, 0);
        return WEXITSTATUS(status);
    }
}
