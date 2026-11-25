
#include "common.h"
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Conveyor state */
static volatile sig_atomic_t conveyor_stopped = 0;

/* Handle conveyor signals */
static void conveyor_signal_handler(int sig) {
    if (sig == SIG_CONVEYOR_STOP) {
        conveyor_stopped = 1;
    } else if (sig == SIG_CONVEYOR_RESUME) {
        conveyor_stopped = 0;
    }
}

/* Install Cleaning signal handlers */
static void install_cleaning_handlers(void) {
    struct sigaction sa = {0};

    sigemptyset(&sa.sa_mask);
    sa.sa_handler = conveyor_signal_handler;

    sigaction(SIG_CONVEYOR_STOP, &sa, NULL);
    sigaction(SIG_CONVEYOR_RESUME, &sa, NULL);
}

/* Return stage name */
const char *stage_name(stage_id_t s) {
    static const char *names[] = {
        "Reception",
        "Cleaning",
        "Washing",
        "Crushing",
        "Malaxation",
        "Pressing"
    };

    return s >= 0 && s < NUM_STAGES
           ? names[s] : "Unknown";
}

/* Read a complete message from a pipe */
int read_full(int fd, void *buffer, size_t size) {
    size_t done = 0;

    while (done < size) {
        ssize_t n = read(
            fd,
            (char *)buffer + done,
            size - done
        );

        if (n == 0) {
            return done == 0 ? 0 : -1;
        }

        if (n < 0) {
            if (errno == EINTR)
                continue;

            return -1;
        }

        done += (size_t)n;
    }

    return 1;
}

/* Write a complete message to a pipe */
int write_full(int fd, const void *buffer, size_t size) {
    size_t done = 0;

    while (done < size) {
        ssize_t n = write(
            fd,
            (const char *)buffer + done,
            size - done
        );

        if (n < 0) {
            if (errno == EINTR)
                continue;

            return -1;
        }

        if (n == 0)
            return -1;

        done += (size_t)n;
    }

    return 0;
}

/* Generate random values */
static float rand_range(float min_val, float max_val) {
    float scale = (float)rand() / (float)RAND_MAX;

    return min_val + scale * (max_val - min_val);
}

/* Simulate processing delay */
static void simulate_delay(int seconds) {
    struct timespec req = {
        .tv_sec = seconds,
        .tv_nsec = 0
    };

    struct timespec rem = {0};

    while (nanosleep(&req, &rem) == -1 &&
           errno == EINTR) {
        req = rem;
    }
}

/* Calculate actual elapsed time */
static double elapsed_seconds(
    const struct timespec *start,
    const struct timespec *end
) {
    return (double)(end->tv_sec - start->tv_sec)
         + (double)(end->tv_nsec - start->tv_nsec)
           / 1000000000.0;
}

/*
 * Send a status message with actual processing time.
 * Generate temperature and pressure readings.
 * Simulate random sensor faults.
 */
static int send_status_with_time(
    int fd,
    stage_id_t stage,
    int batch_id,
    stage_state_t state,
    const config_t *cfg,
    float actual_time
) {
    status_msg_t msg = {0};

    msg.stage = stage;
    msg.batch_id = batch_id;
    msg.state = state;
    msg.processing_time = actual_time;

    if (state == STAGE_RUNNING ||
        state == STAGE_COMPLETED) {

        /* Normal temperature and pressure */
        float temp_span =
            cfg->temp_max - cfg->temp_min;

        float press_span =
            cfg->press_max - cfg->press_min;

        msg.temperature = rand_range(
            cfg->temp_min + 0.1f * temp_span,
            cfg->temp_max - 0.1f * temp_span
        );

        msg.pressure = rand_range(
            cfg->press_min + 0.1f * press_span,
            cfg->press_max - 0.1f * press_span
        );

        /* Random fault simulation */
        if (rand_range(0.0f, 1.0f) < cfg->fault_prob) {

            int fault_type = rand() % 4;

            switch (fault_type) {
                case 0:
                    /* High temperature */
                    msg.temperature = cfg->temp_max + 5.0f;
                    break;

                case 1:
                    /* Low temperature */
                    msg.temperature = cfg->temp_min - 5.0f;
                    break;

                case 2:
                    /* High pressure */
                    msg.pressure = cfg->press_max + 2.0f;
                    break;

                case 3:
                    /* Low pressure */
                    msg.pressure = cfg->press_min - 0.5f;
                    break;
            }
        }
    }

    return write_full(fd, &msg, sizeof msg);
}

/*
 * Send ordinary status messages.
 * RUNNING and STOPPED messages do not contain
 * a completed processing duration.
 */
static int send_status(
    int fd,
    stage_id_t stage,
    int batch_id,
    stage_state_t state,
    const config_t *cfg
) {
    return send_status_with_time(
        fd,
        stage,
        batch_id,
        state,
        cfg,
        0.0f
    );
}

/* Reception process */
static void run_reception(
    int out_fd,
    int status_fd,
    const config_t *cfg
) {
    float remaining_kg = cfg->container_kg;

    int batch_id = 0;
    int mid_pause_done = 0;

    pid_t supervisor_pid = getppid();

    printf(
        "[Sensor] Container initialized with %.2f kg\n",
        remaining_kg
    );
    fflush(stdout);

    /* Start conveyor */
    kill(supervisor_pid, SIG_CONVEYOR_RESUME);
    usleep(50000);

    while (remaining_kg > 0.0001f) {

        /* Temporary pause at 50% */
        if (!mid_pause_done &&
            remaining_kg <= cfg->container_kg * 0.5f) {

            mid_pause_done = 1;

            printf(
                "[Sensor] Container at 50%% (%.2f kg)"
                " -> Pausing conveyor\n",
                remaining_kg
            );
            fflush(stdout);

            kill(supervisor_pid, SIG_CONVEYOR_STOP);

            (void)send_status(
                status_fd,
                RECEPTION,
                batch_id,
                STAGE_STOPPED,
                cfg
            );

            simulate_delay(1);

            printf("[Sensor] Resuming conveyor\n");
            fflush(stdout);

            kill(supervisor_pid, SIG_CONVEYOR_RESUME);
        }

        ++batch_id;

        float current_kg =
            remaining_kg >= cfg->batch_size_kg
            ? cfg->batch_size_kg
            : remaining_kg;

        batch_msg_t batch = {
            .batch_id = batch_id,
            .kg = current_kg
        };

        /* Stage started */
        if (send_status(
                status_fd,
                RECEPTION,
                batch_id,
                STAGE_RUNNING,
                cfg
            ))
            break;

        /* Measure actual processing time */
        struct timespec start, end;

        clock_gettime(CLOCK_MONOTONIC, &start);

        simulate_delay(cfg->stage_time[RECEPTION]);

        clock_gettime(CLOCK_MONOTONIC, &end);

        float actual_time =
            (float)elapsed_seconds(&start, &end);

        /* Send batch to Cleaning */
        if (write_full(out_fd, &batch, sizeof batch)) {
            fprintf(
                stderr,
                "Reception failed to send batch %d\n",
                batch_id
            );
            break;
        }

        remaining_kg -= current_kg;

        if (remaining_kg < 0.0001f)
            remaining_kg = 0.0f;

        printf(
            "[Sensor] Dispatched batch %d (%.2f kg)"
            " | Remaining: %.2f kg\n",
            batch_id,
            current_kg,
            remaining_kg
        );
        fflush(stdout);

        /* Send actual processing time to QC */
        if (send_status_with_time(
                status_fd,
                RECEPTION,
                batch_id,
                STAGE_COMPLETED,
                cfg,
                actual_time
            ))
            break;
    }

    /* Container empty */
    printf(
        "[Sensor] Container empty (0.00 kg)"
        " -> Stopping conveyor feed\n"
    );
    fflush(stdout);

    kill(supervisor_pid, SIG_CONVEYOR_STOP);

    (void)send_status(
        status_fd,
        RECEPTION,
        batch_id,
        STAGE_STOPPED,
        cfg
    );
}

/* Cleaning process */
static void run_cleaning(
    int in_fd,
    int out_fd,
    int status_fd,
    const config_t *cfg
) {
    install_cleaning_handlers();

    batch_msg_t batch;

    int last_batch_id = 0;
    int reported_stop = 0;

    while (1) {

        /*
         * Handle conveyor stop.
         * Drain queued batches to avoid deadlock.
         */
        while (conveyor_stopped) {

            if (!reported_stop) {
                reported_stop = 1;

                printf(
                    "[Cleaning] Conveyor stopped by signal\n"
                );
                fflush(stdout);

                (void)send_status(
                    status_fd,
                    CLEANING,
                    last_batch_id,
                    STAGE_STOPPED,
                    cfg
                );
            }

            struct pollfd pfd = {
                .fd = in_fd,
                .events = POLLIN | POLLHUP
            };

            int prc = poll(&pfd, 1, 100);

            if (prc < 0) {
                if (errno == EINTR)
                    continue;

                perror("Cleaning poll");
                return;
            }

            /* No queued data and upstream closed */
            if (prc > 0 &&
                (pfd.revents & POLLHUP) &&
                !(pfd.revents & POLLIN)) {
                return;
            }

            /* Process already queued batches */
            if (prc > 0 &&
                (pfd.revents & POLLIN)) {
                break;
            }
        }

        if (reported_stop) {
            reported_stop = 0;

            printf(
                "[Cleaning] Conveyor resumed/processing queued data\n"
            );
            fflush(stdout);
        }

        int rc = read_full(
            in_fd,
            &batch,
            sizeof batch
        );

        if (rc == 0)
            break;

        if (rc < 0) {
            fprintf(
                stderr,
                "Cleaning: pipe read error\n"
            );
            break;
        }

        last_batch_id = batch.batch_id;

        /* Cleaning started */
        if (send_status(
                status_fd,
                CLEANING,
                batch.batch_id,
                STAGE_RUNNING,
                cfg
            ))
            break;

        /* Measure actual processing time */
        struct timespec start, end;

        clock_gettime(CLOCK_MONOTONIC, &start);

        simulate_delay(cfg->stage_time[CLEANING]);

        clock_gettime(CLOCK_MONOTONIC, &end);

        float actual_time =
            (float)elapsed_seconds(&start, &end);

        /* Forward batch */
        if (write_full(out_fd, &batch, sizeof batch))
            break;

        /* Cleaning completed */
        if (send_status_with_time(
                status_fd,
                CLEANING,
                batch.batch_id,
                STAGE_COMPLETED,
                cfg,
                actual_time
            ))
            break;
    }
}

/* Run stages */
void run_stage(
    stage_id_t stage,
    int in_fd,
    int out_fd,
    int status_fd,
    const config_t *cfg
) {
    srand(
        (unsigned int)(time(NULL) ^ (getpid() << 16))
    );

    if (stage == RECEPTION) {

        run_reception(out_fd, status_fd, cfg);

    } else if (stage == CLEANING) {

        run_cleaning(in_fd, out_fd, status_fd, cfg);

    } else {

        batch_msg_t batch;
        int rc;

        while ((rc = read_full(
                    in_fd,
                    &batch,
                    sizeof batch
                )) == 1) {

            /* Stage started */
            if (send_status(
                    status_fd,
                    stage,
                    batch.batch_id,
                    STAGE_RUNNING,
                    cfg
                ))
                break;

            /* Measure actual processing time */
            struct timespec start, end;

            clock_gettime(CLOCK_MONOTONIC, &start);

          
        simulate_delay(cfg->stage_time[stage]);
            clock_gettime(CLOCK_MONOTONIC, &end);

            float actual_time =
                (float)elapsed_seconds(&start, &end);

            /* Forward batch to next stage */
            if (out_fd >= 0 &&
                write_full(out_fd, &batch, sizeof batch))
                break;

            /* Report completed stage */
            if (send_status_with_time(
                    status_fd,
                    stage,
                    batch.batch_id,
                    STAGE_COMPLETED,
                    cfg,
                    actual_time
                ))
                break;
        }

        if (rc < 0) {
            fprintf(
                stderr,
                "%s: pipe read error\n",
                stage_name(stage)
            );
        }
    }
}
