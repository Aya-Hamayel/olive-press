#include "dashboard_ipc.h"
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <limits.h>

static int dashboard_fd = -1;

_Static_assert(sizeof(dashboard_msg_t) <= PIPE_BUF,
               "Dashboard message exceeds PIPE_BUF");

void dashboard_close(void)
{
    if (dashboard_fd >= 0) {
        close(dashboard_fd);
        dashboard_fd = -1;
    }
}

static void dashboard_send(const dashboard_msg_t *msg)
{
    if (dashboard_fd < 0) {
        dashboard_fd = open(
            DASHBOARD_FIFO,
            O_WRONLY | O_NONBLOCK | O_NOFOLLOW
        );

        if (dashboard_fd < 0)
            return;

        struct stat st;

        if (fstat(dashboard_fd, &st) < 0 ||
            !S_ISFIFO(st.st_mode)) {
            dashboard_close();
            return;
        }
    }

    ssize_t result;

    do {
        result = write(
            dashboard_fd,
            msg,
            sizeof(*msg)
        );
    } while (result < 0 && errno == EINTR);

    if (result < 0 && errno == EAGAIN)
        return;

    if (result != (ssize_t)sizeof(*msg))
        dashboard_close();
}

void dashboard_publish_status(const status_msg_t *msg)
{
    dashboard_msg_t packet = {0};

    packet.type = DASHBOARD_STATUS;
    packet.status = *msg;

    dashboard_send(&packet);
}

void dashboard_publish_alert(const alert_msg_t *msg)
{
    dashboard_msg_t packet = {0};

    packet.type = DASHBOARD_ALERT;
    packet.alert = *msg;

    dashboard_send(&packet);
}