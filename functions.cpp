#include <iostream>
#include <iomanip>
#include <cstdint>
#include <cerrno>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <linux/if_arp.h>
#include <sys/ioctl.h>
#include "zlib.h"

#include "functions.h"
#include "common.h"

int open_tun_interface()
{
    int fd_tun = open("/dev/net/tun", O_RDWR);
    if (fd_tun == -1)
    {
        std::cerr << program_name << " Cannot open TUN interface " << std::endl;
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    strncpy(ifr.ifr_name, "tun_wran", IFNAMSIZ);
    int res = ioctl(fd_tun, TUNSETIFF, &ifr);
    if (res == -1)
    {
        std::cerr << program_name << " Cannot open TUN interface" << std::endl;
        return -1;
    }

    // TODO: Replace system() with appropriate ioctl() calls if required.
    if (system("sudo ip addr add 192.168.2.1/24 dev tun_wran") < 0)
    {
        return -1;
    }
    if (system("sudo ip link set dev tun_wran mtu 1500") < 0)
    {
        return -1;
    }
    if (system("sudo ip link set up dev tun_wran") < 0)
    {
        return -1;
    }

    return fd_tun;
}

// Naive CRC8 implementation computing one bit per step
uint8_t crc8(const uint8_t *data, size_t size)
{
    uint8_t crc = 0, b = 0;
    for (unsigned int i = 0; i <= size; ++i) // For each byte of the data array (+1 to simulate an appended zero-byte)
    {
        for (int j = 7; j >= 0; --j) // For each bit in the byte
        {
            b = (i == size) ? 0 : ((data[i] >> j) & 0x01); //Return 0 in the last iteration of the outer loop (to simulate the appended zero-byte)
            crc = ((crc & 0x80) != 0) ? (((crc << 1) + b) ^ CRC8_POLYNOMIAL) : ((crc << 1) + b);
        }
    }
    return crc;
}