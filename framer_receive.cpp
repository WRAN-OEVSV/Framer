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
        if (read_DS_US_burst_from_WRAN_interface() == -1)
        {
            return;
        }

        process_DS_US_burst();
    }
}

Framer_receive::Framer_receive(int fd_tun)
{
    this->fd_tun = fd_tun;
    sdu.length = TUN_INTERFACE_MTU;
}

int Framer_receive::read_DS_US_burst_from_WRAN_interface()
{
    // TODO: Interface with existing application
    // Receiver should block until a DS/US burst can be read from WRAN
    // DS_US_burst.data = ...

    DS_US_burst.next_index = 0;
    return 0;
}

void Framer_receive::process_DS_US_burst()
{
    while (DS_US_BURST_UNPROCESSED_BYTES > 0)
    {
        MAC_PDU_start_index = DS_US_burst.next_index;

        if ((DS_US_BURST_LENGTH - DS_US_burst.next_index < MINIMUM_MAC_PDU_LENGTH) || (decode_MAC_PDU_header() == 0) || (MAC_PDU_header.length < MINIMUM_MAC_PDU_LENGTH) || (MAC_PDU_header.length > DS_US_BURST_UNPROCESSED_BYTES + MAC_PDU_HEADER_LENGTH)) // Skip entire DS/US burst if MAC Header checksum incorrect or length of the MAC PDU invalid.
        {
            break;
        }
        if (check_crc32() == 0 || decode_subheaders() == 0) // Skip MAC PDU if CRC is incorrect or subheaders invalid
        {
            skip_MAC_PDU();
        }
        else
        {
            decode_MAC_PDU();
        }
    }
}

int Framer_receive::decode_MAC_PDU_header()
{
    MAC_PDU_header.length = (DS_US_burst.data[DS_US_burst.next_index] << 3) | ((DS_US_burst.data[DS_US_burst.next_index + 1] & 0xE0) >> 5);
    ++(DS_US_burst.next_index);
    MAC_PDU_header.UCS = (DS_US_burst.data[DS_US_burst.next_index] & 0x10) >> 4;
    MAC_PDU_header.QPA = (DS_US_burst.data[DS_US_burst.next_index] & 0x08) >> 3;
    MAC_PDU_header.EC = (DS_US_burst.data[DS_US_burst.next_index] & 0x04) >> 2;
    MAC_PDU_header.EKS = (DS_US_burst.data[DS_US_burst.next_index] & 0x03);
    ++(DS_US_burst.next_index);
    MAC_PDU_header.Type = (DS_US_burst.data[DS_US_burst.next_index] & 0xF8) >> 3;
    MAC_PDU_header.FT = (DS_US_burst.data[DS_US_burst.next_index] & 0x07);
    ++(DS_US_burst.next_index);
    ++(DS_US_burst.next_index);
    MAC_PDU_header.HCS_valid = (crc8(DS_US_burst.data + MAC_PDU_start_index, MAC_PDU_HEADER_LENGTH) == 0);
    return MAC_PDU_header.HCS_valid;
}

int Framer_receive::check_crc32()
{
    uint32_t crc = crc32(0L, Z_NULL, 0); // Get required initial value of crc32() function cf. https://zlib.net/manual.html
    crc = crc32(crc, DS_US_burst.data + MAC_PDU_start_index, MAC_PDU_header.length - MAC_PDU_CRC_LENGTH);
    crc = htonl(crc); // Convert to network byte order (Big Endian; MSB first) in order to compare it with the CRC in the MAC PDU
    return (crc == *((uint32_t *)(DS_US_burst.data + MAC_PDU_start_index + MAC_PDU_header.length - MAC_PDU_CRC_LENGTH)));
}

int Framer_receive::decode_subheaders()
{
    subheaders_length = 0;
    if (MAC_PDU_header.Type != SUBHEADER_FRAGMENTATION) // In the current implementation a fragmentation subheader must be present without any other subheaders
    {
        return 0;
    }

    decode_fragmentation_subheader();

    subheaders_length += FRAGMENTATION_SUBHEADER_LENGTH;

    if (fragmentation_subheader.Purpose == 1) // Packing subheader (indicated by Purpose bit = 1) is not supported
    {
        return 0;
    }
    return 1;
}

void Framer_receive::decode_fragmentation_subheader()
{
    fragmentation_subheader.Purpose = (DS_US_burst.data[DS_US_burst.next_index] & 0x80) >> 7;
    fragmentation_subheader.FC = (DS_US_burst.data[DS_US_burst.next_index] & 0x60) >> 5;
    fragmentation_subheader.FSN = ((DS_US_burst.data[DS_US_burst.next_index] & 0x1F) << 5) | ((DS_US_burst.data[DS_US_burst.next_index + 1] & 0xF8) >> 3);
    ++(DS_US_burst.next_index);
    ++(DS_US_burst.next_index);
}

void Framer_receive::decode_MAC_PDU()
{
    switch (fragmentation_subheader.FC)
    {
    case FC_NO_FRAGMENTATION:
        reset_SDU();

        if (append_MAC_PDU_payload_to_SDU() == 1)
        {
            send_SDU_to_TUN_interface();
        }

        fragmentation_state = NOT_FRAGMENTED;
        break;

    case FC_FIRST_FRAGMENT:
        reset_SDU();
        sequence_number = fragmentation_subheader.FSN;
        fragmentation_state = FRAGMENTED;
        append_MAC_PDU_payload_to_SDU();
        break;

    case FC_MIDDLE_FRAGMENT:
        sequence_number = (sequence_number + 1) % 1024;
        if (fragmentation_state == FRAGMENTED && fragmentation_subheader.FSN == sequence_number) // Add PDU to SDU
        {
            append_MAC_PDU_payload_to_SDU();
        }
        else // Skip this PDU
        {
            fragmentation_state = NOT_FRAGMENTED;
            skip_MAC_PDU();
        }
        break;

    case FC_LAST_FRAGMENT:
        sequence_number = (sequence_number + 1) % 1024;

        if (fragmentation_state == FRAGMENTED && fragmentation_subheader.FSN == sequence_number) // Add PDU to SDU
        {
            if (append_MAC_PDU_payload_to_SDU() == 1)
            {
                send_SDU_to_TUN_interface();
            }
        }
        else // Skip this PDU
        {
            skip_MAC_PDU();
        }

        fragmentation_state = NOT_FRAGMENTED;
        break;

    default:
        break;
    }
}

void Framer_receive::reset_SDU()
{
    sdu.next_index = 0;
}

int Framer_receive::send_SDU_to_TUN_interface()
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

int Framer_receive::append_MAC_PDU_payload_to_SDU()
{
    int SDU_length = MAC_PDU_header.length - MAC_PDU_HEADER_LENGTH - subheaders_length - MAC_PDU_CRC_LENGTH;

    if (sdu.next_index + SDU_length > sdu.length)
    {
        std::cerr << program_name << "Received SDU (" << sdu.next_index << " bytes) longer than configured MTU (" << sdu.length << " bytes)" << std::endl;
        skip_MAC_PDU();

        fragmentation_state = NOT_FRAGMENTED;
        return 0;
    }

    for (int i = 0; i < SDU_length; ++i)
    {
        sdu.data[(sdu.next_index)++] = DS_US_burst.data[(DS_US_burst.next_index)++];
    }

    skip_MAC_PDU(); // Set DS_US_burst.next_index to beginning of next PDU
    return 1;
}

void Framer_receive::skip_MAC_PDU()
{
    DS_US_burst.next_index = MAC_PDU_start_index + MAC_PDU_header.length;
}