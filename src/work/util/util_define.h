#ifndef __NPS_UTIL_DEFINE_H__
#define __NPS_UTIL_DEFINE_H__

#include <syslog.h>

#define NPS_OK	0
#define NPS_ERR	-1

#define NPS_LOG_ERR	LOG_ERR
#define NPS_LOG_INFO	LOG_INFO
#define NPS_LOG_WARNING	LOG_WARNING	

#define NPS_LOG(type, msg, args...)  do {   \
	syslog(type, msg, ##args);	\
    } while(0); 

#define NPS_ERR_QUIT(type, msg, args...) do {    \
	NPS_LOG(type, msg, ##args); \
	exit(1);    \
    } while(0); 

#endif
