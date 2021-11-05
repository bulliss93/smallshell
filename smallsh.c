#define _POSIX_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h> 
#include <sys/wait.h>
#include "smallsh.h"
#include <fcntl.h>
#include <time.h>
#include <features.h>

// global variable for interaction with signals
int fg = 0;

//https://stackoverflow.com/questions/7155810/example-of-waitpid-wnohang-and-sigchld/7255053 for zombie killer inspiration
void userInterface(){
    
    //initialization of variables
    char input[2049];
    int child_array[200] = {0};
    int child = 0;
    int status_code;
    int exit_code=0;
    int terminate = 0;

    
    //main loop
    while(1){ 

        //check on background processes and remove from pid array if they terminated
            for(int m=0; m<200; m++){
                if(child_array[m] != 0){
                    child = waitpid(child_array[m], &status_code, WNOHANG);
                    if(child != 0){
                        if(child == -1){
                            //perror("waitpid");
                        }
                        else{
                        printf("Process %i terminated. Exit code %i\n" , child, status_code);
                        }
                        child_array[m] = 0;
                    }
                }
            }
        // PROMPT     
        int size=0;
        printf(": ");
        fgets(input, 2049, stdin);
        size = strlen(input);
        input[size-1] = '\0';

        // expand variable $$
        if(strstr(input, "$$") != NULL){
            char* dummystring = calloc(2049, sizeof(char));
            char place[2] = {'\0'};
            char* place_ptr = &place[0];
            int a=0;
            char* pid_num = calloc(20, sizeof(char));
            snprintf(pid_num, 20, "%i", getpid());

            while(a < strlen(input)){
                place[0] = input[a];

                if(place[0] == '$'){
                        // IF SUCCESSIVE CHARACTER IS $ REPLACE WITH PID NUMBER
                        if(input[a+1] =='$'){
                            //CHECK IF INSIDE BOUNDS OF INPUT
                            if(a+1 < strlen(input)){
                                dummystring = strcat(dummystring, pid_num);
                                a=a+2;
                                }
                            // IF LAST CHARACTER THEN REPLACE WITH CHARACTER
                            else{
                                dummystring = strcat(dummystring, place_ptr);
                                a=a+1;
                                }
                            }
                        // IF SOLO $ COPY AS NORMAL
                        else{
                            dummystring = strcat(dummystring, place_ptr);
                            a=a+1;
                            }
                        }
                    // IF CHARACTER IS NOT $ THEN SWAP OVER CHARACTER
                    else{
                        dummystring = strcat(dummystring, place_ptr);
                        a=a+1;
                        }
                }
                

            strcpy(input, dummystring);
        }

        // REMOVES & FROM INPUT IF IN FOREGROUND MODE
        if(fg%2 == 1 && input[strlen(input)-1] == '&'){
            input[strlen(input)-1] = '\0';
        }
        
        // CHECK IF COMMAND IS EXIT
        if(strcmp(input, "exit") == 0){
            for(int w=0; w<200; w++){
                if(child_array[w] != 0){
                    terminate = kill(child_array[w], SIGKILL);
                    if(terminate == -1){
                        perror("kill");
                    }
                    child_array[w] = 0;
                }
            }
            exit(0);
        }
        
        // CHECK IF INPUT IS COMMENT OR BLANK LINE
        else if((input[0] == '#') || (strcmp(input, "") == 0)){
            }

        // PROCESSS COMMAND AND IF BACKGROUND PROCESS SAVE CHILD PID IN ARRAY OTHERWISE RETURN EXIT CODE
        else{
            if(input[strlen(input)-1] == '&'){
                int process_pid = processCommand(input, child_array, exit_code);
                if(process_pid != 0){
                    // check for empty pid slot in array
                    for(int j=0; j<200; j++){
                        if(child_array[j] == 0){
                            child_array[j] = process_pid;
                            break;
                        }
                        else if(j == 199 && child_array[j] != 0){
                            printf("Over 200 processes running. Kill some processes.");
                            }
                        }
                    }
                }
            else{
                exit_code = processCommand(input, child_array, exit_code);
            }
        }

        fflush(stdin);
        
    }
}

int processCommand(char input[2049], int* ptr, int exit_code) {
    // INITIALIZE VARIABLES AND MEMORY ALLOCATIONS
    struct command *parse = malloc(sizeof(struct command));
    parse->cmd = calloc(100, sizeof(char));
    parse->inputFile = calloc(50, sizeof(char));
    parse->outputFile = calloc(50, sizeof(char));
    parse->arg = calloc(2000, sizeof(char));
    parse->background = 0;
    char* cmd = input;
    char* token;
    int arg_index = 0;
    int ret;

    // CHECK IF BACKGROUND PROCESS
    if(input[strlen(input)-1] == '&') {
        if(strcmp(parse->cmd, "cd") == 0 || strcmp(parse->cmd, "status") == 0){
        }
        else{
            parse->background = 1;
        }
        // if foreground mode is activated reset background flag to 0
        if(fg%2 == 1){
            parse->background = 0;
        }
    }

    // INITIALIZE TOKEN GENERATION
    token = strtok(cmd, " ");
    
    // TOKEN AQUISITION AND PROCESSING INTO COMMAND STRUCT
    while(token != NULL){
        // INPUT COMMAND 
        if(arg_index == 0){
            strcpy(parse->cmd, token); 
            arg_index++;    
            token = strtok(NULL, " "); 
        }
        // INPUT FILE NAME
        else if(strcmp(token, "<") == 0){  
            token = strtok(NULL, " ");
            strcpy(parse->inputFile, token);
            token = strtok(NULL, " ");
            }
        // OUTPUT FILE NAME
        else if(strcmp(token, ">") == 0){ 
            token = strtok(NULL, " ");
            strcpy(parse->outputFile, token);
            token = strtok(NULL, " ");
            }
        // LIST OF ARGUMENTS
        else{
            if(arg_index > 512){
                printf("Too many arguments.");
                return ret;
                }
            // SKIP & if background mode is on
            else if((parse->background == 1) && strcmp(token,"&") == 0){
                token = strtok(NULL, " ");
                }
            // copy into arg array
            else{
                strcat(parse->arg, token);
                strcat(parse->arg, " ");
                arg_index++;
                token = strtok(NULL, " ");
                }
            }
        }

    // CLEAN UP CONTENTS
    int size = strlen(parse->arg);
        parse->arg[size-1] = '\0';
 
    // CHECK FOR BUILT IN COMMAND CD
    if(strcmp(parse->cmd, "cd") == 0){
        char pwd[100];
        int chdir_val=0;
        // IF NO ARGS PASSED
        if(strlen(parse->arg) == 0){
            chdir_val = chdir(getenv("HOME")); 
            if(chdir_val == -1){
                printf("%s\n", strerror(errno));
                }
            }
        // IF ARGS PASSED
        else{
            chdir_val = chdir(parse->arg);
            if(chdir_val == -1){
                
                printf("%s\n", strerror(errno));
                }
            }
        return ret;
    }

    // CHECK FOR BUILT IN STATUS COMMAND
    else if(strcmp(parse->cmd, "status") == 0){
        printf("Exit Status %i\n", exit_code);
        return 0;
        }

    // OTHERWISE FORK AND EXECUTE EXISTING PROGRAM
    else{
        // initilize variables
        pid_t pid = fork();
        int status;
        int result;
        char* new_args[513];
        int devNull = open("/dev/null", O_RDWR); // open file descriptor
        new_args[0] = calloc(strlen(parse->cmd), sizeof(char));
        strcpy(new_args[0], parse->cmd);
        char* token = strtok(parse->arg, " ");
        // turn long string of command into array of array of chars for execvp
        for(int k=1; k<512; k++){
            if(token == NULL){
                new_args[k] = NULL;
            }
            else{
            new_args[k] = calloc((strlen(token)), sizeof(char));
            strcpy(new_args[k], token);
            token = strtok(NULL, " ");
            }
        }
        // set final array component to null for execvp
        new_args[512] = NULL;
        // create array for write systemcall message with background pid
        char* bm_ptr = calloc(16, sizeof(char));
        bm_ptr = strcat(bm_ptr, "Background PID:");
        char* pid_ptr = calloc(8, sizeof(char));
        snprintf(pid_ptr, 8, "%i", getpid());
        bm_ptr = strcat(bm_ptr, pid_ptr);

        // CHECK IF PARENT OR CHILD OR FAILED
        switch(pid){
            case -1:
		        perror("fork()\n");
		        exit(1);
		        break;
            case 0:
                // In the child process
                
                // setup input file redirection
                if(strlen(parse->inputFile) != 0){
                    int input_fd = open(parse->inputFile, O_RDONLY, 0777);
                    if(input_fd == -1){
                        perror("input file");
                        }
                    else{
                    int ret_fd_in = dup2(input_fd, 0);
                    }
                    close(input_fd);
                    }
                // if in background and no files specified read from dev null
                else if(parse->background == 1 && strlen(parse->inputFile) == 0){
                    result = dup2(devNull, 0);
                    }
                else{
                    result = dup2(0, 0);
                    if(result == -1){
                            perror("Input dup2");
                        }
                    }
                // OUTPUT REDIRECTION
                   // file output redirection      
                if(strlen(parse->outputFile) != 0){
                    int output_fd = open(parse->outputFile, O_RDWR | O_CREAT | O_TRUNC, 0777);
                    if(output_fd == -1){
                        perror("output file");
                        }
                    int ret_fd_out = dup2(output_fd, 1);
                    close(output_fd);
                    if(ret_fd_out == -1){
                        perror("dup2out");
                        }
                    }
                    // if in background and no file specified output to dev null
                else if(parse->background == 1 && strlen(parse->outputFile) == 0){
                        result = dup2(devNull, 1);
                    }

                else{
                    result = dup2(1, 1);
                    if(result == -1){
                            perror("Input dup2");
                        }
                    }
                // execute new program
		        execvp(new_args[0], new_args);
		        // exec only returns if there is an error
		        perror("execvp");
		        exit(2);
		        break;

            default:
                if(parse->background == 1){
                    // if in background dont wait for child to complete                  
                    ret = waitpid(pid, &status, WNOHANG);
                    if(ret != 0){
                        // if it did complete then return pid and status
                        printf("Process %i terminated. Exit code %i", ret, status);
                    }
                    else{
                        // otherwise write the pid num and return the pid for storing in the array of bg pids
                        write(STDOUT_FILENO, bm_ptr, 24);
                        ret = pid;
                    }
                    break;
                }
                else{
                    // if foreground process wait for child termination
                    pid = waitpid(pid, &status, 0);
                    if(pid != -1){
                        // if successful return status
                        ret = status;
                    }
		            else{
                        // otherwise return failure code
                        ret = -1;
                    }
		            break;
                }
	        } 
        }
    return ret;
}

// signal handler for sigint
void handle_SIGINT(int signum){
    // if main chell then catch the signal and do nothing
    if(getppid() == 1){
        write(STDOUT_FILENO, "SIGINT caught.\n", 15);
        return;
        }
    }
    
// signal handler for sigtstp
void handle_SIGTSTP(int signum){
    //  increment global foreground variable
    fg=fg+1;
    // if foreground global var is even then specify normal mode, if its odd specify foreground mode
    if(fg%2 == 0){
        write(STDOUT_FILENO, "Entering normal mode.\n", 23);
        }
    else{
        write(STDOUT_FILENO, "Entering foreground mode.\n", 26);
        }
    }
