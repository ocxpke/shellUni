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

// Variables globales para alarm signal pues solo puede haber un comando en
// foreground a la vez
pid_t pidAlarmSig;
time_t global_time;
int timeSignalGlobal;

// Allocate args to insert in the job struxter in need of relaunching jobs
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

// Free resources for allocated **char
void free_pp_char(char **args) {
  for (int i = 0; args[i]; i++) {
    free(args[i]);
  }
  free(args);
}

// If the job is inmortal we will relauunch it in background mode
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
    new_task->threadWait = NULL;
    new_task->isProcWait = 0;
    new_task->pid_wait = -1;
    new_task->isAlarmSig = 0;
    new_task->timeAlarmSig = 0;
    new_task->initTime = 0;
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
        if (act_task->threadWait) {
          pthread_cancel(*(act_task->threadWait));
        }
        if (act_task->isProcWait) {
          kill(act_task->pid_wait, SIGKILL);
          waitpid(act_task->pid_wait, NULL, WUNTRACED);
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

// Sighup handler
void sighup_handler(int signal) {
  FILE *fp;
  fp = fopen("hup.txt", "a");
  if (fp) {
    fprintf(fp, "SIGHUP recibido.\n");
    fclose(fp);
  }
}

// Sigalrm handler
void sigalrm_handler(int signal) {
  // Chequeamos el de foreground
  if (pidAlarmSig > 0) {
    if ((time(NULL) - global_time) >= timeSignalGlobal) {
      kill(pidAlarmSig, SIGCONT);
      kill(pidAlarmSig, SIGKILL);
    }
  }
  // Comprobamos los que esten en bg o suspended
  block_SIGCHLD();
  job *act_task = get_iterator(tasks);
  while (act_task) {
    if (act_task->isAlarmSig) {
      if ((time(NULL) - act_task->initTime) >= act_task->timeAlarmSig) {
        kill(act_task->pgid, SIGCONT);
        kill(act_task->pgid, SIGKILL);
      }
    }
    next(act_task);
  }
  unblock_SIGCHLD();
}

// Check if we need to append and on that case organize args
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

// Check if we have "+" character
int is_inmortal(char **args) {
  for (int i = 0; args[i]; i++) {
    if (!strcmp(args[i], "+")) {
      args[i] = NULL;
      return (1);
    }
  }
  return (0);
}

// Check if the argument for blocking signals has init keyword and end flag
int is_block_mask(char **args) {
  if (!strcmp(args[0], "mask")) {
    for (int i = 1; args[i]; i++) {
      if (!strcmp(args[i], "-c"))
        return (1);
    }
  }
  return (0);
}

// Here we are sure that the argumet to block signals is correct and procceed to
// block signals, there is a function to do this so there was no need to replay
// this function
void block_signals_mask(char **args, sigset_t *signals_set) {
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
}

// Copy what we read into inputBuff to analyze the command
// NECESSARY TO END WITH /n
void clone_into_buff(char *input, char inputBuff[]) {
  int len = strlen(input);
  if (len > MAX_LINE)
    len = MAX_LINE;
  for (int i = 0; i < len; i++) {
    inputBuff[i] = input[i];
  }
  inputBuff[len] = '\n';
}

// Function that will exec our alarm-thread
void *thread_job(void *arg) {
  waitThread_t *argThreadJ = (waitThread_t *)arg;

  sleep(argThreadJ->wait);

  kill(argThreadJ->pid, SIGCONT);
  kill(argThreadJ->pid, SIGKILL);

  free(argThreadJ);
  pthread_exit(NULL);
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

  // Our shell must ignore signals
  ignore_terminal_signals();
  // we create our new task list
  tasks = new_list("tasks");
  // how me manage when we receive SIGCHLD
  signal(SIGCHLD, signal_handler);
  // manage alarm signal
  signal(SIGALRM, sigalrm_handler);

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

  // Inmortal
  int inmortal = 0;

  // History
  char *entry;

  // Alarm-Thread
  int isThread;
  int timeThread;
  pthread_t *threadWait;
  waitThread_t *argThread = NULL;

  // Alarm-Proc
  int isProcWait;
  int timeProc;
  pid_t pidAlarmProc;

  // Alarm-Signal
  time_t initTime;
  int isAlarmSig;
  int timeAlarmSig;

  while (
      1) /* Program terminates normally inside get_command() after ^D is typed*/
  {
    // Con la libreral de readline implementamos el historial
    entry = readline("COMMAND->");

    /*
    printf("COMMAND->");
    fflush(stdout);
    */

    // Para ejecutar la instruccion del historial
    if ((entry != NULL) && entry[0] == '!') {
      int num = atoi(&entry[1]);
      if (num > 0) {
        HIST_ENTRY **hist_search = history_list();
        int i = 0;
        while ((i < (num - 1)) && hist_search[i]) {
          i++;
        }
        if ((i == (num - 1)) && hist_search[i]) {
          free(entry);
          entry = strdup(hist_search[i]->line);
        }
      }
    }

    // Vaciamos lo que haya en  input buffer
    bzero(inputBuffer, MAX_LINE);
    // Si el usuario ha usado ^D se acaba, sino clonamos
    if (entry == NULL)
      inputBuffer[0] = 0;
    else
      clone_into_buff(entry, inputBuffer);

    // Parseo de lo introducido por el usuario
    get_command(inputBuffer, MAX_LINE, args,
                &background); /* Get next command */

    // Texto vacio == continuar
    if (args[0] == NULL)
      continue; /* Do nothing if empty command */

    // Se añade la entrada al historial y se borra
    add_history(entry);
    free(entry);

    // Cd built-in
    if (!strcmp(args[0], "cd")) {
      if (args[1] != NULL)
        chdir(args[1]);
      continue;
    }

    // Prints the jobs suspended and bg list
    if (!strcmp(args[0], "jobs")) {
      block_SIGCHLD();
      print_job_list(tasks);
      unblock_SIGCHLD();
      continue;
    }

    // Changes suspended job to run in background
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

    // Changes a suspended, or a background job to run in foreground
    if (!strcmp(args[0], "fg")) {
      int pos = 1;
      pthread_t *threadFG;
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
        threadFG = act_task->threadWait;
        isProcWait = act_task->isProcWait;
        pidAlarmProc = act_task->pid_wait;
        isAlarmSig = act_task->isAlarmSig;
        timeAlarmSig = act_task->timeAlarmSig;
        initTime = act_task->initTime;
        block_SIGCHLD();
        free_pp_char(act_task->comm_args);
        delete_job(tasks, act_task);
        act_task = NULL;
        unblock_SIGCHLD();

        pidAlarmSig = pidAlarmProc;
        global_time = initTime;
        timeSignalGlobal = timeAlarmSig;

        pid_wait = waitpid(pid_fg, &status, WUNTRACED);
        set_terminal(getpid());
        status_res = analyze_status(status, &info);

        if (status_res == SUSPENDED) {
          pidAlarmSig = 0;
          block_SIGCHLD();
          act_task = new_job(pid_fg, fg_task_name, STOPPED);
          act_task->inmortal = 0;
          act_task->comm_args = cpy_args(args);
          act_task->threadWait = threadFG;
          act_task->isProcWait = isProcWait;
          act_task->pid_wait = pidAlarmProc;
          act_task->isAlarmSig = isAlarmSig;
          act_task->timeAlarmSig = timeAlarmSig;
          act_task->initTime = initTime;
          add_job(tasks, act_task);
          unblock_SIGCHLD();
          free(fg_task_name);
          printf("Suspended job added\n");
        } else if (status_res == EXITED || /*new*/ status_res == EXITED) {
          if (threadFG)
            pthread_cancel(*threadFG);
          printf("Foreground pid: %d, command: %s, %s, info: %d\n", pid_fg,
                 fg_task_name, status_strings[status_res], info);
          free(fg_task_name);
        }
      }
      continue;
    }

    // Currjob --> prints the first job of the list
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

    // Deljob deletes a job from the job list --> this leads to zombie jobs
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

    // zjobs --> cleans all zombie jobs made by deljob
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
              printf("%ld\n", z_pid);
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

    // bgteam --> executes n times a command in backgorund mode
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
          act_task->threadWait = NULL;
          act_task->isProcWait = 0;
          act_task->pid_wait = -1;
          act_task->isAlarmSig = 0;
          act_task->timeAlarmSig = 0;
          act_task->initTime = 0;
          add_job(tasks, act_task);
          unblock_SIGCHLD();
        }
      }
      continue;
    }

    // Cleans history command
    if (!strcmp(args[0], "histclean")) {
      clear_history();
      continue;
    }

    // Shows all commands executed by user
    if (!strcmp(args[0], "hist")) {
      HIST_ENTRY **hist = history_list();
      for (int i = 0; hist[i]; i++) {
        printf("%d %s\n", i + 1, hist[i]->line);
      }
      continue;
    }

    // Set to 0 / null var needed by alarm-thread
    isThread = 0;
    timeThread = 0;
    threadWait = NULL;
    // Set needed info by alarm-thread
    if (!strcmp(args[0], "alarm-thread")) {
      timeThread = atoi(args[1]);
      if (timeThread <= 0)
        continue;
      isThread = 1;
      int i = 0;
      char *tmp;
      while (args[i + 2]) {
        tmp = args[i + 2];
        args[i] = tmp;
        i++;
      }
      args[i] = NULL;
    }

    // Set to 0 / null var needed by alarm-proc
    isProcWait = 0;
    pidAlarmProc = 0;
    // Set needed info by alarm-proc
    if (!strcmp(args[0], "alarm-proc")) {
      timeProc = atoi(args[1]);
      if (timeProc <= 0)
        continue;
      isProcWait = 1;
      int i = 0;
      char *tmp;
      while (args[i + 2]) {
        tmp = args[i + 2];
        args[i] = tmp;
        i++;
      }
      args[i] = NULL;
    }

    // Set to 0 / null var needed by alarm-signal
    isAlarmSig = 0;
    timeAlarmSig = 0;
    // Set needed info by alarm-signal
    if (!strcmp(args[0], "alarm-signal")) {
      timeAlarmSig = atoi(args[1]);
      if (timeAlarmSig <= 0)
        continue;
      timeSignalGlobal = timeAlarmSig;
      isAlarmSig = 1;
      int i = 0;
      char *tmp;
      while (args[i + 2]) {
        tmp = args[i + 2];
        args[i] = tmp;
        i++;
      }
      args[i] = NULL;
    }

    // Variables needed for mydeamon
    pid_t pid_sub_fork = 0;
    FILE *f_null;
    int finum_null;

    // Doble fork because we need a "nieto" so child needs to create another
    // child and the die the systemd will be the father of our daemon
    if (!strcmp(args[0], "mydaemon")) {
      pid_fork = fork();

      if (pid_fork == 0) {
        new_process_group(getpid());
        restore_terminal_signals();
        pid_sub_fork = fork();
        if (pid_sub_fork == 0) {
          printf("Deamon pid: %d\n", getpid());

          new_process_group(getpid());
          block_signal(SIGHUP, 1);

          f_null = fopen("/dev/null", "r+");
          if (!f_null) {
            perror("Error en deamon");
            continue;
          }

          finum_null = fileno(f_null);
          dup2(finum_null, STDIN_FILENO);
          dup2(finum_null, STDOUT_FILENO);
          dup2(finum_null, STDERR_FILENO);

          execvp(args[1], &args[1]);
          perror("Error executing command");
          exit(EXIT_FAILURE);
        } else {
          new_process_group(pid_sub_fork);
          exit(EXIT_SUCCESS);
        }
      } else {
        new_process_group(pid_fork);
        waitpid(pid_fork, &status, WUNTRACED);
      }

      continue;
    }

    // Exit function
    if (!strcmp(args[0], "exit"))
      exit(EXIT_SUCCESS);

    /** The steps are:
     *	 (1) Fork a child process using fork()
     *	 (2) The child process will invoke execvp()
     * 	 (3) If background == 0, the parent will wait, otherwise continue
     *	 (4) Shell shows a status message for processed command
     * 	 (5) Loop returns to get_commnad() function
     **/

    // We detect if we have to redirect outputs or inputs
    parse_redirections(args, &file_in, &file_out);

    append = check_if_append(args, &file_out);
    if (append == -1)
      continue;

    // Initialize varibale used for inmortal commands
    inmortal = 0;
    inmortal = is_inmortal(args);

    // Set to use for blocking signals
    sigset_t signals_set;

    // Initialize variables for alarm-signal
    pidAlarmSig = 0;

    pid_fork = fork();

    // In alarm-proc we need pid of child and pid of the process that will kill
    // child
    if ((pid_fork > 0) && isProcWait) {
      pidAlarmProc = fork();
      if (pidAlarmProc == 0) {
        new_process_group(getpid());
        restore_terminal_signals();
        sleep(timeProc);
        kill(pid_fork, SIGCONT);
        kill(pid_fork, SIGKILL);
        exit(EXIT_SUCCESS);
      } else if (pidAlarmProc > 0) {
        new_process_group(pidAlarmProc);
      }
    }

    if (pid_fork == -1) {
      // Error
      perror("Error at fork");
      exit(EXIT_FAILURE);
    } else if (pid_fork == 0) {
      // Child

      // Block signals received by user
      if (is_block_mask(args))
        block_signals_mask(args, &signals_set);

      // Redirect file out
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

      // Redirect file in
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

      // Built in command to execute bash script
      if (!strcmp(args[0], "fico"))
        args[0] = "./cuentafich.sh";
      execvp(args[0], args);
      perror("Error executing command");

      // Kill alarm process if something goes wrong
      if (isProcWait) {
        kill(pidAlarmProc, SIGKILL);
        waitpid(pidAlarmProc, NULL, WUNTRACED);
      }

      exit(EXIT_FAILURE);
    } else {
      // Parent

      // Aunque hagamos esto mismo en el child, no sabemos que proceso se
      // ejecutara antes (y es 100% necesario asinarlo al mismo grupo) por el
      // compilador por lo que, aunque redundante es mejor incluirlo pues da
      // mayor seguridad.
      new_process_group(pid_fork);

      // In case of alarm-thread create a new thread + arguments for every case
      if (isThread) {
        threadWait = (pthread_t *)malloc(sizeof(pthread_t));
        argThread = (waitThread_t *)malloc(sizeof(waitThread_t));
        argThread->pid = pid_fork;
        argThread->wait = timeThread;
        pthread_create(threadWait, NULL, thread_job, argThread);
        pthread_detach(*threadWait);
      }

      // Set needed data for alarm-signal case
      if (isAlarmSig) {
        time(&initTime);
        global_time = initTime;
        alarm(timeAlarmSig);
      }

      if (!background && !inmortal) {
        // Parent + no background
        set_terminal(pid_fork);

        pidAlarmSig = pid_fork;
        pid_wait = waitpid(pid_fork, &status, WUNTRACED);
        set_terminal(getpid());

        if (pid_wait == pid_fork) {
          status_res = analyze_status(status, &info);
          if (status_res == SUSPENDED) {
            pidAlarmSig = 0;
            block_SIGCHLD();
            act_task = new_job(pid_fork, args[0], STOPPED);
            act_task->inmortal = 0;
            act_task->comm_args = cpy_args(args);
            act_task->threadWait = NULL;
            if (isThread)
              act_task->threadWait = threadWait;
            act_task->isProcWait = isProcWait;
            act_task->pid_wait = pidAlarmProc;
            act_task->isAlarmSig = isAlarmSig;
            act_task->timeAlarmSig = timeAlarmSig;
            act_task->initTime = initTime;
            add_job(tasks, act_task);
            unblock_SIGCHLD();
            printf("Suspended job added\n");
          }

          // Kill thread (ALARM-THREAD) if proccess has died on fg
          if ((status_res != SUSPENDED) && isThread) {
            pthread_cancel(*threadWait);
          }

          // Kill process (ALARM-PROC) if proccess has died on fg
          if ((status_res != SUSPENDED) && isProcWait) {
            kill(pidAlarmProc, SIGKILL);
            waitpid(pidAlarmProc, NULL, WUNTRACED);
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
        act_task->threadWait = NULL;
        if (isThread)
          act_task->threadWait = threadWait;
        act_task->isProcWait = isProcWait;
        act_task->pid_wait = pidAlarmProc;
        act_task->isAlarmSig = isAlarmSig;
        act_task->timeAlarmSig = timeAlarmSig;
        act_task->initTime = initTime;
        add_job(tasks, act_task);
        unblock_SIGCHLD();
        printf("Background job running... pid: %d, command: %s\n", pid_fork,
               args[0]);
      }
    }

  } /* End while */
}
