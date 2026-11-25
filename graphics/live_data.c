
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include "live_data.h"
#include "graphics.h"

#define MAX_EVENTS 100
#define EVENT_LENGTH 180

static int fifo_fd = -1;

static status_msg_t stageData[NUM_STAGES];

static char latestAlert[ALERT_TEXT_SIZE] =
    "No alerts received";

static int alertCount = 0;
static int completedBatches = 0;
static int lastCompletedBatch = 0;

static char eventLog[MAX_EVENTS][EVENT_LENGTH];
static int eventAlerts[MAX_EVENTS];

static int eventHead = 0;
static int eventCount = 0;

static unsigned char receiveBuffer[
    sizeof(dashboard_msg_t)
];

static size_t receivedBytes = 0;

static const char *stageNames[] = {
    "Reception",
    "Cleaning",
    "Washing",
    "Crushing",
    "Malaxation",
    "Pressing"
};

static const char *stateNames[] = {
    "IDLE",
    "RUNNING",
    "STOPPED",
    "COMPLETED",
    "ERROR"
};

static void addEvent(const char *message, int isAlert)
{
    snprintf(
        eventLog[eventHead],
        EVENT_LENGTH,
        "%s",
        message
    );

    eventAlerts[eventHead] = isAlert;

    eventHead = (eventHead + 1) % MAX_EVENTS;

    if (eventCount < MAX_EVENTS)
        eventCount++;
}

int getEventCount(void)
{
    return eventCount;
}

const char *getEvent(int index)
{
    if (index < 0 || index >= eventCount)
        return "";

    int position =
        (eventHead - 1 - index + MAX_EVENTS)
        % MAX_EVENTS;

    return eventLog[position];
}

int isAlertEvent(int index)
{
    if (index < 0 || index >= eventCount)
        return 0;

    int position =
        (eventHead - 1 - index + MAX_EVENTS)
        % MAX_EVENTS;

    return eventAlerts[position];
}

int getCompletedBatches(void)
{
    return completedBatches;
}

int getActiveStages(void)
{
    int count = 0;

    for (int i = 0; i < NUM_STAGES; i++) {

        if (getStageStatus(i) == STAGE_RUNNING)
            count++;
    }

    return count;
}

int initLiveData(void)
{
    if (mkfifo(DASHBOARD_FIFO, 0600) < 0 &&
        errno != EEXIST) {

        perror("mkfifo");
        return -1;
    }

    fifo_fd = open(
        DASHBOARD_FIFO,
        O_RDWR | O_NONBLOCK | O_NOFOLLOW
    );

    if (fifo_fd < 0) {
        perror("open FIFO");
        return -1;
    }

    struct stat st;

    if (fstat(fifo_fd, &st) < 0 ||
        !S_ISFIFO(st.st_mode)) {

        closeLiveData();
        return -1;
    }

    printf("[GUI] Dashboard IPC ready\n");

    return 0;
}

static void processMessage(
    const dashboard_msg_t *msg)
{
    if (msg->type == DASHBOARD_STATUS) {

        int id = msg->status.stage;

        if (id >= 0 && id < NUM_STAGES &&
            msg->status.state >= STAGE_IDLE &&
            msg->status.state <= STAGE_ERROR) {

            stageData[id] = msg->status;

            updateStageStatus(
                id,
                msg->status.state
            );

            char event[EVENT_LENGTH];

            snprintf(
                event,
                sizeof(event),
                "%s | Batch %d | %s",
                stageNames[id],
                msg->status.batch_id,
                stateNames[msg->status.state]
            );

            addEvent(event, 0);

            if (id == PRESSING &&
                msg->status.state == STAGE_COMPLETED &&
                msg->status.batch_id > lastCompletedBatch) {

                completedBatches++;
                lastCompletedBatch =
                    msg->status.batch_id;
            }

            printf(
                "[GUI] Stage %d | Batch %d | State %d\n",
                id,
                msg->status.batch_id,
                msg->status.state
            );
        }

    } else if (msg->type == DASHBOARD_ALERT) {

        alertCount++;

        snprintf(
            latestAlert,
            sizeof(latestAlert),
            "%.*s",
            ALERT_TEXT_SIZE - 1,
            msg->alert.text
        );

        addEvent(latestAlert, 1);

        printf("[GUI ALERT] %s\n", latestAlert);
    }
}

void pollLiveData(void)
{
    if (fifo_fd < 0)
        return;

    for (int i = 0; i < 128; i++) {

        ssize_t n = read(
            fifo_fd,
            receiveBuffer + receivedBytes,
            sizeof(receiveBuffer) - receivedBytes
        );

        if (n > 0) {

            receivedBytes += (size_t)n;

            if (receivedBytes ==
                sizeof(receiveBuffer)) {

                dashboard_msg_t msg;

                memcpy(
                    &msg,
                    receiveBuffer,
                    sizeof(msg)
                );

                processMessage(&msg);

                receivedBytes = 0;
            }

        } else if (n < 0 && errno == EINTR) {

            continue;

        } else {

            break;
        }
    }
}

const status_msg_t *getLiveStageData(int stageId)
{
    if (stageId < 0 || stageId >= NUM_STAGES)
        return NULL;

    return &stageData[stageId];
}

const char *getLatestAlert(void)
{
    return latestAlert;
}

int getAlertCount(void)
{
    return alertCount;
}

void closeLiveData(void)
{
    if (fifo_fd >= 0) {

        close(fifo_fd);
        fifo_fd = -1;
    }

    receivedBytes = 0;
}
