#include "common.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

static int send_status(int fd, stage_id_t stage, int batch_id, stage_state_t state) {
    status_msg_t msg = {0};
    msg.stage = stage;
    msg.batch_id = batch_id;
    msg.state = state;
    /* No fabricated sensor measurements in this foundation. */
    return write_full(fd, &msg, sizeof msg);
}

void run_stage(stage_id_t stage, int in_fd, int out_fd,
               int status_fd, const config_t *cfg) {
    if (stage == RECEPTION) {
        for (int i = 1; i <= cfg->num_batches; ++i) {
            batch_msg_t batch = { .batch_id = i, .kg = cfg->batch_size_kg };
            if (send_status(status_fd, stage, i, STAGE_RUNNING) ||
                write_full(out_fd, &batch, sizeof batch) ||
                send_status(status_fd, stage, i, STAGE_COMPLETED)) {
                fprintf(stderr, "Reception failed to send batch %d\n", i);
                break;
            }
        }
    } else {
        batch_msg_t batch;
        int rc;
        while ((rc = read_full(in_fd, &batch, sizeof batch)) == 1) {
            if (send_status(status_fd, stage, batch.batch_id, STAGE_RUNNING)) break;
            if (out_fd >= 0 && write_full(out_fd, &batch, sizeof batch)) break;
            if (send_status(status_fd, stage, batch.batch_id, STAGE_COMPLETED)) break;
        }
        if (rc < 0) fprintf(stderr, "%s: pipe read error\n", stage_name(stage));
    }
}
