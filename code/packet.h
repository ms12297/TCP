enum packet_type {
    DATA,
    ACK,
};

typedef struct {
    int seqno;
    int ackno;
    int ctr_flags;
    int data_size;
}tcp_header;

#define MSS_SIZE    1500
#define UDP_HDR_SIZE    8
#define IP_HDR_SIZE    20
#define TCP_HDR_SIZE    sizeof(tcp_header)
#define DATA_SIZE   (MSS_SIZE - TCP_HDR_SIZE - UDP_HDR_SIZE - IP_HDR_SIZE)
typedef struct {
    tcp_header  hdr;
    char    data[0];
}tcp_packet;

tcp_packet* make_packet(int seq);
int get_data_size(tcp_packet *pkt);

// making a list of packets
typedef struct node {
    tcp_packet* pkt;
    struct node* next;
} packet_list;

// function push to add a packet to the list
void push(packet_list** head_ref, tcp_packet* pkt);

// function pop to remove the first packet from the list
void pop(packet_list** head_ref);

// function to remove a specific packet from the list
void pop_curr(packet_list** head_ref, packet_list** curr);
