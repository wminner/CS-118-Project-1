#include <stdio.h>
#include <sys/types.h>  // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h> // sockets, e.g. sockaddr
#include <netinet/in.h> // sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <string.h>     // strcmp, strcpy
#include <sys/wait.h>	// waitpid
#include <sys/stat.h>   // stat
#include <signal.h>	    // signal name macros
#include <unistd.h>     // NULL
#include <fcntl.h>      // open
#include <errno.h>      // errno

#define MAXMSGLEN 1024
#define PORTNUM 9007
#define DATA_PACKET_LEN 1024

// Prototypes
int findFilename(char *buffer, int *namelen);
int getContentType(char *filename, char *ctype, char *ctype_str);
int getContentLength(char *filename, unsigned long long *clen, char **clen_str);
long long buildResponseHeader(char *ctype, char *clen_str, char **response);
void handle_sigchld(int sig);
void dowork(int sock);
// END Prototypes

// Globals
int DEBUG_LOG = 0;          // Logs requests and reponses to a file if 1
FILE *fp;                   // File pointer for logging HTTP requests/responses
// END Globals

void error(char *msg) {
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[]) {
    int sockfd, newsockfd, portno, pid;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    struct sigaction sa;            // sigchld handler

    // Open and clear debug log for logging HTTP requests/responses
    if (DEBUG_LOG) {
        fp = fopen("log_server", "w");
        ftruncate(fileno(fp), 0);
    }

    // Usage
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [PORT]\n", argv[0]);
        exit(1);
    }

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);	
    if (sockfd < 0) 
        error("ERROR opening socket");
    if (setsockopt(sockfd, SOL_SOCKET, (SO_REUSEADDR|SO_REUSEPORT), &(int){ 1 }, sizeof(int)) < 0)
        error("setsockopt(SO_REUSEADDR) failed");

    // Reset memory and fill in address info
    memset((char *) &serv_addr, 0, sizeof(serv_addr));	
    //portno = PORTNUM;
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR on binding");

    listen(sockfd,5);	// 5 simultaneous connection at most

    // Set up sigchld handler
    sa.sa_handler = &handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, 0) == -1) {
        error("ERROR sigchld handler");
    }

    while(1) {
        // Accept connections
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
           
        if (newsockfd < 0) 
            error("ERROR on accept");

        // Fork child for doing actual work
        pid = fork();
        if (pid < 0)
            error("Error on fork");

        // Child fork
        if (pid == 0) {
            close(sockfd);
            dowork(newsockfd);
            exit(0);
        // Parent fork
        } else
            close(newsockfd);   // Close socket as parent doesn't use it
    }
    close(sockfd);
    return 0; 
}

// Child's work receiving and responding to HTTP requests
// Argument: socket for which it will be receiving and sending data on
void dowork(int sock) {
    char *filename = NULL;          // Name of file requested
    char ctype[10];                 // Content-Type
    char ctype_str[26];             // Content-Type string (for response)
    char *clen_str = NULL;          // Content-Length string
    char *response = NULL;          // response of HTTP response
    unsigned long long clen = 0;    // Content-Length (number form)
    int fname_pos;                  // Position of filename in HTTP request
    int namelen = 0;                // Length of filename
    long long res_len = 0;          // Response length
    
    int n;
    char buffer[MAXMSGLEN];

    // Reset memory
    memset(buffer, 0, sizeof(buffer));	

    // Read client's message
    n = read(sock, buffer, MAXMSGLEN);
    if (n < 0) 
        error("ERROR reading from socket");
    printf("REQUEST: %s",buffer);

    // DEBUG log
    if (DEBUG_LOG) {
        fp = fopen("log_server", "a");
        fwrite(buffer, strlen(buffer), 1, fp);
        fclose(fp);
    }

    // Find filename indices
    fname_pos = findFilename(buffer, &namelen);
    if (fname_pos < 0) {
        fprintf(stderr, "ERROR bad request\n");
        goto error;
    }

    // Exact filename using indices
    filename = calloc(namelen+1, sizeof(char));
    if (filename) {
        strncpy(filename, buffer+fname_pos, namelen);
        filename[namelen] = '\0';
    } else
        goto error;

    // Find content-type of file
    if (getContentType(filename, ctype, ctype_str) < 0) {
        fprintf(stderr, "ERROR unrecognized file type\n");
        goto error;
    }

    // Find content-length of file
    if (getContentLength(filename, &clen, &clen_str) < 0) {
        fprintf(stderr, "ERROR file not found\n");
        goto error;
    }

    // Build response string
    res_len = buildResponseHeader(ctype_str, clen_str, &response);
    if (res_len < 0) {
        fprintf(stderr, "ERROR with building response string\n");
        goto error;
    } else {
        // Send header
        n = write(sock, response, res_len);
        if (n < 0)
            error("ERROR writing to socket");

        // DEBUG log
        if (DEBUG_LOG) {
            FILE *fp;
            fp = fopen("log_server", "a");
            fwrite(response, 1, res_len, fp);
        }

        int data_fd = 0;
        char data[DATA_PACKET_LEN];
        memset(data, 0, sizeof(data));
        int amtread = 1;

        data_fd = open(filename, O_RDONLY);
        if (!data_fd)
            goto error;

        while (amtread > 0) {
            amtread = read(data_fd, data, DATA_PACKET_LEN);
            n = write(sock, data, DATA_PACKET_LEN);
            // DEBUG log
            if (DEBUG_LOG) {
                fwrite(data, 1, DATA_PACKET_LEN, fp);
            }
        }
        close(data_fd);
        printf("RESPONSE: %s", response);

        // DEBUG log
        if (DEBUG_LOG) {
            fwrite("\n\n", 1, 2, fp);
            fclose(fp);
        }
    }

    error:
    close(sock);   // Close connection

    // Free allocated memory
    if (filename != NULL) {
        free(filename);
        filename = NULL;
    }
    if (clen_str != NULL) {
        free(clen_str);
        clen_str = NULL;
    }
    if (response) {
        free(response);
        response = NULL;
    }
}

// Example HTTP header request (from Firefox)
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

// Get requested content type of filename to put in HTTP response
//   Success: returns 0 and passes back content-type in ctype
//   Failure: return -1 (unrecognized file extension)
// Supported types:
//   text/html
//   image/jpg
//   image/gif
//   image/png
int getContentType(char *filename, char *ctype, char *ctype_str) {
    // Lookup content-type associated with file extension (if any)
    char *dot_pos;

    strcpy(ctype_str, "Content-Type: ");

    if ((dot_pos = strchr(filename, '.'))) {
        if (strcmp(dot_pos, ".txt") == 0 || strcmp(dot_pos, ".html") == 0) {
            strcpy(ctype, "text/html");
        } else if (strcmp(dot_pos, ".jpg") == 0) {
            strcpy(ctype, "image/jpg");
        } else if (strcmp(dot_pos, ".png") == 0) {
            strcpy(ctype, "image/png");
        } else if (strcmp(dot_pos, ".gif") == 0) {
            strcpy(ctype, "image/gif");
        } else {
            // Unrecognized extension
            return -1;
        }
    } else {
        // No extension in filename, assume text/html
        strcpy(ctype, "text/html\0");
    }
    strcpy(ctype_str+strlen(ctype_str), ctype);
    strcpy(ctype_str+strlen(ctype_str), "\r\n\0");

    //printf("DEBUG getContentType: %s\n", ctype_str);
    return 0;
}

// Get requested content length to put in HTTP response
//   Success: returns 0 and passes back clen and clen_string
//   Failure: returns -1 (file not found)
int getContentLength(char *filename, unsigned long long *clen, char **clen_str) {
    struct stat st;
    int clen_len;
    size_t temp_len;
    char *temp_str;

    if (stat(filename, &st) < 0)
        return -1;
    *clen = st.st_size;
    temp_len = st.st_size;

    for (clen_len = 0; temp_len > 0; temp_len /= 10)
        clen_len++;
    temp_str = calloc(clen_len+19, sizeof(char));
    if (temp_str == NULL)
        return -1;
    strcpy(temp_str, "Content-Length: ");
    sprintf(temp_str+strlen(temp_str), "%llu", *clen);
    strcpy(temp_str+strlen(temp_str), "\r\n");
    //printf("DEBUG getContentLength: %s\n", temp_str);

    *clen_str = temp_str;
    return 0;
}

// Example HTTP header response                         // Number of characters
    // HTTP/1.1 200 OK\r\n                              // 17    
    // Content-Type: image/jpg\r\n                      // 25
    // Date: Fri, 25 Mar 2016 19:42:42 GMT\r\n          // 37
    // Last-Modified: Fri, 04 Sep 2015 22:33:08 GMT\r\n // 46
    // Content-Length: 5969\r\n                         // 18 + len(clen)
    // \r\n                                             // 2
    // [Data]                                           // clen

// length of response header = 127 + strlen(clen)

// Assemble HTTP response from file properties
//   Success: returns length of response and passes back header string in response
//   Failure: return -1
long long buildResponseHeader(char *ctype_str, char *clen_str, char **response) {

    char *first = "HTTP/1.1 200 OK\r\n";
    char *date = "Date: Fri, 25 Mar 2016 19:42:42 GMT\r\n";
    char *last_modified = "Last-Modified: Fri, 04 Sep 2015 22:33:08 GMT\r\n";
    char *spacer = "\r\n";
    char *temp_response;

    // Determine how much memory response should be
    int response_len = 127 + strlen(clen_str);

    // Allocate memory for response and build response string
    temp_response = calloc(response_len+1, sizeof(char));
    if (temp_response == NULL)
        return -1;
    else {
        // Start with response (first line)
        strcpy(temp_response, first);
        // Append content-type string
        strcpy(temp_response + strlen(temp_response), ctype_str);
        // Append date
        strcpy(temp_response + strlen(temp_response), date);
        // Append last-modified
        strcpy(temp_response + strlen(temp_response), last_modified);
        // Append content-length
        strcpy(temp_response + strlen(temp_response), clen_str);
        // Append spacer
        strcpy(temp_response + strlen(temp_response), spacer);
    }

    *response = temp_response;
    return response_len;
}

void handle_sigchld(int sig) {
    (void)sig;  // Silence compiler warning
    // Save and restore errno in case waitpid changes it
    // (which may mess with other functions not expecting async signals)
    int save_errno = errno;
    while (waitpid((pid_t)(-1), 0, WNOHANG) > 0);
    errno = save_errno;
}