/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993, 1994, 1995, 1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-1999 Wichert Akkerman <wichert@cistron.nl>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id$
 */


#define D_LARGEFILE_SOURCE 1
#define D_FILE_OFFSET_BITS 64

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include "trace.h"
#include <asm/unistd.h>
#include <sys/user.h>
#include <assert.h>


#include "defs.h"

#include <fcntl.h>
#if HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#ifdef HAVE_LONG_LONG_OFF_T
/*
 * Hacks for systems that have a long long off_t
 */

#define sys_pread64	sys_pread
#define sys_pwrite64	sys_pwrite
#endif


write_thead thead;
write_open topen;
write_close tclose;
write_rws trw;
write_prws tprw; //Zheng
struct timeval intime = {0,0} , outtime;
int incount = 0;


int trace_open(struct tcb *tcp, char type)
{
    struct user_regs_struct regs;
    struct stat fbuf;
    unsigned long mypc, myebp;
    int i,last_slash=0,pl;
    static char path[500 + 1];
    char fdlink[500];
   
    if(tcp->u_rval < 0)
    	return 0;   // no such file
    
    
    umovestr(tcp, tcp->u_arg[0], 500, path);
    
    if(stat(path, &fbuf) != 0)
    {
	//perror("sys_trace: stat error");
	return 0;
    }

    if(S_ISLNK(fbuf.st_mode))
    {	
    pl=readlink(path,fdlink,500);
    fdlink[pl]='\0';
    if(stat(fdlink, &fbuf) != 0)
    {
	//perror("sys_trace: stat error");
	return 0;
    }
    }
    if(S_ISREG(fbuf.st_mode))
    {
	thead.pid = tcp->pid;
	thead.type = type;

	for(i = 0; path[i] != NULL; i++)
	    if(path[i] == '/')
		last_slash = i;
	
	if(last_slash) //skip the slash
	    last_slash++;
	
	topen.fnamesize = i-last_slash;
	topen.fsize = fbuf.st_size;
	topen.filedes = (tcp->u_rval % 1024);
	thead.inode = (int)fbuf.st_ino;	
	
	fwrite(&thead, sizeof(write_thead), 1, my_trace);		    
	fwrite(&topen, sizeof(write_open), 1, my_trace);		    
	fwrite(&path[last_slash], topen.fnamesize, 1, my_trace);
			    
    }
	    
    return 0;
}

int trace_close(struct tcb *tcp, char type)
{
    struct stat fbuf;
    int pl;
    char fdfile[500];
    char fdlink[500];

 
    sprintf(fdfile,"/proc/%d/fd/%d",tcp->pid,tcp->u_arg[0]);
    pl=readlink(fdfile,fdlink,500);
    fdlink[pl]='\0';
    if(stat(fdlink, &fbuf) != 0)
    {
	//perror("sys_trace: stat error");
	return 0;
    }
    
    if(S_ISREG(fbuf.st_mode))
    {
	thead.type = type;
	thead.pid = tcp->pid;
	tclose.filedes = ( tcp->u_arg[0] % 1024 );
	thead.inode = (int) fbuf.st_ino;

	fwrite(&thead, sizeof(write_thead), 1, my_trace);		    
	fwrite(&tclose, sizeof(write_close), 1, my_trace);		    
    }
    return 0;
}


int trace_rws(struct tcb *tcp, char type)
{
    struct user_regs_struct regs;
    struct stat fbuf;
    unsigned long mypc, myebp;
    int i,last_slash=0, pl;
    char fdfile[500];
    char fdlink[500];
    unsigned long long intimev, outtimev;
 

    if(type != TSEEK)
    {
	intimev = intime.tv_sec * 1000000; //convert to usec
	intimev += intime.tv_usec;
	outtimev = outtime.tv_sec * 1000000; //convert to usec
	outtimev += outtime.tv_usec;
	trw.iotime += (intimev - outtimev);
    }

    if(tcp->u_rval < 0 || tcp->u_arg[0] < 0)
    	return 0;   // no such file or desc closed
    
    sprintf(fdfile, "/proc/%d/fd/%d", tcp->pid, tcp->u_arg[0]);
    pl = readlink(fdfile, fdlink, 500);
    fdlink[pl] = '\0';

    if(stat(fdlink, &fbuf) != 0)
    {
	//perror("sys_trace: stat error");
	return 0;
    }

    if(S_ISREG(fbuf.st_mode))
    {
	
	if (ptrace(PTRACE_GETREGS, tcp->pid, NULL, &regs) < 0)
	{
	    perror("sys_trace: ptrace(PTRACE_GETREGS, ...)");
	    return -1;
	}
	
	mypc = ptrace(PTRACE_PEEKDATA, tcp->pid, regs.esp+8, NULL);
	myebp = regs.ebp;		  
	trw.pc = mypc;    
	//while((mypc > 0x42000000 || mypc < 0x8000000) && myebp != 0 && myebp!= 0xffffffff)
	while((mypc > 0x40000000 || mypc < 0x8000000) && myebp != 0 && myebp!= 0xffffffff)
	{
	    mypc = ptrace(PTRACE_PEEKDATA, tcp->pid, myebp+4, NULL);
	    myebp = ptrace(PTRACE_PEEKDATA, tcp->pid, myebp, NULL);
	    trw.pc += mypc;    
	}
	thead.pid = tcp->pid;
	
	trw.pcf = mypc;    
	
	/*while((mypc > 0x40000000 || mypc < 0x8000000) && myebp != 0 && myebp!= 0xffffffff)
	{
	    mypc = ptrace(PTRACE_PEEKDATA, tcp->pid, myebp+4, NULL);
	    myebp = ptrace(PTRACE_PEEKDATA, tcp->pid, myebp, NULL);
	}*/
	
	//trw.pcf = mypc;
	
	trw.pcall = mypc = 0;
	
	while(mypc < 0x40000000 && myebp != 0 && myebp!= 0xffffffff)
	{
    	    trw.pcall += mypc;
	    mypc = ptrace(PTRACE_PEEKDATA, tcp->pid, myebp+4, NULL);
	    myebp = ptrace(PTRACE_PEEKDATA, tcp->pid, myebp, NULL);
	}
	
	trw.filedes = ( tcp->u_arg[0] % 1024 );
	
	if(type == TSEEKL)
	{
	  trw.iosize = tcp->u_arg[2];
	  trw.iosizer = ptrace(PTRACE_PEEKDATA, tcp->pid, tcp->u_arg[3], NULL);
	  thead.type = TSEEK;
	}
	else if(type == TSEEK){
	  trw.iosize = tcp->u_arg[1];
	  trw.iosizer = tcp->u_rval;
	  thead.type = TSEEK;
	}
	else if(type == TPREAD || type == TPWRITE){
	  trw.iosize = tcp->u_arg[2]; //read, write
	  trw.iosizer = tcp->u_rval;
	  tprw.poffset = tcp->u_arg[3];
	  thead.type = type;
	}
	else if(type == TREAD || type == TWRITE){
	  trw.iosize = tcp->u_arg[2]; //read, write
	  trw.iosizer = tcp->u_rval;
	  thead.type = type;
	}
	else{
	  assert(0);
	}

	trw.fsize = fbuf.st_size;
	thead.inode = (int)fbuf.st_ino;

	//fprintf(my_trace , "%u %u %u\n" ,thead.pid , trw.pcf = mypc , thead.inode );
	fwrite(&thead, sizeof(write_thead), 1, my_trace);
	fwrite(&trw, sizeof(write_rws), 1, my_trace);
	if(type == TPREAD || type == TPWRITE)
	  fwrite(&tprw, sizeof(write_prws), 1, my_trace);
	
	if(type != TSEEK)
	    trw.iotime = 0;
    }
    return 0;
}
//Zheng::}



int
sys_read(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		trace_rws(tcp, TREAD);
		gettimeofday(&outtime, NULL);
		tprintf("%ld, ", tcp->u_arg[0]);
	} else {
		gettimeofday(&intime, NULL);
		if (syserror(tcp))
			tprintf("%#lx", tcp->u_arg[1]);
		else
			printstr(tcp, tcp->u_arg[1], tcp->u_rval);
		tprintf(", %lu", tcp->u_arg[2]);
	}
	return 0;
}

int
sys_write(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		trace_rws(tcp, TWRITE);
		gettimeofday(&outtime, NULL);
		tprintf("%ld, ", tcp->u_arg[0]);
		printstr(tcp, tcp->u_arg[1], tcp->u_arg[2]);
		tprintf(", %lu", tcp->u_arg[2]);
	}
	else
		gettimeofday(&intime, NULL);

	return 0;
}

#if HAVE_SYS_UIO_H
void
tprint_iov(tcp, len, addr)
struct tcb * tcp;
unsigned long len;
unsigned long addr;
{
#if defined(LINUX) && SUPPORTED_PERSONALITIES > 1
	union {
		struct { u_int32_t base; u_int32_t len; } iov32;
		struct { u_int64_t base; u_int64_t len; } iov64;
	} iov;
#define sizeof_iov \
  (personality_wordsize[current_personality] == 4 \
   ? sizeof(iov.iov32) : sizeof(iov.iov64))
#define iov_iov_base \
  (personality_wordsize[current_personality] == 4 \
   ? (u_int64_t) iov.iov32.base : iov.iov64.base)
#define iov_iov_len \
  (personality_wordsize[current_personality] == 4 \
   ? (u_int64_t) iov.iov32.len : iov.iov64.len)
#else
	struct iovec iov;
#define sizeof_iov sizeof(iov)
#define iov_iov_base iov.iov_base
#define iov_iov_len iov.iov_len
#endif
	unsigned long size, cur, end, abbrev_end;
	int failed = 0;

	if (!len) {
		tprintf("[]");
		return;
	}
	size = len * sizeof_iov;
	end = addr + size;
	if (!verbose(tcp) || size / sizeof_iov != len || end < addr) {
		tprintf("%#lx", addr);
		return;
	}
	if (abbrev(tcp)) {
		abbrev_end = addr + max_strlen * sizeof_iov;
		if (abbrev_end < addr)
			abbrev_end = end;
	} else {
		abbrev_end = end;
	}
	tprintf("[");
	for (cur = addr; cur < end; cur += sizeof_iov) {
		if (cur > addr)
			tprintf(", ");
		if (cur >= abbrev_end) {
			tprintf("...");
			break;
		}
		if (umoven(tcp, cur, sizeof_iov, (char *) &iov) < 0) {
			tprintf("?");
			failed = 1;
			break;
		}
		tprintf("{");
		printstr(tcp, (long) iov_iov_base, iov_iov_len);
		tprintf(", %lu}", (unsigned long)iov_iov_len);
	}
	tprintf("]");
	if (failed)
		tprintf(" %#lx", addr);
#undef sizeof_iov
#undef iov_iov_base
#undef iov_iov_len
}

int
sys_readv(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		trace_rws(tcp, TREAD);
		gettimeofday(&outtime, NULL);
		tprintf("%ld, ", tcp->u_arg[0]);
	} else {
		gettimeofday(&intime, NULL);
		if (syserror(tcp)) {
			tprintf("%#lx, %lu",
					tcp->u_arg[1], tcp->u_arg[2]);
			return 0;
		}
		tprint_iov(tcp, tcp->u_arg[2], tcp->u_arg[1]);
		tprintf(", %lu", tcp->u_arg[2]);
	}
	return 0;
}

int
sys_writev(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		trace_rws(tcp, TWRITE);
		gettimeofday(&outtime, NULL);
		tprintf("%ld, ", tcp->u_arg[0]);
		tprint_iov(tcp, tcp->u_arg[2], tcp->u_arg[1]);
		tprintf(", %lu", tcp->u_arg[2]);
	}
	else
			gettimeofday(&intime, NULL);

	return 0;
}
#endif

#if defined(SVR4)

int
sys_pread(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		trace_rws(tcp, TPREAD);
		tprintf("%ld, ", tcp->u_arg[0]);
	} else {
		
		if (syserror(tcp))
			tprintf("%#lx", tcp->u_arg[1]);
		else
			printstr(tcp, tcp->u_arg[1], tcp->u_rval);
#if UNIXWARE
		/* off_t is signed int */
		tprintf(", %lu, %ld", tcp->u_arg[2], tcp->u_arg[3]);
#else
		tprintf(", %lu, %llu", tcp->u_arg[2],
			LONG_LONG(tcp->u_arg[3], tcp->u_arg[4]));
#endif
	}
	return 0;
}

int
sys_pwrite(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		trace_rws(tcp, TPWRITE);
		tprintf("%ld, ", tcp->u_arg[0]);
		printstr(tcp, tcp->u_arg[1], tcp->u_arg[2]);
#if UNIXWARE
		/* off_t is signed int */
		tprintf(", %lu, %ld", tcp->u_arg[2], tcp->u_arg[3]);
#else
		tprintf(", %lu, %llu", tcp->u_arg[2],
			LONG_LONG(tcp->u_arg[3], tcp->u_arg[4]));
#endif
	}
	

	return 0;
}
#endif /* SVR4 */

#ifdef FREEBSD
#include <sys/types.h>
#include <sys/socket.h>

int
sys_sendfile(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%ld, %ld, %llu, %lu", tcp->u_arg[0], tcp->u_arg[1],
			LONG_LONG(tcp->u_arg[2], tcp->u_arg[3]),
			tcp->u_arg[4]);
	} else {
		off_t offset;

		if (!tcp->u_arg[5])
			tprintf(", NULL");
		else {
			struct sf_hdtr hdtr;

			if (umove(tcp, tcp->u_arg[5], &hdtr) < 0)
				tprintf(", %#lx", tcp->u_arg[5]);
			else {
				tprintf(", { ");
				tprint_iov(tcp, hdtr.hdr_cnt, hdtr.headers);
				tprintf(", %u, ", hdtr.hdr_cnt);
				tprint_iov(tcp, hdtr.trl_cnt, hdtr.trailers);
				tprintf(", %u }", hdtr.hdr_cnt);
			}
		}
		if (!tcp->u_arg[6])
			tprintf(", NULL");
		else if (umove(tcp, tcp->u_arg[6], &offset) < 0)
			tprintf(", %#lx", tcp->u_arg[6]);
		else
			tprintf(", [%llu]", offset);
		tprintf(", %lu", tcp->u_arg[7]);
	}
	return 0;
}
#endif /* FREEBSD */

#ifdef LINUX

/* The SH4 ABI does allow long longs in odd-numbered registers, but
   does not allow them to be split between registers and memory - and
   there are only four argument registers for normal functions.  As a
   result pread takes an extra padding argument before the offset.  This
   was changed late in the 2.4 series (around 2.4.20).  */
#if defined(SH)
#define PREAD_OFFSET_ARG 4
#else
#define PREAD_OFFSET_ARG 3
#endif

int
sys_pread(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		trace_rws(tcp, TPREAD);
		tprintf("%ld, ", tcp->u_arg[0]);
	} else {
		
		if (syserror(tcp))
			tprintf("%#lx", tcp->u_arg[1]);
		else
			printstr(tcp, tcp->u_arg[1], tcp->u_rval);
		tprintf(", %lu, ", tcp->u_arg[2]);
		printllval(tcp, "%llu", PREAD_OFFSET_ARG);
	}
	return 0;
}

int
sys_pwrite(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		trace_rws(tcp, TPWRITE);
		tprintf("%ld, ", tcp->u_arg[0]);
		printstr(tcp, tcp->u_arg[1], tcp->u_arg[2]);
		tprintf(", %lu, ", tcp->u_arg[2]);
		printllval(tcp, "%llu", PREAD_OFFSET_ARG);
	}
	

	return 0;
}

int
sys_sendfile(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		off_t offset;

		tprintf("%ld, %ld, ", tcp->u_arg[0], tcp->u_arg[1]);
		if (!tcp->u_arg[2])
			tprintf("NULL");
		else if (umove(tcp, tcp->u_arg[2], &offset) < 0)
			tprintf("%#lx", tcp->u_arg[2]);
		else
			tprintf("[%lu]", offset);
		tprintf(", %lu", tcp->u_arg[3]);
	}
	return 0;
}

int
sys_sendfile64(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		loff_t offset;

		tprintf("%ld, %ld, ", tcp->u_arg[0], tcp->u_arg[1]);
		if (!tcp->u_arg[2])
			tprintf("NULL");
		else if (umove(tcp, tcp->u_arg[2], &offset) < 0)
			tprintf("%#lx", tcp->u_arg[2]);
		else
			tprintf("[%llu]", (unsigned long long int) offset);
		tprintf(", %lu", tcp->u_arg[3]);
	}
	return 0;
}

#endif /* LINUX */

#if _LFS64_LARGEFILE || HAVE_LONG_LONG_OFF_T
int
sys_pread64(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%ld, ", tcp->u_arg[0]);
	} else {
		if (syserror(tcp))
			tprintf("%#lx", tcp->u_arg[1]);
		else
			printstr(tcp, tcp->u_arg[1], tcp->u_rval);
		tprintf(", %lu, ", tcp->u_arg[2]);
		printllval(tcp, "%#llx", 3);
	}
	return 0;
}

int
sys_pwrite64(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%ld, ", tcp->u_arg[0]);
		printstr(tcp, tcp->u_arg[1], tcp->u_arg[2]);
		tprintf(", %lu, ", tcp->u_arg[2]);
		printllval(tcp, "%#llx", 3);
	}
	return 0;
}
#endif

int
sys_ioctl(tcp)
struct tcb *tcp;
{
	const struct ioctlent *iop;

	if (entering(tcp)) {
		tprintf("%ld, ", tcp->u_arg[0]);
		iop = ioctl_lookup(tcp->u_arg[1]);
		if (iop) {
			tprintf("%s", iop->symbol);
			while ((iop = ioctl_next_match(iop)))
				tprintf(" or %s", iop->symbol);
		} else
			tprintf("%#lx", tcp->u_arg[1]);
		ioctl_decode(tcp, tcp->u_arg[1], tcp->u_arg[2]);
	}
	else {
		int ret;
		if (!(ret = ioctl_decode(tcp, tcp->u_arg[1], tcp->u_arg[2])))
			tprintf(", %#lx", tcp->u_arg[2]);
		else
			return ret - 1;
	}
	return 0;
}
