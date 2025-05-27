/**
 * Linux Job Control Shell Project
 *
 * Operating Systems
 * Grados Ing. Informatica & Software
 * Dept. de Arquitectura de Computadores - UMA
 *
 * Some code adapted from "OS Concepts Essentials", Silberschatz et al.
 *
 * To compile and run the program:
 *   $ gcc shell.c job_control.c -o shell
 *   $ ./shell
 *	(then type ^D to exit program)
 **/

#include "job_control.h" /* Remember to compile with module job_control.c */

#define MAX_LINE 256 /* 256 chars per line, per command, should be enough */

job *tasks;

void manejador(int signal) {
  job *act_task;
  int status;
  int info;
  int pid_wait = 0;
  enum status status_res;

  block_SIGCHLD();
  for (int i = 1; i <= list_size(tasks); i++) {
    act_task = get_item_bypos(tasks, i);
    pid_wait = waitpid(act_task->pgid, &status, WUNTRACED | WNOHANG);
    if (pid_wait == act_task->pgid) {
      status_res = analyze_status(status, &info);

      if (status_res == SUSPENDED) {
        printf("Command %s with PID %d, is SUSPENDED\n", act_task->command,
               act_task->pgid);
        act_task->state = SUSPENDED;
      } else if (status_res == EXITED) {
        printf("Command %s with PID %d, has EXITED\n", act_task->command,
               act_task->pgid);
        delete_job(tasks, act_task);
      }
    }
  }
  unblock_SIGCHLD();
}

/**
 * MAIN
 **/
int main(void) {
  char inputBuffer[MAX_LINE]; /* Buffer to hold the command entered */
  int background;             /* Equals 1 if a command is followed by '&' */
  char *args[MAX_LINE / 2]; /* Command line (of 256) has max of 128 arguments */
  /* Probably useful variables: */
  int pid_fork, pid_wait; /* PIDs for created and waited processes */
  int status;             /* Status returned by wait */
  enum status status_res; /* Status processed by analyze_status() */
  int info;               /* Info processed by analyze_status() */

  job *act_task;
  int isForeground = 0;

  ignore_terminal_signals();

  signal(SIGCHLD, manejador);

  tasks = new_list("tasks_list");

  while (
      1) /* Program terminates normally inside get_command() after ^D is typed*/
  {
    printf("COMMAND->");
    fflush(stdout);
    get_command(inputBuffer, MAX_LINE, args,
                &background); /* Get next command */

    if (args[0] == NULL)
      continue; /* Do nothing if empty command */

    // Built-In commands
    if (strcmp(args[0], "cd") == 0) {
      if (chdir(args[1]) == -1)
        perror("No such file or directory");
      continue;
    }

    if (strcmp(args[0], "jobs") == 0) {
      block_SIGCHLD();
      if (list_size(tasks) == 0)
        printf("Jobs list is empty\n");
      else
        print_job_list(tasks);
      unblock_SIGCHLD();
      continue;
    }

    if (strcmp(args[0], "bg") == 0) {
      block_SIGCHLD();
      int pos = 1;
      if (args[1] != NULL)
        pos = atoi(args[1]);
      act_task = get_item_bypos(tasks, pos);
      unblock_SIGCHLD();
      if ((act_task != NULL) && (act_task->state != STOPPED)) {
        act_task->state = BACKGROUND;
        killpg(act_task->pgid, SIGCONT);
      }
      continue;
    }

    if (strcmp(args[0], "fg") == 0) {
      block_SIGCHLD();
      int pos = 1;
      if (args[1] != NULL)
        pos = atoi(args[1]);
      act_task = get_item_bypos(tasks, pos);
      unblock_SIGCHLD();
      if (act_task != NULL) {
        set_terminal(act_task->pgid);
        if (act_task->state == STOPPED)
          killpg(act_task->pgid, SIGCONT);
        isForeground = 1;
        pid_fork = act_task->pgid;
        block_SIGCHLD();
        delete_job(tasks, act_task);
        unblock_SIGCHLD();
      }else
        continue;
    }

    if (strcmp(args[0], "logout") == 0)
      exit(EXIT_SUCCESS);

    // General structure
    if (!isForeground)
      pid_fork = fork();

    if (pid_fork == -1) {
      perror("Error at fork");
      exit(EXIT_FAILURE);
    }
    if (pid_fork == 0) {
      new_process_group(getpid());
      if (!background)
        set_terminal(getpid());
      restore_terminal_signals();
      execvp(args[0], args);
      perror("Error executing command");
      exit(EXIT_FAILURE);
    } else {

      if (background != 1) {
        pid_wait = waitpid(pid_fork, &status, WUNTRACED);
        set_terminal(getpid());
        status_res = analyze_status(status, &info);
        if (status_res == SUSPENDED) {
          block_SIGCHLD();
          add_job(tasks, new_job(pid_fork, args[0], STOPPED));
          unblock_SIGCHLD();
          printf("Foreground pid: %d, command: %s, %s, info: %d\n", pid_fork,
                 args[0], status_strings[status_res], info);
        } else if (status_res == EXITED) {
          if (info != 255) {
            printf("Foreground pid: %d, command: %s, %s, info: %d\n", pid_fork,
                   args[0], status_strings[status_res], info);
          }
        }
      } else {
        block_SIGCHLD();
        add_job(tasks, new_job(pid_fork, args[0], BACKGROUND));
        unblock_SIGCHLD();
        printf("Background pid: %d\n", pid_fork);
      }
      isForeground = 0;
    }

    /** The steps are:
     *	 (1) Fork a child process using fork()
     *	 (2) The child process will invoke execvp()
     * 	 (3) If background == 0, the parent will wait, otherwise continue
     *	 (4) Shell shows a status message for processed command
     * 	 (5) Loop returns to get_commnad() function
     **/

  } /* End while */
}
