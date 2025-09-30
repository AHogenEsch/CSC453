/* create a pipe for interprocess communication;
• fork() two children, one for each program;
• set up each child’s file descriptors appropriately;
• exec() the appropriate program in each child;
• terminate (the parent) with zero status on success, nonzero on failure;
• do appropriate error checking;
• check the exit status of the child processes and terminate with nonzero status in case of
error, and;
• be appropriately documented */
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


void waitForChild(pid_t cpid) {
    int status;
    
    // The last argument '0' means block until the child changes state (terminates).
    pid_t wpid = waitpid(cpid, &status, 0); 

    if (wpid == -1) {
        perror("waitpid error");
        exit(EXIT_FAILURE);
    }

    // Check if the child exited normally
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        // Check for error
        if (exit_code != 0) {
            fprintf(stderr, "  ~~~ ERROR: Child PID %d exited with failure! ~~~\n", wpid);
        }
    }
}


int main (){
    pid_t cpid1 = 0;
    pid_t cpid2 = 0;
    int pipeFD[2];

    // create pipe
    if (pipe(pipeFD) == -1){
        perror("pipe failed");
        exit(EXIT_FAILURE);
    }
    
    // First fork for ls
    cpid1 = fork();
    
    if (cpid1 < 0) {
        // fork() failure
        perror("fork failed");
        close(pipeFD[0]); // close read end of pipe
        close(pipeFD[1]); // close write end of pipe
        exit(EXIT_FAILURE);
    }
    else if(!cpid1){
        // child 1   
        close(pipeFD[0]); // close read end of pipe
            
        // dupe STDIN_FILENO to the write end of the pipe
        if (dup2(pipeFD[1], STDOUT_FILENO) == -1) { 
            perror("dup2 STDOUT_FILENO in child 1 failed");
            close(pipeFD[1]);
            exit(EXIT_FAILURE);
        }
        close(pipeFD[1]); // closing original write end
        // exec for ls
        execlp("ls", "ls", NULL);
        // if this point is reached, execlp has failed. 
        perror("execlp");
        close(pipeFD[1]); // close write end of pipe
        exit(EXIT_FAILURE);
        
    }
    else{
        // parent
        cpid2 = fork();
        if(cpid2 < 0 ){
            // fork() failure
            perror("fork failed");
            close(pipeFD[0]); // close read end of pipe
            close(pipeFD[1]); // close write end of pipe
            exit(EXIT_FAILURE);
        }
        if(!cpid2){
            FILE *filePointer;
            int outFD;
            // child 2
            close(pipeFD[1]); // close the write end of the pipe
            // dupe STDIN_FILENO to the read end of the pipe 
            if (dup2(pipeFD[0], STDIN_FILENO) == -1) { 
                perror("dup2 STDIN_FILENO in child 2 failed");
                close(pipeFD[0]);
                exit(EXIT_FAILURE);
            }
            close(pipeFD[0]); // closing original read end

            // open outfile
            filePointer = fopen("outfile", "w");
            if (filePointer == NULL) {
                perror("outfile");
                close(pipeFD[0]); // close read end of pipe
                close(pipeFD[1]); // close write end of pipe
                exit(EXIT_FAILURE);
            }
            outFD = fileno(filePointer);
            // make the outfile STDOUT_FILENO
            if (dup2(outFD, STDOUT_FILENO) == -1) {
                perror("dup2 STDOUT_FILENO in child 2 failed");
                fclose(filePointer);
                exit(EXIT_FAILURE);
            }
            if (outFD != STDOUT_FILENO) { // check to avoid closing STDOUT if it was 1
                fclose(filePointer);
            }
            // exec for sort
            execlp("sort", "sort", "-r", NULL);
            // if this point is reached, execlp has failed. 
            perror("execlp");
            close(pipeFD[0]); // close read end of pipe
            fclose(filePointer); // closing outfile
            exit(EXIT_FAILURE);
        }
        else{
            //parent    
            
            close(pipeFD[0]); // close read end of pipe
            close(pipeFD[1]); // close write end of pipe

            // wait for both children, check for errors in either child
            if (cpid1 > 0) waitForChild(cpid1);
            if (cpid2 > 0) waitForChild(cpid2); 
            exit(EXIT_SUCCESS);

        }

    }

    
}

