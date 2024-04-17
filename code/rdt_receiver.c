#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <assert.h>

#include "common.h"
#include "packet.h"


/*
 * You are required to change the implementation to support
 * window size greater than one.
 * In the current implementation the window size is one, hence we have
 * only one send and receive packet
 */
tcp_packet *recvpkt;
tcp_packet *sndpkt;

packet_list * head = NULL; // defining the buffer as a list
int ackno = 0; // the expected sequence number

int main(int argc, char **argv) {
    int sockfd; /* socket */
    int portno; /* port to listen on */
    int clientlen; /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    int optval; /* flag value for setsockopt */
    FILE *fp;
    char buffer[MSS_SIZE];
    struct timeval tp;

    /* 
     * check command line arguments 
     */
    if (argc != 3) {
        fprintf(stderr, "usage: %s <port> FILE_RECVD\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    fp  = fopen(argv[2], "w");
    if (fp == NULL) {
        error(argv[2]);
    }

    /* 
     * socket: create the parent socket 
     */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* setsockopt: Handy debugging trick that lets 
     * us rerun the server immediately after we kill it; 
     * otherwise we have to wait about 20 secs. 
     * Eliminates "ERROR on binding: Address already in use" error. 
     */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
            (const void *)&optval , sizeof(int));

    /*
     * build the server's Internet address
     */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)portno);

    /* 
     * bind: associate the parent socket with a port 
     */
    if (bind(sockfd, (struct sockaddr *) &serveraddr, 
                sizeof(serveraddr)) < 0) 
        error("ERROR on binding");

    /* 
     * main loop: wait for a datagram, then echo it
     */
    VLOG(DEBUG, "epoch time, bytes received, sequence number");

    clientlen = sizeof(clientaddr);
    while (1) {
        /*
         * recvfrom: receive a UDP datagram from a client
         */
        //VLOG(DEBUG, "waiting from server \n");
        if (recvfrom(sockfd, buffer, MSS_SIZE, 0,
                (struct sockaddr *) &clientaddr, (socklen_t *)&clientlen) < 0) {
            error("ERROR in recvfrom");
        }
        recvpkt = (tcp_packet *) buffer;
        assert(get_data_size(recvpkt) <= DATA_SIZE);
        if ( recvpkt->hdr.data_size == 0) {
            VLOG(INFO, "End Of File has been reached");
            fclose(fp);
            break;
        }
        /* 
         * sendto: ACK back to the client 
         */
        gettimeofday(&tp, NULL);
        VLOG(DEBUG, "%lu, %d, %d", tp.tv_sec, recvpkt->hdr.data_size, recvpkt->hdr.seqno);

        // case 1: the packet is the expected packet - check if anything buffered becomes in order with the new packet, and write to file, send cumulative ack
        if (recvpkt->hdr.seqno == ackno) {
            // write to file
            fseek(fp, recvpkt->hdr.seqno, SEEK_SET);
            fwrite(recvpkt->data, 1, recvpkt->hdr.data_size, fp);
            ackno += recvpkt->hdr.data_size;
            // check if anything buffered becomes in order
            packet_list * curr = head;
            // printf("check1\n");
            while (curr != NULL) {
                if (curr->pkt->hdr.seqno == ackno) {
                    // write to file
                    fseek(fp, curr->pkt->hdr.seqno, SEEK_SET);
                    fwrite(curr->pkt->data, 1, curr->pkt->hdr.data_size, fp);
                    ackno += curr->pkt->hdr.data_size;
                    // printf("check2\n");
                    pop_curr(&head, &curr); // remove from the buffer, curr now points to the next element so no update required
                    // printf("check3\n");
                } 
                else {
                    // printf("check4\n");
                    curr = curr->next;
                    // printf("check5\n");
                }
            }
        }
        
        // case 2: the packet is not the expected packet - buffer the packet if not already buffered, send the ack for the packet we are expecting
        else {

            // if the packet arrives out of order but is not already cummulatively acked
            if (recvpkt->hdr.seqno > ackno) {
                // buffer the packet if not already buffered
                packet_list * curr = head;
                while (curr != NULL) {
                    if (curr->pkt->hdr.seqno == recvpkt->hdr.seqno) {
                        break;
                    }
                    curr = curr->next;
                }
                if (curr == NULL) {
                    // making the new packet to buffer
                    tcp_packet * newpkt = make_packet(recvpkt->hdr.data_size);
                    memcpy(newpkt, recvpkt, TCP_HDR_SIZE + recvpkt->hdr.data_size);
                    push(&head, newpkt);
                    printf("BUFFERED %d\n", newpkt->hdr.seqno);
                }
            }
            else { // if the packet is already cumulatively acked
                printf("DUPLICATE %d\n", recvpkt->hdr.seqno);
            }
        }

        // send cumulative ack
        printf("ACK %d\n", ackno);
        sndpkt = make_packet(0);
        sndpkt->hdr.ackno = ackno;
        sndpkt->hdr.ctr_flags = ACK;
        if (sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0, 
                (struct sockaddr *) &clientaddr, clientlen) < 0) {
            error("ERROR in sendto");
        }
    }

    return 0;
}
