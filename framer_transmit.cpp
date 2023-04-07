#include <iostream>
#include <iomanip>
#include <thread>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include "zlib.h"

#include "framer_transmit.h"
#include "common.h"
#include "functions.h"

void Framer_transmit::operator()()
{
    while (1)
    {
        if (read_TUN_interface() == 0)
        {
            return;
        }
        generate_and_send_MAC_frame();
    }
}

Framer_transmit::Framer_transmit(int fd_tun)
{
    this->fd_tun = fd_tun;
}

int Framer_transmit::read_TUN_interface()
{
    while ((sdu.length = read(fd_tun, &sdu.data, TUN_INTERFACE_MTU)) == -1)
    {
        if (errno != EINTR)
        {
            std::cerr << program_name << " Error reading from TUN interface" << std::endl;
            return 0;
        }
    }
    fragmentation_state = NOT_FRAGMENTED;
    sdu.next_index = 0;
    return 1;
}

void Framer_transmit::generate_and_send_MAC_frame()
{
    while (SDU_UNPROCESSED_BYTES > 0)
    {
        subheaders_length = 0;  // Length of all included subheaders (Table 7 of IEEE 802.22 specification)
        subheaders_present = 0; // Indication of present subheaders in type field of MAC header according to table 7 of IEEE 802.22 specification
        current_PDU_start_index = MAC_frame.next_index;

        // TODO: Implementation of subheaders

        if (MAC_FRAME_UNPROCESSED_BYTES >= MAC_PDU_HEADER_LENGTH + MAC_PDU_CRC_LENGTH + subheaders_length + FRAGMENTATION_SUBHEADER_LENGTH + MINIMUM_PAYLOAD_LENGTH) // Append PDU containing payload
        {
            subheaders_length += FRAGMENTATION_SUBHEADER_LENGTH;
            subheaders_present |= SUBHEADER_FRAGMENTATION;
            // TODO: Append subheaders if required
            add_SDU_to_MAC_frame();
        }
        else if (MAC_FRAME_UNPROCESSED_BYTES >= MAC_PDU_HEADER_LENGTH + MAC_PDU_CRC_LENGTH + subheaders_length) // Append PDU only containing headers
        {
            // Subheaders not implemented yet.
            // TODO: Append subheaders if required
            send_MAC_frame_to_WRAN_interface();
        }
        else // Send frame without adding data
        {
            send_MAC_frame_to_WRAN_interface();
        }
    }

    if (check_if_TUN_blocks() == 1) // If calling read() on the TUN interface would block send WRAN frame even if it is not full yet
    {
        send_MAC_frame_to_WRAN_interface();
    }
}

int Framer_transmit::send_MAC_frame_to_WRAN_interface()
{
    // TODO: Interface with existing application
    // Unused bytes in the MAC_frame.data array must be padded with 0.

    std::cout << program_name << " Sending " << MAC_frame.next_index << " bytes to WRAN interface" << std::endl;
    for (int i = 0; i < MAC_frame.next_index; ++i)
    {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)*(MAC_frame.data + i) << std::dec;
    }
    std::cout << std::endl;

    MAC_frame.next_index = 0;
    return 0;
}

void Framer_transmit::add_SDU_to_MAC_frame()
{
    available_bytes_for_SDU = MAC_FRAME_UNPROCESSED_BYTES - MAC_PDU_HEADER_LENGTH - MAC_PDU_CRC_LENGTH - subheaders_length;

    if (SDU_UNPROCESSED_BYTES <= available_bytes_for_SDU) // Entire unprocessed SDU can be added to MAC frame
    {
        PDU_length = MAC_PDU_HEADER_LENGTH + subheaders_length + SDU_UNPROCESSED_BYTES + MAC_PDU_CRC_LENGTH;
        SDU_length = SDU_UNPROCESSED_BYTES;
        fragmentation_subheader = (fragmentation_state == NOT_FRAGMENTED) ? FC_NO_FRAGMENTATION : FC_LAST_FRAGMENT;
        fragmentation_state = NOT_FRAGMENTED;
    }
    else // Only available_bytes_for_SDU of the SDU can be added to MAC frame
    {
        PDU_length = MAC_FRAME_UNPROCESSED_BYTES;
        SDU_length = available_bytes_for_SDU;
        fragmentation_subheader = (fragmentation_state == NOT_FRAGMENTED) ? FC_FIRST_FRAGMENT : FC_MIDDLE_FRAGMENT;
        fragmentation_state = FRAGMENTED;
    }

    append_MAC_PDU();
}

void Framer_transmit::append_MAC_PDU_header()
{
    struct MAC_PDU_header_t MAC_PDU_header = {0}; // All Flags of MAC header (including FT) statically set to zero.
    int start_index = MAC_frame.next_index;
    MAC_frame.data[(MAC_frame.next_index)++] = (PDU_length & 0x07F8) >> 3;
    MAC_frame.data[(MAC_frame.next_index)++] = ((PDU_length & 0x0007) << 5) | (MAC_PDU_header.UCS << 4) | (MAC_PDU_header.QPA << 3) | (MAC_PDU_header.EC << 2) | (MAC_PDU_header.EKS << 0);
    MAC_frame.data[(MAC_frame.next_index)++] = (subheaders_present << 3) | (MAC_PDU_header.FT);
    MAC_frame.data[(MAC_frame.next_index)++] = crc8(MAC_frame.data + start_index, 3);
    return;
}

void Framer_transmit::append_fragmentation_subheader()
{
    static uint16_t sequence_number = 0;
    MAC_frame.data[(MAC_frame.next_index)++] = (PURPOSE_FRAGMENTATION << 7) | (fragmentation_subheader << 5) | ((sequence_number & 0x3E0) >> 5);
    MAC_frame.data[(MAC_frame.next_index)++] = ((sequence_number & 0x1F) << 3);
    sequence_number = (sequence_number + 1) % 1024;
    return;
}

void Framer_transmit::append_MAC_PDU()
{
    append_MAC_PDU_header();
    append_fragmentation_subheader();

    for (int i = 0; i < SDU_length; ++i)
    {
        MAC_frame.data[(MAC_frame.next_index)++] = sdu.data[(sdu.next_index)++];
    }

    append_MAC_PDU_CRC();
}

void Framer_transmit::append_MAC_PDU_CRC()
{
    uint32_t crc = crc32(0, MAC_frame.data + current_PDU_start_index, MAC_frame.next_index - current_PDU_start_index);
    crc = htonl(crc); // Convert to network byte order (Big Endian; MSB first) as required by IEEE 802.22.

    *(uint32_t *)(MAC_frame.data + MAC_frame.next_index) = crc;
    MAC_frame.next_index += MAC_PDU_CRC_LENGTH;
}

int Framer_transmit::check_if_TUN_blocks()
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
