#ifndef DASHBOARD_IPC_H
#define DASHBOARD_IPC_H

#include "common.h"

#define DASHBOARD_FIFO "./dashboard.fifo"

typedef enum {
    DASHBOARD_STATUS = 1,
    DASHBOARD_ALERT = 2
} dashboard_msg_type_t;

typedef struct {
    dashboard_msg_type_t type;
    status_msg_t status;
    alert_msg_t alert;
} dashboard_msg_t;

void dashboard_publish_status(const status_msg_t *msg);
void dashboard_publish_alert(const alert_msg_t *msg);
void dashboard_close(void);

#endif