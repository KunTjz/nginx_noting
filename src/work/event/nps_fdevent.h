#ifndef __NPS_FDEVENT_H__
#define __NPS_FDEVENT_H__

#include <sys/types.h>

#define USE_LINUX_EPOLL

typedef handler_t (*fdevent_handler)(int revents);

typedef enum {
    FD_EVENT_TYPE_UNSET = -1,
    FD_EVENT_TYPE_CONNECTION
} fd_evevnt_t;

typedef enum{
    FDEVENT_HANDLER_SELECT,
    FDEVENT_HANDLER_POLL,
    FDEVENT_HANDLER_LINUX_SYSEPOLL,
    FDEVENT_HANDLER_SOLARIS_DEVPOLL,
    FDEVENT_HANDLER_SOLARIS_PORT,
    FDEVENT_HANDLER_FREEBSD_KQUEUE,
    FDEVENT_HANDLER_LIBEV
} fdevent_handler_t;

typedef struct _fnode {
    fdevent_handler handler;
    int		    fd;
    int		    events;
} fnode;

typedef struct {
    int*    ptr;
    size_t  used;
    size_t  size;
} buffer_int;

typedef struct fdevents {
    fdevent_handler_t	    type;
    fdnode**		    fdarray;
    size_t		    maxfds;

#ifdef USE_LINUX_EPOLL
    int			    epoll_fd    
    struct  epoll_event*    epoll_events;
#endif

#ifdef USE_POLL
    struct pollfd*	    pollfds;
    size_t		    size;
    size_t		    used;
    buffer_int		    unused;
#endif

#ifdef	USE_SELECT
    fd_set		    select_read;
    fd_set		    select_write;
    fd_set		    select_error;
    
    fd_set		    select_set_read;
    fd_set		    select_set_write;
    fd_set		    select_set_error;
    
    int			    select_max_fd;

#ifdef USE_LEBEV
    struct  ev_loop*	    libev_loop;
#endif

    int	(*reset)(struct fdevents* ev);
    void (*free)(struct fdevents* ev);
    
    int (*event_set)(struct fdevents* ev, int fde_ndx, int fd, int events);
    int (*event_del)(struct fdevents* ev, int fde_ndx, int fd);
    int (*event_get_revent)(struct fdevents* ev, size_t nfx);
    int (*event_get_fd)(struct fdevents* ev, size_t ndx);

    int (*event_next_fdndx)(struct fdevents* ev, int ndx);

    int (*poll)(struct fdevents* ev, int timeout_ms);
    
    int (*fcntl_set)(struct fdevents* ev, int fd);
} fdevents;

fdevents* fdevent_init(struct server* srv, size_t maxfds, fdevent_handler_t type);
int fdevent_reset(fdevents* ev);
void fdevent_free(fdevents* ev);

int fdevent_event_set(fdevents* ev, int* fde_ndx, int events);
int fdevent_event_del(fdevents* ev, int* fde_ndx, int fd);
int fdevent_event_get_revent(fdevents* ev, size_t ndx);
int fdevent_event_fd(fdevents* ev, size_t ndx);

fdevent_handler fdevent_get_handler(fdevents* ev, int fd);
void* fdevent_get_context(fdevents* ev,int fd);

int fdevent_event_next_fdndx(fdevents* ev, int ndx);

int fdevent_poll(fdevents* ev, int timeout_ms);

int fdevent_register(fdevents* ev, int fd, fdevent_handler handler, void* ctx);
int fdevent_unregister(fdevent* ev, int fd);

int fdevent_fcntl_set(fdevents *ev, int fd);

int fdevent_select_init(fdevents *ev);
int fdevent_poll_init(fdevents *ev);
int fdevent_linux_sysepoll_init(fdevents *ev);

#endif
