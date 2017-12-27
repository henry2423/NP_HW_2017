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

static void sig_alrm(int signo) {
    return ;    //just interrupt the recv()
}

int main(int argc, const char * argv[]) {
    
    int maxfd, udpsockfd;
    int nready;
    ssize_t recv_size;
    fd_set rset, allset;
    char buf[1461];
    char file_name[50];
    struct sockaddr_in servaddr;
    struct sockaddr_in client_socket; // = (struct sockaddr_in *) malloc (sizeof(struct sockaddr_in))
    
    udpsockfd = socket(AF_INET, SOCK_DGRAM, 0);
    
    if(argc != 2) {
        printf("wrong command\n");
        exit(1);
    }
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    //inet_aton("127.0.0.1", &servaddr.sin_addr);
    servaddr.sin_port = ntohs(atol(argv[1]));
    
    bind(udpsockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    
    signal(SIGPIPE, SIG_IGN); //make sure no SIGPIPE happen
    signal(SIGALRM, sig_alrm);
    siginterrupt(SIGALRM, 1);
    
    FD_ZERO(&allset);
    
    maxfd = udpsockfd;       /* initialize */
    FD_SET(udpsockfd, &allset);
    
    
    struct msghdr msg_send;
    struct msghdr msg_receive;
    
    
    
    for ( ; ; ) {
        
        rset = allset; /* structure assignment */
        int client_seq = 0;
        //int client_ack = 0;
        int server_seq = 0;
        int server_ack = 0;
        struct iovec io_send;
        struct iovec io_receive[2];
        struct iovec io_handshake;
        
        if( (nready = select(maxfd + 1, &rset, NULL, NULL, NULL)) < 0) {
            if(errno == EINTR)
                continue;
            else
                printf("Error on nready\n");
        }
        
        
        
        //new client in -> three way hand shake first
        if(FD_ISSET(udpsockfd, &rset)) {  // new client connection
            
            //receive seq0
            bzero(&msg_receive,sizeof(struct msghdr));
            msg_receive.msg_name = &client_socket;
            msg_receive.msg_namelen = sizeof(client_socket);
            io_receive[0].iov_base = &client_seq;
            io_receive[0].iov_len = sizeof(int);
            io_receive[1].iov_base = &file_name;
            io_receive[1].iov_len = 49;
            msg_receive.msg_iov = io_receive;
            msg_receive.msg_iovlen = 2;
            recv_size = recvmsg(udpsockfd, &msg_receive, 0);
            char ip[16];
            inet_ntop(AF_INET, &(client_socket.sin_addr), ip, sizeof(ip));
            int port = ntohs(client_socket.sin_port);
            printf("get message from %s[%d]: %d, %s\n", ip, port, client_seq, file_name);
            
            
            //ack client
            bzero(&msg_send,sizeof(struct msghdr));
            msg_send.msg_name = &client_socket;
            msg_send.msg_namelen = sizeof(client_socket);
            io_handshake.iov_base = &server_ack;
            io_handshake.iov_len = sizeof(int);
            msg_send.msg_iov = &io_handshake;
            msg_send.msg_iovlen = 1;
            sendmsg(udpsockfd, &msg_send, 0);
            
            
        }
        
        
        FILE * client_file;
        /*
         for(int i = 0 ; i<50; i++) {
         if(file_name[i] == '\n' || file_name[i] == '\0') {
         file_name[i] = 'u';
         file_name[i+1] = '\0';
         break;
         }
         }
         */
        client_file = fopen(file_name, "wb");
        
        //receiving data (again and again until client close)
        //start sending data
        bool end_connection = false;
        int client_may_close_connection = 0;
        while(1){
            
            //receive the data from client
            bzero(&msg_receive,sizeof(struct msghdr));
            msg_receive.msg_name = &client_socket;
            msg_receive.msg_namelen = sizeof(struct sockaddr_in);
            io_receive[0].iov_base = &client_seq;
            io_receive[0].iov_len = sizeof(int);
            io_receive[1].iov_base = &buf;
            io_receive[1].iov_len = 1460;
            msg_receive.msg_iov = io_receive;
            msg_receive.msg_iovlen = 2;
            
            alarm(2);
            if( (recv_size = recvmsg(udpsockfd, &msg_receive, 0)) < 0) {
                if(errno == EINTR) {
                    printf("Socket timeout\n");
                    //chech if it is the end of packet
                    server_ack = server_seq;
                    client_may_close_connection++;
                    if(client_may_close_connection == 5)       //client has close the connection
                        break;
                    goto resend;
                }
                else {
                    printf("Socket wrong\n");
                    break;
                }
            }
            else {
                alarm(0);
                printf("client_seq: %d, ", client_seq);
                //std::cout << recv_size << " " << client_seq << " ";
            }
            
            
            //chech if it is the end of packet
            if(client_seq == -1) {
                server_ack = -1;
                end_connection = true;
            }
            else {
                //record the packet order
                if(server_seq == client_seq - 1) {        //order packet in
                    server_seq = client_seq;
                    server_ack = client_seq;
                    buf[recv_size-4] = 0;
                    fwrite(buf, recv_size-4, 1, client_file);
                }
                else if(server_seq >= client_seq ) {      //un-order packet -> re-send packet -> lose it
                    server_ack = server_seq;
                }
                else if(server_seq < client_seq) {        //someting between seq was lose
                    server_ack = server_seq;
                }
            }
            
        resend:
            //send ack to client
            bzero(&msg_send,sizeof(struct msghdr));
            msg_send.msg_name = &client_socket;
            msg_send.msg_namelen = sizeof(client_socket);
            io_send.iov_base = &server_ack;
            io_send.iov_len = sizeof(server_ack);
            msg_send.msg_iov = &io_send;
            msg_send.msg_iovlen = 1;
            sendmsg(udpsockfd, &msg_send, 0);
            printf("server_ack: %d\n", server_ack);
            //std::cout << server_ack << " " << std::endl;
            
            if(end_connection)
                break;
            
        }
        
        //close connection
        while(1){
            
            if(client_may_close_connection == 5){       //client has close the connection
                printf("Socket Terminate\n");
                break;
            }
            //receive the data from client
            bzero(&msg_receive,sizeof(struct msghdr));
            msg_receive.msg_name = &client_socket;
            msg_receive.msg_namelen = sizeof(struct sockaddr_in);
            io_receive[0].iov_base = &client_seq;
            io_receive[0].iov_len = sizeof(int);
            io_receive[1].iov_base = &buf;
            io_receive[1].iov_len = 1460;
            msg_receive.msg_iov = io_receive;
            msg_receive.msg_iovlen = 2;
            
            alarm(3);
            if( (recv_size = recvmsg(udpsockfd, &msg_receive, 0)) < 0) {
                if(errno == EINTR) {
                    printf("Socket Terminate\n");
                    //connection maybe close
                    break;
                }
                else {
                    printf("Socket wrong\n");
                    break;
                }
            }
            else {
                alarm(0);
                //client don't get the -1 packet
                server_ack = -1;
            }
            
            //send ack to client
            bzero(&msg_send,sizeof(struct msghdr));
            msg_send.msg_name = &client_socket;
            msg_send.msg_namelen = sizeof(client_socket);
            io_send.iov_base = &server_ack;
            io_send.iov_len = sizeof(server_ack);
            msg_send.msg_iov = &io_send;
            msg_send.msg_iovlen = 1;
            sendmsg(udpsockfd, &msg_send, 0);
            printf("server_ack: %d\n", server_ack);
            //std::cout << server_ack << " " << std::endl;
            
        }
        
        fclose(client_file);
    }
    
    
    return 0;
}

