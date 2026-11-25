#include "common.h"
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHILD_COUNT (NUM_STAGES + 1)
static volatile sig_atomic_t stop_requested = 0;
static volatile sig_atomic_t conveyor_signal = 0;

static void on_signal(int sig) {
    if (sig == SIG_CONVEYOR_STOP || sig == SIG_CONVEYOR_RESUME) {
        conveyor_signal = sig;
    } else if (sig != SIGCHLD) {
        stop_requested = 1;
    }
}

static int install_handlers(void) {
    struct sigaction sa = {0};
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = on_signal;
    if (sigaction(SIGINT, &sa, NULL) ||
        sigaction(SIGTERM, &sa, NULL) ||
        sigaction(SIGCHLD, &sa, NULL) ||
        sigaction(SIG_CONVEYOR_STOP, &sa, NULL) ||
        sigaction(SIG_CONVEYOR_RESUME, &sa, NULL)) return -1;
    return 0;
}

static void child_main(int role, int pipes[PIPELINE_LINKS][2],
                       int status[2], int alert[2], const config_t *cfg) {
    struct sigaction default_action = { .sa_handler = SIG_DFL };
    sigemptyset(&default_action.sa_mask);
    sigaction(SIGINT, &default_action, NULL);
    sigaction(SIGTERM, &default_action, NULL);
    sigaction(SIGCHLD, &default_action, NULL);
    sigaction(SIG_CONVEYOR_STOP, &default_action, NULL);
    sigaction(SIG_CONVEYOR_RESUME, &default_action, NULL);

    int input = -1, output = -1, status_out = -1, alert_out = -1;
    if (role < NUM_STAGES) {
        if (role > RECEPTION) input = pipes[role - 1][0];
        if (role < PRESSING) output = pipes[role][1];
        status_out = status[1];
    } else {
        input = status[0];
        alert_out = alert[1];
    }
    for (int i = 0; i < PIPELINE_LINKS; ++i) {
        if (pipes[i][0] != input) close(pipes[i][0]);
        if (pipes[i][1] != output) close(pipes[i][1]);
    }
    if (status[0] != input) close(status[0]);
    if (status[1] != status_out) close(status[1]);
    close(alert[0]);
    if (alert[1] != alert_out) close(alert[1]);
    if (role < NUM_STAGES) {
        run_stage((stage_id_t)role, input, output, status_out, cfg);
    } else {
        run_quality(input, alert_out, cfg);
    }
    if (input >= 0) close(input);
    if (output >= 0) close(output);
    if (status_out >= 0) close(status_out);
    if (alert_out >= 0) close(alert_out);
    _exit(0);
}

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "Usage: %s config.txt\n", argv[0]); return 1; }
    config_t cfg;
    if (load_config(argv[1], &cfg)) return 1;
    if (open_log(cfg.log_file)) return 1;
    if (install_handlers()) { perror("sigaction"); close_log(); return 1; }
    /* Avoid termination if a reader disappears during shutdown. */
    struct sigaction ignore_pipe = { .sa_handler = SIG_IGN };
    sigemptyset(&ignore_pipe.sa_mask);
    if (sigaction(SIGPIPE, &ignore_pipe, NULL)) { perror("SIGPIPE"); close_log(); return 1; }

    int pipes[PIPELINE_LINKS][2], status[2], alert[2];
    for (int i = 0; i < PIPELINE_LINKS; ++i) pipes[i][0] = pipes[i][1] = -1;
    status[0] = status[1] = alert[0] = alert[1] = -1;
    for (int i = 0; i < PIPELINE_LINKS; ++i)
        if (pipe(pipes[i])) { perror("pipe"); goto setup_error; }
    if (pipe(status) || pipe(alert)) { perror("pipe"); goto setup_error; }

    pid_t children[CHILD_COUNT] = {0};
    int remaining = 0;
    int expected_batches = (int)ceil(cfg.container_kg / cfg.batch_size_kg);
    log_event("Supervisor started; container=%.2f kg, batch_size=%.2f kg (~%d batches)",
              cfg.container_kg, cfg.batch_size_kg, expected_batches);

    for (int i = 0; i < CHILD_COUNT; ++i) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); stop_requested = 1; break; }
        if (pid == 0) child_main(i, pipes, status, alert, &cfg);
        children[i] = pid;
        ++remaining;
        log_event("Created %s PID=%ld", i == NUM_STAGES ? "QC" : stage_name((stage_id_t)i), (long)pid);
    }
    for (int i = 0; i < PIPELINE_LINKS; ++i) {
        close(pipes[i][0]); close(pipes[i][1]);
    }
    close(status[0]); close(status[1]);
    close(alert[1]);
    int alert_open = 1;
    int failed = stop_requested ? 1 : 0;
    int terminating = 0;
    while (remaining > 0 || alert_open) {
        if (conveyor_signal != 0) {
            int sig = conveyor_signal;
            conveyor_signal = 0;
            if (children[CLEANING] > 0) {
                kill(children[CLEANING], sig);
                log_event("Supervisor forwarded %s to Cleaning (PID=%ld)",
                          sig == SIG_CONVEYOR_STOP ? "SIG_CONVEYOR_STOP" : "SIG_CONVEYOR_RESUME",
                          (long)children[CLEANING]);
            }
        }
        if (stop_requested && !terminating) {
            terminating = 1;
            failed = 1;
            for (int i = 0; i < CHILD_COUNT; ++i)
                if (children[i] > 0) kill(children[i], SIGTERM);
        }
        int st;
        pid_t pid;
        while ((pid = waitpid(-1, &st, WNOHANG)) > 0) {
            --remaining;
            for (int i = 0; i < CHILD_COUNT; ++i) {
                if (children[i] == pid) children[i] = 0;
            }
            log_event("Child PID=%ld exited", (long)pid);
            if ((!WIFEXITED(st) || WEXITSTATUS(st) != 0) && !terminating) {
                failed = 1;
                stop_requested = 1;
            }
        }
        if (alert_open) {
            struct pollfd pfd = { .fd = alert[0], .events = POLLIN | POLLHUP };
            int rc = poll(&pfd, 1, 100);
            if (rc > 0 && (pfd.revents & (POLLIN | POLLHUP))) {
                alert_msg_t msg;
                int got = read_full(alert[0], &msg, sizeof msg);
                if (got == 1) log_event("ALERT: %s", msg.text);
                else { close(alert[0]); alert_open = 0; if (got < 0) failed = 1; }
            } else if (rc < 0 && errno != EINTR) {
                perror("poll"); close(alert[0]); alert_open = 0; failed = 1;
            }
        } else if (remaining > 0) {
            (void)poll(NULL, 0, 10);
        }
    }
    log_event("Supervisor finished");
    close_log();
    return failed ? 1 : 0;
setup_error:
    for (int i = 0; i < PIPELINE_LINKS; ++i) {
        if (pipes[i][0] >= 0) close(pipes[i][0]);
        if (pipes[i][1] >= 0) close(pipes[i][1]);
    }
    if (status[0] >= 0) close(status[0]);
    if (status[1] >= 0) close(status[1]);
    if (alert[0] >= 0) close(alert[0]);
    if (alert[1] >= 0) close(alert[1]);
    close_log();
    return 1;
}