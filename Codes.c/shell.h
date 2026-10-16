
#ifndef SHELL_H
#define SHELL_H

#include <sys/types.h>

#define MAX_JOBS 100
#define MAX_COMMAND 256
#define MAX_ARGS 32

typedef struct {
    int id;
    pid_t pid;
    char command[MAX_COMMAND];
    int active;
    int stopped;
} Job;

void show_menu(void);
void run_command(int background);
void list_jobs(void);
void resume_job(int foreground);
void signal_job(int signal_number);
void show_current_directory(void);
void refresh_jobs(void);

#endif
