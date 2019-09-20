#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>

#define BUFSIZE 1219

#define USAGE                                                \
    "usage:\n"                                               \
    "  transferclient [options]\n"                           \
    "options:\n"                                             \
    "  -s                  Server (Default: localhost)\n"    \
    "  -p                  Port (Default: 19121)\n"           \
    "  -o                  Output file (Default cs6200.txt)\n" \
    "  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"server", required_argument, NULL, 's'},
    {"port", required_argument, NULL, 'p'},
    {"output", required_argument, NULL, 'o'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}};

/* Main ========================================================= */
int main(int argc, char **argv)
{
    int option_char = 0;
    char *hostname = "localhost";
    unsigned short portno = 19121;
    char *filename = "cs6200.txt";

    setbuf(stdout, NULL);

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "s:p:o:hx", gLongOptions, NULL)) != -1)
    {
        switch (option_char)
        {
        case 's': // server
            hostname = optarg;
            break;
        case 'p': // listen-port
            portno = atoi(optarg);
            break;
        default:
            fprintf(stderr, "%s", USAGE);
            exit(1);
        case 'o': // filename
            filename = optarg;
            break;
        case 'h': // help
            fprintf(stdout, "%s", USAGE);
            exit(0);
            break;
        }
    }

    if (NULL == hostname)
    {
        fprintf(stderr, "%s @ %d: invalid host name\n", __FILE__, __LINE__);
        exit(1);
    }

    if (NULL == filename)
    {
        fprintf(stderr, "%s @ %d: invalid filename\n", __FILE__, __LINE__);
        exit(1);
    }

    if ((portno < 1025) || (portno > 65535))
    {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }

    /* Socket Code Here */
    // from /etc/protocols
    const int IP = 0;

    int sockfd, resp_len;
    char resp_buffer[BUFSIZE];
    struct sockaddr_in server_socket;
    struct hostent *server = gethostbyname(hostname);

    // Allocate socket and assign sockfd
    if ((sockfd = socket(AF_INET, SOCK_STREAM, IP)) < 0) {
        perror("Error opening socket\n");
        exit(1);
    }

    // Setup the socket to connect to the server

    // not necessary but typically adopted as a convention 
    // see https://stackoverflow.com/questions/36086642/is-zeroing-out-the-sockaddr-in-structure-necessary
    bzero((char *) &server_socket, sizeof(server_socket));

    server_socket.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&server_socket.sin_addr.s_addr, server->h_length);
    server_socket.sin_port = htons(portno);

    /* Also not necessary */
    memset(resp_buffer, 0, BUFSIZE);

    /* Connect to server */
    if (connect(sockfd, (struct sockaddr*) &server_socket, sizeof(server_socket)) < 0){
        perror("Error connection");
        close(sockfd);
        exit(1);
    }

    /* Open file */
    FILE *fp = fopen(filename, "ab");
    if (NULL == fp) {
        perror("Error opening file");
        close(sockfd);
        exit(1);
    }

    /* Read from socket until empty */
    while ((resp_len = read(sockfd, resp_buffer, BUFSIZE)) > 0) {
        fwrite(resp_buffer, 1, resp_len, fp);
    }

    /* Handle read failure */
    if (resp_len < 0) {
        perror("Error reading from socket");
        close(sockfd);
        exit(1);
    }

    /* Cleanup */
    close(sockfd);
    exit(0);
}
