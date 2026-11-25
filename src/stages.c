#include "common.h"
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t conveyor_stopped = 0;

static void conveyor_signal_handler(int sig) {
    if (sig == SIG_CONVEYOR_STOP) {
        conveyor_stopped = 1;
    } else if (sig == SIG_CONVEYOR_RESUME) {
        conveyor_stopped = 0;
    }
}

static void install_cleaning_handlers(void) {
    struct sigaction sa = {0};
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = conveyor_signal_handler;
    sigaction(SIG_CONVEYOR_STOP, &sa, NULL);
    sigaction(SIG_CONVEYOR_RESUME, &sa, NULL);
}

const char *stage_name(stage_id_t s) {
    static const char *names[] = {
        "Reception", "Cleaning", "Washing", "Crushing", "Malaxation", "Pressing"
    };
    return s >= 0 && s < NUM_STAGES ? names[s] : "Unknown";
}

int read_full(int fd, void *buffer, size_t size) {
    size_t done = 0;
    while (done < size) {
        ssize_t n = read(fd, (char *)buffer + done, size - done);
        if (n == 0) return done == 0 ? 0 : -1;
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        done += (size_t)n;
    }
    return 1;
}

int write_full(int fd, const void *buffer, size_t size) {
    size_t done = 0;
    while (done < size) {
        ssize_t n = write(fd, (const char *)buffer + done, size - done);
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        if (n == 0) return -1;
        done += (size_t)n;
    }
    return 0;
}

static float rand_range(float min_val, float max_val) {
    float scale = (float)rand() / (float)RAND_MAX;
    return min_val + scale * (max_val - min_val);
}

static void simulate_delay(int seconds) {
    struct timespec req = { .tv_sec = seconds, .tv_nsec = 0 };
    struct timespec rem = {0};
    while (nanosleep(&req, &rem) == -1 && errno == EINTR) {
        req = rem;
    }
}

static int send_status(int fd, stage_id_t stage, int batch_id,
                       stage_state_t state, const config_t *cfg) {
    status_msg_t msg = {0};
    msg.stage = stage;
    msg.batch_id = batch_id;
    msg.state = state;
    msg.processing_time = (float)cfg->stage_time[stage];

    if (state == STAGE_RUNNING || state == STAGE_COMPLETED) {
        /* Simulate realistic temperature and pressure readings within configured ranges */
        float temp_span = cfg->temp_max - cfg->temp_min;
        float press_span = cfg->press_max - cfg->press_min;
        msg.temperature = rand_range(cfg->temp_min + 0.1f * temp_span,
                                     cfg->temp_max - 0.1f * temp_span);
        msg.pressure = rand_range(cfg->press_min + 0.1f * press_span,
                                  cfg->press_max - 0.1f * press_span);
    }
    return write_full(fd, &msg, sizeof msg);
}

static void run_reception(int out_fd, int status_fd, const config_t *cfg) {
    float remaining_kg = cfg->container_kg;
    int batch_id = 0;
    int mid_pause_done = 0;
    pid_t supervisor_pid = getppid();

    printf("[Sensor] Container initialized with %.2f kg\n", remaining_kg);
    fflush(stdout);

    /* Ensure conveyor starts in resumed state */
    kill(supervisor_pid, SIG_CONVEYOR_RESUME);
    usleep(50000); /* Allow supervisor & cleaning to register signal */

    while (remaining_kg > 0.0001f) {
        /* Simulate a mid-run conveyor pause/resume cycle to verify signal flow */
        if (!mid_pause_done && remaining_kg <= cfg->container_kg * 0.5f) {
            mid_pause_done = 1;
            printf("[Sensor] Container at 50%% (%.2f kg) -> Pausing conveyor\n", remaining_kg);
            fflush(stdout);
            kill(supervisor_pid, SIG_CONVEYOR_STOP);
            (void)send_status(status_fd, RECEPTION, batch_id, STAGE_STOPPED, cfg);
            simulate_delay(1);
            printf("[Sensor] Resuming conveyor\n");
            fflush(stdout);
            kill(supervisor_pid, SIG_CONVEYOR_RESUME);
        }

        ++batch_id;
        float current_kg = (remaining_kg >= cfg->batch_size_kg)
                           ? cfg->batch_size_kg
                           : remaining_kg;

        batch_msg_t batch = { .batch_id = batch_id, .kg = current_kg };

        if (send_status(status_fd, RECEPTION, batch_id, STAGE_RUNNING, cfg)) break;
        simulate_delay(cfg->stage_time[RECEPTION]);

        if (write_full(out_fd, &batch, sizeof batch)) {
            fprintf(stderr, "Reception failed to send batch %d\n", batch_id);
            break;
        }

        remaining_kg -= current_kg;
        if (remaining_kg < 0.0001f) remaining_kg = 0.0f;

        printf("[Sensor] Dispatched batch %d (%.2f kg) | Remaining in container: %.2f kg\n",
               batch_id, current_kg, remaining_kg);
        fflush(stdout);

        if (send_status(status_fd, RECEPTION, batch_id, STAGE_COMPLETED, cfg)) break;
    }

    /* Container empty sensor triggers conveyor stop */
    printf("[Sensor] Container empty (0.00 kg) -> Stopping conveyor feed\n");
    fflush(stdout);
    kill(supervisor_pid, SIG_CONVEYOR_STOP);
    (void)send_status(status_fd, RECEPTION, batch_id, STAGE_STOPPED, cfg);
}

static void run_cleaning(int in_fd, int out_fd, int status_fd, const config_t *cfg) {
    install_cleaning_handlers();
    batch_msg_t batch;
    int last_batch_id = 0;
    int reported_stop = 0;

    while (1) {
        /* If conveyor is paused, wait until resumed OR until pipe closes (container empty) */
        while (conveyor_stopped) {
            if (!reported_stop) {
                reported_stop = 1;
                printf("[Cleaning] Conveyor stopped by signal\n");
                fflush(stdout);
                (void)send_status(status_fd, CLEANING, last_batch_id, STAGE_STOPPED, cfg);
            }
            struct pollfd pfd = { .fd = in_fd, .events = POLLIN | POLLHUP };
            int prc = poll(&pfd, 1, 100);
            if (prc > 0 && (pfd.revents & POLLHUP) && !(pfd.revents & POLLIN)) {
                /* Upstream closed pipe (container is empty); drain pipeline and exit */
                return;
            }
        }

        if (reported_stop) {
            reported_stop = 0;
            printf("[Cleaning] Conveyor resumed by signal\n");
            fflush(stdout);
        }

        int rc = read_full(in_fd, &batch, sizeof batch);
        if (rc == 0) break; /* Normal EOF: all batches processed */
        if (rc < 0) {
            fprintf(stderr, "Cleaning: pipe read error\n");
            break;
        }

        last_batch_id = batch.batch_id;
        if (send_status(status_fd, CLEANING, batch.batch_id, STAGE_RUNNING, cfg)) break;
        simulate_delay(cfg->stage_time[CLEANING]);
        if (write_full(out_fd, &batch, sizeof batch)) break;
        if (send_status(status_fd, CLEANING, batch.batch_id, STAGE_COMPLETED, cfg)) break;
    }
}

void run_stage(stage_id_t stage, int in_fd, int out_fd,
               int status_fd, const config_t *cfg) {
    srand((unsigned int)(time(NULL) ^ (getpid() << 16)));

    if (stage == RECEPTION) {
        run_reception(out_fd, status_fd, cfg);
    } else if (stage == CLEANING) {
        run_cleaning(in_fd, out_fd, status_fd, cfg);
    } else {
        batch_msg_t batch;
        int rc;
        while ((rc = read_full(in_fd, &batch, sizeof batch)) == 1) {
            if (send_status(status_fd, stage, batch.batch_id, STAGE_RUNNING, cfg)) break;
            simulate_delay(cfg->stage_time[stage]);
            if (out_fd >= 0 && write_full(out_fd, &batch, sizeof batch)) break;
            if (send_status(status_fd, stage, batch.batch_id, STAGE_COMPLETED, cfg)) break;
        }
        if (rc < 0) fprintf(stderr, "%s: pipe read error\n", stage_name(stage));
    }
}