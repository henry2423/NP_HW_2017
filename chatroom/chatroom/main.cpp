//
//  main.cpp
//  chatroom
//
//  Created by Henry on 22/10/2017.
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
#include <signal.h>

//client database
struct Client_data {
    int descriptor;
    char client_name[30];
    char client_ip[30];
    int client_port;
    bool allow_bit = 0;
    char allow_name[50];
} Client_data;

/* Write "n" bytes to a descriptor. */
ssize_t writen(int fd, const void *vptr, size_t n) {
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
        if( (read_cnt = read(fd, read_buf, sizeof(read_buf))) < 0) {
            if(errno == EINTR) {
                goto again;
            }
            return -1;
        }else if ( read_cnt == 0) {
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
        } else if (rc == 0) {
            *ptr = 0;
            return n-1;
        } else {
            return -1;
        }
    }
    
    *ptr = 0;
    return n;
}

void who_handler(struct Client_data *client, int maxi, int sockfd) {
    
    char message[80];
    //search who are online, and write it to "sockfd" itself
    for(int j = 0; j <= maxi; j++) {
        
        if(client[j].descriptor < 0)   // no fd save in the client[i].descriptor or the sockfd no need to send
            continue;
        
        //if it is sockfd itself, have to show the -> me. Otherwise, show it w/o ->me
        if( client[j].descriptor == sockfd ) {
            sprintf(message, "[Server] %s %s/%d ->me \n", client[j].client_name, client[j].client_ip, client[j].client_port);
            writen(sockfd, message, strlen(message));
        }
        else {
            sprintf(message, "[Server] %s %s/%d \n", client[j].client_name, client[j].client_ip, client[j].client_port);
            writen(sockfd, message, strlen(message));
        }
        
    }
}

void user_name_handler(char *pch, struct Client_data *client, int maxi, int sockfd, int client_ownself){
    
    char message[80];
    pch = strtok(NULL, "\r\n");
    
    //make sure it is the right command
    if(pch == NULL) {
        writen(sockfd, "[Server] ERROR: Error command. \n", strlen("[Server] ERROR: Error command. \n"));
        return ;           // back to loop check other sockfd
    }
    
    int sizeof_pch = 0;
    
    for(int j = 0; pch[j] != '\0'; j++) //計算pch的長度
    {
        sizeof_pch = j+1;
    }
    
    //name rule check
    //1. not anonymous
    if(strcmp(pch, "anonymous") == 0) {
        writen(sockfd, "[Server] ERROR: Username cannot be anonymous. \n", strlen("[Server] ERROR: Username cannot be anonymous. \n"));
        return ;
    }
    
    
    //2. 2~12 English letters
    if(sizeof_pch < 2 || sizeof_pch > 12) {
        writen(sockfd, "[Server] ERROR: Username can only consists of 2~12 English letters. \n", strlen("[Server] ERROR: Username can only consists of 2~12 English letters. \n"));
        return ;
    }
    //check if it is English letters
    for(int j = 0; j < sizeof_pch; j++) {
        if(('a' <= pch[j] && pch[j] <= 'z') ||('A' <= pch[j] && pch[j] <= 'Z')) {
            continue;
        }
        else {
            writen(sockfd, "[Server] ERROR: Username can only consists of 2~12 English letters. \n", strlen("[Server] ERROR: Username can only consists of 2~12 English letters. \n"));
            return ;
        }
    }

    
    //3. has to be a unique name
    //serach and check other client name
    for(int j = 0; j <= maxi; j++) {
        if(client[j].descriptor < 0 || client[j].descriptor == sockfd)   // not the listenfd or itself sockfd
            continue;
        
        //if name conflict with other, have to save no
        if(strcmp(client[j].client_name, pch) == 0) {
            sprintf(message, "[Server] ERROR: %s has been used by others. \n", pch);
            writen(sockfd, message, strlen(message));
            return ;
        }
    }

    
    //if pass all 3 rules, then chage the name for user, and tell others
    //send the change message to it
    sprintf(message, "[Server] You're now known as %s. \n", pch);
    writen(sockfd, message, strlen(message));
    
    //tell other users
    sprintf(message, "[Server] %s is now known as %s. \n", client[client_ownself].client_name, pch);
    for(int j = 0; j <= maxi; j++) {
        if(client[j].descriptor < 0 || client[j].descriptor == sockfd)   // not the listenfd or itself sockfd
            continue;
        
        //send the message the other
        writen(client[j].descriptor, message, strlen(message));
    }
    
    
    
    //chage the name officially
    sprintf(client[client_ownself].client_name, "%s", pch);
    
    return ;

}

void private_message_handler(char *pch, struct Client_data *client, int maxi, int sockfd, int client_ownself){
    
    char message[4096];
    //cut the username and message
    pch = strtok(NULL, " ");
    char *username = pch;
    pch = strtok(NULL, "\r\n");  //make sure all the message was gotten
    
    //make sure it is the right command
    if(username == NULL || pch == NULL) {
        writen(sockfd, "[Server] ERROR: Error command. \n", strlen("[Server] ERROR: Error command. \n"));
        return ;           // back to loop check other sockfd
    }
    
    //If the sender’s name is anonymous
    if(strcmp(client[client_ownself].client_name, "anonymous") == 0) {
        writen(sockfd, "[Server] ERROR: You are anonymous. \n", strlen("[Server] ERROR: You are anonymous. \n"));
        return ;           // back to loop check other sockfd
    }
    
    //If the receiver’s name is anonymous
    if(strcmp(username, "anonymous") == 0) {
        writen(sockfd, "[Server] ERROR: The client to which you sent is anonymous. \n", strlen("[Server] ERROR: The client to which you sent is anonymous. \n"));
        return ;           // back to loop check other sockfd
    }
    
    int j = 0;
    //find the receiver for sending message
    for(j = 0; j <= maxi; j++) {
        if(client[j].descriptor < 0)   // not the listenfd
            continue;
        
        //if find the user name match in the client
        if(strcmp(client[j].client_name, username) == 0) {
            
            //check if client you send to have limit
            if(client[j].allow_bit == 1) {
                
                // if you are the one client allow
                if(strcmp(client[j].allow_name, client[client_ownself].client_name) == 0) {
                    //to sender
                    writen(sockfd, "[Server] SUCCESS: Your message has been sent. \n", strlen("[Server] SUCCESS: Your message has been sent. \n"));
                    
                    //to receiver
                    sprintf(message, "[Server] %s tell you %s \n", client[client_ownself].client_name, pch);
                    writen(client[j].descriptor, message, strlen(message));
                }
                else {
                    //to sender
                    sprintf(message, "[Server] you are not allow to tell %s anything\n",client[j].client_name);
                    writen(sockfd, message, strlen(message));
                }
            }
            else {
                //to sender
                writen(sockfd, "[Server] SUCCESS: Your message has been sent. \n", strlen("[Server] SUCCESS: Your message has been sent. \n"));
                
                //to receiver
                sprintf(message, "[Server] %s tell you %s \n", client[client_ownself].client_name, pch);
                writen(client[j].descriptor, message, strlen(message));
            }
            
            return ;
        }
    }
    
    //If the receiver doesn’t exist
    if(j == maxi + 1) {
        writen(sockfd, "[Server] ERROR: The receiver doesn't exist. \n", strlen("[Server] ERROR: The receiver doesn't exist. \n"));
        return ;           // back to loop check other sockfd
    }
    
    return ;
}

void broadcast_message_handler(char *pch, struct Client_data *client, int maxi, int sockfd, int client_ownself) {
    
    char message[4096];
    pch = strtok(NULL, "\r\n");  //make sure all the message was gotten

    //make sure it is the right command
    if(pch == NULL) {
        writen(sockfd, "[Server] ERROR: Error command. \n", strlen("[Server] ERROR: Error command. \n"));
        return ;           // back to loop check other sockfd
    }
    
    //yell message to everyone in the room
    for(int j = 0; j <= maxi; j++) {
        if(client[j].descriptor < 0)   // not the listenfd or itself sockfd
            continue;
        
        //tell them what sender yelling at
        sprintf(message, "[Server] %s yell %s \n", client[client_ownself].client_name, pch);
        writen(client[j].descriptor, message, strlen(message));
    }
    
    return ;
}
int main(int argc, const char * argv[]) {
    
    int i, maxi, maxfd, listenfd, connfd, sockfd;
    int nready;
    struct Client_data client[FD_SETSIZE];
    ssize_t n;
    fd_set rset, allset;
    char buf[4096];
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    //servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    inet_aton("127.0.0.1", &servaddr.sin_addr);
    servaddr.sin_port = htons(55688);
    bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    listen(listenfd, 8);
    
    
    maxfd = listenfd;       /* initialize */
    maxi = -1;              /* index into client[] array */
    for (i = 0; i < FD_SETSIZE; i++)
        client[i].descriptor = -1;     /* -1 indicates available entry */
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    
    char hello_message[50];
    char comming_message[50] = "[Server] Someone is coming! \n";
    char message[80];
    
    signal(SIGPIPE, SIG_IGN); //make sure no SIGPIPE happen
    
    
    for ( ; ; ) {
        rset = allset; /* structure assignment */
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        
        if(FD_ISSET(listenfd, &rset)) {  // new client connection
            
            clilen = sizeof(cliaddr);
            connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);
            
            for(i = 0; i < FD_SETSIZE; i++) {
                if(client[i].descriptor < 0) {
                    
                    // say hello message
                    //printf("someone have connection\n");
                    sprintf(hello_message, "[Server] Hello, anonymous! From: %s/%d \n", inet_ntoa(cliaddr.sin_addr), (int) ntohs(cliaddr.sin_port));
                    writen(connfd, hello_message, strlen(hello_message));
                    //printf("%s",hello_message);
                    //tell others in connection that someone is in
                    for(int j = 0; j <= maxi; j++) {
                        if(client[j].descriptor < 0)   // no fd save in the client[i].descriptor
                            continue;
                        writen(client[j].descriptor, comming_message, strlen(comming_message));
                    }
                    
                    //save the connect to the client, default name to anonymous
                    client[i].descriptor = connfd;
                    sprintf(client[i].client_name, "anonymous");
                    sprintf(client[i].client_ip, "%s", inet_ntoa(cliaddr.sin_addr));
                    client[i].client_port = (int) ntohs(cliaddr.sin_port);
                    client[i].allow_bit = 0;
                    break;
                }
            }
            
            if(i == FD_SETSIZE) {
                printf("too many clients");
                exit(1);
            }
            
            FD_SET(connfd, &allset);        //add new descriptor to set
            
            if(connfd > maxfd)
                maxfd = connfd;         /* for select */
            if(i > maxi)
                maxi = i;           /* max index in client[] array */
            
            if(--nready <= 0)
                continue;           /* no more readable descriptors, redo the loop*/
            
        }
        
        for(i = 0; i <= maxi; i++) {  /* check all clients for data */
            
            if( (sockfd = client[i].descriptor) < 0)       // no fd save in the client[i]
                continue;
            
            if(FD_ISSET(sockfd, &rset)) {
                if( (n = readline(sockfd, buf, 128)) == 0) {
                    //tell other client someone offline
                    sprintf(message, "[Server] %s is offline. \n", client[i].client_name);
                    for(int j = 0; j <= maxi; j++) {
                        if(client[j].descriptor < 0 || client[j].descriptor == sockfd)   // no fd save in the client[i].descriptor or the sockfd no need to send
                            continue;
                        writen(client[j].descriptor, message, strlen(message));
                    }
                    
                    //connection closed by client
                    close(sockfd);
                    FD_CLR(sockfd, &allset);
                    //set client data to default
                    client[i].descriptor = -1;
                    client[i].allow_bit = 0;
                    //sprintf(client[i].client_name, "anonymous");
                }
                else {  //command data in
                    char *pch = strtok(buf, " \r\n");
                    
                    //who message handle
                    if(pch && strcmp(pch, "who") == 0) {
                       // who handler
                        who_handler(client, maxi, sockfd);
                    }
                    else if(pch && strcmp(pch, "name") == 0) {
                        //username handler
                        user_name_handler(pch, client, maxi, sockfd, i);
                    }
                    //private message sending
                    else if(pch && strcmp(pch, "tell") == 0) {
                        //private message handler
                        private_message_handler(pch, client, maxi, sockfd, i);
                    }
                    //boradcast message
                    else if(pch && strcmp(pch, "yell") == 0) {
                        //broadcast meaage handler
                        broadcast_message_handler(pch, client, maxi, sockfd, i);
                    }
                    else if(pch && strcmp(pch, "allow") == 0) {
                        pch = strtok(NULL, "\r\n");
                        client[i].allow_bit = 1;
                        strcpy(client[i].allow_name, pch);
                        sprintf(message, "[Server] You're now only allow %s send you message. \n", pch);
                        writen(sockfd, message, strlen(message));
                    }
                    else if(pch == NULL) {
                        //press enter, do nothing
                    }
                    else {
                        writen(sockfd, "[Server] ERROR: Error command. \n", strlen("[Server] ERROR: Error command. \n"));
                    }
                    /*
                    else if( pch ) {
                        pch = strtok(buf, " ");
                        
                        //change username message
                        if(pch && strcmp(pch, "name") == 0) {
                            //username handler
                            user_name_handler(pch, client, maxi, sockfd, i);
                        }
                        //private message sending
                        else if(pch && strcmp(pch, "tell") == 0) {
                            //private message handler
                            private_message_handler(pch, client, maxi, sockfd, i);
                        }
                        //boradcast message
                        else if(pch && strcmp(pch, "yell") == 0) {
                            //broadcast meaage handler
                            broadcast_message_handler(pch, client, maxi, sockfd, i);
                        }
                        else {
                            writen(sockfd, "[Server] ERROR: Error command. \n", strlen("[Server] ERROR: Error command. \n"));
                        }
                        
                    }
                    else {
                        //writen(sockfd, "[Server] ERROR: Error command. \n", strlen("[Server] ERROR: Error command. \n"));
                    }
                    */
                }
              
                
                if(--nready <= 0)       /* no more readable descriptors, redo the loop*/
                    break;
            }
        }
        
    }
        
        
        
    return 0;
}
