#include "nps_socket.h"
#include <sys/ioctl.h>

int nps_nonblocking(int s)
{
    int nb = 1;
    return ioctl(s, FIONBIO, &nb);
}

int nps_blocking(int s)
{
    int nb = 0;
    return ioctl(s, FIONBIO, &nb);
}
