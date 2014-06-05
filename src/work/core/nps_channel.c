#include "nps_channel.h"
#include "../util/util_define.h"

#include <sys/uio.h>

void nps_close_channel(int* fd)
{
    if (close(fd[0]) == -1
	|| close(fd[1]) == -1)	{
	NPS_LOG(NPS_LOG_WARNING, "close channel failed");
    }
}

int nps_write_channel(int s, nps_channel_t* ch, size_t size)
{
    ssize_t	    n;
    int		    err;
    struct iovec    iov;
    struct msghdr   msg;    
    
    // TO_READ: to learn about msghdr

    union {
	struct cmsghdr	cm;
	char		space[CMSG_SPACE(sizeof(int))];
    } cmsg;

    if (ch->fd == -1) {
	msg.msg_control = NULL;
	msg.msg_controllen = sizeof (cmsg);
    }
    else {
	msg.msg_control = (caddr_t) &cmsg;
	msg.msg_controllen = sizeof(cmsg);

	cmsg.cm.cmsg_len = CMSG_LEN(sizeof(int));
	cmsg.cm.cmsg_level = SOL_SOCKET;
	cmsg.cm.cmsg_type = SCM_RIGHTS;

	*(int*) CMSG_DATA(&cmsg.cm) = ch->fd;
    }

    msg.msg_flags = 0;

    iov[0].iov_base = (char*) ch;
    iov[0].iov_len = size;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg_msg_iov = iov;
    msg.msg_iovlen = 1;

    n = sendmsg(s, &msg, 0);
    
    /*
     * EGAIN常出现在非阻塞描述符操作中,不算一个错误
     * 比如read时没有数据可读，非阻塞的时候
     * 就返回一个EAGAIN
     */
    if (n == -1) {
	err = nps_errno;
	if (err == EAGAIN)
	    return EAGAIN;
	
	NPS_LOG(NPS_LOG_ERR, "sendmsg failed");
	return NPS_ERR;
    }
    
    return NPS_OK;
}

int nps_read_channel(int s, nps_channel_t* ch, size_t size)
{
    ssize_t	    n;
    int		    err;
    struct iovec    iov[1];
    struct msghdr   msg;

    union {
	struct cmsghdr	cm;
	char		space[CMSG_SPACE(sizeof(int))];
    }cmsg;

    iov[0].iov_base = (char*) ch;
    iov[0].iov_len = size;
    
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    msg.msg_control = (caddr_t) &cmsg;
    msg.msg_controllen = sizeof(cmsg);

    n = recvmsg(s, &msg, 0);
    
    if (n == -1) {
	err = errno;
	if (err == EAGAIN) {
	    return EAGAIN;
	}
    
	NPS_LOG(NPS_LOG_ERR, "recvmsg() failed");
	return NPS_ERR;
    }    

    if (n == 0) {
	NPS_LOG(NPS_LOG_WARNING, "recvmsg() returned zore");
	return NPS_ERR;
    }
    
    if ((size_t) n < sizeof(nps_channel_t)) {
	NPS_LOG(NPS_LOG_WARNING, "recvmsg() returned not enough data:%uz", n);
	return NPS_ERR;
    }

    if (ch->command == NPS_CMD_OPEN_CHANNEL) {
	
	if (cmsg.cm.cmsg_len < (socklen_t) CMSG_LEN(sizeof(int))) {
	    NPS_LOG(NPS_LOG_WARNING, "recvmsg() returned too small ancillary data");
	    return NPS_ERR;
	}

	if (cmsg.cm.cmsg_level != SOL_SOCKET || cmsg.cm.cmsg_type != SCM_RIGHTS)
	{
	    NPS_LOG(NPS_LOG_WARNING, "recvmsg() returned invalid ancillary data
		    level %d or type %d", 
		    cmsg.cm.cmgs_level, cmsg.cm.cmsg_type);
	    return NPS_ERR;
	}
	
	ch-fd = *(int*) CMSG_DATA(&cmsg.cm);
    }    

    if (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) {
	NPS(NPS_LOG_WARNING, "recvmsg() turncate data");
    }

    return n;
}

