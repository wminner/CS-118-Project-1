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
#include <fcntl.h>  // Open
#include <errno.h>

#define MAXMSGLEN 100000
#define PORTNUM 9007

// Prototypes
int findFilename(char *buffer, int *namelen);
int getContentType(char *filename, char *ctype, char *ctype_str);
int getContentLength(char *filename, unsigned long long *clen, char **clen_str);
long long buildResponseString(char *filename, char *ctype, unsigned long long clen, char *clen_str, char **response);

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
    FILE *fp;                       // File pointer for logging HTTP requests
    char *filename = NULL;          // Name of file requested
    char ctype[10];                 // Content-Type
    char ctype_str[26];             // Content-Type string (for response)
    char *clen_str = NULL;          // Content-Length string
    char *response = NULL;          // response of HTTP response
    unsigned long long clen = 0;    // Content-Length (number form)
    int fname_pos;                  // Position of filename in HTTP header
    int namelen = 0;                // Length of filename
    long long res_len = 0;          // Response length

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
        printf("Here is the request: %s",buffer);

        // Log HTTP requests (DEBUG)
        fp = fopen("log_server", "a");
        fwrite(buffer, strlen(buffer), 1, fp);
        fclose(fp);

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
            printf("Filename found is \"%s\".\n", filename);
        } else
            goto error;

        // Find content-type of file
        if (getContentType(filename, ctype, ctype_str) < 0) {
            fprintf(stderr, "ERROR bad file type\n");
            goto error;
        }
        //printf("DEBUG getContentType: %s", ctype_str);

        // Find content-length of file
        if (getContentLength(filename, &clen, &clen_str) < 0) {
            fprintf(stderr, "ERROR bad file content-length\n");
            goto error;
        }
        else // DEBUG
            printf("Content-Length is %llu bytes.\n", clen);
        //printf("DEBUG getContentLength: %s", clen_str);

        // Build response string
        res_len = buildResponseString(filename, ctype_str, clen, clen_str, &response);
        if (res_len < 0) {
            fprintf(stderr, "Error with building response string\n");
            goto error;
        } else {
            // Send header
            n = write(newsockfd, response, res_len);
            if (n < 0)
                error("ERROR writing to socket");

            // Send data
            //FILE *data_fp = NULL;
            int data_fd = 0;
            char data[200000];
            memset(data, 0, sizeof(data));
            int amtread = 0;

            //data_fp = fopen(filename, "r");
            data_fd = open(filename, O_RDONLY);
            if (data_fd)
                //amtread = fread(data, 1, clen, data_fp);
                amtread = read(data_fd, data, clen);
            // else 
                // handle error
            printf("amtread is %d\n", amtread);
            n = write(newsockfd, data, clen);

            printf("\nThis is the header: %s", response);
            printf("This is the data: %s\n", data);

            // DEBUG log
            FILE *fp;
            fp = fopen("log_server", "a");
            fwrite(response, 1, res_len, fp);
            fwrite(data, 1, clen, fp);
            fclose(fp);
        }

        // Reply to client
        // n = send(newsockfd, "I got your message", 18, 0);
        // if (n < 0) 
        //     error("ERROR writing to socket");
        
        if (strcmp(buffer, "exit\n") == 0) {
            printf("Exiting...\n");
            break;
        }

        error:
        close(newsockfd);// Close connection

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

        //printf("\n");   // DEBUG
    }
    close(sockfd);
    return 0; 
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
// Possible types:
//   text/html
//   image/jpg
//   image/gif
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
    temp_str = calloc(clen_len+18, sizeof(char));
    if (temp_str == NULL)
        return -1;
    strcpy(temp_str, "Content-Length: ");
    sprintf(temp_str+strlen(temp_str), "%llu", *clen);
    strcpy(temp_str+strlen(temp_str), "\r\n");
    //printf("DEBUG getContentLength: %s\n", temp_str);

    *clen_str = temp_str;
    return 0;
}

// Example HTTP header response
    // HTTP/1.1 200 OK\r\n                              // 17    
    // Content-Type: image/jpg\r\n                      // 25
    // Date: Fri, 25 Mar 2016 19:42:42 GMT\r\n          // 37
    // Last-Modified: Fri, 04 Sep 2015 22:33:08 GMT\r\n // 46
    // Content-Length: 5969\r\n                         // 18 + len(clen)
    // \r\n                                             // 2
    // [Data]                                           // clen

// response_len = 145 + len(clen) + clen

// Assemble HTTP response from file properties
//   Success: returns length of response and passes back response string in response
//   Failure: return -1
long long buildResponseString(char *filename, char *ctype_str, unsigned long long clen, char *clen_str, char **response) {

    char *first = "HTTP/1.1 200 OK\r\n";
    char *date = "Date: Fri, 25 Mar 2016 19:42:42 GMT\r\n";
    char *last_modified = "Last-Modified: Fri, 04 Sep 2015 22:33:08 GMT\r\n";
    char *spacer = "\r\n";
    //char data[200000];
    char *temp_response;
    //int data_fd = 0;
    //int amtread = 0;

    // Determine how much memory response should be
    int response_len = 127 + strlen(clen_str);// + clen;

    // //Get data from file
    // data_fd = open(filename, O_RDONLY);
    // if (data_fd)
    //     //amtread = read(data_fd, data, clen);
    //     read(data_fd, data, clen);
    // else
    //     return -1;

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