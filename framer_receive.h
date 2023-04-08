#ifndef FRAMER_RECEIVE_H
#define FRAMER_RECEIVE_H

#include "functions.h"
#include "common.h"

class Framer_receive
{
    int fd_tun = 0;
    struct sdu_t sdu = {0};
    struct DS_US_burst_t DS_US_burst = {0};
    struct MAC_PDU_header_t MAC_PDU_header = {0};
    struct fragmentation_subheader_t fragmentation_subheader = {0};
    enum fragmentation_state_t fragmentation_state = NOT_FRAGMENTED;
    int MAC_PDU_start_index = 0;
    int sequence_number = 0;
    int subheaders_length = 0;
    
    int read_DS_US_burst_from_WRAN_interface();
    void process_DS_US_burst();
    int decode_MAC_PDU_header();
    int check_crc32();
    int decode_subheaders();
    void decode_fragmentation_subheader();
    void decode_MAC_PDU();
    void reset_SDU();
    int send_SDU_to_TUN_interface();
    int append_MAC_PDU_payload_to_SDU();
    void skip_MAC_PDU();

public:
    Framer_receive(int fd_tun);
    void operator()();
};

#endif