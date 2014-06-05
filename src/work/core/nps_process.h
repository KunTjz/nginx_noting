#ifndef __NPS_PROCESS_H__
#define __NPS_PROCESS_H__

#include <unistd.h>

typedef void (*nps_spawn_proc_pt) (void* data);

typedef struct{
    pid_t		pid;
    int			status;
    int			channel[2];
    
    nps_spawn_proc_pt	proc;
    void*		data;
    char*		name;
    
    unsigned		respawn:1;
    unsigned		just_spawn:1;
    unsigned		detached:1;
    unsigned		exiting:1;
    unsigned		exited:1;
}nps_process_t;

#define NPS_MAX_PROCESSES  4

#define NPS_PROCESS_NORESPAWN     -1
#define NPS_PROCESS_JUST_SPAWN    -2
#define NPS_PROCESS_RESPAWN       -3
#define NPS_PROCESS_JUST_RESPAWN  -4
#define NPS_PROCESS_DETACHED      -5

#define NPS_CMD_OPEN_CHANNEL   1
#define NPS_CMD_CLOSE_CHANNEL  2
#define NPS_CMD_QUIT           3
#define NPS_CMD_TERMINATE      4
#define NPS_CMD_REOPEN         5

#define NPS_SHUTDOWN_SIGNAL	QUIT
#define	NPS_TERMINATE_SIGNAL	TERM
#define	NPS_NOACCEPT_SIGNAL	WINCH
#define	NPS_RECONFIGURE_SIGNAL	HUP

#define nps_signal_helper(n)	SIG##n
#define nps_signal_value(n)	nps_signal_helper(n)

#define NPS_PROCESS_MASTER  0
#define NPS_PROCESS_WORKER  1

void nps_master_process_cycle();

#endif
