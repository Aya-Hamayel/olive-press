#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <signal.h>
#include <sys/types.h>

#define LOG_PATH_SIZE 128
#define ALERT_TEXT_SIZE 128
#define PIPELINE_LINKS (NUM_STAGES - 1)

/* Conveyor control signals */
#define SIG_CONVEYOR_STOP   SIGUSR1
#define SIG_CONVEYOR_RESUME SIGUSR2

typedef enum {
    RECEPTION = 0, CLEANING, WASHING, CRUSHING,
    MALAXATION, PRESSING, NUM_STAGES
} stage_id_t;

typedef enum {
    STAGE_IDLE = 0, STAGE_RUNNING, STAGE_STOPPED,
    STAGE_COMPLETED, STAGE_ERROR
} stage_state_t;

typedef enum {
    ALERT_HIGH_TEMP = 0, ALERT_LOW_TEMP,
    ALERT_HIGH_PRESSURE, ALERT_LOW_PRESSURE,
    ALERT_PROCESS_ERROR
} alert_type_t;

typedef struct {
    int batch_id;
    float kg;
} batch_msg_t;

typedef struct {
    int num_batches;
    float batch_size_kg;
    float container_kg;
    int stage_time[NUM_STAGES];
    float temp_min, temp_max;
    float press_min, press_max;
    float fault_prob;
    char log_file[LOG_PATH_SIZE];
} config_t;

typedef struct {
    stage_id_t stage;
    int batch_id;
    float temperature;
    float pressure;
    float processing_time;
    stage_state_t state;
} status_msg_t;

typedef struct {
    alert_type_t type;
    stage_id_t stage;
    int batch_id;
    char text[ALERT_TEXT_SIZE];
} alert_msg_t;

int load_config(const char *path, config_t *cfg);
void log_event(const char *fmt, ...);
int open_log(const char *path);
void close_log(void);
const char *stage_name(stage_id_t stage);
int read_full(int fd, void *buffer, size_t size);
int write_full(int fd, const void *buffer, size_t size);
void run_stage(stage_id_t stage, int in_fd, int out_fd,
               int status_fd, const config_t *cfg);
void run_quality(int status_fd, int alert_fd, const config_t *cfg);

#endif
