#include "nps_process.h"
#include "nps_channel.h"
#include "nps_socket.h"
#include "../util/util_define.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <stdlib.h>

typedef struct {
    int	    signo;
    char*   signame;
    char*   name;
    void    (*handler)(int signo);
} nps_signal_t;

int		nps_process;
int		nps_process_slot;
int		nps_channel;
int		nps_last_process;
nps_process_t	nps_processes[NPS_MAX_PROCESSES];
pid_t		nps_pid;

/*
 * sig_atomic_t指对变量的赋值或是使用是原子操作，一般用于信号操作，其实就是int型的
 */
sig_atomic_t	nps_reap;  
sig_atomic_t	nps_sigio; 
sig_atomic_t	nps_sigalrm;
sig_atomic_t	nps_terminate;
sig_atomic_t	nps_quit;  
sig_atomic_t	nps_noaccept;
unsigned int	nps_noaccepting;

void nps_signal_handler(int signo);

nps_signal_t signals[] = {
    {   nps_signal_value(NPS_NOACCEPT_SIGNAL),
	"SIG" nps_value(NPS_NPACCEPT_SIGNAL),
	"",
	nps_signal_handler },

    {   nps_signal_value(NPS_TERMINAL_SIGNAL),
	"SIG" nps_value(NPS_TERMINAL_SIGNAL),
	"stop",
	nps_signal_handler },

    {   nps_signal_value(NPS_SHUTDOWN_SIGNAL),
	"SIG" nps_value(NPS_SHUTDOWN_SIGNAL),
	"quit",
	nps_signal_handler},

    {   SIGALRM,
	"SIGALRM",
	"",
	nps_signal_handler},

    {   SIGINT,
	"SIGINT",
	"",
	nps_signal_handler},

    {   SIGIO,
	"SIGIO",
	"",
	nps_signal_handler},

    {   SIGCHLD,
	"SIGCHLD",
	"",
	nps_signal_handler},

    {   SIGSYS,
	"SIGSYS",
	"",
	SIG_IGN },

    {   SIGPIPE,
	"SIGPIPE, SIG_IGN",
	"",
	SIG_IGN},

    {   0,
	NULL,
	"",
	NULL}
};

static void start_worker_processes(int type, int n);
static void nps_worker_process_cycle(void* data);
static void nps_signal_worker_processes(int signo);
static void nps_master_process_exit();
static int  nps_reap_children();
static void nps_pass_open_channel(nps_channel_t* ch);
static void nps_worker_process_exit();
static void nps_worker_process_init(int worker);
static void nps_channel_handler();

int nps_init_signals()
{
    nps_signal_t*	sig;
    struct sigaction	sa;
    
    for (sig = signals; sig->signo != 0; sig++) {

	sa.sa_handler = sig->handler;
	sigemptyset(&sa.sa_mask);
	if (sigaction(sig->signo, &sa, NULL) == -1) {
	    NPS_LOG(NPS_LOG_ERR, "sigaction(%s) failed", sig->signame);
	    return NPS_ERROR;
	}
    }
    
    return NPS_OK;
}

void nps_signal_handler(int signo)
{
    for (sig = signals; sig->signo != 0; sig++) {
	if (sig->signo == signo)
	    break;
    }
    
    switch (nps_process) {

	case NPS_PROCESS_MASTER:
	    switch (signal) {
	
	    	case nps_signal_value(NPS_SHUTDOWN_SIGNAL):
		    nps_quit = 1;
		    action = ", shutting down";
		    break;
	    
		case nps_signal_value(NPS_TERMINATE_SIGNAL):
		case SIGINT:
		    nps_terminate = 1;
		    action = ", exiting";
		    break;
	
		case nps_signal_value(NPS_NOACCEPT_SIGNAL):
		    nps_noaccept = 1;
		    action = ", stop accepting connections";
		    break;

		case SIGALRM:
		    nps_sigalrm = 1;
		    break;
		
		case SIGIO:
		    nps_sigio = 1;
		    break;

		case SIGCHLD:
		    nps_reap = 1;
		    break;
	    }
    
	    break;

	    case NGX_PROCESS_WORKER:
		switch (signo) {
		    
		    case nps_signal_value(NPS_NPACCEPT_SIGNAL):
		    case nps_signal_value(NPS_SHUTDOWN_SIGNAL):
			nps_quit = 1;
			action = ", shutting down";
			break;
		
		    case nps_signal_value(NPS_TERMINATE_SIGNAL):
		    case SIGINT:
			nps_terminate = 1;
			action = ", exiting";
			break;
		    
		    case SIGIO:
			action = ", ignoring";
			break;
		}
		
		break;
    }

    NPS_LOG(NPS_LOG_INFO, "signal %d %s received%s", signo, sig-signame, action);
    
    if (signo == SIGCHLD) {
	// TODO: handle zombie process
    }
}

void nps_master_process_cycle()
{
    sigset_t	    set; 
    unsigned int    sigio;
    unsigned int    delay;   
    unsigned int    live;

    /* block signal */
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGALRM);
    sigaddset(&set, SIGIO);
    sigaddset(&set, SIGINT);
    sigaddset(&set, ngx_signal_value(NGX_RECONFIGURE_SIGNAL));
    sigaddset(&set, ngx_signal_value(NGX_REOPEN_SIGNAL));
    sigaddset(&set, ngx_signal_value(NGX_NOACCEPT_SIGNAL));
    sigaddset(&set, ngx_signal_value(NGX_TERMINATE_SIGNAL));
    sigaddset(&set, ngx_signal_value(NGX_SHUTDOWN_SIGNAL));
    sigaddset(&set, ngx_signal_value(NGX_CHANGEBIN_SIGNAL));

    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
        NPS_LOG(NGX_LOG_WARNING, "sigprocmask() failed");
    }

    sigemptyset(&set);

    nps_process = NPS_PROCESS_MASTER;    

    start_worker_processes(NPS_PROCESS_RESPAWN, NPS_MAX_PROCESSES / 2);
    
    sigio = 0;
    delay = 0;
    live = 1;
    
    // TODO: hanler worker process
    while(1) {
	// enable signal
	sigsuspend(&set);
	
	if (nps_reap) {
	    nps_reap = 0;
	    NPS_LOG(NPS_LOG_INFO, "reap children");
	    
	    live = nps_reap_children();
	}
    
	if (!live && (nps_terminate || nps_quit)) 
	    nps_master_process_exit();
	
	if (nps_terminate) {
	    if (delay == 0) 
		delay == 50;

	    if (sigio) {
		sigio--;
		continue;
	    }

	    sigio = NPS_MAX_PROCESSES /2 + 2;
	    
	    if (delay > 1000)
		nps_signal_worker_processes(SIGKILL);
	    else
		nps_signal_worker_processes(nps_signal_value(NPS_TERMINATE_SIGNAL));
	    
	    continue; 
	}

	if (nps_quit) {
	    nps_signal_worker_processes(nps_signal_value(NPS_SHUTDOWN_SIGNAL));
	    
	    // TODO:close socket;
	}
	
	if (nps_noaccept) {
	    nps_noaccept = 0;
	    nps_noaccepting = 1;
	    nps_signal_worker_processes(nps_signal_value(NPS_SHUTDOWN_SIGNAL));
	}	
    }

    waitpid(-1, NULL, 0);
    NPS_LOG(NPS_LOG_INFO, "master stop");
    // watch workers
}

pid_t nps_spawn_process(int respawn, char* name, nps_spawn_proc_pt proc, void* data)
{
    int	    s;
    pid_t   pid;

    if (respawn >= 0) {
	s = respawn;
    }
    else {
	for (s = 0; s < nps_last_process; ++s) {
	    if (nps_processes[s].pid == -1)
		break;
	}
	
	if (s == NPS_MAX_PROCESSES) {
	    NPS_LOG(NPS_LOG_WARNING, "no more than %d processes can be spawn",
		    s);
	    return NPS_ERR;
	}
    }

    if (respawn != NPS_PROCESS_DETACHED) {

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, nps_processes[s].channel) == -1) {
	    NPS_LOG(NPS_LOG_ERR, "sockerpair failed");
	    return NPS_ERR;
	}
    
	if (nps_nonblocking(nps_processes[s].channel[0]) == -1 
	    || nps_nonblocking(nps_processes[s].channel[1]) == -1) {
	    NPS_LOG(NPS_LOG_ERR, "set nonblocking failed");
	    return NPS_ERR;
	}
	
	// TODO: 设置异步io
	
	if (fcntl(nps_processes[s].channel[0], F_SETOWN, nps_pid) == -1) {
	    NPS_LOG(NPS_LOG_ERR, "fcntl(F_SETOWN) failed");
	    nps_close_channel(nps_processes[s].channel);
	    return NPS_ERR;
	}
	
	if (fcntl(nps_processes[s].channel[0], F_SETFD, FD_CLOEXEC) == -1
	    || fcntl(nps_processes[s].channel[1], F_SETFD, FD_CLOEXEC) == -1) {
	    NPS_LOG(NPS_LOG_ERR, "fcntl(F_SETFD) failed");
	    nps_close_channel(nps_processes[s].channel);
	    return NPS_ERR;
	}

	nps_channel = nps_processes[s].channel[1];
    }
    else {
	nps_processes[s].channel[0] = -1;
	nps_processes[s].channel[1] = -1;
    }

    nps_process_slot = s;

    pid = fork();
    switch(pid) {
	case -1:
	    NPS_LOG(NPS_LOG_ERR, "fork failed");
	    return NPS_ERR;
	case 0:
	    proc(data);
	    break;
	default:
	    break;
    }

    NPS_LOG(NPS_LOG_INFO, "start %s:%d", name, pid);    

    if (respawn > 0) {
	return pid;
    }
    
    nps_processes[s].proc = proc;
    nps_processes[s].data = data;
    nps_processes[s].name = name;
    nps_processes[s].exiting = 0;
    
    switch(respawn) {
	case NPS_PROCESS_NORESPAWN:
	    nps_processes[s].respawn = 0;
	    nps_processes[s].just_spawn = 0;
	    nps_processes[s].detached = 0;
	    break;
	
	case NPS_PROCESS_JUST_SPAWN:
	    nps_processes[s].respawn = 0;
	    nps_processes[s].just_spawn = 1;
	    nps_processes[s].detached = 0;
	    break;

   	case NPS_PROCESS_RESPAWN:
	    nps_processes[s].respawn = 1;
	    nps_processes[s].just_spawn = 0;
	    nps_processes[s].detached = 0;
	    break;

	case NPS_PROCESS_JUST_RESPAWN:
	    nps_processes[s].respawn = 1;
	    nps_processes[s].just_spawn = 1;
	    nps_processes[s].detached = 0;
	    break;

	case NPS_PROCESS_DETACHED:
	    nps_processes[s].respawn = 0;
	    nps_processes[s].just_spawn = 0;
	    nps_processes[s].detached = 1;
	    break;
	
	default:
	    break;
    }
    
    if (s == nps_last_process)
	nps_last_process++;
    
    return pid;
}

static void start_worker_processes(int type, int n)
{
    int		    i;
    nps_channel_t   ch;
    
    NPS_LOG(NPS_LOG_INFO, "start worker processes");
    
    ch.command = NPS_CMD_OPEN_CHANNEL;
    
    for (i = 0; i < n; ++i) {
	nps_spawn_process(type, "worker process", nps_worker_process_cycle, (void*) (intptr_t) i);
	
	ch.pid = nps_processes[nps_process_slot].pid;
	ch.slot = nps_process_slot;
	ch.fd = nps_processes[nps_process_slot].channel[0];

	nps_pass_open_channel(&ch);
    }
}

static void nps_worker_process_cycle(void* data)
{
    int worker = (intptr_t) data;
    
    nps_process = NPS_PROCESS_WORKER; 
    
    NPS_LOG(NPS_LOG_INFO, "process %d start successfully", worker);
    
    while (1) {
	if (nps_exiting) {
	    //TODO: close connections;
	    nps_worker_process_exit();
	}
	
	if (nps_terminate) {
	    NPS_LOG(NPS_LOG_INFO, "worker %d exiting", getpid());
	    nps_worker_process_exit();
	}

	if (nps_quit) {
	    nps_quit = 0;
	    NPS_LOG(NPS_LOG_INFO, "worker %d gracefully shutting down", getpid());
	    
	    if (!nps_exiting) {
		//TODO:nps_close_listening_sockets();
		nps_exiting = 1;
	    }	
	}
    }
}

static void nps_signal_worker_processes(int signo)
{
    int		    i;
    nps_channel_t   ch;
    
    switch (signo) {
	
	case nps_signal_value(NPS_SHUTDOWN_SGINAL):
	    ch.command = NPS_CMD_QUIT;
	    break;

	case nps_signal_value(NPS_TERMINATE_SIGNAL):
	    ch.command = NPS_CMD_TERMINATE;
	    break;

	default:
	    ch.command = 0;
    }

    ch.fd = -1;

    for (i = 0; i < nps_last_process; ++i) {
	
	if (nps_processes[i].detached || nps_processes[i].pid == -1) 
	    continue;

	if (nps_processes[i].just_spawn) {
	    nps_processes[i].just_spawn = 0;
	    continue;
	}

	if (nps_processes[i].exiting 
	    && signo = nps_signal_value(NPS_SHUTDOWN_SIGNAL)) 
	    continue;

	if (ch.command) {
	    if (nps_write_channel(nps_process[i].channel[0], &ch, 
		sizeof(nps_channel_t)) == NPS_OK) {
		
		nps_processes[i].exiting = 1;
		continue;
	    }
	}

	if (kill(nps_processes[i].pid, signo) == -1) {
	    err = errno;
	    NPS_LOG(NPS_LOG_ERR, "kill(%P, %d) failed", nps_processes[i].pid, signo);
	    
	    // 没有这个进程号就标记已经退出
	    if (err == ESRCH) {
		nps_processes[i].exited = 1;
		nps_processes[i].exiting = 0;
		nps_reap = 1;
	    }

	    continue;
	}	
    }
    
}

static void nps_master_process_exit()
{
    unsigned int i;
    
    // TODO:get fd
    if (nps_destroy_pidfile() != NPS_OK)
	NPS_LOG(NPS_LOG_WARNING, "detroy pid file failed");
    
    NPS_LOG(NPS_LOG_INFO, "exit");
    
    // TODO:close listening socket

    // TODO:destroy mempool
    exit(0);
}

static unsigned int nps_reap_children()
{
    nps_channel_t   ch;
    unsigned int    live;    
    int		    i;

    ch.command = NPS_CMD_CLOSE_CHANNEL;
    ch.fd = -1;
    
    live = 0;
    for (i = 0; i < nps_last_process; ++i) {
	
	if (nps_processes[i].pid == -1)
	    continue;
	
	if (nps_processes[i].exited) {
	    
	    if (!nps_processes[i].detached) {
		nps_close_channel(nps_processes[i].channel);
		
		nps_processes[i].channel[0] = -1;
		nps_processes[i].channel[1] = -1;
		
		ch.pid = nps_processes[i].pid;
		ch.slot = i;
		
		//TO READ: 为什么要给所有子进程发送有进程退出的消息 
		for (n = 0; n < nps_last_process; ++n) {
		    if (nps_processes[n].exited
			|| nps_processes[n].pid == -1
			|| nps_processes[n].channel[0] == -1) 
		    {
			continue;
		    }
		 
		    nps_write_channel(nps_processes[n].channel[0], &ch, sizeof(nps_channel_t));   
		}
	    }
	    
	    if (nps_processes[i].respawn
		&& !nps_processes[i].exiting
		&& !nps_terminate
		&& !nps_quit) 
	    {
		if (nps_spawn_process(nps_processes[i].proc,
				      nps_processes[i].data,
				      nps_processes[i].name,
				      i) 
		    == NPS_ERR) 
		{
		    NPS_LOG(NPS_LOG_ERR, "could not respawn");
		    continue;
		}
	    
		// nps_process_slot 在nps_spawn_process 中已经更新
		ch.command = NPS_CMD_OPEN_CHANNEL;
		ch.pid = nps_processes[nps_process_slot].pid;
		ch.slot = nps_process_slot;
		ch.fd = nps_processes[nps_process_slot].channel[0];
		
		// 广播通知，nps_processes更新
		nps_pass_open_channel(&ch);
		live = 1;
	    }
	}
	else if (nps_processes[i].exiting || !nps_processes[i].detached) 
	    live = 1;
    }
	
    return live;
}

static void nps_pass_open_channel(nps_channel_t* ch)
{
    int i;
    
    for (i = 0; i < nps_last_process; ++i) {
	
	if (i == nps_process_slot
	    || nps_processes[i].pid == -1
	    || nps_processes[i].channel[0] == -1) 
	{
	    continue;
	}
	
	nps_write_channel(nps_processes[i].channel[0], ch, sizeof(ngx_channel_t));
    }
}

static void nps_work_process_init(int worker)
{
    sigset_t	set;
    int		n;

    sigemptyset(&set);

    if (sigprocmask(SIG_SETMASK, &set, NULL) == -1) {
	NPS_LOG(NPS_LOG_WARNING, "sigprocmask() failed");
    }
    
    for (n = 0; n < nps_last_process; ++n) {
	
	if (nps_processes[n].pid == -1)
	    continue;

	if (n == nps_process_slot) 
	    continue;
	
	if (nps_processes[n].channel[1] == -1)
	    continue;
    
	if (close(nps_processes[n].channel[1]) == -1)
	    NPS_LOG(NPS_LOG_WARNING, "close() channel failed");   
    }

    // WORK_HERE;     
    if (close(nps_processes[nps_process_slot].channel[0]) == -1) 
	NPS_LOG(NPS_LOG_WARNING, "close() channel failed");

    // TODO:add channel event;
 
}

static void nps_worker_process_exit()
{
    if (nps_exiting) {
    //TODO:close socket; 
    }

    NPS_LOG(NPS_LOG_INFO, "in worker:%d exit", getpid()); 

   // TODO:nps_destroy_pool();
    exit(0);
}

static void nps_channel_handler()
{
    NPS_LOG(NPS_LOG_INFO, "in channel handler");
}
