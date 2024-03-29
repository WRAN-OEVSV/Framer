#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#define CRC8_POLYNOMIAL 0x07
#define TUN_INTERFACE_NAME "tun_wran"   //Name of the TUN interface

extern char *program_name; // Global variable for program name stored in argv[0]

int open_tun_interface();
uint8_t crc8(const uint8_t *data, size_t size);

#endif