#ifndef COMMON_H
#define COMMON_H

#define WRAN_FRAME_LENGTH (2560/8)
#define TUN_INTERFACE_MTU 1500

#define MAC_HEADER_LENGTH (32/8)
#define MAC_CRC_LENGTH (32/8)
#define FRAGMENTATION_HEADER_LENGTH (16/8)  //length is 2 bytes for a fragmentation subheader (3 byte for packing subheader (which is not implemented))
#define MINIMAL_PAYLOAD_LENGTH 2    //Number of MAC payload bytes a MAC PDU should contain at least, otherwise no MAC PDU will be added to the frame. Can be set to 0.


//Bits in fragmentation subheader
#define PURPOSE_FRAGMENTATION 0x0
#define FC_NO_FRAGMENTATION 0x0
#define FC_LAST_FRAGMENT 0x1
#define FC_FIRST_FRAGMENT 0x2
#define FC_MIDDLE_FRAGMENT 0x3

//MAC subheaders (according to IEE 802.22 Table 7)
#define SUBHEADER_FRAGMENTATION 0x1

struct wran_frame_t
{
    uint8_t data[WRAN_FRAME_LENGTH];
    int length;    //Holds the maximum (sender) or actual (receiver) length of a WRAN frame
    int next_index;   //Index of the next unprocessed byte in the array
};

struct sdu_t
{
    int length;    //Actual (sender) or maximum (receiver) length of a SDU
    int next_index;   //Index of the next element in the array which has not been processed yet
    uint8_t data[TUN_INTERFACE_MTU];
};

struct MAC_header_t
{
    unsigned int length : 11;
    unsigned int UCS : 1;
    unsigned int QPA : 1;
    unsigned int EC : 1;
    unsigned int EKS : 2;
    unsigned int Type : 5;
    unsigned int FT : 3;
    unsigned int HCS_valid : 1; // Set to 1 if HCS valid.
};

struct fragmentation_subheader_t
{
unsigned int Purpose : 1;
unsigned int FC : 2;
unsigned int FSN : 10;
};

enum fragmentation_state_t
{
    FRAGMENTED,
    NO_FRAGMENTATION,
};

#define WRAN_FRAME_UNPROCESSED_BYTES (wran_frame.length - wran_frame.next_index)
#define SDU_UNPROCESSED_BYTES (sdu.length - sdu.next_index)
#endif