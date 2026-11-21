#include "common.h"
#include <stdio.h>
#include <string.h>

void run_quality(int status_fd, int alert_fd, const config_t *cfg) {
    (void)cfg;
    status_msg_t msg;
    int rc;
    while ((rc = read_full(status_fd, &msg, sizeof msg)) == 1) {
        printf("[QC] %s batch %d: %s\n", stage_name(msg.stage),
               msg.batch_id,
               msg.state == STAGE_COMPLETED ? "completed" : "running");
        fflush(stdout);
        /* Measurements are not generated yet: do not validate zero placeholders. */
    }
    if (rc < 0) {
        alert_msg_t alert = { .type = ALERT_PROCESS_ERROR, .stage = RECEPTION,
                              .batch_id = -1 };
        snprintf(alert.text, sizeof alert.text, "Quality Control: truncated status message");
        (void)write_full(alert_fd, &alert, sizeof alert);
    }
}
