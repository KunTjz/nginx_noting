#include "util_func.h"
#include "util_define.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

int try_lock(int fd)
{ 
    struct flock lock;

    if (fcntl(fd, F_GETLK, &lock) == -1) {
	NPS_LOG(NPS_LOG_ERR, "fcntl(F_GETLCK) failed:%s", strerror(errno));
	return NPS_ERR;
    }

    switch (lock.l_type) {

	case F_RDLCK:
	    NPS_LOG(NPS_LOG_INFO, "read lock already set by:%d", lock.l_pid);
	    break;

	case F_WRLCK:
	    NPS_LOG(NPS_LOG_INFO, "write lock already set by:%d\n", lock.l_pid);
	    break;

	case F_UNLCK:
	    return NPS_OK;

	default:
	    NPS_LOG(NPS_LOG_ERR, "unknown lock type");
	    break;
    }

    return NPS_ERR;
}

int lock_file(int fd, int type)
{
    struct flock lock;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_type = type;

    /*
     * set lock
     */   
    if (0 == fcntl(fd, F_SETLK, &lock)) {

	switch (lock.l_type) {
	    case F_RDLCK:
		NPS_LOG(NPS_LOG_INFO, "read lock set to fd:%d  by:%d\n", fd, getpid());
		break;

	    case F_WRLCK:
		NPS_LOG(NPS_LOG_INFO, "write lock set to fd:%d  by:%d\n", fd, getpid());
		break;

	    default:
		NPS_LOG(NPS_LOG_ERR, "unkonwn lock type");   
		return NPS_ERR;
	}

	return NPS_OK;
    }else {
	NPS_LOG(NPS_LOG_ERR, "fcntl(F_SETLK) failed:%s, %d", strerror(errno), getpid());
	return NPS_ERR;	
    }
}

int unlock_file(int fd)
{
    struct flock lock;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_type = F_UNLCK;
  
    if (-1 == fcntl(fd, F_SETLK, &lock)) {
	NPS_LOG(NPS_LOG_ERR, "fcntl(F_SETLK) failed:%s, %d", strerror(errno), getpid());
	return NPS_ERR;	
    }
    
    NPS_LOG(NPS_LOG_INFO, "unlock by %d successfully", getpid());

    return NPS_OK;
}

