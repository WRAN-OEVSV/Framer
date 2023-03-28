#include <iostream>
#include <iomanip>
#include <arpa/inet.h>

#include "zlib.h"
#include "framer_receive.h"
#include "common.h"
#include "functions.h"

void Framer_receive::operator()()
{
    while (1)
    {
        if (read_WRAN_frame() == -1)
        {
            return;
        }

        process_WRAN_frame();
    }
}

Framer_receive::Framer_receive(int fd_tun)
{
    this->fd_tun = fd_tun;
    sdu.length = TUN_INTERFACE_MTU;
}

int Framer_receive::read_WRAN_frame()
{
    // TODO: Interface with existing application

    wran_frame.next_index = 0;
    return 0;
}

void Framer_receive::process_WRAN_frame()
{
    while (WRAN_FRAME_UNPROCESSED_BYTES > 0)
    {
        MAC_frame_start_index = wran_frame.next_index;

        if ((decode_MAC_header() == 0) || (MAC_header.length > WRAN_FRAME_UNPROCESSED_BYTES + MAC_HEADER_LENGTH) || (MAC_header.length < MAC_HEADER_LENGTH + MAC_CRC_LENGTH)) // Skip entire WRAN frame if MAC Header checksum incorrect or length field in the MAC header invalid.
        {
            break;
        }
        if (check_crc32() == 0 || decode_subheaders() == 0) // Skip frame if CRC is incorrect or subheaders invalid
        {
            skip_MAC_frame();
        }
        else
        {
            decode_PDU();
        }
    }
}

int Framer_receive::decode_MAC_header()
{
    MAC_header.length = (wran_frame.data[wran_frame.next_index] << 3) | ((wran_frame.data[wran_frame.next_index + 1] & 0xE0) >> 5);
    ++(wran_frame.next_index);
    MAC_header.UCS = (wran_frame.data[wran_frame.next_index] & 0x10) >> 4;
    MAC_header.QPA = (wran_frame.data[wran_frame.next_index] & 0x08) >> 3;
    MAC_header.EC = (wran_frame.data[wran_frame.next_index] & 0x04) >> 2;
    MAC_header.EKS = (wran_frame.data[wran_frame.next_index] & 0x03);
    ++(wran_frame.next_index);
    MAC_header.Type = (wran_frame.data[wran_frame.next_index] & 0xF8) >> 3;
    MAC_header.FT = (wran_frame.data[wran_frame.next_index] & 0x07);
    ++(wran_frame.next_index);
    ++(wran_frame.next_index);
    MAC_header.HCS_valid = (crc8(wran_frame.data + wran_frame.next_index - 4, 4) == 0);
    return MAC_header.HCS_valid;
}

int Framer_receive::check_crc32()
{
    uint32_t crc = crc32(0, wran_frame.data + MAC_frame_start_index, MAC_header.length - MAC_CRC_LENGTH);
    crc = htonl(crc);   //Convert calculated crc to network big endian (network byte order) in order to compare it with the crc in the MAC PDU
    return (crc == *((uint32_t *)(wran_frame.data + MAC_frame_start_index + MAC_header.length - MAC_CRC_LENGTH)));
}

int Framer_receive::decode_subheaders()
{
    subheaders_length = 0;
    if (MAC_header.Type != SUBHEADER_FRAGMENTATION) // In the current implementation a fragmentation subheader must be present without any other subheaders
    {
        return 0;
    }

    decode_fragmentation_subheader();

    subheaders_length += FRAGMENTATION_HEADER_LENGTH;

    if (fragmentation_subheader.Purpose == 1)
    {
        return 0;
    }
    return 1;
}

void Framer_receive::decode_fragmentation_subheader()
{
    fragmentation_subheader.Purpose = (wran_frame.data[wran_frame.next_index] & 0x80) >> 7;
    fragmentation_subheader.FC = (wran_frame.data[wran_frame.next_index] & 0x60) >> 5;
    fragmentation_subheader.FSN = ((wran_frame.data[wran_frame.next_index] & 0x1F) << 5) | ((wran_frame.data[wran_frame.next_index + 1] & 0xF8) >> 3);
    ++(wran_frame.next_index);
    ++(wran_frame.next_index);
}

void Framer_receive::decode_PDU()
{
    switch (fragmentation_subheader.FC)
    {
    case FC_NO_FRAGMENTATION:
        reset_SDU();

        if (append_MAC_PDU_to_SDU() == 1)
        {
            send_SDU();
        }

        fragmentation_state = NO_FRAGMENTATION;
        break;

    case FC_FIRST_FRAGMENT:
        reset_SDU();
        sequence_number = fragmentation_subheader.FSN;
        fragmentation_state = FRAGMENTED;
        append_MAC_PDU_to_SDU();
        break;

    case FC_MIDDLE_FRAGMENT:
        sequence_number = (sequence_number + 1) % 1024;
        if (fragmentation_state == FRAGMENTED && fragmentation_subheader.FSN == sequence_number)
        {
            append_MAC_PDU_to_SDU();
        }
        else
        {
            fragmentation_state = NO_FRAGMENTATION;
            skip_MAC_frame();
        }
        break;

    case FC_LAST_FRAGMENT:
        sequence_number = (sequence_number + 1) % 1024;

        if (fragmentation_state == FRAGMENTED && fragmentation_subheader.FSN == sequence_number) // Add PDU to SDU
        {
            if (append_MAC_PDU_to_SDU() == 1)
            {
                send_SDU();
            }
        }
        else // Skip this PDU
        {
            skip_MAC_frame();
        }

        fragmentation_state = NO_FRAGMENTATION;
        break;

    default:
        break;
    }
}

void Framer_receive::reset_SDU()
{
    sdu.next_index = 0;
}

int Framer_receive::send_SDU()
{
    std::cout << program_name << " Sending " << sdu.next_index << " bytes to TUN interface" << std::endl;
    for (int i = 0; i < sdu.next_index; ++i)
    {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)*(sdu.data + i) << std::dec;
    }
    std::cout << std::endl;

    while ((write(fd_tun, sdu.data, sdu.next_index)) == -1)
    {
        if (errno != EINTR)
        {
            std::cerr << program_name << " Error writing to TUN interface" << std::endl;
            return 0;
        }
    }
    return 1;
}

int Framer_receive::append_MAC_PDU_to_SDU()
{
    int pdu_length = MAC_header.length - MAC_HEADER_LENGTH - subheaders_length - MAC_CRC_LENGTH;

    if (sdu.next_index + pdu_length > sdu.length)
    {
        std::cerr << program_name << " SDU (" << sdu.next_index << " bytes) longer than configured MTU (" << sdu.length << " bytes)" << std::endl;
        skip_MAC_frame();

        fragmentation_state = NO_FRAGMENTATION;
        return 0;
    }

    for (int i = 0; i < pdu_length; ++i)
    {
        sdu.data[(sdu.next_index)++] = wran_frame.data[(wran_frame.next_index)++];
    }
    wran_frame.next_index += MAC_CRC_LENGTH;
    return 1;
}

void Framer_receive::skip_MAC_frame()
{
    wran_frame.next_index = MAC_frame_start_index + MAC_header.length;
}