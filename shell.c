/**
 * Linux Job Control Shell Project
 *
 * Operating Systems
 * Grados Ing. Informatica & Software
 * Dept. de Arquitectura de Computadores - UMA
 *
 * JOSE RAMIREZ GIRON
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

char **cpy_args(char **args) {
  int len = 0;
  while (args[len])
    len++;
  char **ret = (char **)calloc(len + 1, sizeof(char *));
  if (!ret) {
    perror("Error at calloc");
    exit(EXIT_FAILURE);
  }
  for (int i = 0; i < len; i++) {
    ret[i] = strdup(args[i]);
  }
  ret[len] = NULL;
  return (ret);
}

void free_pp_char(char **args) {
  for (int i = 0; args[i]; i++) {
    free(args[i]);
  }
  free(args);
}

void relaunch(job *rela_job) {
  pid_t pid_fork = fork();
  job *new_task;

  if (pid_fork == -1) {
    perror("Error at fork");
    exit(EXIT_FAILURE);
  } else if (pid_fork == 0) {
    new_process_group(getpid());
    restore_terminal_signals();
    execvp(rela_job->comm_args[0], rela_job->comm_args);
    perror("Error executing job");
    exit(EXIT_FAILURE);
  } else {
    new_process_group(pid_fork);
    new_task = new_job(pid_fork, rela_job->comm_args[0], BACKGROUND);
    new_task->inmortal = 1;
    new_task->comm_args = cpy_args(rela_job->comm_args);
    add_job(tasks, new_task);
  }
}

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
        if (act_task->inmortal) {
          print_item(act_task);
          relaunch(act_task);
        }
        free_pp_char(act_task->comm_args);
        delete_job(tasks, act_task);
      } else if ((task_status == CONTINUED)) {
        printf("Stopped job %s launched\n", act_task->command);
        act_task->state = BACKGROUND;
      } else if ((task_status == SUSPENDED)) {
        printf("Job %s, running at background stopped\n", act_task->command);
        act_task->state = STOPPED;
      }
    }
    next(act_task);
  }
  unblock_SIGCHLD();
}

void sighup_handler(int signal) {
  FILE *fp;
  fp = fopen("hup.txt", "a");
  if (fp) {
    fprintf(fp, "SIGHUP recibido.\n");
    fclose(fp);
  }
}

// Append
int check_if_append(char **args, char **file_out) {
  for (int i = 0; args[i]; i++) {
    if (!strcmp(args[i], ">>")) {
      *file_out = args[i + 1];
      if (!file_out) {
        fprintf(stderr, "syntax error in redirection\n");
        return (-1);
      }
      char *act = args[i + 2];
      int z = 0;
      while (args[i + z]) {
        args[i + z] = act;
        z++;
        act = args[i + 2 + z];
      }
      return (1);
    }
  }
  return (0);
}

int is_inmortal(char **args) {
  for (int i = 0; args[i]; i++) {
    if (!strcmp(args[i], "+")) {
      args[i] = NULL;
      return (1);
    }
  }
  return (0);
}

int is_block_mask(char **args) {
  if (!strcmp(args[0], "mask")) {
    for (int i = 1; args[i]; i++) {
      if (!strcmp(args[i], "-c"))
        return (1);
    }
  }
  return (0);
}

void block_signals_mask(char **args, sigset_t *signals_set) {
  printf("holaaaaa\n");
  int i = 1;
  sigemptyset(signals_set);
  while (strcmp(args[i], "-c")) {
    sigaddset(signals_set, atoi(args[i]));
    i++;
  }
  sigprocmask(SIG_BLOCK, signals_set, NULL);
  int z = 0;
  char *tmp;
  i++;
  while (args[i + z]) {
    tmp = args[i + z];
    printf("args:%s\n", tmp);
    args[z] = tmp;
    z++;
  }
  args[z] = NULL;

  for (int w = 0; args[w]; w++)
    printf("%s\n", args[w]);
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

  // Fg, Bg and jobs
  job *act_task;
  pid_t pid_fg;
  char *fg_task_name;

  // Redirections
  char *file_in = NULL;
  char *file_out = NULL;
  FILE *f_in = NULL;
  FILE *f_out = NULL;

  // Sighup
  signal(SIGHUP, sighup_handler);

  // Append >>
  int append = 0;

  int inmortal = 0;

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
        free_pp_char(act_task->comm_args);
        delete_job(tasks, act_task);
        act_task = NULL;
        unblock_SIGCHLD();
        pid_wait = waitpid(pid_fg, &status, WUNTRACED);
        set_terminal(getpid());
        status_res = analyze_status(status, &info);
        if (status_res == SUSPENDED) {
          block_SIGCHLD();
          act_task = new_job(pid_fg, fg_task_name, STOPPED);
          act_task->inmortal = 0;
          act_task->comm_args = cpy_args(args);
          add_job(tasks, act_task);
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

    // currjob
    if (!strcmp(args[0], "currjob")) {
      act_task = get_item_bypos(tasks, 1);
      if (!act_task)
        printf("No hay trabajo actual\n");
      else
        printf("Trabajo actual: PID=%d command=%s\n", act_task->pgid,
               act_task->command);
      act_task = NULL;
      continue;
    }

    // deljob
    if (!strcmp(args[0], "deljob")) {
      block_SIGCHLD();
      act_task = get_item_bypos(tasks, 1);
      unblock_SIGCHLD();
      if (!act_task)
        printf("No hay trabajo actual\n");
      else {
        if (act_task->state == STOPPED) {
          printf(
              "No se permiten borrar trabajos en segundo plano suspendido\n");
        } else {
          printf("Borrando trabajo actual de la lista de jobs: PID=%d "
                 "command=%s\n",
                 act_task->pgid, act_task->command);
          block_SIGCHLD();
          free_pp_char(act_task->comm_args);
          delete_job(tasks, act_task);
          unblock_SIGCHLD();
        }
      }
      act_task = NULL;
      continue;
    }

    // zjobs
    if (!strcmp(args[0], "zjobs")) {
      block_SIGCHLD();
      DIR *d;
      struct dirent *dir;
      char buff[2048];
      d = opendir("/proc");
      if (d) {
        while ((dir = readdir(d)) != NULL) {
          sprintf(buff, "/proc/%s/stat", dir->d_name);
          FILE *fd = fopen(buff, "r");
          if (fd) {
            long z_pid;   // pid
            long z_ppid;  // ppid
            char z_state; // estado: R (runnable), S (sleeping), T(stopped), Z
                          // (zombie)

            // La siguiente línea lee pid, state y ppid de /proc/<pid>/stat
            fscanf(fd, "%ld %s %c %ld", &z_pid, buff, &z_state, &z_ppid);
            if ((z_state == 'Z') && (getpid() == z_ppid)) {
              printf("%d\n", z_pid);
              pid_wait = waitpid(z_pid, &status, WUNTRACED);
              status_res = analyze_status(status, &info);
              /*
              printf("Zombie job %d ended correctly status: %s, info: %d\n",
                     z_pid, status_strings[status_res], info);
                     */
            }
            fclose(fd);
          }
        }
        closedir(d);
      }
      continue;
    }

    // bgteam
    if (!strcmp(args[0], "bgteam")) {
      if ((args[1] == NULL) || (args[2] == NULL)) {
        printf("El comando bgteam requiere dos argumentos\n");
        continue;
      }
      if (atoi(args[1]) <= 0)
        continue;
      for (int i = 0; i < atoi(args[1]); i++) {
        pid_fork = fork();
        if (pid_fork == -1) {
          perror("Error at fork");
        } else if (pid_fork == 0) {
          new_process_group(getpid());
          restore_terminal_signals();
          execvp(args[2], &args[2]);
          perror("Error executing command");
          exit(EXIT_FAILURE);
        } else {
          new_process_group(pid_fork);
          block_SIGCHLD();
          act_task = new_job(pid_fork, args[2], BACKGROUND);
          act_task->inmortal = 0;
          act_task->comm_args = cpy_args(args);
          add_job(tasks, act_task);
          unblock_SIGCHLD();
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

    append = check_if_append(args, &file_out);
    if (append == -1)
      continue;

    inmortal = 0;
    inmortal = is_inmortal(args);

    sigset_t signals_set;

    pid_fork = fork();

    if (pid_fork == -1) {
      // Error
      perror("Error at fork");
      exit(EXIT_FAILURE);
    } else if (pid_fork == 0) {
      // Child

      if (is_block_mask(args))
        block_signals_mask(args, &signals_set);

      if (file_out) {
        if (append)
          f_out = fopen(file_out, "a");
        else
          f_out = fopen(file_out, "w");
        if (!f_out) {
          perror("Error opening out file\n");
          exit(EXIT_FAILURE);
        }
        int fd_out = fileno(f_out);
        dup2(fd_out, STDOUT_FILENO);
        fclose(f_out);
        f_out = NULL;
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
        fclose(f_in);
        f_in = NULL;
        close(fd_in);
      }
      new_process_group(getpid());
      if (!background && !inmortal)
        set_terminal(getpid());
      restore_terminal_signals();
      if (!strcmp(args[0], "fico"))
        args[0] = "./cuentafich.sh";
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
      if (!background && !inmortal) {
        // Parent + no background
        set_terminal(pid_fork);

        pid_wait = waitpid(pid_fork, &status, WUNTRACED);
        set_terminal(getpid());

        if (pid_wait == pid_fork) {
          status_res = analyze_status(status, &info);
          if (status_res == SUSPENDED) {
            block_SIGCHLD();
            act_task = new_job(pid_fork, args[0], STOPPED);
            act_task->inmortal = 0;
            act_task->comm_args = cpy_args(args);
            add_job(tasks, act_task);
            unblock_SIGCHLD();
            printf("Suspended job added\n");
          }
          printf("Foreground pid: %d, command: %s, %s, info: %d\n", pid_fork,
                 args[0], status_strings[status_res], info);
        }

      } else {
        // Parent + background
        block_SIGCHLD();
        act_task = new_job(pid_fork, args[0], BACKGROUND);
        act_task->inmortal = inmortal;
        act_task->comm_args = cpy_args(args);
        add_job(tasks, act_task);
        unblock_SIGCHLD();
        printf("Background job running... pid: %d, command: %s\n", pid_fork,
               args[0]);
      }
    }

  } /* End while */
}
