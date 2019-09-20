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

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  echoserver [options]\n"                                                    \
"options:\n"                                                                  \
"  -p                  Port (Default: 19121)\n"                                \
"  -m                  Maximum pending connections (default: 1)\n"            \
"  -h                  Show this help message\n"                              \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"port",          required_argument,      NULL,           'p'},
  {"maxnpending",   required_argument,      NULL,           'm'},
  {"help",          no_argument,            NULL,           'h'},
  {NULL,            0,                      NULL,             0}
};


int main(int argc, char **argv) {
  int option_char;
  int portno = 19121; /* port to listen on */
  int maxnpending = 1;
  
  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "p:m:hx", gLongOptions, NULL)) != -1) {
   switch (option_char) {
      case 'p': // listen-port
        portno = atoi(optarg);
        break;                                        
      default:
        fprintf(stderr, "%s ", USAGE);
        exit(1);
      case 'm': // server
        maxnpending = atoi(optarg);
        break; 
      case 'h': // help
        fprintf(stdout, "%s ", USAGE);
        exit(0);
        break;
    }
  }

    setbuf(stdout, NULL); // disable buffering

    if ((portno < 1025) || (portno > 65535)) {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }
    if (maxnpending < 1) {
        fprintf(stderr, "%s @ %d: invalid pending count (%d)\n", __FILE__, __LINE__, maxnpending);
        exit(1);
    }


  /* Socket Code Here */
  const int IP = 0;

  int max_message_size = 15, sockfd, clientfd, read_size;
  unsigned int cli_len;
  struct sockaddr_in server_socket, cli_addr;
  char message[BUFSIZE];

  /* Allocate socket and assign socket file descripter */
  if ((sockfd = socket(AF_INET, SOCK_STREAM, IP)) < 0) {
    perror("Error opening socket");
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
    exit(1);
  }

  /* Wait for client conn */
  if (listen(sockfd, maxnpending) < 0){
    perror("Error listening on socket");
    exit(1);
  }
  cli_len = sizeof(cli_addr);

  while (1) {

    /* Bind to connection socket */
    if ((clientfd = accept(sockfd, (struct sockaddr *)&cli_addr, &cli_len)) < 0) {
      perror("Error on accept");
      exit(1);
    }

    /* Zero out buffer */
    memset(message, 0, BUFSIZE);

    /* Read client payload into message buffer */
    if ((read_size = recv(clientfd, message, max_message_size, 0)) < 0){
      perror("Error reading from client socket");
      close(clientfd);
      exit(1);
    }

    /* Null terminate the buffer and write to stdout */
    message[read_size] = '\0';
    printf("%s", message);

    /* Send response to client */
    if (send(clientfd, message, read_size, 0) < 0) {
      perror("Error sending response");
      close(clientfd);
      exit(1);
    }

    close(clientfd);

  }

  close(sockfd);
  exit(0);

}
