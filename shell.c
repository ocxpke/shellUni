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

// No es necesario bloquear la señal de SIGCHLD ya que al llamarse al manejador
// se bloquean por el mismo SO, pero tampoco es algo que este mal
void signal_handler(int signal) {
  pid_t pid_wait;
  job *act_task;
  int status;
  int info;
  enum status task_status;

  block_SIGCHLD();

  act_task = get_iterator(tasks);

  while (act_task) {
    pid_wait =
        waitpid(act_task->pgid, &status, WUNTRACED | WNOHANG | WCONTINUED);
    if (pid_wait == act_task->pgid) {
      task_status = analyze_status(status, &info);
      if ((task_status == EXITED) || (task_status == SIGNALED)) {
        printf("Background job %s ended correctly\n", act_task->command);
        delete_job(tasks, act_task);
      } else if ((task_status == CONTINUED)) {
        printf("Stopped job %s launched\n", act_task->command);
        act_task->state = BACKGROUND;
      } else if ((task_status == SUSPENDED)) {
        printf("Job %s, runnig at background stopped\n", act_task->command);
        act_task->state = STOPPED;
      }
    }
    next(act_task);
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

  ignore_terminal_signals();
  tasks = new_list("tasks");
  signal(SIGCHLD, signal_handler);

  job *act_task;
  pid_t pid_fg;
  char *fg_task_name;

  char *file_in = NULL;
  char *file_out = NULL;
  FILE *f_in = NULL;
  FILE *f_out = NULL;

  while (
      1) /* Program terminates normally inside get_command() after ^D is typed*/
  {
    printf("COMMAND->");
    fflush(stdout);
    get_command(inputBuffer, MAX_LINE, args,
                &background); /* Get next command */

    if (args[0] == NULL)
      continue; /* Do nothing if empty command */

    if (!strcmp(args[0], "cd")) {
      if (args[1] != NULL)
        chdir(args[1]);
      continue;
    }

    if (!strcmp(args[0], "jobs")) {
      block_SIGCHLD();
      print_job_list(tasks);
      unblock_SIGCHLD();
      continue;
    }

    if (!strcmp(args[0], "bg")) {
      int pos = 1;
      if (args[1] != NULL)
        pos = atoi(args[1]);
      block_SIGCHLD();
      act_task = get_item_bypos(tasks, pos);
      if (act_task) {
        act_task->state = BACKGROUND;
        killpg(act_task->pgid, SIGCONT);
      }
      unblock_SIGCHLD();
      continue;
    }

    if (!strcmp(args[0], "fg")) {
      int pos = 1;
      if (args[1] != NULL)
        pos = atoi(args[1]);
      block_SIGCHLD();
      act_task = get_item_bypos(tasks, pos);
      unblock_SIGCHLD();
      if (act_task) {
        set_terminal(act_task->pgid);
        if (act_task->state == STOPPED)
          killpg(act_task->pgid, SIGCONT);
        pid_fg = act_task->pgid;
        fg_task_name = strdup(act_task->command);
        block_SIGCHLD();
        delete_job(tasks, act_task);
        act_task = NULL;
        unblock_SIGCHLD();
        pid_wait = waitpid(pid_fg, &status, WUNTRACED);
        set_terminal(getpid());
        status_res = analyze_status(status, &info);
        if (status_res == SUSPENDED) {
          block_SIGCHLD();
          add_job(tasks, new_job(pid_fg, fg_task_name, STOPPED));
          unblock_SIGCHLD();
          free(fg_task_name);
          printf("Suspended job added\n");
        } else if (status_res == EXITED) {
          printf("Foreground pid: %d, command: %s, %s, info: %d\n", pid_fg,
                 fg_task_name, status_strings[status_res], info);
          free(fg_task_name);
        }
      }
      continue;
    }

    if (!strcmp(args[0], "exit"))
      exit(EXIT_SUCCESS);

    /** The steps are:
     *	 (1) Fork a child process using fork()
     *	 (2) The child process will invoke execvp()
     * 	 (3) If background == 0, the parent will wait, otherwise continue
     *	 (4) Shell shows a status message for processed command
     * 	 (5) Loop returns to get_commnad() function
     **/

    parse_redirections(args, &file_in, &file_out);

    pid_fork = fork();

    if (pid_fork == -1) {
      // Error
      perror("Error at fork");
      exit(EXIT_FAILURE);
    } else if (pid_fork == 0) {
      // Child
      if (file_out) {
        f_out = fopen(file_out, "w");
        if (!f_out) {
          perror("Error opening out file\n");
          exit(EXIT_FAILURE);
        }
        int fd_out = fileno(f_out);
        dup2(fd_out, STDOUT_FILENO);
        close(fd_out);
      }
      if (file_in) {
        f_in = fopen(file_in, "r");
        if (!f_in) {
          perror("Error opening in file\n");
          exit(EXIT_FAILURE);
        }
        int fd_in = fileno(f_in);
        dup2(fd_in, STDIN_FILENO);
        close(fd_in);
      }
      new_process_group(getpid());
      if (!background)
        set_terminal(getpid());
      restore_terminal_signals();
      execvp(args[0], args);
      perror("Error executing command");
      exit(EXIT_FAILURE);
    } else {
      // Parent

      // Aunque hagamos esto mismo en el child, no sabemos que proceso se
      // ejecutara antes (y es 100% necesario asinarlo al mismo grupo) por el
      // compilador por lo que, aunque redundante es mejor incluirlo pues da
      // mayor seguridad.
      new_process_group(pid_fork);
      if (!background) {
        // Parent + background
        set_terminal(pid_fork);

        pid_wait = waitpid(pid_fork, &status, WUNTRACED);
        set_terminal(getpid());

        if (pid_wait == pid_fork) {
          status_res = analyze_status(status, &info);
          if (status_res == SUSPENDED) {
            block_SIGCHLD();
            add_job(tasks, new_job(pid_fork, args[0], STOPPED));
            unblock_SIGCHLD();
            printf("Suspended job added\n");
          }
          printf("Foreground pid: %d, command: %s, %s, info: %d\n", pid_fork,
                 args[0], status_strings[status_res], info);
        }

      } else {
        // Parent without background
        block_SIGCHLD();
        add_job(tasks, new_job(pid_fork, args[0], BACKGROUND));
        unblock_SIGCHLD();
        printf("Background job running... pid: %d, command: %s\n", pid_fork,
               args[0]);
      }
    }

  } /* End while */
}
