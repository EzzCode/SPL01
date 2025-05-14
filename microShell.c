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
int echo(char **args, int arg_count);
int pwd();
int cd(char **args, int arg_count);
char **parse_command(char *input, int *arg_count);
void free_args(char **args, int arg_count);
int execute_external(char **args, int arg_count);
int handle_redirections(char **args, int *arg_count);
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
            status = echo(args, arg_count);

        } else if (strcmp(args[0], "pwd") == 0) {
            status = pwd();

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
            status = execute_external(args, arg_count);
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
// Helper function to handle redirections and modify args array
int handle_redirections(char **args, int *arg_count) {
    int i = 0;
    while (args[i] != NULL) {
        if (strcmp(args[i], "<") == 0) {
            if (args[i+1] == NULL) {
                printf("syntax error near unexpected token `newline'\n");
                return -1;
            }
            char *input_file = args[i+1];
            int stdin_fd = open(input_file, O_RDONLY);
            if (stdin_fd == -1) {
                fprintf(stderr, "cannot access %s: No such file or directory\n", input_file);
                return -1;
            }
            if (dup2(stdin_fd, STDIN_FILENO) == -1) {
                perror("dup2 stdin");
                return -1;
            }
            close(stdin_fd);
            // Remove redirection tokens from args
            for (int j = i; args[j] != NULL; j++) {
                args[j] = args[j+2];
            }
            (*arg_count) -= 2;
            continue;
        }
        else if (strcmp(args[i], ">") == 0) {
            if (args[i+1] == NULL) {
                printf("syntax error near unexpected token `newline'\n");
                return -1;
            }
            char *output_file = args[i+1];
            int stdout_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (stdout_fd == -1) {
                fprintf(stderr, "%s: Permission denied\n", output_file);
                return -1;
            }
            if (dup2(stdout_fd, STDOUT_FILENO) == -1) {
                perror("dup2 stdout");
                return -1;
            }
            close(stdout_fd);
            // Remove redirection tokens from args
            for (int j = i; args[j] != NULL; j++) {
                args[j] = args[j+2];
            }
            (*arg_count) -= 2;
            continue;
        }
        else if (strcmp(args[i], "2>") == 0) {
            if (args[i+1] == NULL) {
                printf("syntax error near unexpected token `newline'\n");
                return -1;
            }
            char *error_file = args[i+1];
            int stderr_fd = open(error_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (stderr_fd == -1) {
                perror("open error file");
                return -1;
            }
            if (dup2(stderr_fd, STDERR_FILENO) == -1) {
                perror("dup2 stderr");
                return -1;
            }
            close(stderr_fd);
            // Remove redirection tokens from args
            for (int j = i; args[j] != NULL; j++) {
                args[j] = args[j+2];
            }
            (*arg_count) -= 2;
            continue;
        }
        i++;
    }
    return 0;
}

// Modified built-in functions to use redirections
int echo(char **args, int arg_count) {
    int error = handle_redirections(args, &arg_count);
    if (error != 0) {
        return -1;
    }
    for (int i = 1; i < arg_count; i++) {
        printf("%s", args[i]);
        if (i < arg_count - 1) {
            printf(" ");
        }
    }
    printf("\n");
    return 0;
}

int pwd() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) {
        printf("%s\n", cwd);
    } else {
        perror("pwd");
    }
    return 0;
}

int cd(char **args, int arg_count) {
    int error = handle_redirections(args, &arg_count);
    if (error != 0) {
        return -1;
    }
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

// Simplified execute_external function
int execute_external(char **args, int arg_count) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        int error = handle_redirections(args, &arg_count);
        if (error != 0) {
            exit(EXIT_FAILURE);
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


