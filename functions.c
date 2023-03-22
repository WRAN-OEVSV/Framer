#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <linux/if_arp.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <string.h>
#include "zlib.h"

#include "functions.h"
#include "common.h"

int open_tun_interface_sender()
{
    int fd_tun = open("/dev/net/tun", O_RDONLY);
    if (fd_tun == -1)
    {
        fprintf(stderr, "%s Cannot open TUN interface1 %s\n", program_name, strerror(errno));
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    strncpy(ifr.ifr_name, "wran_send", IFNAMSIZ);
    int res = ioctl(fd_tun, TUNSETIFF, &ifr);
    if (res == -1)
    {
        fprintf(stderr, "%s Cannot open TUN interface %s\n", program_name, strerror(errno));
        return -1;
    }

    // TODO: Replace system() with appropriate ioctl() calls if required.
    if (system("sudo ip addr add 192.168.2.1/24 dev wran_send") < 0)
    {
        return -1;
    }
    if (system("sudo ip link set dev wran_send mtu 1500") < 0)
    {
        return -1;
    }
    if (system("sudo ip link set up dev wran_send") < 0)
    {
        return -1;
    }

    return fd_tun;
}

int open_tun_interface_receiver()
{
    int fd_tun = open("/dev/net/tun", O_WRONLY);
    if (fd_tun == -1)
    {
        fprintf(stderr, "%s Cannot open TUN interface %s\n", program_name, strerror(errno));
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    strncpy(ifr.ifr_name, "wran_receive", IFNAMSIZ);
    int res = ioctl(fd_tun, TUNSETIFF, &ifr);
    if (res == -1)
    {
        fprintf(stderr, "%s Cannot open TUN interface %s\n", program_name, strerror(errno));
        return -1;
    }

    // TODO: Replace system() with appropriate ioctl() calls if required.
    if (system("sudo ip link set dev wran_receive mtu 1500") < 0)
    {
        return -1;
    }
    if (system("sudo ip link set up dev wran_receive") < 0)
    {
        return -1;
    }

    return fd_tun;
}

// TODO: Proper CRC implementation
uint8_t crc8(const uint8_t *data, size_t size)
{
return 0;
}

uint32_t calculate_crc32(const uint8_t *data, size_t size)
{
    uint32_t crc = crc32(0, data, size);
    return crc;
}

uint32_t check_crc32(const uint8_t *data, size_t size)
{
    uint32_t crc = crc32(0, data, size - MAC_CRC_LENGTH);
    
    printf("%02x", crc);
    
    crc = htonl(crc);
    return (crc == *((uint32_t *)(data + size - MAC_CRC_LENGTH)));
}