#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <assert.h>
#include <signal.h>

int active_client = 0; 
int kill_serv = 0;
//Initialize a data structure pcc_total - for ASCII char number i, pcc_total[i] will keep the number of observations from all clients.
uint32_t pcc_total[127] = {0};

void kill_print(void){
    int i;
    for(i = 32; i < 127; i++){
        printf("char '%c' : %u times\n", i, pcc_total[i]);
    }
}

/**
 * @brief function for SIGINT handeling for server
 */
void SIGINT_handler(int signal_num){
    if(active_client == 0){
        kill_print();
        exit(0);
    }
    else{
        kill_serv = 1;
    }
}

int main(int argc, char *argv[]){
    int listenfd  = -1, connfd = -1, skip_client = 0;
    uint32_t N, nboN;
    char *N_transfer;
    int  N_bytes_Left = 0, read_b, i, vOfByte;
    uint16_t port;

    if(argc != 2){
        perror("Wrong amount of parameters for client");
        exit(1); 
    }

    port = atoi(argv[1]);
    struct sockaddr_in serv_addr;
    struct sockaddr_in peer_addr;
    socklen_t addrsize = sizeof(struct sockaddr_in);
    
    //init SIGINT handler
    struct sigaction sig_handler;
    sig_handler.sa_handler = &SIGINT_handler;
    sigemptyset(&sig_handler.sa_mask);
    sig_handler.sa_flags = SA_RESTART;
    if(sigaction(SIGINT, &sig_handler, 0) != 0){
        perror("Error: failed to allocate SIGINT in sig_handler in server");
        exit(1);
    }

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0){
        perror("Error: opening socket in server");
        exit(1);
    } 
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0){
        perror("Error: setsockopt(SO_REUSEADDR) failed");
        exit(1);

    }
    memset(&serv_addr, 0, addrsize);
    serv_addr.sin_family = AF_INET;
    // INADDR_ANY = any local machine address
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);
    if( 0 != bind(listenfd, (struct sockaddr*) &serv_addr, addrsize)){
        perror("\n Error : Bind Failed. \n");
        exit(1);
    }
    //Listen to incoming TCP connections on the specified server port - queue of size 10.
    if( 0 != listen(listenfd, 10)){
        perror("\n Error : Listen Failed. \n");
        exit(1);
    }
    /**
     * @brief loop steps:
     * 1) Accept TCP connection
     * 2) Read a stream of bytes from the client
     * 3) Computes the stream's printable character count
     * 4) Write the result to the client over the TCP connection
     * 5) Updates the pcc_total global data structure
     */
    N_transfer = (char*)&nboN;
    while(kill_serv != 1){
        /**
         * @brief TODO: notice no skip_client_flag
         */
        connfd = accept(listenfd, (struct sockaddr*) &peer_addr, &addrsize);
        skip_client = 0;
        if(connfd < 0){
            perror("\n Error : Accept Failed. \n");
            exit(1);
        }
        active_client = 1;
        //read N from client
        while(N_bytes_Left < sizeof(nboN)){
            read_b = read(connfd, N_transfer, sizeof(nboN)-N_bytes_Left);
            //if we recieved EOF while having more bytes to read
            if(read_b == 0 && N_bytes_Left != sizeof(nboN)){
                close(connfd);
                active_client = 0;
                skip_client = 1;
                perror("\n Error : Connection terminated unexpectedly in server");
                break;
            }
            else if(read_b == -1){
                if(errno != ETIMEDOUT && errno != ECONNRESET && errno != EPIPE){
                  perror("\n Error : read failed in server \n");
                  exit(1);
                }
                else{
                    close(connfd);
                    active_client = 0;
                    skip_client = 1;
                    perror("\n Error : Connection terminated unexpectedly due to TCP errors in server");
                    break;
                }
            }
            //managed to read from client
            else{
                N_transfer += read_b;
                N_bytes_Left += read_b;

            }
        }
        if( skip_client == 1){
            continue;
        }
        N = ntohl(nboN);
        /**
         * @brief Since The pcc_global statistics must not reflect the data received on the (terminated) connections - 
         * we will initate a temporray pcc_temp array to hold the current statistics, 
         * but will only add them to the global array iff the connection won't ne terminated.
         */
        uint32_t pcc_temp[127] = {0};
        char data[1024];
        N_bytes_Left = 0;
        uint32_t C = 0;
        int readBytesAmount = 0;

     //   while(N_bytes_Left < N){
            read_b = read(connfd, data, sizeof(data));
            readBytesAmount += read_b;
            N_bytes_Left += read_b;
            if(read_b == -1){
                if(errno != ETIMEDOUT && errno != ECONNRESET && errno != EPIPE){
                    perror("\n Error : read failed in server \n");
                    exit(1);
                }
                else{
                    close(connfd);
                    active_client = 0;
                    skip_client = 1;
                    perror("\n Error : Connection terminated unexpectedly due to TCP errors in server");
                    break;
                }
            }
           /* if(read_b == 0){
                break;
            }*/
            //count chars to temp_pcc
            for(i=0; i < read_b; i++){
                vOfByte = (int)data[i];
                if(vOfByte>=32 && vOfByte<=126){
                    pcc_temp[vOfByte]++;
                    C++;
                }
            }
       // }
        if(skip_client == 1){
                continue;
        }
        /**
         * @brief if readBytesAmount == N, then the connection wasn't terminated 
         * and we will write back C to cilent and add the statistics to pcc_total.
         */
        if(readBytesAmount == N){
            uint32_t nboC = htonl(C);
            N_transfer = (char*)&nboC;
            N_bytes_Left = 0;
            int rc;
            int sN = sizeof(nboC);
            while(N_bytes_Left < sN){
                rc = write(connfd, N_transfer, sN-N_bytes_Left);
                if(rc == 0 && N_bytes_Left < sN){
                    close(connfd);
                    active_client = 0;
                    skip_client = 1;
                    perror("\n Error : Connection terminated unexpectedly in server");
                    break;
                }
                else if(rc == -1){
                    if(errno != ETIMEDOUT && errno != ECONNRESET && errno != EPIPE){
                        perror("\n Error : read failed in server \n");
                        exit(1);
                    }
                    else{
                        close(connfd);
                        active_client = 0;
                        skip_client = 1;
                        perror("\n Error : Connection terminated unexpectedly due to TCP errors in server");
                        break;
                    }
                }
                //managed to write to client
                else{
                    N_transfer += rc;
                    N_bytes_Left += rc;

                }
            }
            if( skip_client == 1){
                continue;
            }
            for(i=0; i< 127; i++){
                pcc_total[i] += pcc_temp[i];
            }
        }
        /**
         * @brief if read_b != N, then the connection was terminated 
         * and we won't write back C to cilent and won't add the statistics to pcc_total - 
         * Just print an error message to stderr and keep accepting new client connections.
         */
        else{
            perror("Client connection terminates due to TCP errors or unexpected connection close");
        }
        close(connfd);
        active_client = 0;
    }
    kill_print();
    exit(0);
}
