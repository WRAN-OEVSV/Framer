#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#include "functions.h"
#include "receiver.h"
#include "common.h"

char *program_name;

int main(int argc, char **argv)
{
    int fd_tun;
    struct sdu_t sdu = {0};
    sdu.length = TUN_INTERFACE_MTU;
    struct wran_frame_t wran_frame = {0};
    struct MAC_header_t MAC_header = {0};
    struct fragmentation_subheader_t fragmentation_subheader = {0};
    enum fragmentation_state_t fragmentation_state = NO_FRAGMENTATION;
    uint16_t sequence_number = 0;
    unsigned char skip_frame_flag = 0;
    unsigned int mac_frame_start_index = 0;

    program_name = argv[0];

    if ((fd_tun = open_tun_interface_receiver()) == -1)
    {
        exit(EXIT_FAILURE);
    }

    while (1) // TODO: Add exit condition (signal handling)
    {

        // Read WRAN frame
        if (read_wran_frame(&wran_frame) == -1)
        {
            fprintf(stderr, "%s Error reading WRAN frame: %s", program_name, strerror(errno));
            close(fd_tun);
            exit(EXIT_FAILURE);
        }
        ///****************

        while (WRAN_FRAME_UNPROCESSED_BYTES > 0)
        {
            skip_frame_flag = 0;
            mac_frame_start_index = wran_frame.next_index;

            // Decode MAC header
            if (decode_MAC_header(&wran_frame, &MAC_header) == 0                                         // FCS incorrect -> Discard entire WRAN frame. Only skipping this frame is not possible as the length given in the MAC header may not be correct.
                && MAC_header.length <= (wran_frame.length - wran_frame.next_index + MAC_HEADER_LENGTH)) // Length given in MAC header is longer than length of the remaining wran_frame.
            {
                break; // Exit the inner while loop and read new WRAN frame
            }
            else // FCS correct
            {
                // In the current implementation a fragmentation header must always be present.

                if (check_crc32(wran_frame.data + mac_frame_start_index, MAC_header.length) != 1)
                {
#ifdef DEBUG
                    fprintf(stderr, "%s Invalid CRC-32\n", program_name);
#endif

                    skip_frame_flag = 1;
                }

                if (MAC_header.Type != SUBHEADER_FRAGMENTATION) // Fragmentation subheader must be present without any other subheaders
                {
                    fprintf(stderr, "%s Unsupported subheaders in MAC frame\n", program_name);
                    skip_frame_flag = 1;
                }

                decode_fragmentation_subheader(&wran_frame, &fragmentation_subheader);

                if (fragmentation_subheader.Purpose == 1)
                {
                    fprintf(stderr, "%s Unsupported Purpose bit in fragmentation subheader\n", program_name);
                    skip_frame_flag = 1;
                }

                if (skip_frame_flag == 1)
                {
                    wran_frame.next_index += MAC_header.length - MAC_HEADER_LENGTH - FRAGMENTATION_HEADER_LENGTH;
                }
                else
                {
                    switch (fragmentation_subheader.FC)
                    {
                    case FC_NO_FRAGMENTATION:
                        reset_sdu(&sdu);

                        if (append_MAC_PDU_to_SDU(&wran_frame, &sdu, &MAC_header) == 0)
                        {
                            send_sdu(fd_tun, &sdu);
                        }

                        fragmentation_state = NO_FRAGMENTATION;
                        break;

                    case FC_LAST_FRAGMENT:
                        sequence_number = (sequence_number + 1) % 1024;

                        if (fragmentation_state == FRAGMENTED && fragmentation_subheader.FSN == sequence_number) // Add PDU to SDU
                        {
                            if (append_MAC_PDU_to_SDU(&wran_frame, &sdu, &MAC_header) == 0)
                            {
                                send_sdu(fd_tun, &sdu);
                            }
                        }
                        else // Skip PDU
                        {
#ifdef DEBUG
                            printf("%s Skipping PDU FC_LAST_FRAGMENT\n", program_name);
#endif
                            wran_frame.next_index += MAC_header.length - MAC_HEADER_LENGTH - FRAGMENTATION_HEADER_LENGTH;
                        }

                        fragmentation_state = NO_FRAGMENTATION;
                        break;

                    case FC_FIRST_FRAGMENT:
                        reset_sdu(&sdu);
                        sequence_number = fragmentation_subheader.FSN;

                        if (append_MAC_PDU_to_SDU(&wran_frame, &sdu, &MAC_header) == 0)
                        {
                            fragmentation_state = FRAGMENTED;
                        }
                        else // If an error occured when adding the MAC PDU to the SDU
                        {
                            fragmentation_state = NO_FRAGMENTATION;
                        }

                        break;

                    case FC_MIDDLE_FRAGMENT:
                        sequence_number = (sequence_number + 1) % 1024;
                        if (fragmentation_state == FRAGMENTED && fragmentation_subheader.FSN == sequence_number)
                        {
                            if (append_MAC_PDU_to_SDU(&wran_frame, &sdu, &MAC_header) != 0)
                            {
                                fragmentation_state = NO_FRAGMENTATION;
                            }
                        }
                        else
                        {
#ifdef DEBUG
                            printf("%s Skipping PDU FC_MIDDLE_FRAGMENT\n", program_name);
#endif
                            fragmentation_state = NO_FRAGMENTATION;
                            wran_frame.next_index += MAC_header.length - MAC_HEADER_LENGTH - FRAGMENTATION_HEADER_LENGTH;
                        }

                        break;

                    default:
                        fprintf(stderr, "%s FC incorrectly decoded\n", program_name);
                        close(fd_tun);
                        exit(EXIT_FAILURE);
                        break;
                    }
                }
            }
        }
    }

    close(fd_tun);
    exit(EXIT_SUCCESS);
}

int read_wran_frame(struct wran_frame_t *wran_frame)
{
    // TODO: Interface to existing application

    // wran_frame->data =;
    // wran_frame->length= ...;

  //***

    wran_frame->next_index = 0; // Reset next_index as new frame was read.
    return 0;
}

// Decode MAC header according to Table 6 of IEEE 802.22.
int decode_MAC_header(struct wran_frame_t *wran_frame, struct MAC_header_t *MAC_header)
{
    MAC_header->length = (wran_frame->data[wran_frame->next_index] << 3) | ((wran_frame->data[wran_frame->next_index + 1] & 0xE0) >> 5);
    ++(wran_frame->next_index);
    MAC_header->UCS = (wran_frame->data[wran_frame->next_index] & 0x10) >> 4;
    MAC_header->QPA = (wran_frame->data[wran_frame->next_index] & 0x08) >> 3;
    MAC_header->EC = (wran_frame->data[wran_frame->next_index] & 0x04) >> 2;
    MAC_header->EKS = (wran_frame->data[wran_frame->next_index] & 0x03);
    ++(wran_frame->next_index);
    MAC_header->Type = (wran_frame->data[wran_frame->next_index] & 0xF8) >> 3;
    MAC_header->FT = (wran_frame->data[wran_frame->next_index] & 0x07);
    ++(wran_frame->next_index);
    ++(wran_frame->next_index);
    MAC_header->HCS_valid = (crc8(wran_frame->data + wran_frame->next_index - 4, 4) == 0);

#ifdef DEBUG
    if (MAC_header->HCS_valid == 0)
    {
        fprintf(stderr, "%s MAC Header Check Sequence incorrect\n", program_name);
    }
#endif

    return MAC_header->HCS_valid;
}

void decode_fragmentation_subheader(struct wran_frame_t *wran_frame, struct fragmentation_subheader_t *fragmentation_subheader)
{
    fragmentation_subheader->Purpose = (wran_frame->data[wran_frame->next_index] & 0x80) >> 7;
    fragmentation_subheader->FC = (wran_frame->data[wran_frame->next_index] & 0x60) >> 5;
    fragmentation_subheader->FSN = ((wran_frame->data[wran_frame->next_index] & 0x1F) << 5) | ((wran_frame->data[wran_frame->next_index + 1] & 0xF8) >> 3);
    ++(wran_frame->next_index);
    ++(wran_frame->next_index);
    return;
}

void reset_sdu(struct sdu_t *sdu)
{
    sdu->next_index = 0;
    return;
}

int send_sdu(int fd, struct sdu_t *sdu)
{
    int ret = 0;
#ifdef DEBUG
    printf("Sending SDU with %d bytes: ", sdu->next_index);
    for (int i = 0; i < sdu->next_index; ++i)
    {
        printf("%02x", *(sdu->data + i));
    }
    printf("\n");
#endif

    while ((ret = write(fd, sdu->data, sdu->next_index)) == -1)
    {
        if (errno != EINTR)
        {
            fprintf(stderr, "%s Error writing to TUN interface: %s\n", program_name, strerror(errno));
            close(fd);
            exit(EXIT_FAILURE);
        }
    }
    reset_sdu(sdu);
    return ret;
}

int append_MAC_PDU_to_SDU(struct wran_frame_t *wran_frame, struct sdu_t *sdu, const struct MAC_header_t *MAC_header)
{
    int length = MAC_header->length - MAC_HEADER_LENGTH - FRAGMENTATION_HEADER_LENGTH - MAC_CRC_LENGTH;

    if (sdu->next_index + length > sdu->length)
    {
        fprintf(stderr, "%s SDU (%d bytes) longer than configured MTU (%d bytes)\n", program_name, sdu->next_index + length, sdu->length);
        wran_frame->next_index += MAC_header->length - MAC_HEADER_LENGTH - FRAGMENTATION_HEADER_LENGTH;
        return -1;
    }

    for (int i = 0; i < length; ++i)
    {
        sdu->data[(sdu->next_index)++] = wran_frame->data[(wran_frame->next_index)++];
    }
    wran_frame->next_index += MAC_CRC_LENGTH;
    return 0;
}