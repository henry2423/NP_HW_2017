//
//  main.cpp
//  chatroom_cli
//
//  Created by Henry on 27/10/2017.
//  Copyright © 2017 henry. All rights reserved.
//

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

using namespace std;

ssize_t writen(int fd, const void *vptr, size_t n) {                        /* Write "n" bytes to a descriptor. */
    size_t nleft;
    ssize_t nwritten;
    const char *ptr;
    
    ptr = (const char *) vptr;
    nleft = n;
    while (nleft > 0) {
        if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
            if (nwritten < 0 && errno == EINTR)
                nwritten = 0;   /* and call write() again */
            else
                return (-1);    /* error */
        }
        
        nleft -= nwritten;
        ptr += nwritten;
    }
    
    return (n);
}

static int read_cnt;
static char *read_ptr;
static char read_buf[4096];

static ssize_t my_read(int fd, char *ptr) {
    
    if(read_cnt <= 0) {
    again:
        if( (read_cnt = read(fd, read_buf, sizeof(read_buf))) < 0) {        //read all the command in
            if(errno == EINTR) {
                goto again;
            }
            return -1;
        }else if ( read_cnt == 0) {     //if no word in
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
        if( (rc = my_read(fd, &c)) == 1) {          //read a word in
            *ptr++ = c;
            if(c == '\n')       //when encounter a \n
                break;
        } else if (rc == 0) {      //when no more word in
            *ptr = 0;
            return n-1;
        } else {                    //something wrong
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
    
    for ( ; ; ) {
        
        if (stdineof == 0)
            FD_SET(fileno(fp), &rset);
        FD_SET(sockfd, &rset);
        maxfdp1 = max(fileno(fp), sockfd) + 1;
        select(maxfdp1, &rset, NULL, NULL, NULL);
        
        if (FD_ISSET(sockfd, &rset)) { /* socket is readable */
            if ( (n = read(sockfd, buf, sizeof(buf))) == 0 ) {
                if (stdineof == 1)
                    return ;     /* normal termination */
                else {
                    printf("str_cli: server terminated prematurely \n");
                    exit(1);
                }
            }
            //fputs(buf, stdout);
            writen(fileno(stdout), buf, n);
        }
        
        if (FD_ISSET(fileno(fp), &rset)) { /* input is readable */
            if ( fgets(buf, sizeof(buf), fp) == NULL ) {
                stdineof = 1;
                shutdown(sockfd, SHUT_WR); /* send FIN */
                FD_CLR(fileno(fp), &rset);
                continue;
            }
            
            //buf[n] = '\0';
            //User can type command below to terminate the process at any time
            if(strcmp(buf, "exit\n") == 0) {
                stdineof = 1;
                shutdown(sockfd, SHUT_WR); /* send FIN */
                FD_CLR(fileno(fp), &rset);
                continue;
            }
            
            writen(sockfd, buf, strlen(buf));
        }
    }
}

int main(int argc, const char * argv[]) {
    
    int sockfd;
    //struct sockaddr_in servaddr;
    struct addrinfo hints, *servinfo;
    
    if(argc != 3) {
        printf("The arguments are Wrong.\n");
        exit(1);
    }
    
    
    signal(SIGPIPE, SIG_IGN); //make sure no SIGPIPE happen
    //sockfd = socket(AF_INET, SOCK_STREAM, 0);
    //bzero(&servaddr, sizeof(servaddr));
    //servaddr.sin_family = AF_INET;
    //servaddr.sin_port = htons(atoi(argv[2]));
    //inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
    
    bzero(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    if (( getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
        fprintf(stderr, "Hetaddrinfo Error\n");
        exit(1);
    }
    
    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, 0);

    if ( connect(sockfd, (struct sockaddr *) servinfo->ai_addr , servinfo->ai_addrlen) ) {
        printf("Error : Connect Failed \n");
        exit(1);
    }
    
    str_cli(stdin, sockfd);

    
    return 0;
}
