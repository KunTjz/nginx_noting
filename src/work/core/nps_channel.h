#ifndef __NPS_CHANNEL_H__
#define __NPS_CHANNEL_H__

#include <unistd.h>

typedef struct{
    unsigned int    command;
    pid_t	    pid;
    int		    slot;
    int		    fd;
} nps_channel_t;

void nps_close_channel(int* fd);
int nps_write_channel(int s, nps_channel_t* ch, size_t size);
int nps_read_channel(int s, nps_channel_t* ch, size_t size);

#endif
