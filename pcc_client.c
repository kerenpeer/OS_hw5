#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <errno.h> 
 
 
 
 int main(int argc, char *argv[]){
     uint32_t ip, N, C;
     uint16_t port;
     char *file_path, *ip_address, *N_transfer,*N_transfer_back, write_Buff[1024];
     FILE *file;
     int conv, N_bytes_Left, rc, sockfd = -1;
     ssize_t read_b, write_b;
     void *pointer;
     int32_t nboC;
 

     if(argc != 4){
       perror("Wrong amount of parameters for client");
       exit(1);  
     }
     ip_address = argv[1];
     port = atoi(argv[2]);
     file_path = argv[3];
     
     //Open the specified file for reading
     file = fopen(file_path,"rb");
     if(file == NULL){
         perror("Failed to open file in client");
         exit(1); 
        }
    //Create a TCP connection to the specified server port on the specified server IP.
    struct sockaddr_in serv_addr; // where we Want to get to
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("\n Error : Could not create socket \n");
        exit(1);
    }
    if((conv = inet_pton(AF_INET,ip_address,&ip)) != 1){
        if(conv == 0){
            perror("\n Error : src does not contain a character string representing a valid network address in thespecified address family \n");
        }
        if(conv == -1){
            perror("\n Error :  af does not contain a valid address family \n");
        }
        exit(1);
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = ip; 
    if(connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0){
        perror("\n Error : Connect Failed. \n");
        exit(1);
    }

    //Transfer the contents of the file to the server over the TCP connection
    //compute N- the size of the file
    fseek(file, 0L, SEEK_END);
    N = (uint32_t)ftell(file);
    fseek(file, 0L, SEEK_SET);
    // N is in network byte order
    N = htonl(N);

    //The client sends the server N
    N_transfer = (char*)&N;
    N_bytes_Left = 0;
    while(N_bytes_Left < N){
        rc = write(sockfd, N_transfer+N_bytes_Left, N-N_bytes_Left);
        if(rc < 0){
            perror("\n Error : failed to write N in cilent");
            exit(1);
        }
        N_bytes_Left += rc;
    }
    //Send the server N bytes - the file’s content
    //First - read file content to buffer
    read_b = fread(write_Buff,1,sizeof(write_Buff), file);
    //finished reading file into buffer
    if(read_b != sizeof(write_Buff)){
        perror("\n Error : An error has occured or EOF in cilent");
        exit(1); 
    }
    pointer = write_Buff;
    while(read_b > 0){
        write_b = write(sockfd, pointer, read_b);
        if(write_b <=0){
            perror("\n Error : problem in writing file to socket in cilent");
            exit(1);
        }
        read_b -= write_b;
        pointer += write_b;
    }
    //The server sends the client C, the number of printable characters
    // read data from server into recv_buff block until there's something to read
    N_transfer_back = (char*)&nboC; 
    N_bytes_Left = 0;
    while(N_bytes_Left < N){
        read_b = read(sockfd, N_transfer_back+N_bytes_Left, N-N_bytes_Left);
        if( read_b <= 0 ){
            perror("\n Error : problem in reading C from socket in cilent");
            exit(1);
        }
         N_bytes_Left += read_b;
    }
    fclose(file);
    close(sockfd);
    C = ntohl(nboC);
    printf("# of printable characters: %u\n", C);
    exit(0);
 }