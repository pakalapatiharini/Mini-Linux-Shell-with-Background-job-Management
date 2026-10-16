
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_JOBS 100
#define MAX_CMD 256

typedef struct {
    int id;
    pid_t pid;
    char command[MAX_CMD];
    int active;
    int stopped;
} Job;

static Job jobs[MAX_JOBS];
static int next_id = 1;

static Job *find_job(int id)
{
    for (int i = 0; i < MAX_JOBS; i++)
        if (jobs[i].active && jobs[i].id == id)
            return &jobs[i];

    return NULL;
}

static void add_job(pid_t pid, const char *cmd, int stopped)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jobs[i].active) {
            jobs[i].id = next_id++;
            jobs[i].pid = pid;
            jobs[i].active = 1;
            jobs[i].stopped = stopped;
            snprintf(jobs[i].command, MAX_CMD, "%s", cmd);

            printf("Job [%d] added. PID: %d\n",
                   jobs[i].id, pid);
            return;
        }
    }

    printf("Job table full.\n");
}

static void refresh_jobs(void)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        Job *j = &jobs[i];
        int status;

        if (!j->active)
            continue;

        pid_t r = waitpid(j->pid, &status,
                          WNOHANG | WUNTRACED | WCONTINUED);

        if (r == j->pid) {
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                printf("\nJob [%d] finished: %s\n",
                       j->id, j->command);
                j->active = 0;
            } else if (WIFSTOPPED(status)) {
                j->stopped = 1;
            } else if (WIFCONTINUED(status)) {
                j->stopped = 0;
            }
        } else if (r < 0 && errno == ECHILD) {
            j->active = 0;
        }
    }
}

static void list_jobs(void)
{
    refresh_jobs();
    int found = 0;

    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].active) {
            printf("[%d] %s | PID: %d | %s\n",
                   jobs[i].id,
                   jobs[i].stopped ? "Stopped" : "Running",
                   jobs[i].pid, jobs[i].command);
            found = 1;
        }
    }

    if (!found)
        printf("No active jobs.\n");
}

static void run_command(int background)
{
    char command[MAX_CMD];
    char copy[MAX_CMD];
    char *args[64];
    int argc = 0;

    printf("Enter command: ");
    if (!fgets(command, sizeof(command), stdin))
        return;

    command[strcspn(command, "\n")] = '\0';
    if (command[0] == '\0')
        return;

    snprintf(copy, sizeof(copy), "%s", command);

    char *token = strtok(command, " \t");
    while (token && argc < 63) {
        args[argc++] = token;
        token = strtok(NULL, " \t");
    }
    args[argc] = NULL;

    if (argc == 0)
        return;

    if (strcmp(args[0], "cd") == 0) {
        const char *path = args[1] ? args[1] : getenv("HOME");
        if (chdir(path) < 0)
            perror("cd");
        return;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return;
    }

    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        execvp(args[0], args);
        perror("Command failed");
        _exit(127);
    }

    if (background) {
        add_job(pid, copy, 0);
    } else {
        int status;
        pid_t r;

        do {
            r = waitpid(pid, &status, WUNTRACED);
        } while (r < 0 && errno == EINTR);

        if (r == pid && WIFSTOPPED(status))
            add_job(pid, copy, 1);
    }
}

static void resume_job(int foreground)
{
    int id;

    printf("Enter job ID: ");
    if (scanf("%d", &id) != 1) {
        while (getchar() != '\n' && !feof(stdin)) {}
        printf("Invalid job ID.\n");
        return;
    }
    while (getchar() != '\n' && !feof(stdin)) {}

    Job *j = find_job(id);

    if (!j) {
        printf("Job not found.\n");
        return;
    }

    if (kill(j->pid, SIGCONT) < 0) {
        perror("SIGCONT");
        return;
    }

    j->stopped = 0;

    if (foreground) {
        int status;
        pid_t r;

        do {
            r = waitpid(j->pid, &status, WUNTRACED);
        } while (r < 0 && errno == EINTR);

        if (r == j->pid) {
            if (WIFSTOPPED(status)) {
                j->stopped = 1;
                printf("Job [%d] stopped again.\n", id);
            } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
                j->active = 0;
                printf("Job [%d] finished.\n", id);
            }
        }
    } else {
        printf("Job [%d] resumed in background.\n", id);
    }
}

static void signal_job(int sig)
{
    int id;

    printf("Enter job ID: ");
    if (scanf("%d", &id) != 1) {
        while (getchar() != '\n' && !feof(stdin)) {}
        printf("Invalid job ID.\n");
        return;
    }
    while (getchar() != '\n' && !feof(stdin)) {}

    Job *j = find_job(id);

    if (!j) {
        printf("Job not found.\n");
        return;
    }

    if (kill(j->pid, sig) < 0) {
        perror("Signal");
        return;
    }

    if (sig == SIGSTOP) {
        j->stopped = 1;
        printf("Job [%d] stopped.\n", id);
    } else if (sig == SIGTERM || sig == SIGKILL) {
        printf("Termination requested for job [%d].\n", id);
    }
}

static void menu(void)
{
    printf("\n========== MINI LINUX SHELL ==========\n");
    printf("1. Execute command (foreground)\n");
    printf("2. Run command in background\n");
    printf("3. View all jobs\n");
    printf("4. Bring job to foreground\n");
    printf("5. Resume job in background\n");
    printf("6. Stop a job\n");
    printf("7. Terminate a job\n");
    printf("8. Show current directory\n");
    printf("9. Exit\n");
    printf("======================================\n");
    printf("Enter choice: ");
}

int main(void)
{
    int choice;

    printf("Welcome to Mini Linux Shell!\n");

    while (1) {
        refresh_jobs();
        menu();

        if (scanf("%d", &choice) != 1) {
            if (feof(stdin))
                break;

            while (getchar() != '\n' && !feof(stdin)) {}
            printf("Please enter a number.\n");
            continue;
        }

        while (getchar() != '\n' && !feof(stdin)) {}

        switch (choice) {
            case 1:
                run_command(0);
                break;

            case 2:
                run_command(1);
                break;

            case 3:
                list_jobs();
                break;

            case 4:
                resume_job(1);
                break;

            case 5:
                resume_job(0);
                break;

            case 6:
                signal_job(SIGSTOP);
                break;

            case 7:
                signal_job(SIGTERM);
                break;

            case 8: {
                char cwd[1024];
                if (getcwd(cwd, sizeof(cwd)))
                    printf("Current directory: %s\n", cwd);
                else
                    perror("getcwd");
                break;
            }

            case 9:
                printf("Exiting MiniShell.\n");
                return 0;

            default:
                printf("Invalid choice. Select 1-9.\n");
        }
    }

    return 0;
}
