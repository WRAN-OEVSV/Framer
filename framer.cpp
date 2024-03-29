#include <iostream>
#include <iomanip>
#include <thread>
#include <unistd.h>

#include "framer.h"
#include "common.h"
#include "functions.h"
#include "framer_receive.h"
#include "framer_transmit.h"

char *program_name;

int main(int argc, char **argv)
{
    int fd_tun = 0;
    program_name = argv[0];

    fd_tun = open_tun_interface();
    if (fd_tun < 0)
    {
        exit(EXIT_FAILURE);
    }

    Framer_transmit framer_transmit_inst(fd_tun);
    Framer_receive framer_receive_inst(fd_tun);

    std::thread framer_transmit_thread(framer_transmit_inst);
    std::thread framer_receive_thread(framer_receive_inst);

    framer_transmit_thread.join();
    framer_receive_thread.join();

    close(fd_tun);

    exit(EXIT_SUCCESS);
}