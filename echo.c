int echo_main(int argc, char *argv[]) {
    // Write your code here
    // Do not write a main() function. Instead, deal with echo_main() as the main function of your program.
    if (argc < 2) {
        write(1, "\n", 1);
        return 0;
    }
    
    for (int i = 1; i < argc; i++) {
        write(1, argv[i], strlen(argv[i]));
        
        if (i < argc - 1) {
            write(1, " ", 1);
        }
    }
    
    write(1, "\n", 1);
    return 0;
}