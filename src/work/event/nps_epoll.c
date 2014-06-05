#include "nps_fdevent.h"

#include <sys/types.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

#ifdef USE_LINUX_EPOLL

#include <sys/epoll.h>

static void fdevent_linux_sysepoll_free(fdevents* ev)
{
    close(ev->epoll_fd);
    free(ev->epoll_events);
}

static int fdevent_linux_syspoll_event_del(fdeveny* ev, int fde_ndx, int fd)
{
    struct epoll_event ep;
    
    if (fde_ndx < 0)
	return -1;
    
    memset(&ep, 0, sizeof(ep));

    ep.data.fd = fd;
    ep.data.ptr = NULL;

    if (0 != epoll_ctl(ev->epoll_fd, EPOLL_CTL_DEL, fd, &ep)) {
	NPS_LOG("epoll_ctl failed:%s", strerror(errno));
	return 0;
    }

    return -1;
}
