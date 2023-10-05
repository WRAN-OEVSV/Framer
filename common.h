#ifndef COMMON_H
#define COMMON_H

#define DS_US_BURST_LENGTH (2560 / 8) // Static length of the to be created / to be received DS or US bursts in bytes
#define TUN_INTERFACE_MTU 1500

#define MAC_PDU_HEADER_LENGTH 4
#define MAC_PDU_CRC_LENGTH 4
#define FRAGMENTATION_SUBHEADER_LENGTH 2 // Length is 2 bytes for a fragmentation subheader (3 byte for packing subheader (which is not implemented))
#define MINIMUM_MAC_PDU_LENGTH 4         // Minimal size of a MAC PDU (according to IEEE 802.22 a PDU shall be discarded if its length is smaller than 4 bytes)
#define MINIMUM_PAYLOAD_LENGTH 2         // Number of payload bytes a MAC PDU should contain at least, otherwise no MAC PDU will be added to the frame. Can be set to 0.

// Bits in fragmentation subheader
#define PURPOSE_FRAGMENTATION 0x0
#define FC_NO_FRAGMENTATION 0x0
#define FC_LAST_FRAGMENT 0x1
#define FC_FIRST_FRAGMENT 0x2
#define FC_MIDDLE_FRAGMENT 0x3

// MAC PDU subheaders (according to IEE 802.22 Table 7)
#define SUBHEADER_FRAGMENTATION 0x2

// Struct for the created / received DS/US burst
struct DS_US_burst_t
{
    uint8_t data[DS_US_BURST_LENGTH];
    int next_index; // Index of the next unprocessed byte in the array
};

struct sdu_t
{
    int length;     // Actual (transmitter) or maximum (receiver) length of a SDU
    int next_index; // Index of the next element in the array which has not been processed yet
    uint8_t data[TUN_INTERFACE_MTU];
};

struct MAC_PDU_header_t
{
    unsigned int length : 11;
    unsigned int UCS : 1;
    unsigned int QPA : 1;
    unsigned int EC : 1;
    unsigned int EKS : 2;
    unsigned int Type : 5;
    unsigned int FT : 3;
    unsigned int HCS_valid : 1; // Set to 1 if HCS (CRC of the header) is valid
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
    NOT_FRAGMENTED,
};

#define DS_US_BURST_UNPROCESSED_BYTES (DS_US_BURST_LENGTH - DS_US_burst.next_index)
#define SDU_UNPROCESSED_BYTES (sdu.length - sdu.next_index)
#endif