// This program was written by Victor Murta
// It is a simple shell program that takes in command names from stdin
// and executes them
// I pledge that I have neither given nor received unauthorized aid on this assignment
#include "stdio.h"

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "Socket.h"
#include "ToUpper.h" /* definitions shared by client and server */


#include "Socket.h"

#define MAX_ARGS 100
#define MAX_LINE 1024

int isParent = 1;
int numChildProcesses = 0;
ServerSocket welcome_socket;
Socket connect_socket;

void execution_service();
/* 
 * A server program that provides the service of executing shell commands
 * It is implemented using the "daemon-service" model.  In this model,
 * multiple clients can be serviced
 * concurrently by the service.  The server main process is 
 * a simple loop that accepts incoming socket connections, and for 
 * each new connection established, uses fork() to create a child 
 * process that is a new instance of the service process.  This child 
 * process will provide the service for the single client program that 
 * established the socket connection.  
 *
 * Each new instance of the server process accepts input strings from 
 * its client program, converts the characters to upper case and returns 
 * the converted string back to the client.
 *
 * Since the main process (the daemon) is intended to be continuously
 * available, it has no defined termination condition and must be
 * terminated by an external action (Ctrl-C or kill command).
 *
 * The server has one parameter: the port number that will be used
 * for the "welcoming" socket where the client makes a connection.
 */

/* variables to hold socket descriptors */


int main(int argc, char* argv[])
{
  pid_t spid, term_pid; /* pid_t is typedef for Linux process ID */ 
  int chld_status;
  bool forever = true;

  if (argc < 2)
     {
      printf("No port specified\n");
      return (-1);
     }

  /* create a "welcoming" socket at the specified port */
  welcome_socket = ServerSocket_new(atoi(argv[1]));
  if (welcome_socket < 0)
      {
      printf("Failed new server socket\n");
      return (-1);
     }

  /* The daemon infinite loop begins here; terminate with external action*/
    while (forever) {
        /* accept an incoming client connection; blocks the
        * process until a connection attempt by a client.
        * creates a new data transfer socket.
        */
        connect_socket = ServerSocket_accept(welcome_socket);
        if (connect_socket < 0) {
            printf("Failed accept on server socket\n");
            exit (-1);
        }

        spid = fork();  /* create child == service process */
        if (spid == -1){
            perror("fork"); 
            exit (-1);
        }

        if (spid == 0) {
            printf("connected to client");
            execution_service();
            Socket_close(connect_socket);
            exit (0);
        }
        else {
            Socket_close(connect_socket);
            /* reap a zombie every time through the loop, avoid blocking*/
            term_pid = waitpid(-1, &chld_status, WNOHANG);
        }
    }
}



void ctrlCSignalHandler(int signal_number){
    if (isParent == 0 || numChildProcesses == 0) {
        printf("\nExiting Process\n");
        fflush(stdout);
        exit(0);
    }
    fflush(stdout);
}



void parseArgs(char *command, char **argv){
    char *token = strtok(command, " \n");
    *argv++ = token;
    while (token != NULL){
        token = strtok(NULL, " \n");
        *argv++ = token;
    }
}

void execution_service() {
    int i, c, rc;
    int count = 0;
    bool forever = true;
    char line_data[MAX_LINE]; 

    /* will not use the server socket */
    Socket_close(welcome_socket);


    // temporary file used to redirect output
    char filename[50]; 
    sprintf(filename, "tmp%d", getpid()); 
    

    while(forever) {
        bool has_line_ended = false;
        printf("stuff is being written to the file \n");
        while (!has_line_ended) { 
            for (i = 0; i < MAX_LINE; i++) {
                c = Socket_getc(connect_socket);
                putchar(c);
                if (c == EOF) {
                    fprintf(stderr, "Socket_getc EOF or error\n"); 
                    return; /* assume socket EOF ends service for this client */           
                }
                else {
                    line_data[i] = c;
                }
                if (c == '\n') {
                    has_line_ended = true;
                    line_data[i+1] = '\0';
                    break;
                }
            }
        }

        // case where buffer overflows, don't run anything
        if (line_data[strlen(line_data) -1] != '\n') {
            fprintf(stderr, "Character limit was exceeded, this command was not run\n");

            // skip to the end of this whole ass line of code
            char c;
            while ((c = fgetc(stdin)) != '\n' && c != EOF);
        }

        // valid input
        else {
            fprintf(stderr, "Valid input \n");
            isParent = fork();

            // child process
            if (!isParent){
                FILE *fp = freopen(filename, "w+", stdout);        
                char *argv[MAX_ARGS];
                parseArgs(line_data, argv);
                
                // check if the command actually exists
                struct stat file_info;
                if (stat(argv[0], &file_info) < 0){
                    // if we were unable to find the command and a file path was given
                    if (strchr(argv[0], '/') != NULL){
                        fprintf(stderr, "\nUnable to find command\n");
                        exit(0);
                    }

                    // search the path for it;
                    char *paths = getenv("PATH");
                    char *potential_path = strtok(paths, ":");
                    char potential_full_path[MAX_LINE];
                    // concatenate the path and the command name
                    snprintf(potential_full_path, sizeof(potential_full_path), "%s%s", potential_path, argv[0]);

                    // test every path
                    while (stat(potential_full_path, &file_info) < 0 && (potential_path = strtok(NULL, ":")) != NULL){
                        snprintf(potential_full_path, sizeof(potential_full_path), "%s%c%s", potential_path, '/', argv[0]);
                        // printf("%s",potential_full_path);
                    }

                    //all paths failed
                    if (potential_path == NULL){
                        fprintf(stderr, "\nUnable to find command\n");
                        exit(0);
                    // found it!
                    } else {
                        argv[0] = potential_full_path;
                    }
                }

                printf("executing command %s\n", argv[0]);
                int ok = execv(argv[0], argv);
                if (ok < 0) {
                    fprintf(stderr, "Error executing command: %s\n", strerror( errno ));                   
                }
                fflush(stdout);
                fclose(fp);
                exit(0);
            }


            // parent process 
            else if (isParent > 0){
                numChildProcesses++;
                wait(NULL);
                numChildProcesses--;
                FILE *read_handle = fopen(filename, "r");
                char line[MAX_LINE];
                while (fgets(line, MAX_LINE, read_handle) != NULL) {
                    char c = line[0];
                    for (int i = 1; i < strlen(line) + 1; i++){
                        rc = Socket_putc(c, connect_socket);
                        putchar(c);
                        if (rc == EOF) {
                            printf("Socket_putc EOF or error\n");             
                            return;  /* assume socket EOF ends service for this client */
                        }
                        c = line[i];
                    }                   
                } 
                fclose(read_handle);
            }

            // parent process handles the error
            else {
                fprintf(stderr, "Error forking child: %s\n", strerror( errno ));
            }
        }
        // remove(filename);
    }
} /* end while loop of the service process */