/* 
 * A client program that uses the service of executing shell commands
 *
 * This program reads input lines from stdin and sends them to
 * a server which forks itself and executes the command in the child. 
 * The server then returns the output to the client.
 *
 * The client has two positional parameters: (1) the DNS host name 
 * where the server program is running, and (2) the port number of 
 * the server's "welcoming" socket.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "Socket.h"

#define MAX_LINE 1024

int main(int argc, char* argv[])
{
    int i, c, rc;
    int count = 0;

    char line_data[MAX_LINE]; 

    /* variable to hold socket descriptor */
    Socket connect_socket;

    if (argc < 3){
        printf("No host and port\n");
        return (-1);
    }

    /* connect to the server at the specified host and port;
    * blocks the process until the connection is accepted
    * by the server; creates a new data transfer socket.
    */
    connect_socket = Socket_new(argv[1], atoi(argv[2]));
    if (connect_socket < 0){
        printf("Failed to connect to server\n");
        return (-1);
    }

    /* get a string from stdin up to and including 
    * a newline or to the maximim input line size.  
    * Continue getting strings from stdin until EOF.
    */ 
    printf("%%");
    fflush( stdout );


    while ((fgets(line_data, sizeof(line_data), stdin) != NULL)) {

        count = strlen(line_data) + 1; /* count includes '\0' */
        printf("line received\n");

        /* send the characters of the input line to the server
        * using the data transfer socket.
        */
        for (i = 0; i < count; i++) {
            c = line_data[i];
            rc = Socket_putc(c, connect_socket);
            if (rc == EOF) {
                printf("Socket_putc EOF or error\n");             
                Socket_close(connect_socket);
                exit (-1);  /* assume socket problem is fatal, end client */
            }
        }

        /* receive the converted characters for the string from
        * the server using the data transfer socket.  
        */ 
        for (i = 0; i < MAX_LINE; i++) {
            c = Socket_getc(connect_socket);
            if (c == EOF){
                printf("Socket_getc EOF or error\n");             
                Socket_close(connect_socket);
                exit (-1);  /* assume socket problem is fatal, end client */
            }

            else {
                line_data[i] = c;
                if (line_data[i] == 0x03){
                    // if we want to handle errors, we could 
                    // get more info here
                    line_data[i] = '\0';
                    break;
                }
            }
        }

        // TODO: handle error codes that go after the end of text;

        printf("finished processing thingy");
        /* be sure the string is terminated */
        if (i == MAX_LINE){
            line_data[i-1] = '\0';
        }

        /* display the converted string on stdout */
        printf("%s", line_data);
        printf("%%");
    } /* end of while loop; at EOF */
    Socket_close(connect_socket);
    exit(0);
}
