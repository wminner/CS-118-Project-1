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

// Prototypes
int parseFilename(char *buffer, int *namelen);

// END Prototypes

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno; //pid;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    FILE *fp;
    char *filename = NULL;
    int namelen = 0;
    int fname_pos;

    // Open and clear temp file for logging HTTP requests
    fp = fopen("log_server", "w");
    ftruncate(fileno(fp), 0);

    // Usage
    if (argc > 1) {
        fprintf(stderr, "%s accets no additional arguements.\n", argv[0]);
        exit(1);
    }

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);	
    if (sockfd < 0) 
        error("ERROR opening socket");
    if (setsockopt(sockfd, SOL_SOCKET, (SO_REUSEADDR|SO_REUSEPORT), &(int){ 1 }, sizeof(int)) < 0)
        error("setsockopt(SO_REUSEADDR) failed");

    // Reset memory
    memset((char *) &serv_addr, 0, sizeof(serv_addr));	
    
    // Fill in address info
    portno = PORTNUM;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR on binding");

    listen(sockfd,5);	// 5 simultaneous connection at most

    while(1) {
        // Accept connections
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
           
        if (newsockfd < 0) 
            error("ERROR on accept");

        int n;
        char buffer[MAXMSGLEN+1];

        // Reset memory
        memset(buffer, 0, sizeof(buffer));	

        // Read client's message
        n = recv(newsockfd, buffer, sizeof(buffer)-1, 0);
        if (n < 0) 
            error("ERROR reading from socket");
        printf("Here is the message: %s",buffer);

        // Log HTTP requests (DEBUG)
        fp = fopen("log_server", "a");
        fwrite(buffer, strlen(buffer), 1, fp);
        fclose(fp);

        // Find filename
        fname_pos = parseFilename(buffer, &namelen);
        if (fname_pos < 0)
            fprintf(stderr, "ERROR bad request\n");
        else {
            // Print filename found (DEBUG)
            filename = (char*) calloc(namelen+1, sizeof(char));
            strncpy(filename, buffer+fname_pos, namelen);
            filename[namelen] = '\0';
            printf("Filename found is \"%s\".\n\n", filename);
        }

        // Reply to client
        n = send(newsockfd, "I got your message", 18, 0);
        if (n < 0) 
            error("ERROR writing to socket");
        close(newsockfd);// Close connection

        if (strcmp(buffer, "exit\n") == 0) {
            printf("Exiting...\n");
            break;
        }
        if (filename)
            free(filename);
    }
    close(sockfd);
    return 0; 
}

// Example HTTP Header (from Firefox)
    // GET /test.txt HTTP/1.1
    // Host: 127.0.0.1:9007
    // User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:45.0) Gecko/20100101 Firefox/45.0
    // Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8
    // Accept-Language: en-US,en;q=0.5
    // Accept-Encoding: gzip, deflate
    // Connection: keep-alive

// Find requested filename in HTTP header
// Returns position of filename and passes back length of the file name
// Returns -1 on fail
int parseFilename(char *buffer, int *namelen) {
    int i;
    for (i = 0; i < MAXMSGLEN; i++) {
        // Check for GET
        if (strncmp(buffer, "GET /", 5) == 0) {
            // Find length of filename
            char *start = buffer + i + 5;
            char *end = strchr(buffer+i+5, ' ');
            *namelen = end-start;
            return i + 5;
        }
        // Reached end of buffer (NULL bytes)
        if (buffer[i] == '\0')
            break;
    }
    return -1;
}
