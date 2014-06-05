/*
 * purpose: build a network server framework 
 * time:    2014.1
 * author:  richard
 */

#include "../util/util_define.h"
#include "nps_daemon.h"
#include "nps_process.h"

int main(int argc, char* argv[])
{
    if (nps_daemon() != NPS_OK)
	return 1;
    
    if (create_pidfile() != NPS_OK)
	return 1; 	
    
    /*statr watcher and worker*/
    nps_master_process_cycle();   
  
   if ((destroy_pidfile() != NPS_OK)
	return 1;
             
   return 0;
}
