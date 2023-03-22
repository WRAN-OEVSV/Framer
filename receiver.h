#ifndef RECEIVER_H
#define RECEIVER_H

int read_wran_frame(struct wran_frame_t *wran_frame);
int decode_MAC_header(struct wran_frame_t *wran_frame, struct MAC_header_t *MAC_header);
void decode_fragmentation_subheader(struct wran_frame_t *wran_frame, struct fragmentation_subheader_t *fragmentation_subheader);
void reset_sdu(struct sdu_t* sdu);
int send_sdu(int fd, struct sdu_t *sdu);
int append_MAC_PDU_to_SDU(struct wran_frame_t *wran_frame, struct sdu_t *sdu, const struct MAC_header_t *MAC_header);

#endif