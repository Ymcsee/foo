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
    "  transferserver [options]\n"                           \
    "options:\n"                                             \
    "  -f                  Filename (Default: 6200.txt)\n" \
    "  -h                  Show this help message\n"         \
    "  -p                  Port (Default: 19121)\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"filename", required_argument, NULL, 'f'},
    {"help", no_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    {NULL, 0, NULL, 0}};

int main(int argc, char **argv)
{
    int option_char;
    int portno = 19121;             /* port to listen on */
    char *filename = "6200.txt"; /* file to transfer */

    setbuf(stdout, NULL); // disable buffering

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "p:hf:x", gLongOptions, NULL)) != -1)
    {
        switch (option_char)
        {
        case 'p': // listen-port
            portno = atoi(optarg);
            break;
        default:
            fprintf(stderr, "%s", USAGE);
            exit(1);
        case 'h': // help
            fprintf(stdout, "%s", USAGE);
            exit(0);
            break;
        case 'f': // file to transfer
            filename = optarg;
            break;
        }
    }


    if ((portno < 1025) || (portno > 65535))
    {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }
    
    if (NULL == filename)
    {
        fprintf(stderr, "%s @ %d: invalid filename\n", __FILE__, __LINE__);
        exit(1);
    }

    /* Socket Code Here */
    const int IP = 0;

    int sockfd, clientfd, read_size;
    unsigned int cli_len;
    struct sockaddr_in server_socket, cli_addr;
    char resp_buffer[BUFSIZE];
    FILE *fp;

    /* Open file */
    if ((fp = fopen(filename, "rb")) == NULL) {
        perror("Error opening file");
        exit(1);
    }

    /* Allocate socket and assign socket file descripter */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, IP)) < 0) {
        perror("Error opening socket");
        fclose(fp);
        exit(1);
    }

    /* Prep socket */
    bzero((char *)&server_socket, sizeof(server_socket));
    server_socket.sin_family = AF_INET;
    server_socket.sin_addr.s_addr = INADDR_ANY;
    server_socket.sin_port = htons(portno);

    /* Needed to statify Bonnie grader */
    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    /* Bind to socket */
    if (bind(sockfd, (struct sockaddr *)&server_socket, sizeof(server_socket)) < 0) {
        perror("Error binding socket");
        fclose(fp);
        exit(1);
    }

    /* Wait for client conn */
    listen(sockfd, 5);
    cli_len = sizeof(cli_addr);

    while (1) {
        /* Bind to connection socket */
        if ((clientfd = accept(sockfd, (struct sockaddr *)&cli_addr, &cli_len)) < 0) {
            perror("Error on accept");
            fclose(fp);
            exit(1);
        }

        /* Read into response buffer and send in chunks */
        while ((read_size = fread(resp_buffer, 1, BUFSIZE, fp)) > 0) {
            if (send(clientfd, resp_buffer, read_size, 0) < 0) {
                perror("Error sending response");
                fclose(fp);
                close(clientfd);
                exit(1);
            }
            memset(resp_buffer, 0, BUFSIZE);
        }

        /* Close connection */
        close(clientfd);
    }

    /* Cleanup */
    close(sockfd);
    fclose(fp);
    exit(0);

}
