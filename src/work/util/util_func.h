#ifndef __NPS_UTIL_FUNC_H__
#define __NPS_UTIL_FUNC_H__

int try_lock(int fd);
int lock_file(int fd, int type);
int unlock_file(int fd);

#endif
