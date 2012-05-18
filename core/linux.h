#ifndef _PILL_LINUX_H
#define _PILL_LINUX_H

#include "process.h"
#include "types.h"
#include "network.h"
#include <linux/module.h> /* for MODULE_NAME_LEN */
/***********
 * DEFINES *
 ***********/

/* Processes */


/* Modules */

/* Module name length in the MODULE_DATA structure */
#define MODULE_DATA_NAME_LEN 32
/* Module name length in the struct module definition ( linux/module.h ) */
#ifndef MODULE_NAME_LEN
#define MODULE_NAME_LEN (64 - sizeof(unsigned long))
#endif
/* Length of Kernel instruction dump in bytes */
#define K_DUMP_MAX 150


/* Sockets */

/***********
 * STRUCTS *
 ***********/

typedef struct {
	Bit32u name;
	Bit32u task;
	Bit32u tstruct;
	Bit32u pid;
	Bit32u mm;
} LinuxTaskOffsets;

typedef struct {
	/* Process variables */
	int 				readInit;
	hvm_address			initTaskAddress;
	Bit32u				initTaskPID;
	LinuxTaskOffsets	savedOffsets;
	/* Module variables */
	int					readModules;
	hvm_address			warnOnSlowpath;
	hvm_address			sprintSymbol;
	hvm_address			firstModule;
	/* Sockets variables */
} VMMLinuxStaticMemory;

/*********************
 * FUNC DECLARATIONS *
 *********************/

hvm_status LinuxInitStructures(void);
hvm_status LinuxReadTask(hvm_address addr, PROCESS_DATA* ptask);
hvm_status LinuxAnalyzeInitTask(void);
hvm_status LinuxGetNextProcess(hvm_address cr3, PROCESS_DATA* pprev, PROCESS_DATA* pnext);
hvm_status LinuxGetModulesAddr(hvm_address * addr);
hvm_status LinuxGetNextModule(hvm_address cr3, MODULE_DATA* pprev, MODULE_DATA* pnext);
hvm_status LinuxFindProcess(hvm_address cr3, hvm_address *pts);
hvm_status LinuxFindProcessPid(hvm_address cr3, hvm_address *pid);
hvm_status LinuxFindProcessTid(hvm_address cr3, hvm_address *tid);
hvm_status LinuxBuildSocketList(hvm_address cr3, SOCKET * buf, Bit32u maxsize, Bit32u *psize);
#endif
