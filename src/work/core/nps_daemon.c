#include "nps_daemon.h"
#include "../util/util_define.h"
#include "../util/util_func.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <fcntl.h>

int pid_file_fd = -1;

int nps_daemon()
{
    int	    fd;
    int	    i;
    struct rlimit rl;
  
    switch (fork()) {
	case 0:
	    break;
	case -1:
	    NPS_LOG(NPS_LOG_ERR, "fork() failed:%s", strerror(errno));
	    return NPS_ERR;
	default:
	    exit(0);
    }
    
    if (setsid() == -1){
	NPS_LOG(NPS_LOG_ERR, "setsid() failed:%s", strerror(errno));
	return NPS_ERR;
    }

    switch (fork()) {
	case 0:
	    break;
	case -1:
	    NPS_LOG(NPS_LOG_ERR, "fork() failed:%s", strerror(errno));
	    return NPS_ERR;
	default:
	    exit(0);
    }    

    umask(0);

    if (getrlimit(RLIMIT_NOFILE, &rl) == -1){
	NPS_LOG(NPS_LOG_ERR, "getrlimit() failed:%s", strerror(errno));
	return NPS_ERR;
    }   
    
    if (rl.rlim_max == RLIM_INFINITY)
	rl.rlim_max = 1024;
    for (i = 0; i < rl.rlim_max; ++i)
	close(i);

    fd = open("/dev/null", O_RDWR);
    if (fd == -1){
	NPS_LOG(NPS_LOG_ERR, "open() failed:%s", strerror(errno));
	return NPS_ERR;
    }

    if (dup2(fd, fileno(stdin)) == -1) {
        NPS_LOG(NPS_LOG_ERR, "fup2(STDIN) failed:%s", strerror(errno));
	return NPS_ERR;
    }
	
    if (dup2(fd, fileno(stdout)) == -1) {
	NPS_LOG(NPS_LOG_ERR, "fup2(STDOUT) failed:%s", strerror(errno));
	return NPS_ERR;
    }

    if (dup2(fd, fileno(stderr)) == -1) {
	 NPS_LOG(NPS_LOG_ERR, "fup2(STDERR) failed:%s", strerror(errno));
	return NPS_ERR;
    }
    
    return NPS_OK;
}

int create_pidfile()
{
    int fd;
    char buf[16];

    fd = open(NPS_LOCK_FILE, O_RDWR | O_CREAT, NPS_DEFAULT_FILE_MODE);

    if (fd == -1) {
	NPS_LOG(NPS_LOG_ERR, "open(LOCK_FILE) failed:%s", strerror(errno));
	return NPS_ERR;
    }
    
    if (lock_file(fd, F_WRLCK) != NPS_OK) {
   	NPS_LOG(NPS_LOG_ERR, "lock(LOCK_FILE) failed:%s", strerror(errno));
	return NPS_ERR;
    }

    /* empty the file */
    if (ftruncate(fd, 0) == -1) {
	NPS_LOG(NPS_LOG_ERR, "ftruncate() failed:%s", strerror(errno));
	return NPS_ERR;
    }
    
    sprintf(buf, "%ld", (long)getpid());
    if (write(fd, buf, strlen(buf) + 1) == -1) {
   	NPS_LOG(NPS_LOG_ERR, "write(LOCK_FILE) failed:%s", strerror(errno));
	return NPS_ERR;
    }

    pid_file_fd = fd;
}

int destroy_pidfile()
{   
    int fd = pid_file_fd;

    if (unlock_file(fd) != NPS_OK) {
	return NPS_ERR;   
    }

    if (close(fd) == -1) {
	NPS_LOG(NPS_LOG_ERR, "close() failed:%s", strerror(errno));
	return NPS_ERR;
    }

    if (unlink(NPS_LOCK_FILE) == -1) {
 	NPS_LOG(NPS_LOG_ERR, "unlink(%s) failed:%s", NPS_LOCK_FILE, strerror(errno));
	return NPS_ERR;
    }

    pid_file_fd = -1;
    
    return NPS_OK;
}
