
/*
 A simple client in the internet domain using TCP
 Usage: ./client hostname port (./client 192.168.0.151 10000)
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>      // define structures like hostent
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>

#define MAXMSGLEN 100000
//#define PORTNUM 9007
//#define IPADDR "127.0.0.1"
#define PORTNUM 80
#define IPADDR "www.google.com"
#define GET_IMAGE "GET /images/branding/googlelogo/1x/googlelogo_color_272x92dp.png HTTP/1.1\r\n\
Host: 127.0.0.1:9007\r\n\
User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:45.0) Gecko/20100101 Firefox/45.0\r\n\
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n\
Accept-Language: en-US,en;q=0.5\r\n\
Accept-Encoding: gzip, deflate\r\n\
Connection: keep-alive\r\n\
\r\n"

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd; //Socket descriptor
    int portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server; //contains tons of information, including the server's IP address
    int finished = 0;
    FILE *fp;

    // Open and clear temp file for logging HTTP requests
    fp = fopen("log_client", "w");
    ftruncate(fileno(fp), 0);

    char buffer[MAXMSGLEN];
    // if (argc < 3) {
    //    fprintf(stderr,"usage %s hostname port\n", argv[0]);
    //    exit(0);
    // }

    // Usage
    if (argc > 1) {
        fprintf(stderr, "%s accets no additional arguements.\n", argv[0]);
        exit(1);
    }
    
    while(1) {
        //portno = atoi(argv[2]);
        portno = PORTNUM;
        sockfd = socket(AF_INET, SOCK_STREAM, 0); //create a new socket
        if (sockfd < 0) 
           error("ERROR opening socket");
        
        //server = gethostbyname(argv[1]); //takes a string like "www.yahoo.com", and returns a struct hostent which contains information, as IP address, address type, the length of the addresses...
        server = gethostbyname(IPADDR);
        if (server == NULL) {
            fprintf(stderr,"ERROR, no such host\n");
            exit(0);
        }
        
        memset((char *) &serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET; //initialize server's address
        bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
        serv_addr.sin_port = htons(portno);

        if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) //establish a connection to the server
            error("ERROR connecting");
    
        //printf("Please enter the message: ");
        memset(buffer, 0, MAXMSGLEN);
        //fgets(buffer, MAXMSGLEN-1, stdin);	//read message
        strcpy(buffer, GET_IMAGE);

        n = send(sockfd, buffer, strlen(buffer), 0); //write to the socket
        if (n < 0) 
             error("ERROR writing to socket");
        
        if (strcmp(buffer, "exit\n") == 0) {
            finished = 1;
        }

        memset(buffer, 0, MAXMSGLEN);
        n = recv(sockfd, buffer, MAXMSGLEN-1, 0); //read from the socket
        if (n < 0) 
             error("ERROR reading from socket");
        printf("%s\n",buffer);	//print server's response
        close(sockfd); //close socket

        // Log HTTP responses (DEBUG)
        fp = fopen("log_client", "a");
        fwrite(buffer, strlen(buffer), 1, fp);
        fclose(fp);

        break; // DEBUG

        if (finished) {
            printf("Exiting...\n");
            break;
        }
    }
    close(sockfd);

    return 0;
}
