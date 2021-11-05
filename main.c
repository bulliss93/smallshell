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

#include "smallsh.h"


int main(){

    struct sigaction sigint;
    struct sigaction sigtstp;

    sigint.sa_handler = handle_SIGINT;
    sigint.sa_flags = SA_RESTART;
    
    sigtstp.sa_handler = handle_SIGTSTP;
    sigtstp.sa_flags = SA_RESTART;

    sigaction(SIGINT, &sigint, NULL);
    sigaction(SIGTSTP, &sigtstp, NULL);

    userInterface();
    
    return 0;
}