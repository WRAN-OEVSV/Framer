#include <iostream>
#include <iomanip>
#include <thread>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include "zlib.h"

#include "framer_send.h"
#include "common.h"
#include "functions.h"

void Framer_send::operator()()
{
    while (1)
    {
        if (read_TUN_interface() == 0)
        {
            return;
        }
        generate_and_send_WRAN_frame();
    }
}

Framer_send::Framer_send(int fd_tun)
{
    this->fd_tun = fd_tun;
    wran_frame.length = WRAN_FRAME_LENGTH;
}

int Framer_send::read_TUN_interface()
{
    while ((sdu.length = read(fd_tun, &sdu.data, TUN_INTERFACE_MTU)) == -1)
    {
        if (errno != EINTR)
        {
            std::cerr << program_name << " Error reading from TUN interface" << std::endl;
            return 0;
        }
    }
    fragmentation_state = NO_FRAGMENTATION;
    sdu.next_index = 0;
    return 1;
}

void Framer_send::generate_and_send_WRAN_frame()
{
    while (SDU_UNPROCESSED_BYTES > 0)
    {
        // TODO: Check if subheaders should be sent.

        subheaders_length = 0;  // Length of all included subheaders (Table 7 of IEEE 802.22 specification)
        subheaders_present = 0; // Indication of present subheaders in type field of MAC header according to table 7 of IEEE 802.22 specification
        current_frame_start_index = wran_frame.next_index;

        if (WRAN_FRAME_UNPROCESSED_BYTES >= MAC_HEADER_LENGTH + MAC_CRC_LENGTH + subheaders_length + FRAGMENTATION_HEADER_LENGTH + MINIMAL_PAYLOAD_LENGTH) // Append PDU containing payload
        {
            subheaders_length += FRAGMENTATION_HEADER_LENGTH;
            subheaders_present |= SUBHEADER_FRAGMENTATION;
            // TODO: Append subheaders if required
            generate_frame_with_PDU();
        }
        else if (WRAN_FRAME_UNPROCESSED_BYTES >= MAC_HEADER_LENGTH + MAC_CRC_LENGTH + subheaders_length) // Append PDU only containing headers
        {
            // Subheaders not implemented yet.
            // TODO: Append subheaders if required
            send_WRAN_frame();
        }
        else // Send frame without adding data
        {
            send_WRAN_frame();
        }
    }

    if (check_if_TUN_blocks() == 1) // If calling read() on the TUN interface would block send WRAN frame even if it is not full yet
    {
        send_WRAN_frame();
    }
}

int Framer_send::send_WRAN_frame()
{
    // TODO: Interface with existing application

    std::cout << program_name << " Sending " << wran_frame.next_index << " bytes to WRAN interface" << std::endl;
    for (int i = 0; i < wran_frame.next_index; ++i)
    {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)*(wran_frame.data + i) << std::dec;
    }
    std::cout << std::endl;

    wran_frame.next_index = 0;
    return 0;
}

void Framer_send::generate_frame_with_PDU()
{
    available_bytes_for_sdu = WRAN_FRAME_UNPROCESSED_BYTES - MAC_HEADER_LENGTH - MAC_CRC_LENGTH - subheaders_length;

    if (SDU_UNPROCESSED_BYTES <= available_bytes_for_sdu) // Entire unprocessed SDU can be sent
    {
        pdu_length = MAC_HEADER_LENGTH + subheaders_length + SDU_UNPROCESSED_BYTES + MAC_CRC_LENGTH;
        sdu_length = SDU_UNPROCESSED_BYTES;
        fragmentation_header = (fragmentation_state == NO_FRAGMENTATION) ? FC_NO_FRAGMENTATION : FC_LAST_FRAGMENT;
        fragmentation_state = NO_FRAGMENTATION;
    }
    else // Only available_bytes_for_sdu of the SDU can be sent
    {
        pdu_length = WRAN_FRAME_UNPROCESSED_BYTES;
        sdu_length = available_bytes_for_sdu;
        fragmentation_header = (fragmentation_state == NO_FRAGMENTATION) ? FC_FIRST_FRAGMENT : FC_MIDDLE_FRAGMENT;
        fragmentation_state = FRAGMENTED;
    }

    append_MAC_header();
    append_fragmentation_subheader();
    append_SDU_to_frame();
    append_MAC_CRC();
}

void Framer_send::append_MAC_header()
{
    struct MAC_header_t MAC_header = {0}; // All Flags of MAC header (including FT) statically set to zero.
    int start_index = wran_frame.next_index;
    wran_frame.data[(wran_frame.next_index)++] = (pdu_length & 0x07F8) >> 3;
    wran_frame.data[(wran_frame.next_index)++] = ((pdu_length & 0x0007) << 5) | (MAC_header.UCS << 4) | (MAC_header.QPA << 3) | (MAC_header.EC << 2) | (MAC_header.EKS << 0);
    wran_frame.data[(wran_frame.next_index)++] = (subheaders_present << 3) | (MAC_header.FT);
    wran_frame.data[(wran_frame.next_index)++] = crc8(wran_frame.data + start_index, 3);
    return;
}

void Framer_send::append_fragmentation_subheader()
{
    static uint16_t sequence_number = 0;
    wran_frame.data[(wran_frame.next_index)++] = (PURPOSE_FRAGMENTATION << 7) | (fragmentation_header << 5) | ((sequence_number & 0x3E0) >> 5);
    wran_frame.data[(wran_frame.next_index)++] = ((sequence_number & 0x1F) << 3);
    sequence_number = (sequence_number + 1) % 1024;
    return;
}

void Framer_send::append_SDU_to_frame()
{
    for (int i = 0; i < sdu_length; ++i)
    {
        wran_frame.data[(wran_frame.next_index)++] = sdu.data[(sdu.next_index)++];
    }
    return;
}

void Framer_send::append_MAC_CRC()
{
    uint32_t crc = crc32(0, wran_frame.data + current_frame_start_index, wran_frame.next_index - current_frame_start_index);
    crc = htonl(crc); // Convert to network byte order (Big Endian; MSB first) as required by IEEE 802.22.

    for (int i = 0; i < MAC_CRC_LENGTH; ++i)
    {
        *(wran_frame.data + wran_frame.next_index) = *((uint8_t *)(&crc) + i);
        ++(wran_frame.next_index);
    }
    return;
}

int Framer_send::check_if_TUN_blocks()
{
    struct pollfd pollfd;
    pollfd.fd = fd_tun;
    pollfd.events = POLLIN;

    while (poll(&pollfd, 1, 0) == -1)
    {
        if (errno != EINTR)
        {
            std::cerr << program_name << " Error calling poll()" << std::endl;
            return 1;
        }
    }
    return ((pollfd.revents & POLLIN) == 0);
}
