/**
 * Linux Control Job Shell Project
 * job_control module
 *
 * Operating Systems
 * Grados Ing. Informatica, Computadores & Software
 * Dept. de Arquitectura de Computadores - UMA
 *
 * Some code adapted from "Operating System Concepts Essentials", Silberschatz
 *et al.
 **/
#include "job_control.h"

/**
 *  get_command() reads in the next command line, separating it into distinct
 *  tokens using whitespace as delimiters. setup() sets the args parameter as a
 *  null-terminated string.
 **/
void get_command(char inputBuffer[], int size, char *args[], int *background) {
  int length, /* # of characters in the command line */
      i,      /* Loop index for accessing inputBuffer array */
      start,  /* Index where beginning of next command parameter is */
      ct;     /* Index of where to place the next parameter into args[] */

  ct = 0;
  *background = 0;

  /* Read what the user enters on the command line */
  length = read(STDIN_FILENO, inputBuffer, size);

  start = -1;
  if (length == 0) {
    printf("\nBye\n");
    exit(0); /* ^d was entered, end of user command stream */
  }
  if (length < 0) {
    perror("error reading the command");
    exit(-1); /* Terminate with error code of -1 */
  }

  /* Examine every character in the inputBuffer */
  int end = 0, iesc = 0;
  for (i = 0; i < length; i++) {
    if (end)
      break;
    if (i > 1 && i > iesc)
      inputBuffer[i - 1 - iesc] = inputBuffer[i - 1];
    switch (inputBuffer[i]) {
    case ' ':
    case '\t': /* Argument separators */
      if (start != -1) {
        args[ct] = &inputBuffer[start]; /* Set up pointer */
        ct++;
      }
      inputBuffer[i] = '\0'; /* Add a null char; make a C string */
      start = -1;
      inputBuffer[i - iesc] = '\0'; /* add a null char; make a C string */
      iesc = 0;
      break;
    case '#': /* Comment found */
      if (i > 0 && '\\' == inputBuffer[i - 1]) {
        iesc++; /* Escaped comment symbol */
        if (start == -1)
          start = i; // Start of new argument
        break;
      }
    case '\n': /* Should be the final char examined */
      if (start != -1) {
        args[ct] = &inputBuffer[start];
        ct++;
      }
      inputBuffer[i] = '\0';
      args[ct] = NULL; /* No more arguments to this command */
      end = 1;
      break;
    default:                     /* Some other character */
      if (inputBuffer[i] == '&') /* Background indicator */
      {
        *background = 1;
        if (start != -1) {
          args[ct] = &inputBuffer[start];
          ct++;
        }
        inputBuffer[i] = '\0';
        args[ct] = NULL; /* No more arguments to this command */
        i = length;      /* Make sure the for loop ends now */
      } else if (start == -1)
        start = i; /* Start of new argument */
    } /* End switch */
  } /* End for */
  args[ct] = NULL; /* Just in case the input line was > MAXLINE */
  if (i > 1 && i > iesc)
    inputBuffer[i - 1 - iesc] = inputBuffer[i - 1];
}

/**
 * Parse redirections operators '<' '>' once args structure has been built.
 * Call the function immediately after get_commad():
 *      ...
 *     while(...){
 *          // Shell main loop
 *          ...
 *          get_command(...);
 *          char *file_in, *file_out;
 *          parse_redirections(args, &file_in, &file_out);
 *          ...
 *     }
 *
 * For a valid redirection, a blank space is required before and after
 * redirection operators '<' or '>'.
 **/
void parse_redirections(char **args, char **file_in, char **file_out) {
  *file_in = NULL;
  *file_out = NULL;

  char **args_start = args;
  while (*args) {
    int is_in = !strcmp(*args, "<");
    int is_out = !strcmp(*args, ">");

    if (is_in || is_out) {
      args++;
      if (*args) {
        if (is_in)
          *file_in = *args;
        if (is_out)
          *file_out = *args;
        char **aux = args + 1;
        while (*aux) {
          *(aux - 2) = *aux;
          aux++;
        }
        *(aux - 2) = NULL;
        args--;
      } else {
        /* Syntax error */
        fprintf(stderr, "syntax error in redirection\n");
        args_start[0] = NULL; // Do nothing
      }
    } else {
      args++;
    }
  }
  /* Debug:
   * *file_in && fprintf(stderr, "[parse_redirections] file_in='%s'\n",
   * *file_in); *file_out && fprintf(stderr, "[parse_redirections]
   * file_out='%s'\n", *file_out);
   */
}

/**
 * Returns a pointer to a list item with its fields initialized.
 * Returns NULL if memory allocation fails
 **/
job *new_job(pid_t pid, const char *command, enum job_state state) {
  job *aux;
  aux = (job *)malloc(sizeof(job));
  if (!aux)
    return NULL;
  aux->pgid = pid;
  aux->state = state;
  aux->command = strdup(command);
  aux->next = NULL;
  return aux;
}

/**
 * Inserts an item as head of the list
 **/
void add_job(job *list, job *item) {
  job *aux = list->next;
  list->next = item;
  item->next = aux;
  list->pgid++;
}

/**
 * Deletes from the list the item passed as second argument.
 * Returns 0 if the item does not exist.
 **/
int delete_job(job *list, job *item) {
  job *aux = list;
  while (aux->next != NULL && aux->next != item)
    aux = aux->next;
  if (aux->next) {
    aux->next = item->next;
    free(item->command);
    free(item);
    list->pgid--;
    return 1;
  } else
    return 0;
}

/**
 * Looks an item up by its PID and returns it.
 * Returns NULL if the item is not found.
 **/
job *get_item_bypid(job *list, pid_t pid) {
  job *aux = list;
  while (aux->next != NULL && aux->next->pgid != pid)
    aux = aux->next;
  return aux->next;
}

/**
 * Looks an item up by its position inside the list, beginning with 1 (as item 0
 * is devoted to hold the name and number of items of the list), and returns it.
 * Returns NULL if the item is not found.
 **/
job *get_item_bypos(job *list, int n) {
  job *aux = list;
  if (n < 1 || n > list->pgid)
    return NULL;
  n--;
  while (aux->next != NULL && n) {
    aux = aux->next;
    n--;
  }
  return aux->next;
}

/**
 * Prints a line with the info o an item: pid, command name and state
 **/
void print_item(job *item) {

  printf("pid: %d, command: %s, state: %s\n", item->pgid, item->command,
         state_strings[item->state]);
}

/**
 * Walks the list and call print function for each item in it
 **/
void print_list(job *list, void (*print)(job *)) {
  int n = 1;
  job *aux = list;
  printf("Contents of %s:\n", list->command);
  while (aux->next != NULL) {
    printf(" [%d] ", n);
    print(aux->next);
    n++;
    aux = aux->next;
  }
}

/**
 * Interpret the status value returned by wait */
enum status analyze_status(int status, int *info) {
  /* Suspended process */
  if (WIFSTOPPED(status)) {
    *info = WSTOPSIG(status);
    return (SUSPENDED);
  }
  /* Continued process */
  else if (WIFCONTINUED(status)) {
    *info = 0;
    return (CONTINUED);
  }
  /* Terminated process by signal*/
  else if (WIFSIGNALED(status)) {
    *info = WTERMSIG(status);
    return (SIGNALED);
  }
  /*Terminated process by exit */
  else if (WIFEXITED(status)) {
    *info = WEXITSTATUS(status);
    return (EXITED);
  }
  /* Should never get here*/
  return -1;
}

/**
 * Changes default action for terminal related signals
 **/
void terminal_signals(void (*func)(int)) {
  signal(SIGINT, func);  /* crtl+c Interrupt from keyboard */
  signal(SIGQUIT, func); /* ctrl+\ Quit from keyboard */
  signal(SIGTSTP, func); /* crtl+z Stop typed at keyboard */
  signal(SIGTTIN, func); /* Background process tries terminal input */
  signal(SIGTTOU, func); /* Background process tries terminal output */
}

/**
 * Blocks or masks a signal.
 * The signal handler execution for the signal is deferred until the signal
 * is unblocked.
 * If several instances of the signal ocurred after being blocked, when
 * unblocked, the handler for that signal executes only once.
 **/
void block_signal(int signal, int block) {
  /* Declare and initialize signal masks */
  sigset_t block_sigchld;
  sigemptyset(&block_sigchld);
  sigaddset(&block_sigchld, signal);
  if (block) {
    /* Blocks signal */
    sigprocmask(SIG_BLOCK, &block_sigchld, NULL);
  } else {
    /* Unblocks signal */
    sigprocmask(SIG_UNBLOCK, &block_sigchld, NULL);
  }
}
