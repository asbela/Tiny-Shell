/***************************************************************************
 *  Title: Runtime environment 
 * -------------------------------------------------------------------------
 *    Purpose: Runs commands
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.3 $
 *    Last Modification: $Date: 2009/10/12 20:50:12 $
 *    File: $RCSfile: runtime.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: runtime.c,v $
 *    Revision 1.3  2009/10/12 20:50:12  jot836
 *    Commented tsh C files
 *
 *    Revision 1.2  2009/10/11 04:45:50  npb853
 *    Changing the identation of the project to be GNU.
 *
 *    Revision 1.1  2005/10/13 05:24:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.6  2002/10/24 21:32:47  sempi
 *    final release
 *
 *    Revision 1.5  2002/10/23 21:54:27  sempi
 *    beta release
 *
 *    Revision 1.4  2002/10/21 04:49:35  sempi
 *    minor correction
 *
 *    Revision 1.3  2002/10/21 04:47:05  sempi
 *    Milestone 2 beta
 *
 *    Revision 1.2  2002/10/15 20:37:26  sempi
 *    Comments updated
 *
 *    Revision 1.1  2002/10/15 20:20:56  sempi
 *    Milestone 1
 *
 ***************************************************************************/
#define __RUNTIME_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>

/************Private include**********************************************/
#include "runtime.h"
#include "io.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

/************Global Variables*********************************************/

#define NBUILTINCOMMANDS (sizeof BuiltInCommands / sizeof(char*))

int nextjid = 1;
int recentjid = 0;

/************Function Prototypes******************************************/
/* run command */
static void
RunCmdFork(commandT*, bool);
/* runs an external program command after some checks */
static void
RunExternalCmd(commandT*, bool);
/* resolves the path and checks for exutable flag */
static bool
ResolveExternalCmd(commandT*, char*);
/* forks and runs a external program */
static void
Exec(char*, commandT*, bool);
/* runs a builtin command */
static void
RunBuiltInCmd(commandT*);
/* checks whether a command is a builtin command */
static bool
IsBuiltIn(char*);

static void
listjobs(struct job_t *jobs);

struct job_t *
getjobjid(struct job_t *jobs, int jid); 
/************External Declaration*****************************************/

/**************Implementation***********************************************/


/*
 * RunCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: none
 
 * Runs the given command.
 */
void
RunCmd(commandT* cmd)
{
  RunCmdFork(cmd, TRUE);
} /* RunCmd */


/*
 * RunCmdFork
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   bool fork: whether to fork
 *
 * returns: none
 *
 * Runs a command, switching between built-in and external mode
 * depending on cmd->argv[0].
 */
void
RunCmdFork(commandT* cmd, bool fork)
{
  if (cmd->argc <= 0)
    return;
  int i;
  for (i = 0; i < cmd->argc; i++) {
    if (cmd->argv[i][0] == '|') {
      commandT** tcmd1 = malloc(sizeof(commandT*));
      commandT** tcmd2 = malloc(sizeof(commandT*));
      getCmds(cmd, tcmd1, tcmd2, i);
      commandT* cmd1 = *tcmd1;
      commandT* cmd2 = *tcmd2;
      RunCmdPipe(cmd1, cmd2);
      free(tcmd1);
      free(tcmd2);
      return;
    }
  }
  if (IsBuiltIn(cmd->argv[0]))
    {
      RunBuiltInCmd(cmd);
    }
  else
    {
      RunExternalCmd(cmd, fork);
    }
} /* RunCmdFork */

void getCmds(commandT* cmd, commandT** cmd1, commandT** cmd2, int argsplit) {
  char buf1[MAXLINE], buf2[MAXLINE];
  jobListToString(cmd->argv, 0, argsplit, buf1);
  jobListToString(cmd->argv, argsplit + 1, MAXARGS, buf2);
  
  *cmd1 = getCommand(buf1);
  *cmd2 = getCommand(buf2);
}

/*
 * RunCmdBg
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: none
 *
 * Runs a command in the background.
 */
void
RunCmdBg(commandT* cmd)
{
} /* RunCmdBg */


/*
 * RunCmdPipe
 *
 * arguments:
 *   commandT *cmd1: the commandT struct for the left hand side of the pipe
 *   commandT *cmd2: the commandT struct for the right hand side of the pipe
 *
 * returns: none
 *
 * Runs two commands, redirecting standard output from the first to
 * standard input on the second.
 */
void
RunCmdPipe(commandT* cmd1, commandT* cmd2)
{
  int pid, ppid, fd[2];
  if (pipe(fd) < 0) {
    perror("pipe error");
  }

  if ((pid = fork()) < 0) {
    perror("fork error");
  } else if (pid == 0) {
    dup2(fd[1], 1);
    close(fd[0]);
    setpgid(0, fgpid(jobs));
    RunExternalCmd(cmd1, 0);
  } else {
    if (fgpid(jobs) == 0) 
      addjob(jobs, pid, FG, "nil");
    if ((ppid = fork()) < 0) {
      perror("fork error");
    } else if (ppid == 0) {
      dup2(fd[0], 0);
      close(fd[1]);
      setpgid(0, fgpid(jobs));
      RunCmdFork(cmd2, 0);
    } else {
      close(fd[0]);
      close(fd[1]);
      waitpid(pid, NULL, 0);
      waitpid(ppid, NULL, 0);
      if (fgpid(jobs) == pid)
        deletejob(jobs, pid);
    }
  }
  freeCommand(cmd1);
  freeCommand(cmd2);
} /* RunCmdPipe */


/*
 * RunCmdRedirOut
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   char *file: the file to be used for standard output
 *
 * returns: none
 *
 * Runs a command, redirecting standard output to a file.
 */
void
RunCmdRedirOut(commandT* cmd, char* file)
{
} /* RunCmdRedirOut */


/*
 * RunCmdRedirIn
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   char *file: the file to be used for standard input
 *
 * returns: none
 *
 * Runs a command, redirecting a file to standard input.
 */
void
RunCmdRedirIn(commandT* cmd, char* file)
{
}  /* RunCmdRedirIn */


/*
 * RunExternalCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   bool fork: whether to fork
 *
 * returns: none
 *
 * Tries to run an external command. 
 */
static void
RunExternalCmd(commandT* cmd, bool fork)
{
  char buf[250];
  if (ResolveExternalCmd(cmd, buf))
    Exec(buf, cmd, fork);
  else
    printf("./tsh: line 1: %s: No such file or directory\n", cmd->argv[0]);
}  /* RunExternalCmd */


/*
 * ResolveExternalCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: bool: whether the given command exists
 *
 * Determines whether the command to be run actually exists by searching 
 * path and current directory.
 */
static bool
ResolveExternalCmd(commandT* cmd, char *abspath)
{
  int i = 0, j = 0;
  char *path = getenv("PATH");
  DIR *dir;
  struct dirent *dp;
  char pbuf[250];
  char *file = strrchr(cmd->argv[0], '/');
  int dirlen;
  if (file != NULL)
    dirlen = strlen(cmd->argv[0]) - strlen(file) + 1;
  else {
    dirlen = 1;
    file = cmd->argv[0];
  }
  char tpath[dirlen];
  memset(tpath, 0, dirlen);
  strncpy(tpath, cmd->argv[0], dirlen - 1);

  if (dirlen > 1) {
    if ((dir = opendir(tpath))) {
      while ((dp = readdir(dir)) != NULL)
        if (!strcmp(dp->d_name, file + 1)) {
          (void) closedir(dir);
          sprintf(abspath, "%s/%s", tpath, file);
          return 1;
        }
      (void) closedir(dir);
    }
  }

  while (i < strlen(path)) {
    memset(pbuf, 0, 250);
    while (j < 250 && path[i] != ':' && i < strlen(path)) {
      pbuf[j] = path[i];
      j++; i++;
    }
    pbuf[j] = '\0';
    j = 0; i++;
    if ((dir = opendir(strcat(pbuf, tpath)))) {
      while ((dp = readdir(dir)) != NULL)
        if (!strcmp(dp->d_name, file)) {
          (void) closedir(dir);
          sprintf(abspath, "%s/%s", pbuf, file);
          return 1;
        }
      (void) closedir(dir);
    }
  }
  return 0;
} /* ResolveExternalCmd */


/*
 * Exec
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   bool forceFork: whether to fork
 *
 * returns: none
 *
 * Executes a command.
 */
static void
Exec(char *abspath, commandT* cmd, bool forceFork)
{
  if (!forceFork) {
    if (execv(abspath, cmd->argv) < 0)
      printf("%s not found", cmd->argv[0]);
    return;
  }
  sigset_t mask;
  pid_t pid;
  bool bg;
  char buf[MAXLINE];
  int i = 1;
  int status;
  if (sigemptyset(&mask) != 0) {
    perror("sigemptyset error");
  }
  if (sigaddset(&mask, SIGCHLD) != 0) {
    perror("sigaddset error");
  }
  if (sigprocmask(SIG_BLOCK, &mask, NULL) != 0) {
    perror("sigprocmask error");
  }
  strncpy(buf, cmd->argv[0], MAXLINE);
  if (*cmd->argv[cmd->argc - 1] == '&') {
    cmd->argv[--cmd->argc] = NULL;
    bg = 1;
  } else
    bg = 0;
  jobListToString(cmd->argv, 0, cmd->argc, buf);
  if ((pid = fork()) < 0) {
    perror("fork error");
  } else if (pid == 0) {
    if (sigprocmask(SIG_UNBLOCK, &mask, NULL) != 0) {
      perror("sigprocmask error");
    }
    if (setpgid(0, 0) < 0) {
      perror("setpgid error");
    }
    if (execv(abspath, cmd->argv) < 0)
      printf("%s not found", cmd->argv[0]);
  } else {
    if (sigprocmask(SIG_UNBLOCK, &mask, NULL) != 0) {
      perror("sigprocmask error");
    }
    if (bg)	
      addjob(jobs, pid, BG, buf);
    else {
      addjob(jobs, pid, FG, buf);
      waitpid(pid, &status, WUNTRACED);
      if (!WIFSTOPPED(status)) 
        deletejob(jobs, pid);
    }
  }
} /* Exec */


/*
 * IsBuiltIn
 *
 * arguments:
 *   char *cmd: a command string (e.g. the first token of the command line)
 *
 * returns: bool: TRUE if the command string corresponds to a built-in
 *                command, else FALSE.
 *
 * Checks whether the given string corresponds to a supported built-in
 * command.
 */
static bool
IsBuiltIn(char* cmd)
{
  return !(strcmp(cmd, (char *) "cd") && strcmp(cmd, (char *) "jobs") && 
	strcmp(cmd, (char *) "bg") && strcmp(cmd, (char *) "fg"));
} /* IsBuiltIn */


/*
 * RunBuiltInCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: none
 *
 * Runs a built-in command.
 */
static void
RunBuiltInCmd(commandT* cmd)
{
  if (!(strcmp(cmd->argv[0], (char *) "cd"))) {
    if (cmd->argv[1])
      chdir(cmd->argv[1]);
    else
      chdir(getenv("HOME"));
    return;
  }
  if (!(strcmp(cmd->argv[0], (char *) "jobs"))) {
    listjobs(jobs);
    return;
  }
  struct job_t *job;
  int jid;
  int status;
  if (cmd->argv[1] == NULL) {
    if (recentjid = 0) 
      return;
    jid = recentjid;
  } else
    jid = atoi(cmd->argv[1]);
  if (!(job = getjobjid(jobs, jid)))
    printf("(%d): No such process\n", jid);
  else {
    if (kill(-(job->pid), SIGCONT) < 0) 
      if (errno != ESRCH)
        perror("kill error");

    if (!(strcmp(cmd->argv[0], (char *) "bg"))) {
      //printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);
      job->state = BG;
      recentjid = job->jid;
    } else {
      job->state = FG;
      waitpid(job->pid, &status, WUNTRACED);
      if (!WIFSTOPPED(status))
        deletejob(jobs, job->pid);
    }
  }
} /* RunBuiltInCmd */


/*
 * CheckJobs
 *
 * arguments: none
 *
 * returns: none
 *
 * Checks the status of running jobs.
 */
void
CheckJobs()
{
  int i;
  for (i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid != 0) {
      int status;
      int pid;
      pid = waitpid(jobs[i].pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
      if (pid == jobs[i].pid) {
        if (!(WIFCONTINUED(status))) {
          printf("[%d]   %s                    %s\n", jobs[i].jid, "Done", jobs[i].cmdline);
	  deletejob(jobs, jobs[i].pid);
        }
      }
    }
  }
} /* CheckJobs */

/*
 * fgpid
 *
 * arguments: none
 *
 * returns: fgjob
 *
 * Returns pid of foreground process. 
 */

/***********************************************
 *  * Helper routines that manipulate the job list
 *   **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid > max)
	    max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) 
{
    int i;
    
    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == 0) {
	    jobs[i].pid = pid;
	    jobs[i].state = state;
	    jobs[i].jid = nextjid++;
	    if (nextjid > MAXJOBS)
		nextjid = 1;
	    strcpy(jobs[i].cmdline, cmdline);
            return 1;
	}
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) 
{
    int i;
    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == pid) {
	    clearjob(&jobs[i]);
	    nextjid = maxjid(jobs)+1;
	    return 1;
	}
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].state == FG)
	    return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid)
	    return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) 
{
    int i;

    if (jid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid == jid)
	    return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) 
{
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid != 0) {
	    printf("[%d]   ", jobs[i].jid);
	    switch (jobs[i].state) {
		case BG: 
		    printf("Running");
		    break;
		case FG: 
		    printf("Foreground");
		    break;
		case ST: 
		    printf("Stopped");
		    break;
	    default:
		    printf("listjobs: Internal error: job[%d].state=%d ", 
			   i, jobs[i].state);
	    }
	    printf("                 %s", jobs[i].cmdline);
	    if (jobs[i].state == BG)
	      printf("&");
	    printf("\n");
	}
    }
}
/******************************
 *  * end job list helper routines
 *   ******************************/

void 
jobListToString(char **argv, int argstart, int argend, char *buf) {
  int i = argstart, j = 0, k = 0;
  bool quote = 0;
  while (argv[i]) {
    if (i >= argend) 
      break;
    for (j = 0; j < strlen(argv[i]); j++) {
      if (argv[i][j] == ' ') {
        quote = 1;
        break;
      }
    }
    if (quote) {
      buf[k] = '\"';
      k++;
    }
    for (j = 0; j < strlen(argv[i]); j++) {
      buf[k] = argv[i][j];
      k++;
    }
    if (quote) {
      buf[k] = '\"';
      k++;
    }
    buf[k] = ' ';
    k++;
    i++;
  }
  buf[k] = '\0';
  printf("%s\n", buf);
}
