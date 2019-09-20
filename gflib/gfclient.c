
#include "gfclient-student.h"

/* Global vars are evil but this is the easiest place to put them */
struct addrinfo *server_addr_info;
char *resp_regex = "GETFILE (FILE_NOT_FOUND|OK|ERROR|INVALID)[ ]{0,1}([0-9]{0,20})[ ]{0,1}";
regex_t resp_regex_compiled;

struct gfcrequest_t {
    size_t bytesrecieved;
    void *headerarg;
    void (*headerfunc)(void*, size_t, void *);
    size_t filelen;
    const char *path;
    unsigned short port;
    const char *server;
    int sockfd;
    gfstatus_t status;
    void *writearg;
    void (*writefunc)(void*, size_t, void *);
};

// optional function for cleaup processing.
void gfc_cleanup(gfcrequest_t **gfr){
    close((*gfr)->sockfd);
    free(*gfr);
    *gfr = NULL;
}

gfcrequest_t *gfc_create(){
    gfcrequest_t *gfr;

    /* Don't do this consistently but might as well */
    if ((gfr = (gfcrequest_t *)malloc(sizeof(gfcrequest_t))) == NULL) {
        perror("Unable to allocate gfcrequest_t");
        exit(1);
    }

    gfr->status = GF_INVALID;
    gfr->filelen = 0;
    gfr->bytesrecieved = 0;
    gfr->headerfunc = NULL;
    gfr->writefunc = NULL;

    return gfr;
}

size_t gfc_get_bytesreceived(gfcrequest_t **gfr){
    return (*gfr)->bytesrecieved;
}

size_t gfc_get_filelen(gfcrequest_t **gfr){
    return (*gfr)->filelen;
}

gfstatus_t gfc_get_status(gfcrequest_t **gfr){
    return (*gfr)->status;
}

void gfc_global_init(){

    /* Init `addrinfo` for server */
    server_addr_info = (struct addrinfo *)malloc(sizeof(struct addrinfo));
    memset(server_addr_info, 0, sizeof(struct addrinfo));
    server_addr_info->ai_family = AF_INET;
    server_addr_info->ai_socktype = SOCK_STREAM;
    server_addr_info->ai_protocol = 0;

    /* Precompile regex */
    if (regcomp(&resp_regex_compiled, resp_regex, REG_EXTENDED)){
        perror("Could not compile regular expression.\n");
        exit(-1);
    };

}

void gfc_global_cleanup(){
    free(server_addr_info);
    regfree(&resp_regex_compiled);
}

static gfstatus_t parse_gfc_strstatus(char *status) {
    if (strcmp(status, "ERROR") == 0) {
        return GF_ERROR;
    } else if (strcmp(status, "INVALID") == 0) {
        return GF_INVALID;
    } else if (strcmp(status, "FILE_NOT_FOUND") == 0){
        return GF_FILE_NOT_FOUND;
    } else {
        return GF_OK;
    }
}

static int parse_header(gfcrequest_t **gfr, char *header_buffer){

    int filelength = 0;
    size_t num_groups = 3;
    
    regmatch_t groups[num_groups];
    gfstatus_t status;
    char group_strs[num_groups-1][100];

    if (regexec(&resp_regex_compiled, header_buffer, num_groups, groups, 0) != 0){
        perror("Header doesn't match regex\n");
        regfree(&resp_regex_compiled);
        return -1;
    }

    for (unsigned int g = 1; g < num_groups; g++) {
        if (groups[g].rm_so == (size_t)-1) {
            break;
        }

        char header_buffer_copy[strlen(header_buffer) + 1];
        strcpy(header_buffer_copy, header_buffer);
        header_buffer_copy[groups[g].rm_eo] = 0;
        sprintf(group_strs[g-1], "%s", header_buffer_copy + groups[g].rm_so);
    }

    status = parse_gfc_strstatus(group_strs[0]);

    if (status == GF_OK && (filelength = atoi(group_strs[num_groups - 2])) == 0) {
        perror("Response status is OK but file length is not an int\n");
        regfree(&resp_regex_compiled);
        return -1;
    }

    (*gfr)->filelen = filelength;
    (*gfr)->status = status;
    
    return 0;
}

int gfc_perform(gfcrequest_t **gfr){
    // currently not implemented.  You fill this part in.
    char request[BUFSIZ];
    char buffer[BUFSIZ];
    struct addrinfo *host;
    int sockfd;
    char portno[6];
    ssize_t resplen;

    char *response_end = "\r\n\r\n";
    char *header_template = "GETFILE GET %s\r\n\r\n";

    sprintf(portno, "%d", (*gfr)->port);

    getaddrinfo((*gfr)->server, portno, server_addr_info, &host);

    if ((sockfd = socket(host->ai_family, host->ai_socktype, host->ai_protocol)) < 0) {
        (*gfr)->status = GF_OK;
		freeaddrinfo(host);
        perror("Error opening socket");
        return 0;
    }

    (*gfr)->sockfd = sockfd;

    struct timeval tv;
    tv.tv_sec = .02;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);


    if (connect(sockfd, host->ai_addr, host->ai_addrlen) < 0) {
        (*gfr)->status = GF_ERROR;
        freeaddrinfo(host);
        perror("Error connecting");
        return 0;
    }

    /* Create request string */
    sprintf(request, header_template, (*gfr)->path);

    perror("Can you see this shit 1\n");


    /* Write to socket */
    if (send(sockfd, request, strlen(request), 0) < 0) {
        (*gfr)->status = GF_ERROR;
        perror("Error writing to socket");
        freeaddrinfo(host);
        return -1;
    }

    char header_buffer[BUFSIZ];
    char content_buffer[BUFSIZ];
    perror("Can you see this shit 2\n");
    printf("Before initial rev %ld\n", strlen(buffer));
    if ((resplen = recv(sockfd, buffer, BUFSIZ, 0)) <= 0) {
        if (resplen == 0) {
            perror("Client closed request while reading first part of request");
        } else {
            perror("Reading first part of request failed");
            (*gfr)->status = GF_ERROR;
        }
        freeaddrinfo(host);
        return resplen;
    }

    char err[300];
    sprintf(err, "Can you see this shit 3. Header: %s Content: %s \n", header_buffer, content_buffer);
    perror(err);

    printf("After initial rev %ld buffer is: %s\n", strlen(buffer), buffer);
    char *rest = strstr(buffer, response_end);
    if (rest == NULL) {
        perror("Invalid header");
        freeaddrinfo(host);
        return -1;
    }
    strcpy(header_buffer, strtok(buffer, response_end));
    strcpy(content_buffer, rest + strlen(response_end));
    memset(buffer, 0 ,BUFSIZ);


    printf("Header: %s of length %ld\n", header_buffer, strlen(header_buffer));

    if (parse_header(gfr, header_buffer) < 0){
        freeaddrinfo(host);
        return -1;
    }

    if ((*gfr)->headerfunc) {
        (*gfr)->headerfunc(header_buffer, (size_t) strlen(header_buffer), (*gfr)->headerarg);
    }

    memset(header_buffer, 0, BUFSIZ);

    if ((*gfr)->status == GF_ERROR || (*gfr)->status == GF_FILE_NOT_FOUND){
        freeaddrinfo(host);
        return 0;
    }

    printf("Content buffer length %ld\n", strlen(content_buffer));

    if ((*gfr)->writefunc) {
        int content_buffer_len = strlen(content_buffer);
        (*gfr)->writefunc(content_buffer, (size_t) content_buffer_len, (*gfr)->writearg);
        memset(content_buffer, 0, BUFSIZ);
        (*gfr)->bytesrecieved = (size_t) content_buffer_len;

        perror("Can you see this shit 4\n");
        
        while ((*gfr)->bytesrecieved < (*gfr)->filelen){
            if ((resplen = recv(sockfd, buffer, BUFSIZ, 0)) <= 0) {
                if (resplen == 0) {
                    perror("Client closed request while reading first part of request");
                } else {
                    perror("Reading first part of request failed");
                    (*gfr)->status = GF_ERROR;
                }
                freeaddrinfo(host);
                return -1;
            }
            (*gfr)->writefunc(buffer, resplen, (*gfr)->writearg);
            (*gfr)->bytesrecieved += resplen;
            char err1[300];
            sprintf(err1, "Bytes read: %lu Bytes receieved so far: %lu File length: %ld\n", resplen, (*gfr)->bytesrecieved, (*gfr)->filelen);
            perror(err1);
            printf("Bytes read: %lu Bytes receieved so far: %lu File length: %ld\n", resplen, (*gfr)->bytesrecieved, (*gfr)->filelen);
            memset(buffer, 0 ,BUFSIZ);
            if (resplen < BUFSIZ) {
                break;
            }
        }
    }

    freeaddrinfo(host);

    return 0;
}



void gfc_set_headerarg(gfcrequest_t **gfr, void *headerarg){
    (*gfr)->headerarg = headerarg;
}

void gfc_set_headerfunc(gfcrequest_t **gfr, void (*headerfunc)(void*, size_t, void *)){
    (*gfr)->headerfunc = headerfunc;
}

void gfc_set_path(gfcrequest_t **gfr, const char* path){
    (*gfr)->path = path;
}

void gfc_set_port(gfcrequest_t **gfr, unsigned short port){
    (*gfr)->port = port;
}

void gfc_set_server(gfcrequest_t **gfr, const char* server){
    (*gfr)->server = server;
}

void gfc_set_writearg(gfcrequest_t **gfr, void *writearg){
    (*gfr)->writearg = writearg;
}

void gfc_set_writefunc(gfcrequest_t **gfr, void (*writefunc)(void*, size_t, void *)){
    (*gfr)->writefunc = writefunc;
}

const char* gfc_strstatus(gfstatus_t status){
    const char *strstatus = NULL;

    switch (status)
    {
        default: {
            strstatus = "UNKNOWN";
        }
        break;

        case GF_INVALID: {
            strstatus = "INVALID";
        }
        break;

        case GF_FILE_NOT_FOUND: {
            strstatus = "FILE_NOT_FOUND";
        }
        break;

        case GF_ERROR: {
            strstatus = "ERROR";
        }
        break;

        case GF_OK: {
            strstatus = "OK";
        }
        break;
        
    }

    return strstatus;
}

