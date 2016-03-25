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
#include <sys/stat.h>
#include <signal.h>	/* signal name macros, and the kill() prototype */
#include <unistd.h>

#define MAXMSGLEN 100000
#define PORTNUM 9007

// Prototypes
int findFilename(char *buffer, int *namelen);
int getContentType(char *filename, char *ctype);
long long getContentLength(char *filename);

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
    FILE *fp;               // File pointer for logging HTTP requests
    char *filename = NULL;  // Name of file requested
    char *ctype = NULL;     // Content-Type
    long long clen = 0;     // Content-Length
    int fname_pos;          // Position of filename in HTTP header
    int namelen = 0;        // Length of filename

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
        fname_pos = findFilename(buffer, &namelen);
        if (fname_pos < 0) {
            fprintf(stderr, "ERROR bad request\n");
            goto reply;
        }

        // Print filename found (DEBUG)
        filename = (char*) calloc(namelen+1, sizeof(char));
        strncpy(filename, buffer+fname_pos, namelen);
        filename[namelen] = '\0';
        printf("Filename found is \"%s\".\n", filename);

        // Find content-type of file
        if (getContentType(filename, ctype) < 0) {
            fprintf(stderr, "ERROR bad file type\n");
            goto reply;
        }

        // Find content-length of file
        clen = getContentLength(filename);
        if (clen < 0) {
            fprintf(stderr, "ERROR bad file content-length\n");
            goto reply;
        }
        else // DEBUG
            printf("Content-Length is %lld bytes.\n", clen);

        reply:
        // Reply to client
        n = send(newsockfd, "I got your message", 18, 0);
        if (n < 0) 
            error("ERROR writing to socket");
        close(newsockfd);// Close connection

        if (strcmp(buffer, "exit\n") == 0) {
            printf("Exiting...\n");
            break;
        }

        // Free allocated memory
        if (filename)
            free(filename);
        if (ctype)
            free(ctype);

        printf("\n");   // DEBUG
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
//   Success: returns position of filename and passes back length of the file name
//   Failure: returns -1 (bad header)
int findFilename(char *buffer, int *namelen) {
    char *get_pos;
    // Check for GET
    if ((get_pos = strstr(buffer, "GET /"))) {
        // Find length of filename
        char *start = get_pos + 5;
        char *end = strchr(get_pos+5, ' ');
        *namelen = end-start;
        return (buffer - get_pos + 5);
    } else
        return -1;
}

// Get requested content type of filename to put in HTTP header
//   Success: returns 0
//   Failure: return -1 (unrecognized file extension)
// Possible types:
//   text/html
//   image/jpg
//   image/gif
int getContentType(char *filename, char *ctype) {
    // Lookup content-type associated with file extension (if any)
    char *dot_pos;
    ctype = (char*) calloc(10, sizeof(char));

    if ((dot_pos = strchr(filename, '.'))) {
        if (strcmp(dot_pos, ".txt") == 0) {
            printf("Found text extension.\n");
            strcpy(ctype, "text/html\0");
        } else if (strcmp(dot_pos, ".jpg") == 0) {
            printf("Found jpg extension.\n");
            strcpy(ctype, "image/jpg\0");
        } else if (strcmp(dot_pos, ".gif") == 0) {
            printf("Found gif extension.\n");
            strcpy(ctype, "image/gif\0");
        } else {
            // Unrecognized extension
            printf("Unrecognized extension\n");
            free(ctype);
            ctype = NULL;
            return -1;
        }
    } else {
        // No extension in filename, assume text/html
        printf("No extension found, assuming text/html\n");
        strcpy(ctype, "text/html\0");
    }
    return 0;
}

// Get requested content length to put in HTTP header
//   Success: returns 0 and passes back content-type in ctype
//   Failure: returns -1 (file not found)
long long getContentLength(char *filename) {
    struct stat st;
    if (stat(filename, &st) < 0)
        return -1;
    return st.st_size;
}