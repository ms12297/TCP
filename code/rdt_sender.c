#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>

#include"packet.h"
#include"common.h"

#define STDIN_FD    0
#define RETRY  120 //millisecond

int next_seqno=0;
int send_base=0;
int window_size = 10;

int sockfd, serverlen;
struct sockaddr_in serveraddr;
struct itimerval timer; 
tcp_packet *sndpkt;
tcp_packet *recvpkt;
sigset_t sigmask;       

packet_list * head = NULL; // defining the list of packets
int lastACK = 0; // last ACK received
int dupACK = 0; // duplicate ACK counter


void resend_packets(int sig)
{
    if (sig == SIGALRM)
    {
        //Resend all packets range between 
        //sendBase and nextSeqNum

        // assignment description specifies we only resend the base so:
        sndpkt = head->pkt;

        VLOG(INFO, "Timeout happened for packet %d", sndpkt->hdr.seqno);
        if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0, 
                    ( const struct sockaddr *)&serveraddr, serverlen) < 0)
        {
            error("sendto");
        }

        dupACK = 0;
    }
}


void start_timer()
{
    sigprocmask(SIG_UNBLOCK, &sigmask, NULL);
    setitimer(ITIMER_REAL, &timer, NULL);
}


void stop_timer()
{
    sigprocmask(SIG_BLOCK, &sigmask, NULL);
}


/*
 * init_timer: Initialize timer
 * delay: delay in milliseconds
 * sig_handler: signal handler function for re-sending unACKed packets
 */
void init_timer(int delay, void (*sig_handler)(int)) 
{
    signal(SIGALRM, sig_handler);
    timer.it_interval.tv_sec = delay / 1000;    // sets an interval of the timer
    timer.it_interval.tv_usec = (delay % 1000) * 1000;  
    timer.it_value.tv_sec = delay / 1000;       // sets an initial value
    timer.it_value.tv_usec = (delay % 1000) * 1000;

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGALRM);
}


int main (int argc, char **argv)
{
    int portno, len;
    int next_seqno;
    char *hostname;
    char buffer[DATA_SIZE];
    bzero(buffer, DATA_SIZE);
    FILE *fp;

    /* check command line arguments */
    if (argc != 4) {
        fprintf(stderr,"usage: %s <hostname> <port> <FILE>\n", argv[0]);
        exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    fp = fopen(argv[3], "r");
    if (fp == NULL) {
        error(argv[3]);
    }

    // size of file to determine num of packets
    fseek(fp, 0, SEEK_END);
    int file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // storing the entire file as packets in an array for easy access later
    int num_packets = file_size / DATA_SIZE;
    if (file_size % DATA_SIZE != 0) { // if there is a remainder, add one more packet
        num_packets++;
    }
    tcp_packet *packets[num_packets];
    
    // reading into the packets array
    len = fread(buffer, 1, DATA_SIZE, fp);
    int pktidx = 0;
    while (len > 0) {
        send_base = next_seqno;
        next_seqno = send_base + len;
        tcp_packet *pkt = make_packet(len);
        memcpy(pkt->data, buffer, len);
        pkt->hdr.seqno = send_base;
        pkt->hdr.data_size = len;
        packets[pktidx] = pkt;
        pktidx++;
        bzero(buffer, DATA_SIZE);
        len = fread(buffer, 1, DATA_SIZE, fp);
    }

    // close the file
    fclose(fp);

    // printf("Number of packets: %d\n", num_packets);
    // printf("File size: %d\n", file_size);
    // printf("Last packet size: %d\n", packets[num_packets - 1]->hdr.data_size);
    // printf("Count: %d\n", pktidx);
    
    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");


    /* initialize server server details */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serverlen = sizeof(serveraddr);

    /* covert host into network byte order */
    if (inet_aton(hostname, &serveraddr.sin_addr) == 0) {
        fprintf(stderr,"ERROR, invalid host %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(portno);

    assert(MSS_SIZE - TCP_HDR_SIZE > 0);

    //Stop and wait protocol

    init_timer(RETRY, resend_packets);
    next_seqno = 0;
    pktidx = 0;
    int restart = 1; // flag to restart the timer

    while (1)
    {
        //Wait for ACK
        do {
            while (window_size > 0 && pktidx < num_packets) { // while there is space in the window and packets left to send
                // send packets in the window
                sndpkt = packets[pktidx];
                push(&head, sndpkt); // adds to the end of the list

                VLOG(INFO, "Sending packet %d to %s", 
                        sndpkt->hdr.seqno, inet_ntoa(serveraddr.sin_addr));

                if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0, 
                            ( const struct sockaddr *)&serveraddr, serverlen) < 0)
                {
                    error("sendto");
                }

                if (restart == 1) { // starting timer for the initial send_base
                    stop_timer();
                    start_timer();
                    restart = 0;
                }

                send_base = next_seqno;
                next_seqno = send_base + len;

                pktidx++;
                window_size--;
            }

            // if all packets have been sent and all ACKs have been received, end of file has been reached and we break
            if (pktidx == num_packets && head == NULL) {
                VLOG(INFO, "End Of File has been reached");
                sndpkt = make_packet(0);
                sendto(sockfd, sndpkt, TCP_HDR_SIZE,  0,
                        (const struct sockaddr *)&serveraddr, serverlen);
                return 0; // maybe even return here?
            }

            if(recvfrom(sockfd, buffer, MSS_SIZE, 0,
                        (struct sockaddr *) &serveraddr, (socklen_t *)&serverlen) < 0)
            {
                error("recvfrom");
            }

            recvpkt = (tcp_packet *)buffer;
            printf("Received ACK: %d\n", recvpkt->hdr.ackno);
            assert(get_data_size(recvpkt) <= DATA_SIZE);

            if (recvpkt->hdr.ackno == lastACK) {
                dupACK++;
            } else {
                lastACK = recvpkt->hdr.ackno;
                dupACK = 0;
            }

            if (dupACK == 3) {
                VLOG(INFO, "Triple duplicate ACK received");
                stop_timer(); 
                start_timer(); // restart the timer
                sndpkt = head->pkt; // resend the send_base packet
                if(sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0, 
                            ( const struct sockaddr *)&serveraddr, serverlen) < 0)
                {
                    error("sendto");
                }
                dupACK = 0;
            }
            
            // while the window is not empty and the ACK number is greater than the sequence number of the first packet in the window
            // i.e. the ACK addresses an in-flight packet:
            while (head != NULL && recvpkt->hdr.ackno >= head->pkt->hdr.seqno + head->pkt->hdr.data_size)
            {
                pop(&head); // removing the packet from the list as it has been received
                window_size++; // allow more packets to be sent

                if (restart == 0) { // stop the timer because the base has shifted, it will restart for the base in the first loop when a new packet is sent
                    stop_timer();
                    start_timer();
                    restart = 1;
                }
            }

        } while(recvpkt->hdr.ackno != next_seqno);

    }

    return 0;

}



