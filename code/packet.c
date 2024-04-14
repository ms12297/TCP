#include <stdlib.h>
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
    new_node->pkt = pkt;
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
