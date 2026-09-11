#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <time.h>

#define ARG_MAX_COUNT 1024
#define HISTORY_MAXITEMS 100
#define MAX_BACKGROUND_PROCESSES 100

typedef struct { pid_t pid; char *cmd; } BackgroundProcess;

char **history; int history_len = 0;
pid_t *pids; time_t *start_times; double *durations;
BackgroundProcess background_processes[MAX_BACKGROUND_PROCESSES];
int bg_process_count = 0;

void init_history() {
    history = calloc(HISTORY_MAXITEMS, sizeof(char *));
    pids = calloc(HISTORY_MAXITEMS, sizeof(pid_t));
    start_times = calloc(HISTORY_MAXITEMS, sizeof(time_t));
    durations = calloc(HISTORY_MAXITEMS, sizeof(double));
    if (!history || !pids || !start_times || !durations) { fprintf(stderr, "error: memory allocation failed\n"); exit(EXIT_FAILURE); }
}

void add_to_history(char *cmd, pid_t pid, double duration) {
    char *line = strdup(cmd); if (!line) return;
    if (history_len == HISTORY_MAXITEMS) {
        free(history[0]);
        memmove(history, history + 1, sizeof(char *) * (HISTORY_MAXITEMS - 1));
        memmove(pids, pids + 1, sizeof(pid_t) * (HISTORY_MAXITEMS - 1));
        memmove(start_times, start_times + 1, sizeof(time_t) * (HISTORY_MAXITEMS - 1));
        memmove(durations, durations + 1, sizeof(double) * (HISTORY_MAXITEMS - 1));
        history_len--;
    }
    history[history_len] = line; pids[history_len] = pid; start_times[history_len] = time(NULL); durations[history_len] = duration; history_len++;
}

void print_history1() { for (int i = 0; i < history_len; i++) printf("%d %s (pid: %d, duration: %.2f seconds)\n", i + 1, history[i], pids[i], durations[i]); }
void print_history() { for (int i = 0; i < history_len; i++) printf("%d %s \n", i + 1, history[i]); }

void check_bg_processes() {
    for (int i = 0; i < bg_process_count; i++) {
        int status; pid_t result = waitpid(background_processes[i].pid, &status, WNOHANG);
        if (result == background_processes[i].pid) {
            printf("[Background] PID: %d finished command: %s\n", background_processes[i].pid, background_processes[i].cmd);
            free(background_processes[i].cmd);
            for (int j = i; j < bg_process_count - 1; j++) background_processes[j] = background_processes[j + 1];
            bg_process_count--; i--;
        }
    }
}

void execute_single_command(char *cmd) {
    char *args[ARG_MAX_COUNT]; int tokenCount = 0; int background = 0;
    size_t cmd_len = strlen(cmd);
    if (cmd_len > 0 && cmd[cmd_len - 1] == '&') { background = 1; cmd[cmd_len - 1] = '\0'; }
    char *token = strtok(cmd, " ");
    while (token && tokenCount < ARG_MAX_COUNT) { args[tokenCount++] = token; token = strtok(NULL, " "); }
    args[tokenCount] = NULL; if (!tokenCount) return;
    time_t start = time(NULL);
    pid_t pid = fork();
    if (pid == 0) { execvp(args[0], args); perror("exec"); exit(EXIT_FAILURE); }
    if (pid > 0) {
        int status;
        if (!background) {
            waitpid(pid, &status, 0); time_t end = time(NULL);
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                double duration = difftime(end, start); char *full_command = calloc(ARG_MAX_COUNT, 1);
                if (full_command) { snprintf(full_command, ARG_MAX_COUNT, "%s", args[0]); for (int i = 1; i < tokenCount; i++) { strncat(full_command, " ", ARG_MAX_COUNT - strlen(full_command) - 1); strncat(full_command, args[i], ARG_MAX_COUNT - strlen(full_command) - 1); } add_to_history(full_command, pid, duration); free(full_command); }
            }
        } else {
            printf("[Background] PID: %d running command: %s\n", pid, args[0]);
            add_to_history(cmd, pid, 0.0);
            if (bg_process_count < MAX_BACKGROUND_PROCESSES) background_processes[bg_process_count++] = (BackgroundProcess){pid, strdup(args[0])};
        }
    } else perror("fork");
}

void execute_piped_commands(char *cmd_parts[], int num_parts, char *original_command) {
    int fd[2]; pid_t pid; int fd_in = 0;
    for (int i = 0; i < num_parts; i++) {
        if (pipe(fd) == -1) { perror("pipe"); return; }
        char *args[ARG_MAX_COUNT]; int tokenCount = 0;
        char *token = strtok(cmd_parts[i], " ");
        while (token && tokenCount < ARG_MAX_COUNT) { args[tokenCount++] = token; token = strtok(NULL, " "); }
        args[tokenCount] = NULL;
        if ((pid = fork()) == 0) {
            if (dup2(fd_in, STDIN_FILENO) == -1) _exit(EXIT_FAILURE);
            if (i < num_parts - 1 && dup2(fd[1], STDOUT_FILENO) == -1) _exit(EXIT_FAILURE);
            close(fd[0]); close(fd[1]); if (fd_in != STDIN_FILENO) close(fd_in);
            execvp(args[0], args); perror("exec"); exit(EXIT_FAILURE);
        }
        close(fd[1]); if (fd_in != STDIN_FILENO) close(fd_in); fd_in = fd[0];
    }
    if (fd_in != STDIN_FILENO) close(fd_in);
    int status; waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) add_to_history(original_command, pid, 0.0);
}

void launch_command(char *cmd) {
    char original_cmd[ARG_MAX_COUNT]; strncpy(original_cmd, cmd, ARG_MAX_COUNT - 1); original_cmd[ARG_MAX_COUNT - 1] = '\0';
    char *cmd_part = strtok(cmd, "|"); char *cmd_parts[ARG_MAX_COUNT]; int num_parts = 0;
    while (cmd_part && num_parts < ARG_MAX_COUNT) { cmd_parts[num_parts++] = cmd_part; cmd_part = strtok(NULL, "|"); }
    if (num_parts == 1) execute_single_command(original_cmd); else if (num_parts > 1) execute_piped_commands(cmd_parts, num_parts, original_cmd);
    check_bg_processes();
}

int handle_builtin(char *input) {
    if (!strcmp(input, "exit")) { add_to_history(input, 0, 0.0); return -1; }
    if (!strcmp(input, "history")) { print_history(); add_to_history(input, 0, 0.0); return 0; }
    if (!strncmp(input, "cd", 2) && (input[2] == '\0' || isspace((unsigned char)input[2]))) {
        char *dir = input + 2; while (isspace((unsigned char)*dir)) dir++;
        if (!*dir || chdir(dir) != 0) perror("cd"); add_to_history(input, 0, 0.0); return 0;
    }
    return 1;
}

int is_blank(char *input) { for (size_t i = 0; input[i]; i++) if (!isspace((unsigned char)input[i])) return 0; return 1; }

int main(void) {
    init_history();
    printf("******************************\n***Welcome to Simple-Shell****\n******************************\n");
    while (1) {
        check_bg_processes(); printf("simple-shell>$$> "); fflush(stdout);
        char *input = NULL; size_t len = 0; ssize_t nread = getline(&input, &len, stdin);
        if (nread == -1) { free(input); break; }
        if (nread && input[nread - 1] == '\n') input[nread - 1] = '\0';
        if (is_blank(input)) { free(input); continue; }
        int ret = handle_builtin(input); if (ret == -1) { free(input); break; } else if (ret == 1) launch_command(input); free(input);
    }
    printf("Execution summary:\n"); print_history1();
    for (int i = 0; i < history_len; i++) free(history[i]);
    free(history); free(pids); free(start_times); free(durations);
    for (int i = 0; i < bg_process_count; i++) free(background_processes[i].cmd);
    return 0;
}
