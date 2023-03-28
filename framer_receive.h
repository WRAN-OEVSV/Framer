#ifndef FRAMER_RECEIVE_H
#define FRAMER_RECEIVE_H

#include "functions.h"
#include "common.h"

class Framer_receive
{
    int fd_tun = 0;
    struct sdu_t sdu = {0};
    struct wran_frame_t wran_frame = {0};
    struct MAC_header_t MAC_header = {0};
    struct fragmentation_subheader_t fragmentation_subheader = {0};
    enum fragmentation_state_t fragmentation_state = NO_FRAGMENTATION;
    int MAC_frame_start_index = 0;
    int sequence_number = 0;
    int subheaders_length = 0;
    
    int read_WRAN_frame();
    void process_WRAN_frame();
    int decode_MAC_header();
    int check_crc32();
    int decode_subheaders();
    void decode_fragmentation_subheader();
    void decode_PDU();
    void reset_SDU();
    int send_SDU();
    int append_MAC_PDU_to_SDU();
    void skip_MAC_frame();

public:
    Framer_receive(int fd_tun);
    void operator()();
};

#endif