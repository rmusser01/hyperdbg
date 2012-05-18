#include "linux.h"
#include "debug.h"
#include "mmu.h"
#include "common.h"
#include "vmmstring.h"
#include "types.h"
/* Needed for sprint_symbol */
#include <linux/kallsyms.h>
#include <linux/sched.h>
#include <linux/mm_types.h>
#include <linux/fdtable.h>
#include <linux/fs.h>
#include "network.h"
#include <linux/net.h>
#include <net/sock.h>
#include <net/inet_sock.h>

/* Declaring global to have some information tracking */
VMMLinuxStaticMemory linuxMem;

hvm_status LinuxInitStructures () {
  /* Processes */
  linuxMem.readInit = 1;
  linuxMem.initTaskAddress = (hvm_address) (&init_task);
  /* Modules */
  // linuxMem.warnOnSlowpath = (hvm_address) warn_on_slowpath;
  linuxMem.sprintSymbol = (hvm_address) sprint_symbol;
  linuxMem.readModules = 1;
	
  return HVM_STATUS_SUCCESS;
}

/*************/
/* PROCESSES */
/*************/
hvm_status LinuxGetNextProcess(hvm_address cr3, PROCESS_DATA* pprev, PROCESS_DATA* pnext ) {

  hvm_status r;
  hvm_phy_address phy_cr3;
  struct task_struct ts;
  struct mm_struct process_mm;
  
  if(!pnext) return HVM_STATUS_UNSUCCESSFUL;

  if(!pprev) {

    r = MmuReadVirtualRegion(cr3, (hvm_address)&init_task, &ts, sizeof(ts));
    if(r != HVM_STATUS_SUCCESS) {
      return HVM_STATUS_UNSUCCESSFUL;
    }

    pnext->pobj = (hvm_address)(ts.tasks.next) - ((hvm_address)&ts.tasks - (hvm_address)&ts);

  }
  else {
    if(!pprev->pobj) return HVM_STATUS_UNSUCCESSFUL;

    r = MmuReadVirtualRegion(cr3, pprev->pobj, &ts, sizeof(ts));
    if(r != HVM_STATUS_SUCCESS) {
      return HVM_STATUS_UNSUCCESSFUL;
    }

    pnext->pobj = (hvm_address)(ts.tasks.next) - ((hvm_address)&ts.tasks - (hvm_address)&ts);

    if(pnext->pobj == (hvm_address)(&init_task))
      return HVM_STATUS_END_OF_FILE;
  }

  r = MmuReadVirtualRegion(cr3, pnext->pobj, &ts, sizeof(ts));
  if(r != HVM_STATUS_SUCCESS) {
    Log("[Linux] Fail on read task_struct on va 0x%08hx", pnext->pobj);
    return HVM_STATUS_UNSUCCESSFUL;
  }

  /* In linux kernel >= 2.6.X process id is in field 'tgid' and  thread id is in field 'pid' */
  if(ts.tgid <= 0) {
    pnext -> pid = 0;
  }
  else {
    pnext->pid = ts.tgid;
  }

  if(!ts.mm) {
    pnext->cr3 = 0;
  }
  else {
    r = MmuReadVirtualRegion(cr3, (hvm_address)(ts.mm), &process_mm, sizeof(process_mm));
    if(r != HVM_STATUS_SUCCESS) {
      Log("[Linux] Fail on read mm_struct on va 0x%08hx", ts.mm);
      return HVM_STATUS_UNSUCCESSFUL;
    }

    r = MmuGetPhysicalAddress(cr3, (hvm_address)(process_mm.pgd), &phy_cr3);
    if(r != HVM_STATUS_SUCCESS) {
      Log("[Linux] GetPhysicalAddress of va 0x%08hx failed!");
      return HVM_STATUS_UNSUCCESSFUL;
    }
    pnext->cr3 = (hvm_address)phy_cr3;
  }

  if(!ts.comm) {
    vmm_strncpy(pnext->name, "UNKNOWN", 7);
  }
  else {
    vmm_strncpy(pnext->name, ts.comm, TASK_COMM_LEN);
  }

  return HVM_STATUS_SUCCESS;
}

hvm_status LinuxFindProcess(hvm_address cr3, hvm_address *pts)
{
  PROCESS_DATA prev, next;
  hvm_status r;
  hvm_bool found;

  /* Iterate over ("visible") system processes */
  found = FALSE;
  r = LinuxGetNextProcess(context.GuestContext.cr3, NULL, &next);

  while (1) {
    if (r == HVM_STATUS_END_OF_FILE) {
      /* No more processes */
      break;
    } else if (r != HVM_STATUS_SUCCESS) {
      Log("[LinuxFindProcess] Can't read next process. Current process: %.8x", next.pobj);
      break;
    }

    if (next.cr3 == cr3) {
      /* Found! */
      found = TRUE;
      *pts = next.pobj;
      break;
    }

    /* Go on with next process */
    prev = next;
    r = LinuxGetNextProcess(context.GuestContext.cr3, &prev, &next);
  }

  return found ? HVM_STATUS_SUCCESS : HVM_STATUS_UNSUCCESSFUL;
}

hvm_status LinuxFindProcessPid(hvm_address cr3, hvm_address *pid)
{
  hvm_status r;
  hvm_address pts;
  struct task_struct ts;
  r = LinuxFindProcess(cr3, &pts);
  if (r != HVM_STATUS_SUCCESS) return r;

  r = MmuReadVirtualRegion(cr3, pts, &ts, sizeof(ts));
  if(r != HVM_STATUS_SUCCESS) return r;
  
  *pid = ts.tgid;
  
  return HVM_STATUS_SUCCESS;
}

hvm_status LinuxFindProcessTid(hvm_address cr3, hvm_address *tid)
{
  hvm_status r;
  hvm_address pts;
  struct task_struct ts;
  r = LinuxFindProcess(cr3, &pts);
  if (r != HVM_STATUS_SUCCESS) return r;

  r = MmuReadVirtualRegion(cr3, pts, &ts, sizeof(ts));
  if(r != HVM_STATUS_SUCCESS) return r;
  
  *tid = ts.pid; /* May look like it's wrong but it's not. Maybe. */
  
  return HVM_STATUS_SUCCESS;
}


hvm_status LinuxGetNextModule(hvm_address cr3, MODULE_DATA* pprev, MODULE_DATA* pnext) {
  return HVM_STATUS_UNSUCCESSFUL;
}

/***********/
/* SOCKETS */
/***********/
hvm_status LinuxBuildSocketList(hvm_address cr3, SOCKET * buf, Bit32u maxsize, Bit32u *psize) {
  Bit32u max, i;
  hvm_status r;
  hvm_address helper;
	
  struct task_struct task, *nextTask;
  struct files_struct files;
  struct fdtable fdt;
  struct file file;
  struct dentry dentry;
  struct inode inode;
  struct super_block s_b;
  struct file_system_type f_s_t;
  // dentry_type is for debug only
  char inode_type[10], dentry_type[20];
  struct socket skt;
  struct inet_sock inet_sk;

  if ( maxsize <= 0 ) {
    Log( "[Linux] [LinuxBuildSocketList] Maxsize received is less than one!" );
    return HVM_STATUS_UNSUCCESSFUL;
  }
  /* We didn't get any socket yet. */
  *psize = 0;
  /* Setting first process to read */
  nextTask = (struct task_struct*) linuxMem.initTaskAddress;
	
  /* For each process */
  do {
    /* Read new Task */
    r = MmuReadVirtualRegion(context.GuestContext.cr3, (hvm_address) nextTask, &task,sizeof(task));
    if ( r != HVM_STATUS_SUCCESS ) {
      Log( "[LINUX] [LinuxBuildSocketList] Failed virtual memory read at address %08hx", (hvm_address) nextTask);	
      return HVM_STATUS_UNSUCCESSFUL;
    }
		
    /* If proc has files */
    if ( !task.files ) goto cont;
    /* Now we have to get FD, passing by fdt */
    r = MmuReadVirtualRegion(context.GuestContext.cr3, (hvm_address) task.files, &files,sizeof(files));
    if ( r != HVM_STATUS_SUCCESS || !files.fdt ) goto cont;
    r = MmuReadVirtualRegion(context.GuestContext.cr3, (hvm_address) files.fdt, &fdt, sizeof(fdt));
    if ( r != HVM_STATUS_SUCCESS ) goto cont;
			
    max = fdt.max_fds;
    /* Loops all files of this process */
    for ( i = 0; i < max; i++ ) {
      /* Get the file ( two steps since fd is a double pointer )  */
      r = MmuReadVirtualRegion(context.GuestContext.cr3, (hvm_address) fdt.fd + i*sizeof(struct file*), &helper, sizeof(hvm_address));
      r = MmuReadVirtualRegion(context.GuestContext.cr3, helper, &file, sizeof(struct file));
      if ( r != HVM_STATUS_SUCCESS ) continue;
      /* Get the dentry */
      r = MmuReadVirtualRegion(context.GuestContext.cr3, (hvm_address) file.f_path.dentry, &dentry, sizeof(dentry));
      if ( r != HVM_STATUS_SUCCESS || !dentry.d_inode ) continue;
      /* Get the inode */
      r = MmuReadVirtualRegion(context.GuestContext.cr3, (hvm_address) dentry.d_inode, &inode, sizeof(inode));
      if ( r != HVM_STATUS_SUCCESS ) continue;
      /* Check if inode is actually a socket */
      r = MmuReadVirtualRegion(context.GuestContext.cr3, (hvm_address) inode.i_sb, &s_b, sizeof(struct super_block));
      if ( r != HVM_STATUS_SUCCESS ) continue;
      r = MmuReadVirtualRegion(context.GuestContext.cr3, (hvm_address) s_b.s_type, &f_s_t, sizeof(struct file_system_type));
      if ( r != HVM_STATUS_SUCCESS ) continue;
      r = MmuReadVirtualRegion(context.GuestContext.cr3, (hvm_address) f_s_t.name, inode_type, sizeof(inode_type));
      if ( r != HVM_STATUS_SUCCESS || vmm_strncmp( inode_type, "sockfs", 6 )) continue;
      r = MmuReadVirtualRegion(context.GuestContext.cr3, (hvm_address) dentry.d_iname, dentry_type, sizeof(dentry_type));
      if ( r != HVM_STATUS_SUCCESS ) continue;
      /* Inode is actually a socket, we can read its info! */
      /*	Log ( "Got a socket" );
		Log ( "*** NAME: %s", dentry_type );
		Log ( "*** INODE: %08x", dentry.d_inode );
		Log ( "*** FS_NAME: %s", inode_type ); */

      /* Ok, apparently (judging from volatility linux and from
	 svalorzen idea) once we get hold of the inode address of a
	 socket, we just need some black magic (i.e.: subtract a mystic
	 offset) and find the socket struct object. This black magic
	 works because of this:

	 struct socket_alloc {
	 struct socket socket;
	 struct inode vfs_inode;
	 };

	 Our inode should be == to &vfs_inode. So by subtracting the
	 size of a struct socket, we should get &socket and then have
	 fun with it. The code below is how it should be done.
	 -- joystick
      */
      r = MmuReadVirtualRegion(context.GuestContext.cr3, ( (hvm_address) dentry.d_inode ) -sizeof(struct socket), &skt, sizeof(struct socket));
      if ( r != HVM_STATUS_SUCCESS ) continue;
      r = MmuReadVirtualRegion(context.GuestContext.cr3, (hvm_address) skt.sk, &inet_sk, sizeof(struct inet_sock));
      if(r != HVM_STATUS_SUCCESS) continue;
      /*	Log( "INODE: %08x - SOCKET: %08x - INET_SOCK: %08x", dentry.d_inode, ((hvm_address)dentry.d_inode)-sizeof(struct socket), (hvm_address)skt.sk);
		Log("Filtering, TYPE: %d, PROT: %d, PORT: %d", inet_sk.sk.sk_type, inet_sk.sk.sk_protocol, inet_sk.sport ); */
			
      /* Filter out UNIX sockets and strange protocols ( RFCOMM, NETLINK etc. ) */
      if(inet_sk.sk.sk_protocol == 6 || inet_sk.sk.sk_protocol == 17) {
	/* Start getting info! */
	buf[*psize].pid = task.pid;
	buf[*psize].protocol = inet_sk.sk.sk_protocol;
				
	if ( inet_sk.sk.sk_protocol == 6 ) {
	  if ( inet_sk.sk.sk_state == 1 )
	    buf[*psize].state = SocketStateEstablished;
	  else { 
	    if ( inet_sk.sk.sk_state == 10 )
	      buf[*psize].state = SocketStateListen;
	    else
	      buf[*psize].state = SocketStateUnknown;
	  }
	}
	else {
	  Log("[Linux] Unknown socket state");
	  buf[*psize].state = SocketStateUnknown;
	}

	buf[*psize].local_ip = inet_sk.inet_saddr;
	buf[*psize].local_port = vmm_ntohs(inet_sk.inet_sport);
	buf[*psize].remote_ip = inet_sk.inet_daddr;
	buf[*psize].remote_port = vmm_ntohs(inet_sk.inet_dport);
			
	/* Finished reading a socket! */
	*psize += 1;
	/* If we got the maximum allowed number of sockets, end */
	if ( *psize == maxsize ) {
	  goto end;
	}
      }	/* Filtering out strange protocols */
    }	/* For each file */

  cont:
    nextTask = next_task(nextTask);

  } while ( (hvm_address) nextTask != linuxMem.initTaskAddress );

 end:
	
  /*	for ( i = 0; i < *psize; i++ )
	Log( "SOCKET PID: %d - PROT: %d - BHO: %d, IP: %d", buf[i].pid, buf[i].protocol, buf[i].state, buf[i].local_ip ); */

  return HVM_STATUS_SUCCESS;
}
