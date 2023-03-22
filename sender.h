#ifndef SENDER_H
#define SENDER_H

void send_wran_frame(struct wran_frame_t *wran_frame);
void append_MAC_header(struct wran_frame_t *wran_frame, uint16_t payload_length, uint8_t present_subheaders);
void append_MAC_CRC(struct wran_frame_t *wran_frame, unsigned int start_index);
void append_fragmentation_subheader(struct wran_frame_t *wran_frame, uint8_t FC, uint16_t length);
void append_SDU_to_frame(struct wran_frame_t *wran_frame, struct sdu_t *sdu, unsigned int length);

#endif