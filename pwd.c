#define PATH_MAX 4096
int pwd_main() {
    // Write your code here
    // Do not write a main() function. Instead, deal with pwd_main() as the main function of your program.
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)))
        {
            cwd[PATH_MAX] = '\n';
            write(1, cwd, PATH_MAX + 1);
        }
        else
        {
            const char *error_msg = "error\n";
            write(2, error_msg, 6);
            exit(-1);
        }
        return 0;
}