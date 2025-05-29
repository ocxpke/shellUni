/**
 * Linux Job Control Shell Project
 * Function prototypes, macros and type declarations for job_control module
 *
 * Operating Systems
 * Grados Ing. Informatica & Software
 * Dept. de Arquitectura de Computadores - UMA
 *
 * Some code adapted from "Operating System Concepts Essentials", Silberschatz
 *et al.
 **/
#ifndef _JOB_CONTROL_H
#define _JOB_CONTROL_H

// Hay extensiones no estandarizadas en POSIX pero que si se aplican con el
// compilador GCC, por eso incluimos esta define
#define _GNU_SOURCE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>


/**
 * Enumerations
 **/
enum status { SUSPENDED, SIGNALED, EXITED, CONTINUED };
enum job_state { FOREGROUND, BACKGROUND, STOPPED };
static char *status_strings[] = {"Suspended", "Signaled", "Exited",
                                 "Continued"};
static char *state_strings[] = {"Foreground", "Background", "Stopped"};

/* Job type for job list */
typedef struct job_ {
  pid_t pgid;    /* Group id = process lider id */
  char *command; /* Program name */
  enum job_state state;
  struct job_ *next; /* Next job in the list */
} job;

/* Type for job list iterator */
typedef job *job_iterator;

/**
 * Public Functions
 **/
void get_command(char inputBuffer[], int size, char *args[], int *background);
void parse_redirections(char **args, char **file_in, char **file_out);
job *new_job(pid_t pid, const char *command, enum job_state state);
void add_job(job *list, job *item);
int delete_job(job *list, job *item);
job *get_item_bypid(job *list, pid_t pid);
job *get_item_bypos(job *list, int n);
enum status analyze_status(int status, int *info);

/**
 * Private Functions: Better use through macros below
 **/
void print_item(job *item);
void print_list(job *list, void (*print)(job *));
void terminal_signals(void (*func)(int));
void block_signal(int signal, int block);

/**
 * Public macros
 **/

#define list_size(list) list->pgid /* Number of jobs in the list */
#define empty_list(list)                                                       \
  !(list->pgid) /* Returns 1 (true) if the list is empty */

#define new_list(name)                                                         \
  new_job(0, name, FOREGROUND) /* Name must be const char * */

#define get_iterator(list) list->next /* Return pointer to first job */
#define has_next(iterator) iterator   /* Return pointer to next job */
#define next(iterator)                                                         \
  ({                                                                           \
    job_iterator old = iterator;                                               \
    iterator = iterator->next;                                                 \
    old;                                                                       \
  }) /* Updates iterator to point to next job */

#define print_job_list(list) print_list(list, print_item)

#define restore_terminal_signals() terminal_signals(SIG_DFL)
#define ignore_terminal_signals() terminal_signals(SIG_IGN)

#define set_terminal(pid) tcsetpgrp(STDIN_FILENO, pid)
#define new_process_group(pid) setpgid(pid, pid)

#define block_SIGCHLD() block_signal(SIGCHLD, 1)
#define unblock_SIGCHLD() block_signal(SIGCHLD, 0)

/** Macro for debugging
 *    To debug integer i, use:    debug(i,%d);
 *    It will print out:  current line number, function name and file name, and
 *also variable name, value and type
 **/
#define debug(x, fmt)                                                          \
  fprintf(stderr, "\"%s\":%u:%s(): --> %s= " #fmt " (%s)\n", __FILE__,         \
          __LINE__, __FUNCTION__, #x, x, #fmt)

#endif
