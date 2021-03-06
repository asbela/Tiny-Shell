/***************************************************************************
 *  Title: tsh
 * -------------------------------------------------------------------------
 *    Purpose: A simple shell implementation 
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.4 $
 *    Last Modification: $Date: 2009/10/12 20:50:12 $
 *    File: $RCSfile: tsh.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
#define __MYSS_IMPL__

/************System include***********************************************/
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/************Private include**********************************************/
#include "tsh.h"
#include "io.h"
#include "interpreter.h"
#include "runtime.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

#define BUFSIZE 80

/************Global Variables*********************************************/

/************Function Prototypes******************************************/
/* handles SIGINT and SIGSTOP signals */
static void
sig(int);

void sigchld_handler(int sig);
void sigint_handler(int sig);
void sigtstp_handler(int sig);

/************External Declaration*****************************************/

/**************Implementation***********************************************/

/*
 * main
 *
 * arguments:
 *   int argc: the number of arguments provided on the command line
 *   char *argv[]: array of strings provided on the command line
 *
 * returns: int: 0 = OK, else error
 *
 * This sets up signal handling and implements the main loop of tsh.
 */
int
main(int argc, char *argv[])
{
  /* Initialize command buffer */
  char* cmdLine = malloc(sizeof(char*) * BUFSIZE);
  /* shell initialization */
  if (signal(SIGINT, sig) == SIG_ERR)
    PrintPError("SIGINT");
  if (signal(SIGTSTP, sig) == SIG_ERR)
    PrintPError("SIGTSTP");
  
  initjobs(jobs);

  while (!forceExit) /* repeat forever */
    {
      //printf("tsh> ");
      /* read command line */
      getCommandLine(&cmdLine, BUFSIZE);
      if (strcmp(cmdLine, "exit") != 0) {
      /* checks the status of background jobs */
        CheckJobs();
      /* interpret command and line
       * includes executing of commands */
        Interpret(cmdLine);
      } else
        forceExit = TRUE;
    }
  
  /* shell termination */
  free(cmdLine);
  return 0;
} /* main */

/*
 * sig
 *
 * arguments:
 *   int signo: the signal being sent
 *
 * returns: none
 *
 * This should handle signals sent to tsh.
 */
static void
sig(int signo)
{
  int pid = fgpid(jobs);
  if (signo == SIGINT)
    printf("\n");
  if (pid != 0) {
    if (signo == SIGINT) {
      if (kill(-pid, SIGINT) < 0)
        printf("kill error\n");
    } else {
      printf("\n");
      struct job_t *job = getjobpid(jobs, pid);
      kill(-pid, SIGSTOP);
      job->state = ST;
      printf("[%d]   Stopped                 %s\n", job->jid, job->cmdline);
    }
  }
} /* sig */

/*****************
 *  * Signal handlers
 *   *****************/

/* 
 *  * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *   *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    *    to the foreground job.  
 *     */
void sigint_handler(int sig) 
{
    int pid = fgpid(jobs);
    
    if (pid != 0) 
		if (kill(-pid, SIGINT) < 0)
			printf("kill error\n");
}

/*
 *  * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *   *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *    *     foreground job by sending it a SIGTSTP.  
 *     */
void sigtstp_handler(int sig) 
{
    int pid = fgpid(jobs);
    
    if (pid != 0) {
		//printf("Job [%d] (%d) stopped by signal %d\n", pid2jid(pid), pid, sig);
		getjobpid(jobs, pid)->state = ST;
		kill(-pid, SIGSTOP);
	}
}
