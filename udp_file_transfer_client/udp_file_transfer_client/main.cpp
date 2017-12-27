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
    
    int sockfd;
    ssize_t recv_size;
    struct sockaddr_in servaddr;
    struct sockaddr_in clientaddr;
    struct timeval tv;
    
    signal(SIGPIPE, SIG_IGN); //make sure no SIGPIPE happen
    signal(SIGALRM, sig_alrm);
    siginterrupt(SIGALRM, 1);
    
    if(argc != 4) {
        printf("wrong command\n");
        exit(1);
    }
    
    tv.tv_sec = 0;
    tv.tv_usec = 60000;
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_aton(argv[1], &servaddr.sin_addr);
    servaddr.sin_port = ntohs(atol(argv[2]));
    
    
    FILE * client_file;
    char file_name[50];
    client_file = fopen(argv[3], "rb");
    if (client_file == NULL) {
        fputs ("File error",stderr);
        exit (1);
    }
    
    strcpy(file_name, argv[3]);
    
    int client_seq = 0;
    //int client_ack;
    //int server_seq;
    int server_ack;
    struct msghdr msg_send;
    struct msghdr msg_receive;
    struct iovec io_send[2];
    struct iovec io_receive;
    //three way handshake
    
resend_handshake:
    //making packet
    bzero(&msg_send,sizeof(struct msghdr));
    msg_send.msg_name = &servaddr;
    msg_send.msg_namelen = sizeof(servaddr);
    io_send[0].iov_base = &client_seq;
    io_send[0].iov_len = sizeof(client_seq);
    io_send[1].iov_base = &file_name;
    io_send[1].iov_len = 50;
    msg_send.msg_iov = io_send;
    msg_send.msg_iovlen = 2;
    sendmsg(sockfd, &msg_send, 0);
    

    //waiting server ack
    bzero(&msg_receive,sizeof(struct msghdr));
    msg_receive.msg_name = &clientaddr;
    msg_receive.msg_namelen = sizeof(clientaddr);
    io_receive.iov_base = &server_ack;
    io_receive.iov_len = sizeof(server_ack);
    msg_receive.msg_iov = &io_receive;
    msg_receive.msg_iovlen = 1;
    
    
    recv_size = recvmsg(sockfd, &msg_receive, 0);
    if(recv_size < 0) {
        if(errno == EWOULDBLOCK) {
            printf("Socket timeout\n");
            goto resend_handshake;
        }
        else {
            printf("Socket error\n");
            exit(1);
        }
    }
    else {
        std::cout << server_ack << " " << std::endl;
    }
    
   
    
    
    //start sending data
    long read_size = 1460;
    fseek(client_file, 0, SEEK_END);
    long file_size = ftell(client_file);
    fseek(client_file, 0, SEEK_SET);
    bool end = false;
    char *buf;
    
    while( !end ){
        
        if(server_ack != client_seq) {        //if server_ack != client_seq server lost packet
            fseek (client_file, 0, SEEK_END);
            file_size = ftell(client_file) - (long )((server_ack)*1460);
            fseek(client_file, (server_ack)*1460, SEEK_SET);
            read_size = 1460;
            client_seq = server_ack;
            std::cout << "resend" << file_size << " ";
        }
        
        if(file_size == 0) {
            read_size = 0;
            client_seq = -1;
        }
        else if(file_size < read_size) {
            client_seq++;
            read_size = file_size;
        }
        else {
            client_seq++;
        }
        
        buf = (char*) malloc (sizeof(char)*read_size);
        fread(buf, 1, read_size, client_file);
        printf("%ld",read_size);
        std::cout << read_size << " ";
        
        //send to server
    resend:
        bzero(&msg_send,sizeof(struct msghdr));
        msg_send.msg_name = &servaddr;
        msg_send.msg_namelen = sizeof(servaddr);
        io_send[0].iov_base = &client_seq;
        io_send[0].iov_len = sizeof(client_seq);
        io_send[1].iov_base = buf;
        io_send[1].iov_len = read_size;
        msg_send.msg_iov = io_send;
        msg_send.msg_iovlen = 2;
        sendmsg(sockfd, &msg_send, 0);
        
        std::cout << client_seq << " ";
        
        //waiting for ack -> timeout handle
        bzero(&msg_receive,sizeof(struct msghdr));
        msg_receive.msg_name = &clientaddr;
        msg_receive.msg_namelen = sizeof(clientaddr);
        io_receive.iov_base = &server_ack;
        io_receive.iov_len = sizeof(server_ack);
        msg_receive.msg_iov = &io_receive;
        msg_receive.msg_iovlen = 1;
        
        recv_size = recvmsg(sockfd, &msg_receive, 0);
        if(recv_size < 0 && server_ack != -1) {
            if(errno == EWOULDBLOCK) {
                printf("Socket timeout\n");
                if(client_seq == -1)
                    break;
                goto resend;
            }
            else {
                printf("Socket error\n");
                exit(1);
            }
        }
        else {
            std::cout << server_ack << " " << std::endl;
            if(server_ack == -1)
                break;
        }
        
        file_size = file_size - read_size;
        free(buf);
    }
    
    
    close(sockfd);
    fclose(client_file);
    return 0;
}


