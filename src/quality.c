
#include "common.h"
#include <stdio.h>
#include <string.h>

/* Send an alert from Quality Control to Supervisor */
static void send_alert(int fd, alert_type_t type,
                       const status_msg_t *msg,
                       const char *description) {

    alert_msg_t alert = {0};

    alert.type = type;
    alert.stage = msg->stage;
    alert.batch_id = msg->batch_id;

    snprintf(alert.text, sizeof(alert.text),
             "%s - Batch %d: %s",
             stage_name(msg->stage),
             msg->batch_id,
             description);

    if (write_full(fd, &alert, sizeof(alert)) != 0) {
        fprintf(stderr, "[QC] Failed to send alert\n");
    }
}

/* Monitor temperature and pressure readings */
static void check_sensors(int alert_fd,
                          const status_msg_t *msg,
                          const config_t *cfg) {

    if (msg->temperature > cfg->temp_max) {
        send_alert(alert_fd, ALERT_HIGH_TEMP, msg,
                   "Temperature above maximum");

    } else if (msg->temperature < cfg->temp_min) {
        send_alert(alert_fd, ALERT_LOW_TEMP, msg,
                   "Temperature below minimum");
    }

    if (msg->pressure > cfg->press_max) {
        send_alert(alert_fd, ALERT_HIGH_PRESSURE, msg,
                   "Pressure above maximum");

    } else if (msg->pressure < cfg->press_min) {
        send_alert(alert_fd, ALERT_LOW_PRESSURE, msg,
                   "Pressure below minimum");
    }
}

/* Compare actual processing time with expected time */
static void check_processing_time(int alert_fd,
                                  const status_msg_t *msg,
                                  const config_t *cfg) {

    float expected_time = (float)cfg->stage_time[msg->stage];
    float actual_time = msg->processing_time;

    /* Allow a tolerance for normal scheduling delays */
    float tolerance = expected_time * 0.20f;

    if (tolerance < 0.1f) {
        tolerance = 0.1f;
    }

    float max_allowed_time = expected_time + tolerance;

    printf("[QC TIME] %s Batch %d: "
           "Expected = %.2f s, Actual = %.2f s\n",
           stage_name(msg->stage),
           msg->batch_id,
           expected_time,
           actual_time);

    fflush(stdout);

    if (actual_time > max_allowed_time) {

        char description[ALERT_TEXT_SIZE];

        snprintf(description, sizeof(description),
                 "Processing delay: expected %.2f s, "
                 "actual %.2f s",
                 expected_time,
                 actual_time);

        send_alert(alert_fd, ALERT_PROCESS_ERROR,
                   msg, description);
    }
}

/* Main Quality Control process */
void run_quality(int status_fd, int alert_fd,
                 const config_t *cfg) {

    status_msg_t msg;
    int rc;

    while ((rc = read_full(status_fd, &msg,
                           sizeof(msg))) == 1) {

        /* Validate stage identifier */
        if (msg.stage < RECEPTION || msg.stage >= NUM_STAGES) {
            fprintf(stderr, "[QC] Invalid stage ID\n");

            send_alert(alert_fd, ALERT_PROCESS_ERROR,
                       &msg, "Invalid stage ID");
            continue;
        }

        const char *state_name = "unknown";

        switch (msg.state) {

            case STAGE_IDLE:
                state_name = "idle";
                break;

            case STAGE_RUNNING:
                state_name = "running";
                break;

            case STAGE_STOPPED:
                state_name = "stopped";
                break;

            case STAGE_COMPLETED:
                state_name = "completed";
                break;

            case STAGE_ERROR:
                state_name = "error";
                break;
        }

        printf("[QC] %s batch %d: %s\n",
               stage_name(msg.stage),
               msg.batch_id,
               state_name);

        fflush(stdout);

        /* Check sensor readings for active stages */
        if (msg.state == STAGE_RUNNING ||
            msg.state == STAGE_COMPLETED) {

            check_sensors(alert_fd, &msg, cfg);
        }

        /* Check actual processing time after completion */
        if (msg.state == STAGE_COMPLETED) {

            check_processing_time(alert_fd, &msg, cfg);
        }

        /* Detect stage errors */
        if (msg.state == STAGE_ERROR) {

            send_alert(alert_fd, ALERT_PROCESS_ERROR,
                       &msg, "Production stage reported an error");
        }

        /* STAGE_STOPPED is not necessarily a fault.
           The conveyor may be temporarily paused. */
    }

    /* Handle incomplete or corrupted pipe messages */
    if (rc < 0) {

        alert_msg_t alert = {
            .type = ALERT_PROCESS_ERROR,
            .stage = RECEPTION,
            .batch_id = -1
        };

        snprintf(alert.text, sizeof(alert.text),
                 "Quality Control: truncated status message");

        (void)write_full(alert_fd, &alert, sizeof(alert));
    }
}
