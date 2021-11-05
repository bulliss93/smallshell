#ifndef SMALLSH
#define SMALLSH

struct command {
    char* cmd;
    char* arg;
    char* inputFile;
    char* outputFile;
    int background;
};

char** args_to_array(char* cmd, char* args);

void userInterface();

int processCommand(char input[2049], int child_array[200], int exit_code);

void handle_SIGINT(int signum);

void handle_SIGTSTP(int signum);

#endif 

