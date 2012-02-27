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
  thread.h
  --
  Description : encapsulates all the info needed for a simple thread.
                Takes care of killing off threads when program ends.

		on sgi's, you must link with -lmpc

		Semas and threads can be declared whether or not
		threads can really be used.

		An error only occurs when a user calls go() on a thread.

		This allows code to be written which protects potentially
		shared data and is valid whether there is threading or not.

		Conditional compilation can be done using ifdef
		THREADS_AVAILABLE, and run-time conditional execution can
		be based on the value of Thread::available().


		This note below should no longer be true -- terminateThread
		does this.
		NOTE: kill only acts on the thread which calls it in win95/nt;
		this is not usually the desired behavior, but i have not found
		a way to have one thread kill another in win95/nt.  usually
		a controlling process wants to make this call to kill a child
		process (the thread associated with the Thread object) -- this
		will not work right now in win95/nt.

		Modified by Russ Taylor to work under Posix Threads when not on
		Windows or SGI (where Hans had it working before).

  ----------------------------------------------------------------------------
  Author: weberh
  Created: Mon Mar 23 16:42:55 1998
  Revised: Thu May 14 09:59:05 1998 by weberh
  $Source: /afs/unc/proj/stm/src/CVS_repository/nano/src/lib/nmMP/thread.h,v $
  $Locker:  $
  $Revision: 1.7 $
\*****************************************************************************/
#ifndef _THREAD_H_
#define _THREAD_H_

#ifdef _WIN32
#include <windows.h>
#endif

#if defined(sgi) || (defined(_WIN32) && !defined(__CYGWIN__)) || defined(linux)
#define THREADS_AVAILABLE
#else
#undef THREADS_AVAILABLE
#endif

// multi process stuff
#ifdef sgi
#include <task.h>
#include <ulocks.h>
#elif defined(_WIN32)
#include <process.h>
#else
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#endif
#include <stdio.h>

// kill 
#include <sys/types.h>
#include <signal.h>

// sleep 
#ifndef _WIN32
#include <unistd.h>
#endif

// make the SGI compile without tons of warnings
#ifdef sgi
#pragma set woff 1110,1424,3201
#endif

// and reset the warnings
#ifdef sgi
#pragma reset woff 1110,1424,3201
#endif

class Semaphore {
public:
  // mutex by default (0 is a sync primitive)
  Semaphore( int cNumResources = 1 );

  // This does not copy the state of the semaphore, just creates
  // a new one with the same resource count
  Semaphore( const Semaphore& s );
  ~Semaphore();

  // routine to reset it (0 on success, -1 failure)
  // (may create new semaphore)
  int reset( int cNumResources = 1 );

  // routines to use it (p blocks, cond p does not)
  // p returns 1 when it has acquired the resource, -1 on fail
  // v returns 0 when it has released the resource, -1 on fail
  // condP returns 0 if it could not access the resource
  // and 1 if it could (-1 on fail)
  int p();
  int v();
  int condP();

  // read values
  int numResources();

protected:
  // common init routine
  void init();

  int cResources;

  // arch specific details
#ifdef sgi
  // single mem area for dynamically alloced shared mem
  static usptr_t *ppaArena;
  static void allocArena();

  // the semaphore struct in the arena
  usema_t *ps;
  ulock_t l;
  Boolean fUsingLock;
#elif defined(_WIN32)
  HANDLE hSemaphore;
#else
  sem_t	*semaphore;
#endif
};

// A ptr to this struct will be passed to the 
// thread function.  The user data ptr will be in pvUD,
// and ps should contain a semaphore for mutex access to
// the data.

// The user should create and manage the semaphores.

struct ThreadData {
  void *pvUD;
  Semaphore *ps;
};

typedef void (*THREAD_FUNC) ( void *pvThreadData );

class Thread {
public:  
  // args are the routine to run in the thread
  // a ThreadData struct which will be passed into
  // the thread (it will be passed as a void *).
  Thread( THREAD_FUNC pfThread, const ThreadData& td );
  ~Thread();

  // start/kill the thread (0 on success, -1 on failure)
  int go();
  int kill();
  
  // thread info
  // check if running
  // get proc id
  bool running();
  pthread_t pid();

  // run-time user function to test it threads are available
  // (same value as #ifdef THREADS_AVAILABLE)
  static bool available();

  // Number of processors available on this machine.
  static unsigned number_of_processors();

  // this can be used to change the ThreadData user data ptr
  // between calls to go (ie, when a thread object is used
  // many times with different args).  This will take
  // effect the next time go() is called.
  void userData( void *pvNewUserData );
  void *userData();
  
protected:  
  // user func and data ptrs
  void (*pfThread)(void *pvThreadData);
  ThreadData td;
  
  // utility func for calling the specified function.
  static void threadFuncShell(void *pvThread);

  // Posix version of the utility function, makes the
  // function prototype match.
  static void *threadFuncShellPosix(void *pvThread);

  // the process id
  pthread_t ulProcID;

  // arch specific details
};
#endif //_THREAD_H_



/*****************************************************************************\
  $Log: thread.h,v $
  Revision 1.7  2006/07/30 13:32:15  taylorr
  Adding things to make it compile under Linux.

  Revision 1.6  2006/04/08 02:08:57  taylorr
  2006-04-07  Russell M. Taylor II  <taylorr@cs.unc.edu>

  	* thread.h : Ported to use Pthreads by default if not on
  		SGI or Windows.  If it is on linux, then it can still
  		tell you how many processors there are; otherwise, not.
  	* thread.C : Same.

  Revision 1.4  2002/03/19 02:00:14  marshbur
   - All the changes in this commit are the same.  On the SGI only,
  they add compiler pragmas, before any stream includes, in order to
  ignore warnings generated by the system include files.  The warnings
  are reset after the stream includes.

  Revision 1.3  2002/03/15 17:41:01  helser
  All files changed in this commit were updated to use the
  standard iostream headers. This paves the way for using STL
  in nano, since the standard iostream and STL are compatible.
  Also includes a few changes to workspace/project files to help
  with this change.

  Revision 1.2  2000/01/06 15:59:25  weigle
  Tom wants to work in the new tree, so I'm making sure we're up to date.

  Revision 1.7  1999/12/15 15:26:11  juliano
  get rid of the typedef around struct ThreadData.  Now, compiler errors have the real name, not the phony struct tag.

  Revision 1.6  1999/05/28 21:44:22  lovelace
  One small change that was left out of the previous NT commit.  These
  files had been changed in the include directory, but not in the
  multip directory.

  Revision 1.4  1999/03/23 15:22:56  hudson
  Added a new control panel (for latency compensation techniques).
  Added (an early version of) the first compensation technique -
    showing both the Phantom tip and true tip positions.
  Migrated a few small pieces of data onto nmb_Decoration.

  Revision 1.3  1998/06/01 20:56:26  kumsu
  put in code to compile with aCC for pxfl

  Revision 1.2  1998/05/28 13:14:15  hudson
  Put #ifdef sgi around some less-than-portable constructs in util.C
  (nonblockingGetch) because FLOW doesn't support them.

  Revision 1.1  1998/05/27 19:30:42  hudson
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

