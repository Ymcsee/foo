
#include "gfserver-student.h"

/* 
 * Modify this file to implement the interface specified in
 * gfserver.h.
 */

const char *response_end = "\r\n\r\n";
const char *header_template = "GETFILE %s %zu\r\n\r\n";

struct gfserver_t {
    int sockfd;
    unsigned short port;
    int max_npending;
    gfh_error_t (*handler)(gfcontext_t **, const char *, void*);
    void* args;
};

struct gfcontext_t {
    int clientfd;
    struct sockaddr_in *client_addr;
    char *path;
};

void gfs_abort(gfcontext_t **ctx){
    close((*ctx)->clientfd);
    free(*ctx);
    *ctx = NULL;
}

gfserver_t* gfserver_create(){
    gfserver_t *gfs;
    int sockfd;

    if ((gfs = (gfserver_t *)malloc(sizeof(gfserver_t))) == NULL) {
        perror("Unable to allocate memory");
        exit(1);
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Unable to create the socket");
        exit(1);
    }

    gfs->sockfd = sockfd;

    return gfs;
}

ssize_t gfs_send(gfcontext_t **ctx, const void *data, size_t len){
    return send((*ctx)->clientfd, data, len, 0);
}

/*bzero(item->path, sizeof(item->path));
	strcpy(item->path, path);*/

ssize_t gfs_sendheader(gfcontext_t **ctx, gfstatus_t status, size_t file_len){
    char header[BUFSIZ];

    char *strstatus;
    switch (status) {
        case GF_INVALID: {
            strstatus = "INVALID";
        }
        break;

        case GF_FILE_NOT_FOUND: {
            strstatus = "FILE_NOT_FOUND";
        }
        break;

        case GF_OK: {
            strstatus = "OK";
        }
        break;

        default: {
            strstatus = "ERROR";
        }
        break;
    }

    /* Only care about sending length if status is ok */
    if (status == GF_OK) {
        sprintf(header, "GETFILE %s %zu \r\n\r\n", strstatus , file_len);
    } else {
        sprintf(header, "GETFILE %s \r\n\r\n", strstatus);
    }

    return send((*ctx)->clientfd, header, strlen(header), 0);
}

/* Regex to parse request path into ctx */
static int parse_request(gfcontext_t *ctx, char *req_buffer) {

    char *req_regex = "^GETFILE GET (/[./a-zA-Z0-9-].*)[ ]{0,1}\r\n\r\n$";

    regex_t resp_regex_compiled;
    int groups_num = 2;
    regmatch_t groups[groups_num];

    if (regcomp(&resp_regex_compiled, req_regex, REG_EXTENDED)){
        perror("Could not compile regular expression.\n");
        return -1;
    };

    char path[MAX_REQUEST_LEN];


    if (regexec(&resp_regex_compiled, req_buffer, groups_num, groups, 0) != 0){
        char err[200];
        sprintf(err, "Request: %s doesn't match regex\n", req_buffer);
        perror(err);
        regfree(&resp_regex_compiled);
        memset(err, 0, 200);
        return -1;
    }

    char req_buffer_copy[strlen(req_buffer) + 1];
    strcpy(req_buffer_copy, req_buffer);
    req_buffer_copy[groups[1].rm_eo] = 0;
    sprintf(path, "%s", req_buffer_copy + groups[1].rm_so);

    regfree(&resp_regex_compiled);

    printf("Parsed file path %s from request %s\n", path, req_buffer);

    ctx->path = path;

    return 0;
}

void gfserver_serve(gfserver_t **gfs){

    const int IP = 0;

    struct sockaddr_in server_socket;
    char req_buffer[MAX_REQUEST_LEN];
    char buffer[MAX_REQUEST_LEN];
    int req_size;


    /* Allocate socket and assign socket file descripter */
    if (((*gfs)->sockfd = socket(AF_INET, SOCK_STREAM, IP)) < 0) {
        perror("Error opening socket");
        exit(1);
    }

    /* Prep socket */
    bzero((char *)&server_socket, sizeof(server_socket));
    server_socket.sin_family = AF_INET;
    server_socket.sin_addr.s_addr = htons(INADDR_ANY);
    server_socket.sin_port = htons((*gfs)->port);

    /* Needed to statify Bonnie grader */
    int optval = 1;
    setsockopt((*gfs)->sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    // bind the socket to the address
    if (bind((*gfs)->sockfd, (struct sockaddr*)&server_socket, sizeof(server_socket)) < 0) {
        perror("Error binding socket");
        exit(1);
    }

    // listen to maximum pending connections
    if (listen((*gfs)->sockfd, (*gfs)->max_npending) < 0) {
        perror("Error listening to socket");
        exit(1);
    }

    while (1) {

        /* Allocate request struct */
        gfcontext_t *ctx = (gfcontext_t *) malloc(sizeof(gfcontext_t));
        socklen_t client_len = sizeof(ctx->client_addr);

         /* Accept connection */
        if ((ctx->clientfd = accept((*gfs)->sockfd, (struct sockaddr *)&(ctx->client_addr), &client_len)) < 0){
            perror("Error accepting client");
            gfs_abort(&ctx);
            continue;
        }

        /* Zero out request buffer */
        memset(req_buffer, 0, MAX_REQUEST_LEN);
        memset(buffer, 0, MAX_REQUEST_LEN);


        /* Receive request */
        int bytes_received = 0;
        while (strstr(req_buffer, response_end) == NULL && bytes_received < MAX_REQUEST_LEN){
            if ((req_size = recv(ctx->clientfd, buffer, MAX_REQUEST_LEN, 0)) < 0){
                perror("Error reading from client socket");
                gfs_abort(&ctx);
                continue;
            }
            strcpy(req_buffer + bytes_received, buffer);
            bytes_received += req_size;
            memset(buffer, 0, MAX_REQUEST_LEN);
        }

        /* Parse request into request buffer and if it fails, write
            INVALID response immediately and short circuit
         */
        if (parse_request(ctx, req_buffer) < 0) {
            gfs_sendheader(&ctx, GF_INVALID, 0);
            gfs_abort(&ctx);
            continue;
        }

        /* Proceed to actually handle request */
        (*gfs)->handler(&ctx, ctx->path, (*gfs)->args);
    }

    exit(0);
}

void gfserver_set_handlerarg(gfserver_t **gfs, void* arg){
    (*gfs)->args = arg;
}

void gfserver_set_handler(gfserver_t **gfs, gfh_error_t (*handler)(gfcontext_t **, const char *, void*)){
    (*gfs)->handler = handler;
}

void gfserver_set_maxpending(gfserver_t **gfs, int max_npending){
    (*gfs)->max_npending = max_npending;
}

void gfserver_set_port(gfserver_t **gfs, unsigned short port){
    (*gfs)->port = port;
}


