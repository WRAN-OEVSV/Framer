#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "common.h"

extern char *program_name;  //Global variable for program name stored in argv[0]

int open_tun_interface_sender();
int open_tun_interface_receiver();
uint8_t crc8(const uint8_t * data, size_t size);
uint32_t calculate_crc32(const uint8_t *data, size_t size);
uint32_t check_crc32(const uint8_t *data, size_t size);

#endif