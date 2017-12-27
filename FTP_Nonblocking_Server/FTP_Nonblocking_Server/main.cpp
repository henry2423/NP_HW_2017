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
#include <fcntl.h>
#include <signal.h>
#include <string>
#include <vector>

#define MAXLINE 8000
using namespace std;

struct Client_data {
    int descriptor;
    char client_name[30];
    bool sleep = 0;
    //file record
    int file_count_ptr = 0;
    off_t client_file_offset;
    
} Client_data;

struct File_str {
    char name[50];
} File_str;

struct Client_files {
    char client_name[30];
    vector<struct File_str> file_name;
    vector<FILE *> file_fd;
    vector<ssize_t> file_size;
} Client_files;

struct File_buf {
    char file_name[50];
    ssize_t file_size;
} File_buf;

static void sig_alrm(int signo) {
    return ;    //just interrupt the recv()
}


int main(int argc, const char * argv[]) {
    
    int maxfd, maxfd_data, connfd, listenfd, val, sockfd, flags;
    int nready, i, maxi;
    ssize_t n, nwritten;
    fd_set rset, wset, allset, listenset, writableset;
    struct Client_data client[FD_SETSIZE];
    vector<struct Client_files> Client_files;
    char file_buf[MAXLINE];
    char file_name[50];
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;
    //struct sockaddr_in client_socket; // = (struct sockaddr_in *) malloc (sizeof(struct sockaddr_in))
    
 
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    connfd = socket(AF_INET, SOCK_STREAM, 0);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);;
    
    if(argc != 2) {
        printf("wrong command\n");
        exit(1);
    }
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    //inet_aton("127.0.0.1", &servaddr.sin_addr);
    //servaddr.sin_port = htons(55600);
    servaddr.sin_port = ntohs(atol(argv[1]));
    
    bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    listen(listenfd, 8);
    
    signal(SIGPIPE, SIG_IGN); //make sure no SIGPIPE happen
    
    maxfd = listenfd;       /* initialize */
    maxi = -1;              /* index into client[] array */
    for (i = 0; i < FD_SETSIZE; i++)
        client[i].descriptor = -1;     /* -1 indicates available entry */
    FD_ZERO(&allset);
    FD_ZERO(&listenset);
    FD_ZERO(&writableset);
    FD_SET(listenfd, &allset);
 

    for ( ; ; ) {
        
        listenset = allset; /* structure assignment */
        writableset = allset;
        
        if( (nready = select(maxfd + 1, &listenset, &writableset, NULL, NULL)) < 0) {
            if(errno == EINTR)
                continue;
            else
                printf("Error on nready\n");
        }
        
        // new client connection
        if(FD_ISSET(listenfd, &listenset)) {
            char buf[50];
            clilen = sizeof(cliaddr);
            connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);
            
            //get client user_name
            if( (n = read(connfd, buf, 50)) == 0) { //no connection
                //connect close by client
                cout << "listenfd connection fail" << endl;
                close(connfd);
            }
            else {
                //connection succeed and setting client
                buf[n] = '\n';
                char *pch = strtok(buf, "\n");
                
                for(i = 0; i < FD_SETSIZE; i++) {
                    if(client[i].descriptor < 0) {
                        client[i].descriptor = connfd;
                        strcpy(client[i].client_name, pch);
                        client[i].sleep = 0;
                        client[i].file_count_ptr = 0;
                        break;
                    }
                }
                cout << client[i].client_name << endl;
                
                
                //give the new client file which got by old same name client
                
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
        }

        
        //receiving data (again and again until client close)
        //start sending data or receiving data
        for(i = 0; i <= maxi; i++) {  /* check all clients for data */
            
            if( (sockfd = client[i].descriptor) < 0)       // no fd save in the client[i]
                continue;
            
            //sending the file to client
            if(FD_ISSET(sockfd, &writableset)) {        //if client have file need to send

                //check if server need to send files to client
                for(int k = 0; k < Client_files.size(); k++) {  //find client files database
                    if( strcmp(Client_files[k].client_name, client[i].client_name) == 0) {
                        if(client[i].file_count_ptr < Client_files[k].file_fd.size())  { //see if client get all the data
                            //send files to client
                            //send file name first and file size
                            if( client[i].client_file_offset == 0 ) {    //start sending new file, so send client file name
                                
                                //wrap data in to struct
                                struct File_buf file_data;
                                strcpy(file_data.file_name, Client_files[k].file_name[client[i].file_count_ptr].name);
                                file_data.file_size = Client_files[k].file_size[client[i].file_count_ptr];
                                printf("Sending file %s, size %zd\n",file_data.file_name, file_data.file_size);

                                ssize_t send_size = 0;
                                if ( (send_size = write(sockfd, &file_data, sizeof(file_data))) < 0) {
                                    if (errno != EWOULDBLOCK) {
                                        printf("write error to file, close socket\n");
                                        close(sockfd);
                                        FD_CLR(sockfd, &allset);
                                        //set client data to default
                                        client[i].descriptor = -1;
                                        break;
                                    }
                                    else {      //EWOULDBLOCK
                                        printf("sockfd might block, send %ld\n", send_size);
                                        break;
                                    }
                                } else {
                                    printf("Sending %zd bytes header\n",send_size);
                                    
                                }
                            }
                            
                            //send data
                            ssize_t send_size = 0;
                            fseek(Client_files[k].file_fd[client[i].file_count_ptr], client[i].client_file_offset, SEEK_SET);
                            ssize_t numbytes;
                            while(!feof(Client_files[k].file_fd[client[i].file_count_ptr])){
                                numbytes = fread(file_buf, sizeof(char), sizeof(file_buf), Client_files[k].file_fd[client[i].file_count_ptr]);
                                //cout << buf << endl;
                                printf("fread %zd bytes, ", numbytes);
                                if ( (send_size = write(sockfd, file_buf, numbytes)) < 0) {
                                    if (errno != EWOULDBLOCK) {
                                        printf("write error to file, close socket\n");
                                        close(sockfd);
                                        FD_CLR(sockfd, &allset);
                                        //set client data to default
                                        client[i].descriptor = -1;
                                        break;
                                    }
                                    else {      //EWOULDBLOCK
                                        printf("sockfd might block, send %ld\n", send_size);
                                        client[i].client_file_offset += send_size;
                                        break;
                                    }
                                } else {
                                    printf("Sending %zd bytes\n",send_size);
                                    client[i].client_file_offset += send_size;
                                    //if file has been sent successfully
                                    if(client[i].client_file_offset == Client_files[k].file_size[client[i].file_count_ptr]) {
                                        //update the client files ptr
                                        client[i].file_count_ptr++;
                                        client[i].client_file_offset = 0;
                                        break;
                                    }
                                }
                            }
                            break;
                        }
                        
                    }
                }
                
            }
            
            //client want send data
            if(FD_ISSET(sockfd, &listenset)) {
                
                //get client file name
                val = fcntl(sockfd, F_GETFL, 0);
                flags = O_NONBLOCK;
                val &= ~flags;
                fcntl(sockfd,F_SETFL,val);
                long file_size;
                long current_file_size = 0;
                struct File_buf file_data;
                
                if( (n = read(sockfd, &file_data, sizeof(file_data))) == 0) { //no connection
                    //connect fail jump outm, clear client
                    cout << "sockfd connection fail" << endl;
                    close(sockfd);
                    FD_CLR(sockfd, &allset);
                    //set client data to default
                    client[i].descriptor = -1;
                    break;
                }
                else {
                    //get file name
                    strcpy(file_name, file_data.file_name);
                    printf("%s ",file_name);
                    file_size = file_data.file_size;
                    printf("%zd\n",file_size);
                }
                
                //change file name temp
                /*
                for(int i = 0 ; i<50; i++) {
                    if(file_name[i] == '\n' || file_name[i] == '\0') {
                        file_name[i] = 'u';
                        file_name[i+1] = '\0';
                        break;
                    }
                }
                */
                FILE * client_file;
                client_file = fopen(file_name, "wb");
                
                //get client file data
                val = fcntl(sockfd, F_GETFL, 0);
                fcntl(sockfd, F_SETFL, val | O_NONBLOCK);
                char to[MAXLINE], fr[MAXLINE];
                char *toiptr, *tooptr, *friptr, *froptr;
                toiptr = tooptr = to;       /* initialize buffer pointers */
                friptr = froptr = fr;
                for(; ;){
                    FD_ZERO(&rset);
                    FD_ZERO(&wset);
                    
                    if (friptr < &fr[MAXLINE])
                        FD_SET(sockfd, &rset);  /* read from socket */
                    if (froptr != friptr)
                        FD_SET(fileno(client_file), &wset);   /* data to write to stdout */
                    
                    maxfd_data = max(sockfd , fileno(client_file)) + 1;
                    select(maxfd_data, &rset, &wset, NULL, NULL);
                    
                    
                    if (FD_ISSET(sockfd, &rset)) {
                        if ( (n = read(sockfd, friptr, &fr[MAXLINE] - friptr)) < 0) {
                            if (errno != EWOULDBLOCK) {
                                printf("read error on socket\n");
                                fclose(client_file);
                                break;
                            }
                        } else if (n == 0) {
                            printf("EOF on socket\n");
                            close(sockfd);
                            FD_CLR(sockfd, &allset);
                            fclose(client_file);
                            //set client data to default
                            client[i].descriptor = -1;
                            break;     /* normal termination */
                            
                        } else {
                            fprintf(stderr, "read %zd bytes from socket\n", n);
                            //cout << friptr << endl;
                            friptr += n;     /* # just read */
                            //FD_SET(fileno(client_file), &wset);     /* try and write below */
                        }
                    }
                    
                    if (FD_ISSET(fileno(client_file), &wset) && ((n = friptr - froptr) > 0)) {

                        if ( (nwritten = write(fileno(client_file), froptr, n)) < 0) {
                            if (errno != EWOULDBLOCK) {
                                printf("write error to file");
                                fclose(client_file);
                                break;
                            }
                        } else {
                            fprintf(stderr, "wrote %zd bytes to file\n", nwritten);
                            froptr += nwritten; /* # just written */
                            if (froptr == friptr)
                                froptr = friptr = fr;   /* back to beginning of buffer */
                            
                            current_file_size += nwritten;
                            if(current_file_size == file_size) {
                                printf("EOF on file\n");
                                fclose(client_file);
                                
                                //record the file to the same name client
                                client[i].file_count_ptr++;     //itself client already has the file
                                //record the file in client_files structure
                                int j = 0;
                                for(j = 0; j < Client_files.size(); j++) {
                                    //the client_files have the data
                                    if(strcmp(Client_files[j].client_name, client[i].client_name) == 0) {
                                        struct File_str str;
                                        strcpy(str.name, file_name);
                                        Client_files[j].file_name.push_back(str);
                                                        Client_files[j].file_fd.push_back(fopen(file_name, "rb"));
                                        fseek(Client_files[j].file_fd.back(), 0, SEEK_END);
                                        long file_size = ftell(Client_files[j].file_fd.back());
                                        fseek(Client_files[j].file_fd.back(), 0, SEEK_SET);
                                        Client_files[j].file_size.push_back(file_size);
                                        break;
                                    }
                                }
                                //if can't find in client_files, then create a new one
                                if(j == Client_files.size()) {
                                    struct Client_files temp_files;
                                    struct File_str str;
                                    strcpy(str.name, file_name);
                                    temp_files.file_name.push_back(str);
                                    strcpy(temp_files.client_name, client[i].client_name);
                                    temp_files.file_fd.push_back(fopen(file_name, "rb"));
                                    fseek(temp_files.file_fd.back(), 0, SEEK_END);
                                    long file_size = ftell(temp_files.file_fd.back());
                                    fseek(temp_files.file_fd.back(), 0, SEEK_SET);
                                    temp_files.file_size.push_back(file_size);
                                    
                                    Client_files.push_back(temp_files);
                                }
                                break;     /* normal termination */
                            }
                        }
                    }
                    
                }
                
                
      
                if(--nready <= 0)       /* no more readable descriptors, redo the loop*/
                    break;
            }
        }
        
    }
    
    
    return 0;
}


