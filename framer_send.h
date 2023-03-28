#ifndef FRAMER_SEND_H
#define FRAMER_SEND_H

#include "common.h"

extern char *program_name;

class Framer_send
{
    int fd_tun = 0;
    int available_bytes_for_sdu = 0;
    int current_frame_start_index = 0;
    int subheaders_length = 0;
    uint16_t pdu_length = 0;
    uint16_t sdu_length = 0;
    uint8_t subheaders_present = 0;
    uint8_t fragmentation_header = 0;
    struct sdu_t sdu = {0};
    struct wran_frame_t wran_frame = {0};
    enum fragmentation_state_t fragmentation_state = NO_FRAGMENTATION;

    int read_TUN_interface();
    void generate_and_send_WRAN_frame();
    int send_WRAN_frame();
    void generate_frame_with_PDU();
    void append_MAC_header();
    void append_fragmentation_subheader();
    void append_SDU_to_frame();
    void append_MAC_CRC();
    int check_if_TUN_blocks();

public:
    void operator()();
    Framer_send(int fd_tun);
};

#endif