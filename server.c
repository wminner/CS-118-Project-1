/* A simple server in the internet domain using TCP
   The port number is passed as an argument 
   This version runs forever, forking off a separate 
   process for each connection
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */
#include <unistd.h>

#define MAXMSGLEN 100000
#define PORTNUM 9007

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno, pid;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    // if (argc < 2) {
    //    fprintf(stderr,"ERROR, no port provided\n");
    //    exit(1);
    // }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);	//create socket
    if (sockfd < 0) 
        error("ERROR opening socket");
    if (setsockopt(sockfd, SOL_SOCKET, (SO_REUSEADDR|SO_REUSEPORT), &(int){ 1 }, sizeof(int)) < 0)
        error("setsockopt(SO_REUSEADDR) failed");
    memset((char *) &serv_addr, 0, sizeof(serv_addr));	//reset memory
    //fill in address info
    //portno = atoi(argv[1]);
    portno = PORTNUM;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR on binding");

    listen(sockfd,5);	//5 simultaneous connection at most

    while(1) {
        //accept connections
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
           
        if (newsockfd < 0) 
            error("ERROR on accept");
           
        int n;
        char buffer[MAXMSGLEN+1];
    	 
        memset(buffer, 0, sizeof(buffer));	//reset memory

        //read client's message
        n = recv(newsockfd, buffer, sizeof(buffer)-1, 0);
        if (n < 0) error("ERROR reading from socket");
        printf("Here is the message: %s\n",buffer);

        //reply to client
        n = write(newsockfd,"I got your message",18);
        if (n < 0) error("ERROR writing to socket");
        close(newsockfd);//close connection

        if (strcmp(buffer, "exit\n") == 0) {
            printf("Exiting...\n");
            break;
        }
    }

 
    close(sockfd);
    
    return 0; 
}

