#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include<arpa/inet.h> 
#include <netdb.h>
#include<fcntl.h>

#include<pthread.h>
#include<sys/wait.h>
#include<signal.h>

#include "data_structs.h"


#define DEFAULT_TCP_IP      "localhost"
#define DEFAULT_TCP_PORT    "2014"
#define DEFAULT_PEER_NAME   "Kobe"

static char my_tcp_ip[20]      = DEFAULT_TCP_IP;
static char my_tcp_port[20]    = DEFAULT_TCP_PORT;
static char my_peer_name[20]   = DEFAULT_PEER_NAME;

char TCP_SERVER_DONE = 0;

struct pdu transfer_pdu;
struct pdu receive_pdu;

/*===============================================================================
    Upload function
=================================================================================*/
int upload(char *filename, int sd)
{
    int fp;
    memset(&transfer_pdu, '\0',101);

    if ((fp = open(filename, O_RDONLY)) == -1){
        fprintf(stderr, "Error: The file name doesn't exist!\n");
        return 0;
    } else {
        close (fp);
        transfer_pdu.type = 'R';

        strcpy(transfer_pdu.peer, my_peer_name);
        strcpy(transfer_pdu.content, filename);
        strcpy(transfer_pdu.address, my_tcp_ip);
        strcpy(transfer_pdu.port, my_tcp_port);

        (void)write(sd, (char *)&transfer_pdu, 101);

        /*check if registration is successful*/
        memset(&receive_pdu, '\0',101);
        (void)read(sd, (char *)&receive_pdu, 101);
        
        if (receive_pdu.type == 'A') {
            fprintf(stderr, "Upload successfully!\n");
            return 1;
        } else if (receive_pdu.type == 'E') {
            fprintf(stderr, "Error: you have a duplicate peer name, try again!\n");
            return 0;
        }

    }
}


/*===============================================================================
    TCP server set up function
=================================================================================*/
void *tcp_setup()
{
    int sd , new_sd, client_len;
    struct sockaddr_in server, client;
    struct hostent *hp;   /* pointer to host information entry    */
    struct pdu tbuf,rbuf;
    char buf[100];

    /*****************************************************
                Create TCP socket for server
    ******************************************************/
    if ((sd = socket(AF_INET,SOCK_STREAM,0))==-1){
        fprintf(stderr, "Can't create TCP socket\n");
        exit(1);
    }
    bzero((char *)&server, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons((u_short)atoi(my_tcp_port));  //my tcp port here
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sd, (struct sockaddr *)&server, sizeof(server)) == -1){
        fprintf(stderr, "Can't bind name to socket\n");
    }
      
    listen(sd,5);
    fprintf(stderr, "Content Server is set up and ready!\n");
    while(1){
        
        client_len = sizeof(client);
        new_sd = accept(sd, (struct sockaddr *)&client, &client_len);
        if (new_sd < 0){
            fprintf(stderr, "Can't accept client \n");
            exit(1);
        }

        /*****************************************************
                    Lister for peer client to download
        ******************************************************/
        memset(&rbuf,'\0',101);
        (void)read(new_sd, (char *)&rbuf,101);
          
        /*check if other peers want to download specific files, multi-thread will be triggered*/
        if (rbuf.type == 'D'){
            FILE *fp;
            int n;
            if (fp = fopen(rbuf.content, "r")){
                /* Notify client to start file transfer. */
                tbuf.type = 'C';
                send(new_sd, &tbuf, 101, 0);
                
                while( (n = fread(buf, 1, 100, fp)) > 0) {
                    send(new_sd, buf, n, 0);
                }
                fclose(fp);
            } else {
                fprintf(stderr,"ERROR!\n");
                tbuf.type = 'E';
                send(new_sd, &tbuf, 101, 0);
            }
        }
        printf("File \"%s\" was sent!\n", rbuf.content);
        close(new_sd);
    }
    
    
    pthread_exit(NULL);
    return NULL;
}

/*===============================================================================
    Download function
=================================================================================*/
int download(char* file_name, char *addr, char *SERVER_TCP_PORT)
{
    struct pdu tbuf,rbuf;
    memset(&tbuf, '\0', 101);
    memset(&rbuf, '\0', 101);

    tbuf.type = 'D';
    strcpy(tbuf.content, file_name);

    int sd;
    char buf[100];
    char *host = addr;
    struct sockaddr_in server;
    struct hostent *hp;  /* pointer to host information entry    */
    
    /*****************************************************
                Create TCP socket for client
    ******************************************************/
    /* Create a stream socket file descriptor */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "Can't create a socket\n");
        exit(1);
    }
    /* Fill the socket address strucO_WRONLY|O_CREAT|O_APPENDt */
    bzero((char *)&server, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons((u_short)atoi(SERVER_TCP_PORT));
    
    if (hp = gethostbyname(host)) {
        bcopy(hp->h_addr, (char *)&server.sin_addr, hp->h_length);
    } else if ( inet_aton(host, (struct in_addr *) &server.sin_addr) ){
        fprintf(stderr, "Can't get server's address\n");
        exit(1);
    }

    /* Connecting to the server */
    if (connect(sd, (struct sockaddr *)&server, sizeof(server)) == -1){
        fprintf(stderr, "Can't connect to server\n");
        exit(1);
    } else {
        printf("Connected to server at port %s\n", SERVER_TCP_PORT);
    }
    
    /*****************************************************
                Request TCP server for download
    ******************************************************/
    
    (void)write(sd, (char *)&tbuf, 101);
    (void)read(sd, (char *)&rbuf, 101);
    
    if (rbuf.type == 'C'){
        FILE *fp;
        int r;
        fp = fopen(file_name, "w");
        
        while( (r = recv(sd, buf, 100, 0)) > 0 ) {
            fwrite(buf, 1, r, fp);
        }
        fclose(fp);
    } else {
        fprintf(stderr, "Download Error!\n");
        return 0;
    }
    close(sd);
    return 1;
}


/*===============================================================================
    De-register Function
=================================================================================*/
int offline(char *filename, int sd)
{
    int fp;
    memset(&transfer_pdu, '\0',101);

    if ((fp = open(filename, O_RDONLY)) == -1){
        fprintf(stderr, "Error: No such file!\n");
        return 0;
    } else {
        close (fp);
        transfer_pdu.type = 'T';

        strcpy(transfer_pdu.peer, my_peer_name);
        strcpy(transfer_pdu.content, filename);
        strcpy(transfer_pdu.address, my_tcp_ip);
        strcpy(transfer_pdu.port, my_tcp_port);

        (void)write(sd, (char *)&transfer_pdu, 101);

        /*check if registration is successful*/
        memset(&receive_pdu, '\0',101);
        (void)read(sd, (char *)&receive_pdu, 101);
        
        if (receive_pdu.type == 'A') {
            return 1;
        } else if (receive_pdu.type == 'E') {
            return 0;
        }

    }
}



/*===============================================================================
    Main Peer Program
=================================================================================*/
int main(int argc, char *argv[])
{
    struct sockaddr_in server, client;
    char    *host = "localhost";    /* host to use if none supplied */
    char    *service = "3000";  /* 32-bit integer to hold time  */ 
    struct hostent  *phe;   /* pointer to host information entry    */
    struct sockaddr_in sin; /* an Internet endpoint address     */
    int s;  /* socket descriptor and socket type for UDP    */
    pthread_t tid;

    // struct sockaddr_in localAddress;
    socklen_t addressLength =sizeof(sin);
    char cmd;
    char buffer[20];
    
    switch (argc) {
        case 1:
            host = "localhost";
            break;
        case 3:
            service = argv[2];
            /* FALL THROUGH */
        case 2:
            host = argv[1];
            break;
        default:
            fprintf(stderr, "usage: UDPtime [host [port]]\n");
            exit(1);
    }

    /*****************************************************
                Create UDP socket for peer client
    ******************************************************/
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;

    /* Map service name to port number */
    sin.sin_port = htons((u_short)atoi(service));

    if ( phe = gethostbyname(host) ){
        memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);

    } else if ( (sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE ){
        fprintf(stderr, "Can't get host entry \n");
        exit(1);
    }

    /* Allocate a socket */
    s = socket(PF_INET, SOCK_DGRAM, 0);
    if (s < 0){
        fprintf(stderr, "Can't create socket \n");
        exit(1);
    }

    /* Connect the socket */
    if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0){
        fprintf(stderr, "Can't connect to %s %s \n", host, service);
        exit(1);
    }


    /*****************************************************
                    Main program start here
    ******************************************************/
    fprintf(stderr,"*********************************\n");
    fprintf(stderr,"*******    Peer Program   *******\n");
    fprintf(stderr,"*********************************\n");
    printf(">>>>>Please type your peer name :  ");
    scanf("%s", my_peer_name);
    
    printf(">>>>>Please type in your IP Address:  \n");
    scanf("%s", my_tcp_ip);
    printf(">>>>>Please provide your port number as TCP server:  \n");
    scanf("%s", my_tcp_port);

    fprintf(stderr,"\n\n>>>>> Commands: <<<<<\n");
    fprintf(stderr,"\'u\': Upload a file.\n");
    fprintf(stderr,"\'d\': Download a file.\n");
    fprintf(stderr,"\'o\': List files available for download.\n");
    fprintf(stderr,"\'t\': De-register a file.\n");


    char *str;
    char upload_flag;
    while (1){
        memset(&transfer_pdu,'\0',101);
        memset(&receive_pdu,'\0',101);

        printf(">>>>>Please enter a command:  \n");
        //scanf("%c", &cmd);
        memset(buffer, 0, 20);
        read(0, buffer, 20);
        cmd = buffer[0];
        switch(cmd)
        {
            case 'u': /*this peer wants to upload file and register to INDEX server simultaneously*/

                printf(">>>>>Please type a file name you want to upload:  \n");
                scanf("%s", buffer);
                str = buffer;
                upload_flag = upload(str, s);

                 /*if it's the first time to successfully upload a file,
                    peer will set up a content server.*/
                if (upload_flag && !TCP_SERVER_DONE){
                    TCP_SERVER_DONE = 1;
                    if(pthread_create(&tid, NULL, &tcp_setup, NULL) == -1){
                        fprintf(stderr, "Error: pthread_create error\n");
                        exit(1);
                    }
                }
                break;

            case 'd':  /*if this peer wants to download from other peers*/

                printf(">>>>>Please enter a file name you want to download:  \n"); 
                scanf("%s", transfer_pdu.content);
                char temp[20];
                strcpy(temp, transfer_pdu.content);
                transfer_pdu.type = 'S';
                (void)write(s, (char *)&transfer_pdu, 101);

                read(s, (char *)&receive_pdu, 101);
                
                if (receive_pdu.type == 'E'){
                    fprintf(stderr, "Error: File not available!\n");
                    
                } else if (receive_pdu.type == 'S'){
                    char tcp_svr_addr[20];
                    char tcp_svr_port[20];
                    
                    
                    strcpy(tcp_svr_addr, receive_pdu.address);
                    strcpy(tcp_svr_port, receive_pdu.port);


                    if (download(transfer_pdu.content, tcp_svr_addr, tcp_svr_port)) {

                        upload_flag = upload(temp, s);
                        /* if download successful, register the file to index server. */
                        if (upload_flag && !TCP_SERVER_DONE){
                            TCP_SERVER_DONE = 1;
                            if(pthread_create(&tid, NULL, &tcp_setup, NULL) == -1){
                                fprintf(stderr, "Error: pthread_create error\n");
                                exit(1);
                            }
                        }
                        printf("Download completed!\n");
                    } else {
                        printf("Download Error!\n");
                    }
                }
                break;

            case 'o': /*list all available files*/
                transfer_pdu.type = 'O';
                (void)write(s, (char *)&transfer_pdu, 101);
                
                printf("========== Files Online ==========\n");
                
                while(read(s, (char *)&receive_pdu, 101) > 0){
                    if(receive_pdu.type == 'E'){
                        break;
                    }
                    write(0, "------", 5);
                    write(0, receive_pdu.content, 20);
                    write(0, "\n", 1);
                }
                break;
                
            case 't': /*de-register a file*/
                printf(">>>>>Please type a file name you want to de-register:  \n");
                scanf("%s", buffer);
                str = buffer;
                if (offline(str, s)){
                    printf("De-registration successfully!\n");
                } else {
                    fprintf(stderr, "De-registration Error!\n");
                }
                break;

            default:
                fprintf(stderr, "Error: invalid command!\n");
                break;

        }
    }
    exit(0);
} 

