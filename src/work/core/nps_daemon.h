#ifndef __NPS_DAEMON_H__
#define __NPS_DAEMON_H__

#define NPS_LOCK_FILE		"npserver.pid"
#define NPS_DEFAULT_FILE_MODE   0600

int nps_daemon();
int create_pidfile();
int destroy_pidfile();

#endif
