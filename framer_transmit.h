#ifndef FRAMER_TRANSMIT_H
#define FRAMER_TRANSMIT_H

#include "common.h"

extern char *program_name;

class Framer_transmit
{
    int fd_tun = 0;
    int available_bytes_for_SDU = 0;
    int current_PDU_start_index = 0;
    int subheaders_length = 0;
    uint16_t PDU_length = 0;
    uint16_t SDU_length = 0;
    uint8_t subheaders_present = 0;
    uint8_t fragmentation_subheader = 0;
    struct sdu_t sdu = {0};
    struct DS_US_burst_t DS_US_burst = {0};
    enum fragmentation_state_t fragmentation_state = NOT_FRAGMENTED;

    int read_SDU_from_TUN_interface();
    void generate_and_send_DS_US_burst();
    int send_DS_US_burst_to_WRAN_interface();
    void add_SDU_to_DS_US_burst();
    void append_MAC_PDU_header();
    void append_fragmentation_subheader();
    void append_MAC_PDU();
    void append_MAC_PDU_CRC();
    int check_if_TUN_interface_blocks();

public:
    void operator()();
    Framer_transmit(int fd_tun);
};

#endif