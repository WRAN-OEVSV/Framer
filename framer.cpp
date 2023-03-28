#include <iostream>
#include <iomanip>
#include <thread>
#include <unistd.h>

#include "common.h"
#include "functions.h"
#include "framer_receive.h"
#include "framer_send.h"

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

    Framer_send framer_send_inst(fd_tun);
    Framer_receive framer_receive_inst(fd_tun);

    std::thread framer_send_thread(framer_send_inst);
    std::thread framer_receive_thread(framer_receive_inst);

    framer_send_thread.join();
    framer_receive_thread.join();

    close(fd_tun);

    exit(EXIT_SUCCESS);
}