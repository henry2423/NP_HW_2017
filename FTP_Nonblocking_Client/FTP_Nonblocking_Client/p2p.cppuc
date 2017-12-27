#include <iostream>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <cerrno>
#include <limits>
#include <signal.h>

using namespace std;


struct Client_data {
    int descriptor;
    char name[30];
} Client_data;

ssize_t writen(int fd, const void *vptr, size_t n) {

    size_t nleft;
    ssize_t nwritten;
    const char *ptr;

    ptr = (const char *) vptr;
    nleft = n;

    while(nleft > 0) {

        if( (nwritten = write(fd, ptr, nleft)) <= 0) {
            if(nwritten < 0 && errno == EINTR)
                nwritten = 0;
            else
                return (-1);
        }

        nleft -= nwritten;
        ptr += nwritten;
    }


    return n;
}


static int read_cnt;
static char *read_ptr;
static char read_buf[4096];

static ssize_t my_read(int fd, char *ptr) {

    if(read_cnt <= 0) {
    again:
        if( (read_cnt = read(fd, read_buf, sizeof(read_buf))) < 0) {
            if(errno == EINTR)
                goto again;
            return -1;
        }
        else if (read_cnt == 0){
            return 0;
        }


        read_ptr = read_buf;
    }


    read_cnt--;
    *ptr = *read_ptr++;
    return 1;
}


ssize_t readline(int fd, void *vptr, size_t maxlen) {

    ssize_t n, rc;
    char c, *ptr;

    ptr = (char *) vptr;
    for( n = 1; n < maxlen; n++) {
        if( (rc = my_read(fd, &c)) == 1) {
            *ptr++ = c;
            if(c == '\n')
                break;
        }
        else if(rc == 0) {
            *ptr = 0;
            return n-1;
        }
        else {
            return -1;
        }

    }

    *ptr = 0;
    return n;
}

void str_cli(FILE *fp, int sockfd) {
    int maxfdp1, stdineof;
    fd_set rset;
    char buf[4096];
    ssize_t n;
    stdineof = 0;
    FD_ZERO(&rset);

    for(;;){

        if(stdineof == 0)
            FD_SET(fileno(fp), &rset);
        FD_SET(sockfd, &rset);

        maxfdp1 = max(fileno(fp), sockfd) + 1;
        select(maxfdp1, &rset, NULL, NULL, NULL);

        if(FD_ISSET(sockfd, &rset)) {
            if( (n = read(sockfd, buf, sizeof(buf))) == 0) {
                if(stdineof == 1) {
                    printf("The server has closed the connection. \n");
                    return ;        //normal term.
                }
                else {
                    printf("The server has closed the connection. \n");
                    return ;
                }
            }

            writen(fileno(stdout), buf, n);
        }

        if(FD_ISSET(fileno(fp), &rset)) {
            if( fgets(buf, sizeof(buf), fp) == NULL) {
                stdineof = 1;
                shutdown(sockfd, SHUT_WR);
                FD_CLR(fileno(fp), &rset);
                continue;
            }

            writen(sockfd, buf, strlen(buf));

        }

    }

    return ;


}

int main(int argc, char **argv) {

    int i, maxi, maxfd, listenfd, connfd, sockfd, stdineof = 0;
    int nready;
    struct Client_data client[FD_SETSIZE];
    ssize_t n;
    fd_set rset, allset;
    char buf[4096];
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;
    char client_itself_name[30];
    struct addrinfo hints, *servinfo;

    //save client itself name
    strcpy(client_itself_name, argv[1]);

    //listen socket
/*
    bzero(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if(( getaddrinfo("127.0.0.1", argv[2], &hints,&servinfo)) != 0) {
        fprintf(stderr, "Hetaddrinfo Error \n");
        exit(1);
    }
    listenfd = socket(servinfo->ai_family, servinfo->ai_socktype, 0);
    if (bind(listenfd, (struct sockaddr *) &servinfo->ai_addr, sizeof(servinfo->ai_addr)) < 0) {
        cout << "wrong" << endl;
    }
    listen(listenfd, 8);


*/

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_aton("127.0.0.1", &servaddr.sin_addr);
    servaddr.sin_port = ntohs(atol(argv[2]));
    bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    listen(listenfd, 8);


    //client own command
    FILE *fp = stdin;


    //add socket to set
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    FD_SET(fileno(fp), &allset);
    maxfd = max(fileno(fp), listenfd);

    //other clients handle default
    maxi = -1;
    for(i = 0; i < FD_SETSIZE; i++) {
        client[i].descriptor = -1;
    }


    char message[100];

    //other clients hanle by argv
    for(i = 3; i < argc; i++) {

        //socket setting
        bzero(&hints, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if(( getaddrinfo("127.0.0.1", argv[i], &hints,&servinfo)) != 0) {
            fprintf(stderr, "Hetaddrinfo Error \n");
            exit(1);
        }
        sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, 0);
        client[++maxi].descriptor = sockfd;
        if(connect(sockfd, (struct sockaddr *) servinfo->ai_addr, servinfo->ai_addrlen) ) {
            printf("Error : Command Connect Failed \n");
            cout << argv[i] << endl;
            exit(1);
        }

        //borcast my name to other
        sprintf(message, "%s\n", client_itself_name);
        writen(sockfd, message, strlen(message));

        //get name from them
        if( (n = readline(sockfd, buf, 128)) == 0) { //no connection
            //connect close by client
            cout << "no connection" << endl;
            close(sockfd);
            client[maxi--].descriptor = -1;
        }
        else {
            char *pch = strtok(buf, " \r\n");
            if(pch != NULL)
                strcpy(client[maxi].name, pch);

            //register to allset
            FD_SET(sockfd, &allset);
            maxfd = max(maxfd, sockfd);
        }
    }


    for(;;) {

        //cout << "listen" << endl;
        rset = allset;
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        //cout << "someone select" << endl;
        if(FD_ISSET(listenfd, &rset)) {

            //cout << "some one in" << endl;
            clilen = sizeof(cliaddr);
            connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);

            for(i = 0; i < FD_SETSIZE; i++) {

                // save client fd in
                if(client[i].descriptor < 0) {

                    //get name from them
                    if( (n = readline(connfd, buf, 128)) == 0) { //no connection
                        //connect close by client
                        cout << "no connection" << endl;
                        close(connfd);
                        ///////////////////handle////////////////////////
                    }
                    else {
                        //register to client data set
                        char *pch = strtok(buf, " \r\n");
                        if(pch != NULL)
                            strcpy(client[i].name, pch);
                        client[i].descriptor = connfd;
                                        //cout << client[i].name << " is in." << endl;
                        //share my name to him
                        sprintf(message, "%s\n", client_itself_name);
                        writen(connfd, message, strlen(message));

                    }


                    break;
                }

            }

            if(i == FD_SETSIZE) {
                printf("Too many clients");
                exit(1);
            }

            FD_SET(connfd, &allset);

            if(connfd > maxfd)
                maxfd = connfd;
            if(i > maxi)
                maxi = i;

            if(--nready <= 0)
                continue;
        }

        //check client
        for(i = 0; i <= maxi; i++) {

            if((sockfd = client[i].descriptor) < 0)
                continue;

            if(FD_ISSET(sockfd, &rset)) {

                if( (n = readline(sockfd, buf, 128)) == 0) { //close connection
                    //connect close by client
                    sprintf(message, "%s has left the chat room.\n", client[i].name);
                    writen(fileno(stdout), message, strlen(message));

                    close(sockfd);
                    FD_CLR(sockfd, &allset);
                    client[i].descriptor = -1;
                }
                else {

                   writen(fileno(stdout), buf, strlen(buf));
                } //end

                if(--nready <= 0)
                    break;

            }


        }   //end client check


        //client it self stdin check
        if(FD_ISSET(fileno(fp), &rset)) {

            if( fgets(buf, sizeof(buf), fp) == NULL) {
                stdineof = 1;
                shutdown(sockfd, SHUT_WR);
                FD_CLR(fileno(fp), &rset);
                continue;
            }
            else {
                char *pch = strtok(buf, " \r\n");

                if(pch && strcmp(pch, "ALL") == 0){
                    //boardcast to everyone
                    pch = strtok(NULL, "\r\n");
                    for(int j = 0; j <= maxi; j++) {
                        if(client[j].descriptor < 0) //himself is not in the dataset
                            continue;

                        sprintf(message, "%s SAID %s\n", client_itself_name, pch);
                        writen(client[j].descriptor, message, strlen(message));
                    }

                }
                else if(pch && strcmp(pch, "EXIT") == 0){
                    //connect close by client
                    stdineof = 1;
                    shutdown(sockfd, SHUT_WR);
                    FD_CLR(fileno(fp), &rset);
                    return 0;
                }
                else if(pch){
                    //say to specific person
                    for(int j = 0; j <= maxi; j++) {
                        if(client[j].descriptor < 0)
                            continue;

                        //if it is himself
                        if(strcmp(pch, client_itself_name) == 0) {
                            pch = strtok(NULL, "\r\n");  //message
                            sprintf(message, "%s SAID %s\n", client_itself_name, pch);
                            writen(fileno(stdout), message, strlen(message));
                            break;
                        }

                        //find that one
                        if (strcmp(pch, client[j].name) == 0) {
                            pch = strtok(NULL, "\r\n");  //message
                            sprintf(message, "%s SAID %s\n", client_itself_name, pch);
                            writen(client[j].descriptor, message, strlen(message));
                            break;
                        }
                    }
                }
                else {
                    //wrong command
                }
            }

        }



    }   //end for loop





    return 0;
}
