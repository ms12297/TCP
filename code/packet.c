#include <stdlib.h>
#include <sys/time.h>
#include"packet.h"

static tcp_packet zero_packet = {.hdr={0}};
/*
 * create TCP packet with header and space for data of size len
 */
tcp_packet* make_packet(int len)
{
    tcp_packet *pkt;
    pkt = malloc(TCP_HDR_SIZE + len);

    *pkt = zero_packet;
    pkt->hdr.data_size = len;
    return pkt;
}

int get_data_size(tcp_packet *pkt)
{
    return pkt->hdr.data_size;
}

// function to add a packet to the list's end
void push(packet_list** head_ref, tcp_packet* pkt)
{
    packet_list* new_node = (packet_list*) malloc(sizeof(packet_list));
    new_node->pkt = pkt; // set the packet
    gettimeofday(&new_node->start, NULL); // set the start time
    new_node->retrans = 0; // initially packet is not retransmitted
    new_node->next = NULL;

    if (*head_ref == NULL) {
        *head_ref = new_node;
        return;
    }
    else {
        packet_list* temp = *head_ref;
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = new_node;
    }
}

// function to remove the first packet from the list
void pop(packet_list** head_ref)
{
    packet_list* temp = *head_ref;
    *head_ref = temp->next;
    free(temp);
}

// function to remove a specific packet from the list
void pop_curr(packet_list** head_ref, packet_list** curr)
{
    packet_list* temp = *head_ref;
    if (temp == *curr) {
        *head_ref = temp->next;
        free(temp);
        *curr = *head_ref; // update curr to the next element (or NULL if the list is empty)
        return;
    }
    while (temp != NULL && temp->next != *curr) {
        temp = temp->next;
    }
    temp->next = (*curr)->next;
    free(*curr);
    *curr = temp->next; // update curr to the next element
}
