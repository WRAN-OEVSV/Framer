#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include "functions.h"
#include "common.h"
#include "sender.h"

char *program_name; // Global variable for program name in argv[0]

int main(int argc, char **argv)
{

    int fd_tun;
    struct sdu_t sdu = {0};
    struct wran_frame_t wran_frame = {0};
    wran_frame.length = WRAN_FRAME_LENGTH; // lenght holds the maximum length of a WRAN frame
    enum fragmentation_state_t sdu_fragmentation_status = NO_FRAGMENTATION;
    program_name = argv[0];

    if ((fd_tun = open_tun_interface_sender()) == -1)
    {
        exit(EXIT_FAILURE);
    }

    while (1) // TODO: Add exit condition (signal handling)
    {

        // Read IP packet from TUN interface into sdu structure
        while ((sdu.length = read(fd_tun, &sdu.data, TUN_INTERFACE_MTU)) == -1)
        {
            // errno==EINTR -> read interrupted by incoming signal, therefore try to read again
            if (errno != EINTR)
            {
                fprintf(stderr, "%s Error reading from TUN interface: %s", program_name, strerror(errno));
                close(fd_tun);
                exit(EXIT_FAILURE);
            }
        }
        sdu_fragmentation_status = NO_FRAGMENTATION;
        sdu.next_index = 0;

#ifdef DEBUG
        printf("--------------------\n%s Received SDU (%d byes): ", program_name, sdu.length);
        for (int i = 0; i < sdu.length; ++i)
        {
            printf("%02x", *(sdu.data + i));
        }
        printf("\n");
#endif

        ///****************
        // Generate and send WRAN frames until whole SDU is processed

        while (SDU_UNPROCESSED_BYTES > 0)
        {
            // TODO: Check if subheaders should be sent.
            unsigned int additional_subheaders_length = 0; // For future implementation of subheaders (Table 7 of IEEE 802.22 specification)

            ///****************

            // Check wheter the frame contains enough free bytes
            if (WRAN_FRAME_UNPROCESSED_BYTES < MAC_HEADER_LENGTH + MAC_CRC_LENGTH + additional_subheaders_length) // Not enough bytes available -> Send frame without adding data
            {
                send_wran_frame(&wran_frame);
            }

            else
            {
                // For simplification a fragmentation header is always sent if SDU is transmitted

                if (WRAN_FRAME_UNPROCESSED_BYTES < MAC_HEADER_LENGTH + MAC_CRC_LENGTH + additional_subheaders_length + FRAGMENTATION_HEADER_LENGTH + MINIMAL_PAYLOAD_LENGTH) // Less than MINIMAL_PAYLOAD_LENGTH bytes can be added to PDU -> Do not add data to frame
                {
                    if (additional_subheaders_length == 0) // No additional subheaders -> Do not add PDU containing only MAC header and CRC
                    {
                        send_wran_frame(&wran_frame);
                    }
                    else // Send PDU only containing subheaders without SDU
                    {
                        fprintf(stderr, "Subheaders not implemented yet");
                        exit(EXIT_FAILURE);
                    }
                }

                else // Add MAC PDU containing subheaders and SDU
                {
                    unsigned int available_bytes_for_sdu = WRAN_FRAME_UNPROCESSED_BYTES - MAC_HEADER_LENGTH - MAC_CRC_LENGTH - additional_subheaders_length - FRAGMENTATION_HEADER_LENGTH;
                    unsigned int wran_frame_next_index = wran_frame.next_index;
                    if (SDU_UNPROCESSED_BYTES <= available_bytes_for_sdu) // Entire unprocessed SDU can be sent
                    {
                        if (sdu_fragmentation_status == NO_FRAGMENTATION) // Fragmentation header: FC_NO_FRAGMENTATION
                        {
                            append_MAC_header(&wran_frame, MAC_HEADER_LENGTH + additional_subheaders_length + FRAGMENTATION_HEADER_LENGTH + SDU_UNPROCESSED_BYTES + MAC_CRC_LENGTH, SUBHEADER_FRAGMENTATION);
                            append_fragmentation_subheader(&wran_frame, FC_NO_FRAGMENTATION, SDU_UNPROCESSED_BYTES);
                            append_SDU_to_frame(&wran_frame, &sdu, SDU_UNPROCESSED_BYTES);
                            append_MAC_CRC(&wran_frame, wran_frame_next_index);
                        }
                        else // Fragmentation header: FC_LAST_FRAGMENT
                        {
                            append_MAC_header(&wran_frame, MAC_HEADER_LENGTH + additional_subheaders_length + FRAGMENTATION_HEADER_LENGTH + SDU_UNPROCESSED_BYTES + MAC_CRC_LENGTH, SUBHEADER_FRAGMENTATION);
                            append_fragmentation_subheader(&wran_frame, FC_LAST_FRAGMENT, SDU_UNPROCESSED_BYTES);
                            append_SDU_to_frame(&wran_frame, &sdu, SDU_UNPROCESSED_BYTES);
                            append_MAC_CRC(&wran_frame, wran_frame_next_index);
                        }
                        sdu_fragmentation_status = NO_FRAGMENTATION;
                    }
                    else // Only available_bytes_for_sdu of the SDU can be sent
                    {
                        if (sdu_fragmentation_status == NO_FRAGMENTATION) // Fragmentation header: FC_FIRST_FRAGMENT
                        {
                            append_MAC_header(&wran_frame, WRAN_FRAME_UNPROCESSED_BYTES, SUBHEADER_FRAGMENTATION);
                            append_fragmentation_subheader(&wran_frame, FC_FIRST_FRAGMENT, available_bytes_for_sdu);
                            append_SDU_to_frame(&wran_frame, &sdu, available_bytes_for_sdu);
                            append_MAC_CRC(&wran_frame, wran_frame_next_index);
                        }
                        else // Fragmentation header: FC_MIDDLE_FRAGMENT
                        {
                            append_MAC_header(&wran_frame, WRAN_FRAME_UNPROCESSED_BYTES, SUBHEADER_FRAGMENTATION);
                            append_fragmentation_subheader(&wran_frame, FC_MIDDLE_FRAGMENT, available_bytes_for_sdu);
                            append_SDU_to_frame(&wran_frame, &sdu, available_bytes_for_sdu);
                            append_MAC_CRC(&wran_frame, wran_frame_next_index);
                        }
                        sdu_fragmentation_status = FRAGMENTED;
                    }
                }
            }
        }

        ///****************
        // Check if read() would block. If so, send frame even if it is not full yet

        struct pollfd pollfd;
        pollfd.fd = fd_tun;
        pollfd.events = POLLIN;

        while (poll(&pollfd, 1, 0) == -1)
        {
            if (errno != EINTR)
            {
                fprintf(stderr, "%s Error calling poll(): %s", program_name, strerror(errno));
                close(fd_tun);
                exit(EXIT_FAILURE);
            }
        }

        if ((pollfd.revents & POLLIN) == 0) // read() will block -> Send frame
        {
            send_wran_frame(&wran_frame);
        }
    };

    ///****************

    close(fd_tun);
    exit(EXIT_SUCCESS);
}

void send_wran_frame(struct wran_frame_t *wran_frame)
{
#ifdef DEBUG
    printf("%s Sending %d bytes: ", program_name, wran_frame->next_index);
    for (int i = 0; i < wran_frame->next_index; ++i)
    {
        printf("%02x", *(wran_frame->data + i));
    }
    printf("\n");

    for (int i = 0; i < wran_frame->next_index; ++i)
    {
        //printf("wran_frame->data[%d] = 0x%02x;\n", i, *(wran_frame->data + i));
    }
    printf("\n");

#endif

    wran_frame->next_index = 0;

    // TODO: Interface to existing software
    // Send data from array index 0 .. next_index-1 -> Padding if required
}

// Adds MAC header to WRAN frame according to Table 6 of IEEE 802.22
void append_MAC_header(struct wran_frame_t *wran_frame, uint16_t length, uint8_t present_subheaders)
{
    struct MAC_header_t MAC_header = {0}; // All Flags of MAC header (including FT) statically set to zero.

    unsigned int start_index = wran_frame->next_index;
    wran_frame->data[(wran_frame->next_index)++] = (length & 0x07F8) >> 3;
    wran_frame->data[(wran_frame->next_index)++] = ((length & 0x0007) << 5) | (MAC_header.UCS << 4) | (MAC_header.QPA << 3) | (MAC_header.EC << 2) | (MAC_header.EKS << 0);
    wran_frame->data[(wran_frame->next_index)++] = (present_subheaders << 3) | (MAC_header.FT);
    wran_frame->data[(wran_frame->next_index)++] = crc8(wran_frame->data + start_index, 3);
    return;
}

void append_MAC_CRC(struct wran_frame_t *wran_frame, unsigned int start_index)
{

    uint32_t crc = calculate_crc32(wran_frame->data + start_index, wran_frame->next_index - start_index);

    crc = htonl(crc); // Convert to network byte order (Big Endian; MSB first) as required by IEEE 802.22.

    for (int i = 0; i < MAC_CRC_LENGTH; ++i)
    {
        *(wran_frame->data + wran_frame->next_index) = *((uint8_t *)(&crc) + i);
        ++(wran_frame->next_index);
    }

    return;
}

void append_fragmentation_subheader(struct wran_frame_t *wran_frame, uint8_t FC, uint16_t length)
{
    static uint16_t sequence_number = 0;
    wran_frame->data[(wran_frame->next_index)++] = (PURPOSE_FRAGMENTATION << 7) | (FC << 5) | ((sequence_number & 0x3E0) >> 5);
    wran_frame->data[(wran_frame->next_index)++] = ((sequence_number & 0x1F) << 3);
    sequence_number = (sequence_number + 1) % 1024;
    return;
}

void append_SDU_to_frame(struct wran_frame_t *wran_frame, struct sdu_t *sdu, unsigned int length)
{
    for (int i = 0; i < length; ++i)
    {
        wran_frame->data[(wran_frame->next_index)++] = sdu->data[(sdu->next_index)++];
    }
    return;
}