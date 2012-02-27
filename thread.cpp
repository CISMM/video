/****************************************************************************\

  Copyright 1999 The University of North Carolina at Chapel Hill.
  All Rights Reserved.

  Permission to use, copy, modify and distribute this software and its
  documentation for educational, research and non-profit purposes,
  without fee, and without a written agreement is hereby granted,
  provided that the above copyright notice and the following three
  paragraphs appear in all copies.

  IN NO EVENT SHALL THE UNIVERSITY OF NORTH CAROLINA AT CHAPEL HILL BE
  LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
  CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS, ARISING OUT OF THE
  USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY
  OF NORTH CAROLINA HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH
  DAMAGES.

  THE UNIVERSITY OF NORTH CAROLINA SPECIFICALLY DISCLAIM ANY
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE
  PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE UNIVERSITY OF
  NORTH CAROLINA HAS NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT,
  UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

  The author may be contacted via:

  US Mail:             Hans Weber
                       Department of Computer Science
                       Sitterson Hall, CB #3175
                       University of N. Carolina
                       Chapel Hill, NC 27599-3175

  Phone:               (919) 962 - 1928

  EMail:               weberh@cs.unc.edu

\****************************************************************************/

/*****************************************************************************\
  thread.cpp
  --
  Description :

  ----------------------------------------------------------------------------
  Author: weberh
  Created: Mon Mar 23 16:51:16 1998
  Revised: Thu May 14 10:00:28 1998 by weberh
  $Source: /afs/unc/proj/stm/src/CVS_repository/nano/src/lib/nmMP/thread.C,v $
  $Locker:  $
  $Revision: 1.7 $
\*****************************************************************************/

#include "thread.h"
#include <stdio.h>
#include <string.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

// The following are copied from myUtil.h to remove the dependencies from this file.
#include <iostream>
using namespace std;
#define ALL_ASSERT(exp, msg) if(!(exp)){ cerr << "\n" << "Assertion failed! " << \
endl << msg << " (" << __FILE__ << ", " << __LINE__ << ")" << "\n"; exit(-1);}

// init all fields in init()
Semaphore::Semaphore( int cNumResources ) : 
  cResources(cNumResources)
{
  init();
}

// create a new internal structure for the semaphore
// (binary copy is not ok)
// This does not copy the state of the semaphore
Semaphore::Semaphore( const Semaphore& s ) : 
  cResources(s.cResources)
{
  init();
}

void Semaphore::init() {
#ifdef sgi
  if (Semaphore::ppaArena==NULL) {
    Semaphore::allocArena();
  }
  if (cResources==1) {
    fUsingLock=true;
    ps=NULL;
    // use lock instead of semaphore
    if ((l = usnewlock(Semaphore::ppaArena)) == NULL) {
      cerr << "Semaphore::Semaphore: error allocating lock from arena." << "\n";
      return;
    }
  } else {    
    fUsingLock=false;
    l=NULL;
    if ((ps = usnewsema(Semaphore::ppaArena, cResources)) == NULL) {
      cerr << "Semaphore::Semaphore: error allocating semaphore from arena." << "\n";
      return;
    }
  }
#elif defined(_WIN32)
  // args are security, initial count, max count, and name
  // TCH 20 Feb 2001 - Make the PC behavior closer to the SGI behavior.
  int numMax = cResources;
  if (numMax < 1) {
    numMax = 1;
  }
  hSemaphore = CreateSemaphore(NULL, cResources, numMax, NULL);
  if (!hSemaphore) {
    // get error info from windows (from FormatMessage help page)
    LPVOID lpMsgBuf;
    
    FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		   FORMAT_MESSAGE_FROM_SYSTEM,
		   NULL,    GetLastError(),
		   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
		     // Default language
		   (LPTSTR) &lpMsgBuf,    0,    NULL );
    cerr << "Semaphore::Semaphore: error creating semaphore, "
      "WIN32 CreateSemaphore call caused the following error: "
	 << lpMsgBuf << "\n";
    // Free the buffer.
    LocalFree( lpMsgBuf );
    return;
  }
#elif __APPLE__
  // We need to use sem_open on the mac because sem_init is not implemented
    int numMax = cResources;
    if (numMax < 1) {
      numMax = 1;
    }
    char *tempname = new char[100];
    sprintf(tempname, "/tmp/vrpn_sem.XXXXXXX");
    semaphore = sem_open(mktemp(tempname), O_CREAT, 0600, numMax);
    if (semaphore == SEM_FAILED) {
        perror("Semaphore::Semaphore: error opening semaphore");
        delete [] tempname;
        return;
    }
    delete [] tempname;

#else
  // Posix threads are the default.
  int numMax = cResources;
  if (numMax < 1) {
    numMax = 1;
  }
  semaphore = new sem_t;
  if (sem_init(semaphore, 0, numMax) != 0) {
      cerr << "Semaphore::Semaphore: error initializing semaphore." << "\n";
      return;
  }
#endif
}

Semaphore::~Semaphore() {
#ifdef sgi
  if (fUsingLock) {
    usfreelock( l, Semaphore::ppaArena );
  } else {
    usfreesema( ps, Semaphore::ppaArena );
  }
#elif defined(_WIN32)
  if (!CloseHandle(hSemaphore)) {
    // get error info from windows (from FormatMessage help page)
    LPVOID lpMsgBuf;
    
    FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		   FORMAT_MESSAGE_FROM_SYSTEM,
		   NULL,    GetLastError(),
		   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
		     // Default language
		   (LPTSTR) &lpMsgBuf,    0,    NULL );
    cerr << "Semaphore::~Semaphore: error destroying semaphore, "
      "WIN32 CloseHandle call caused the following error: "
	 << lpMsgBuf << "\n";
    // Free the buffer.
    LocalFree( lpMsgBuf );
  }
#elif __APPLE__
  if (sem_close(semaphore) != 0) {
      perror("Semaphore::~Semaphore: error destroying semaphore.");
      return;
  }
#else
  // Posix threads are the default.
  if (sem_destroy(semaphore) != 0) {
      cerr << "Semaphore::~Semaphore: error destroying semaphore." << "\n";
  }
  delete semaphore;
#endif
}

// routine to reset it
#ifdef sgi

int Semaphore::reset( int cNumResources ) {
  cResources = cNumResources;
  if (fUsingLock) {
    if (cResources==1) {
      if (usinitlock( l )) {
	perror("Semaphore::reset: usinitlock:");
	return -1;
      }
    } else {
      // need to make new semaphore
      usfreelock( l, Semaphore::ppaArena );
      init();
    }
  } else {
    if (cResources!=1) {
      if (usinitsema( ps, cNumResources )) {
	perror("Semaphore::reset: usinitsema:");
	return -1;
      }
    } else {
      // need to make new lock
      usfreesema( ps, Semaphore::ppaArena );
      init();
    }
  }

  return 0;
}

#elif defined(_WIN32)

int Semaphore::reset( int cNumResources ) {
  cResources = cNumResources;
  // close the old one and create a new one
  if (!CloseHandle(hSemaphore)) {
    // get error info from windows (from FormatMessage help page)
    LPVOID lpMsgBuf;
    
    FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		   FORMAT_MESSAGE_FROM_SYSTEM,
		   NULL,    GetLastError(),
		   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
		     // Default language
		   (LPTSTR) &lpMsgBuf,    0,    NULL );
    cerr << "Semaphore::reset: error destroying semaphore, "
      "WIN32 CloseHandle call caused the following error: "
	 << lpMsgBuf << "\n";
    // Free the buffer.
    LocalFree( lpMsgBuf );
  }
  init();

  return 0;
}

#else  // not sgi, not _WIN32

int Semaphore::reset( int cNumResources) {
  // Posix by default.

  cResources = cNumResources;
  // Destroy the old semaphore and then create a new one with the correct
  // value.
  if (sem_close(semaphore) != 0) {
      perror("Semaphore::reset: error destroying semaphore.");
      return -1;
  }
  init();
  return 0;
}

#endif

// routines to use it (p blocks, cond p does not)
// 1 on success, -1 fail
int Semaphore::p() {
#ifdef sgi
  if (fUsingLock) {
    if (ussetlock(l)!=1) {
      perror("Semaphore::p: ussetlock:");
      return -1;
    }
  } else {
    if (uspsema(ps)!=1) {
      perror("Semaphore::p: uspsema:");
      return -1;
    }
  }
#elif defined(_WIN32)
  switch (WaitForSingleObject(hSemaphore, INFINITE)) {
  case WAIT_OBJECT_0:
    // got the resource
    break;
  case WAIT_TIMEOUT:
    ALL_ASSERT(0,"Semaphore::p: infinite wait time timed out!");
    return -1;
    break;
  case WAIT_ABANDONED:
    ALL_ASSERT(0,"Semaphore::p: thread holding resource died");
    return -1;
    break;
  case WAIT_FAILED:
    // get error info from windows (from FormatMessage help page)
    LPVOID lpMsgBuf;
    
    FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		   FORMAT_MESSAGE_FROM_SYSTEM,
		   NULL,    GetLastError(),
		   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
		   // Default language
		   (LPTSTR) &lpMsgBuf,    0,    NULL );
    cerr << "Semaphore::p: error waiting for resource, "
      "WIN32 WaitForSingleObject call caused the following error: "
	 << lpMsgBuf << "\n";
    // Free the buffer.
    LocalFree( lpMsgBuf );
    return -1;
    break;
  default:
    ALL_ASSERT(0,"Semaphore::p: unknown return code");
    return -1;
  }
#else
  // Posix by default
  // Handle the case where there is an interrupt in the middle of our
  // wait by retrying.
  int ret;
  do {
	ret = sem_wait(semaphore);
  } while ( (ret != 0) && (ret != EINTR) );
  if (ret != 0) {
    perror("Semaphore::p: ");
    return -1;
  }
#endif
  return 1;
}

// 0 on success, -1 fail
int Semaphore::v() {
#ifdef sgi
  if (fUsingLock) {
    if (usunsetlock(l)) {
      perror("Semaphore::v: usunsetlock:");
      return -1;
    }
  } else {
    if (usvsema(ps)) {
      perror("Semaphore::v: uspsema:");
      return -1;
    }
  }
#elif defined(_WIN32)
  if (!ReleaseSemaphore(hSemaphore,1,NULL)) {
    // get error info from windows (from FormatMessage help page)
    LPVOID lpMsgBuf;
    
    FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		   FORMAT_MESSAGE_FROM_SYSTEM,
		   NULL,    GetLastError(),
		   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
		     // Default language
		   (LPTSTR) &lpMsgBuf,    0,    NULL );
    cerr << "Semaphore::v: error v'ing semaphore, "
      "WIN32 ReleaseSemaphore call caused the following error: "
	 << (LPTSTR) lpMsgBuf << "\n";
    // Free the buffer.
    LocalFree( lpMsgBuf );
    return -1;
  }
#else
  // Posix by default
  if (sem_post(semaphore) != 0) {
    perror("Semaphore::p: ");
    return -1;
  }
#endif
  return 0;
}

// 0 if it can't get the resource, 1 if it can
// -1 if fail
int Semaphore::condP() {
  int iRetVal=1;
#ifdef sgi
  if (fUsingLock) {
    // don't spin at all
    iRetVal = uscsetlock(l, 0);
    if (iRetVal<=0) {
      perror("Semaphore::condP: uscsetlock:");
      return -1;
    }
  } else {
    iRetVal = uscpsema(ps);
    if (iRetVal<=0) {
      perror("Semaphore::condP: uscpsema:");
      return -1;
    }
  }
#elif defined(_WIN32)
  switch (WaitForSingleObject(hSemaphore, 0)) {
  case WAIT_OBJECT_0:
    // got the resource
    break;
  case WAIT_TIMEOUT:
    // resource not free
    iRetVal=0;
    break;
  case WAIT_ABANDONED:
    ALL_ASSERT(0,"Semaphore::condP: thread holding resource died");
    return -1;
    break;
  case WAIT_FAILED:
    // get error info from windows (from FormatMessage help page)
    LPVOID lpMsgBuf;
    
    FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		   FORMAT_MESSAGE_FROM_SYSTEM,
		   NULL,    GetLastError(),
		   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
		   // Default language
		   (LPTSTR) &lpMsgBuf,    0,    NULL );
    cerr << "Semaphore::condP: error waiting for resource, "
      "WIN32 WaitForSingleObject call caused the following error: "
	 << lpMsgBuf << "\n";
    // Free the buffer.
    LocalFree( lpMsgBuf );
    return -1;
    break;
  default:
    ALL_ASSERT(0,"Semaphore::p: unknown return code");
    return -1;
  }
#else
  // Posix by default
  iRetVal = sem_trywait(semaphore);
  if (iRetVal == 0) {  iRetVal = 1;
  } else if (errno == EAGAIN) { iRetVal = 0;
  } else {
    perror("Semaphore::condP: ");
    iRetVal = -1;
  }
#endif
  return iRetVal;
}

int Semaphore::numResources() {
  return cResources;
}

// static var definition
#ifdef sgi
usptr_t *Semaphore::ppaArena = NULL;

// this will set things up so that usinit creates temp files which never
// show up in the file system (can't be used across processes, but fine
// in a single process with multiple threads).
// static ptrdiff_t __gpdDummy = usconfig( CONF_ARENATYPE, US_SHAREDONLY );


// for umask stuff
#include <sys/types.h>
#include <sys/stat.h>

void Semaphore::allocArena() {
#if 0
  char rgchTemp[]="/tmp/weberh.XXXXXX";
  // generate a unique filename
  mktemp(rgchTemp);
  if (rgchTemp[0]=='\0') {
    cerr << "Thread::allocArena: could not generate unique file name." << "\n";
    return;
  }
  // set mask to read/write/execute by all so temp file can be overwritten
  mode_t mOldUMask = umask(S_IRWXO);
#endif
  // /dev/zero is a special file which can only be shared between
  // processes/threads which share file descriptors.
  // It never shows up in the file system.
  if ((ppaArena = usinit("/dev/zero"))==NULL) {
    perror("Thread::allocArena: usinit:");
  }
#if 0
  umask(mOldUMask);
#endif
}
#endif

Thread::Thread(void (*pfThread)(void *pvThreadData), 
	       const ThreadData& td) :
  pfThread(pfThread), td(td), ulProcID(0) 
{}

int Thread::go() {
  if (ulProcID) {
    cerr << "Thread::go: already running (pid=" << ulProcID << ")." << "\n";
    return -1;
  }
#ifdef sgi
  if ((ulProcID=sproc( &threadFuncShell, PR_SALL, (void *)this ))==
      ((unsigned long)-1)) {
    perror("Thread::go: sproc");
    return -1;
  }
// Threads not defined for the CYGWIN environment yet...
#elif defined(_WIN32) && !defined(__CYGWIN__)
  // pass in func, let it pick stack size, and arg to pass to thread
  if ((ulProcID=_beginthread( &threadFuncShell, 0, (void *)this )) ==
      ((unsigned long)-1)) {
    perror("Thread::go: _beginthread");
    return -1;
  }
  //  cerr << "Thread:go: thread id is " << ulProcID << "." << "\n";
#else
  // Pthreads by default
  if (pthread_create(&ulProcID, NULL, &threadFuncShellPosix, (void*)this) != 0) {
    perror("Thread::go:pthread_create: ");
    return -1;
  }
#endif
  return 0;
}

int Thread::kill() {
  // kill the os thread
  if (ulProcID>0) {
#ifdef sgi
    if (::kill( (long) ulProcID, SIGKILL)<0) {
      perror("Thread::kill: kill:");
      return -1;
    }
#elif defined(_WIN32)
    // Return value of -1 passed to TerminateThread causes a warning.
    if (!TerminateThread( (HANDLE) ulProcID, 1 )) {
      cerr << "Thread::kill: problem with terminateThread call." << "\n";
    }
#else
    // Posix by default
    if (pthread_kill(ulProcID, SIGKILL) != 0) {
      perror("Thread::kill:pthread_kill: ");
      return -1;
    }
#endif
  } else {
    cerr << "Thread::kill: thread is not currently alive." << "\n";
    return -1;
  }
  ulProcID = 0;
  return 0;
}

bool Thread::running() {
  return ulProcID!=0;
}

pthread_t Thread::pid() {
  return ulProcID;
}

bool Thread::available() {
#ifdef THREADS_AVAILABLE
  return true;
#else
  return false;
#endif
}

void Thread::userData( void *pvNewUserData ) {
  td.pvUD = pvNewUserData;
}

void *Thread::userData() {
  return td.pvUD;
}

void Thread::threadFuncShell( void *pvThread ) {
  Thread *pth = (Thread *)pvThread;
  pth->pfThread( &pth->td );
  // thread has stopped running
  pth->ulProcID = 0;
}

// This is a Posix-compatible function prototype that
// just calls the other function.
void *Thread::threadFuncShellPosix( void *pvThread ) {
  threadFuncShell(pvThread);
  return NULL;
}


Thread::~Thread() {
  if (running()) {
    kill();
  }
}

unsigned Thread::number_of_processors() {
#ifdef _WIN32
  // Copy the hardware information to the SYSTEM_INFO structure. 
  SYSTEM_INFO siSysInfo;
  GetSystemInfo(&siSysInfo); 
  return siSysInfo.dwNumberOfProcessors;
#elif linux
  // For Linux, we look at the /proc/cpuinfo file and count up the number
  // of "processor	:" entries (tab between processor and colon) in
  // the file to find out how many we have.
  FILE *f = fopen("/proc/cpuinfo", "r");
  int count = 0;
  if (f == NULL) {
    perror("Thread::number_of_processors:fopen: ");
    return 1;
  }

  char line[512];
  while (fgets(line, sizeof(line), f) != NULL) {
    if (strncmp(line, "processor\t:", strlen("processor\t:")) == 0) {
      count++;
    }
  }

  fclose(f);
  if (count == 0) {
    cerr << "Thread::number_of_processors: Found zero, returning 1" << "\n";
    count = 1;
  }
  return count;

#elif __APPLE__
	int numCPU;
	int mib[4];
	size_t len = sizeof(numCPU); 

	/* set the mib for hw.ncpu */
	mib[0] = CTL_HW;
	mib[1] = HW_AVAILCPU;  // alternatively, try HW_NCPU;

	/* get the number of CPUs from the system */
	sysctl(mib, 2, &numCPU, &len, NULL, 0);

	if( numCPU < 1 ) 
	{
	     mib[1] = HW_NCPU;
	     sysctl( mib, 2, &numCPU, &len, NULL, 0 );

	     if( numCPU < 1 )
	     {
		  numCPU = 1;
	     }
	}

	return numCPU;
#else
  cerr << "Thread::number_of_processors: Not yet implemented on this architecture." << "\n";
  return 1;
#endif
}

/*****************************************************************************\
  $Log: thread.C,v $
  Revision 1.7  2007/07/09 21:24:19  taylorr
  2007-07-09  Russell M. Taylor II  <taylorr@cs.unc.edu>

          * thread.C : Fixed bug in condP() on Posix;
                  it will now handle the case where the
                  semaphore runs out of counts.

  Revision 1.6  2006/04/08 02:08:57  taylorr
  2006-04-07  Russell M. Taylor II  <taylorr@cs.unc.edu>

  	* thread.h : Ported to use Pthreads by default if not on
  		SGI or Windows.  If it is on linux, then it can still
  		tell you how many processors there are; otherwise, not.
  	* thread.C : Same.

  Revision 1.4  2001/12/18 20:41:28  marshbur

          * thread.C: remove compiler warnings

  Revision 1.3  2001/03/02 17:51:45  hudson
  	* thread.C (Semaphore::p) : removed a line that shouldn't have been
  	committed.

  Revision 1.2  2001/02/20 17:50:32  hudson

  	* thread.C (Semaphore::v) : add cast to make error message work.
  	(Semaphore::init) : change NT initialization of semaphore to behave
  	more like SGI.

  Revision 1.1.1.1  1999/12/14 20:40:08  weigle
  This is the new directory structure.  More documentation will be forth
  coming, but there is a README.1ST which should get you compiling. 

  NOTE: This does not currently go make libraries for you if they can not
  be found, only complains that they are not there.  There are two places
  where you must do a gmake to get libraries:
     ./nano/src/lib  and  ./nano/src/app/nano/lib

  Revision 1.5  1999/10/05 02:03:15  helser
  Topo file read and write works for all images, including images of partial
  scans. The Topo file read/write routines do not attempt to translate
  between versions - files or streams that have Topo 4.0 headers write out
  Topo 4.0 files, and files or streams that have 5.0 headers write 5.0
  files. Added modifications to the header when we write out that are needed
  because we get new data from the AFM.

  Fixed minor problem with colormap - default width of color map is reduced
  when you first choose it.

  Fixed a few compilation warnings on the PC.

  Revision 1.4  1999/05/26 20:28:37  lovelace
  These changes allow microscape to be compiled on the WIN32 platform
  using the Cygnus Solutions Cygwin Environment (a WIN32 port of the
  gcc compiler).

  Revision 1.3  1999/03/23 15:22:56  hudson
  Added a new control panel (for latency compensation techniques).
  Added (an early version of) the first compensation technique -
    showing both the Phantom tip and true tip positions.
  Migrated a few small pieces of data onto nmb_Decoration.

  Revision 1.2  1998/07/08 16:13:53  hudson
  Removed some compile-time warnings.

  Revision 1.1  1998/05/27 19:30:41  hudson
  * Added multip/Makefile
                 README
                 myUtil.h
                 util.C
                 threads.[Ch]
  * Wrote sharedGraphicsServer() and spawnSharedGraphics() in microscape.c

  This is Hans Weber's multithreading library, used for a shared-memory
  multiprocessing version of microscape.

  Revision 4.2  1998/04/23 04:14:25  weberh
  *** empty log message ***

  Revision 4.1  1998/03/31 03:03:04  weberh
   pc threading compiles
  >

  Revision 4.0  1998/03/29 22:40:29  weberh
   sgi threading works. now on to pc threading.

  Revision 1.3  1998/03/28 21:07:40  weberh
  works.

  Revision 1.2  1998/03/25 23:06:22  weberh
  *** empty log message ***

  Revision 1.1  1998/03/23 22:57:19  weberh
  Initial revision

\*****************************************************************************/

