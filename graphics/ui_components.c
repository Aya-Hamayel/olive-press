
#include <GL/glut.h>
#include <stdio.h>

#include "ui_components.h"
#include "graphics.h"
#include "machine_icons.h"
#include "live_data.h"

static const char *stageNames[NUM_STAGES] = {
    "RECEPTION",
    "CLEANING",
    "WASHING",
    "CRUSHING",
    "MALAXATION",
    "PRESSING"
};

void drawText(float x, float y, const char *text)
{
    glColor3f(0.92f, 0.95f, 0.96f);
    glRasterPos2f(x, y);

    while (*text) {

        glutBitmapCharacter(
            GLUT_BITMAP_HELVETICA_18,
            (unsigned char)*text++
        );
    }
}

static void drawSmallText(
    float x, float y, const char *text)
{
    glColor3f(0.72f, 0.82f, 0.87f);
    glRasterPos2f(x, y);

    while (*text) {

        glutBitmapCharacter(
            GLUT_BITMAP_HELVETICA_12,
            (unsigned char)*text++
        );
    }
}

static void drawStatusText(
    float x, float y,
    const char *text,
    float r, float g, float b)
{
    glColor3f(r, g, b);
    glRasterPos2f(x, y);

    while (*text) {

        glutBitmapCharacter(
            GLUT_BITMAP_HELVETICA_12,
            (unsigned char)*text++
        );
    }
}

void drawRectangle(
    float x, float y,
    float w, float h,
    float r, float g, float b)
{
    glColor3f(r, g, b);

    glBegin(GL_QUADS);

        glVertex2f(x, y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h);
        glVertex2f(x, y + h);

    glEnd();
}

static void drawStage(int id, float x, float y)
{
    StageStatus status = getStageStatus(id);

    const status_msg_t *data =
        getLiveStageData(id);

    float r = 0.83f;
    float g = 0.63f;
    float b = 0.29f;

    const char *statusText = "IDLE";

    switch (status) {

        case STAGE_RUNNING:

            r = 0.18f;
            g = 0.68f;
            b = 0.46f;

            statusText = "RUNNING";
            break;

        case STAGE_STOPPED:

            r = 0.53f;
            g = 0.59f;
            b = 0.65f;

            statusText = "STOPPED";
            break;

        case STAGE_COMPLETED:

            r = 0.25f;
            g = 0.55f;
            b = 0.95f;

            statusText = "COMPLETED";
            break;

        case STAGE_ERROR:

            r = 0.89f;
            g = 0.35f;
            b = 0.39f;

            statusText = "ERROR";
            break;

        case STAGE_IDLE:
        default:
            break;
    }

    drawRectangle(
        x, y, 280, 95,
        0.09f, 0.16f, 0.19f
    );

    drawRectangle(
        x, y, 5, 95,
        r, g, b
    );

    drawText(
        x + 15,
        y + 72,
        stageNames[id]
    );

    drawStatusText(
        x + 15,
        y + 49,
        statusText,
        r, g, b
    );

    drawMachineIcon(
        id,
        x + 245,
        y + 65
    );

    char buffer[100];

    if (data != NULL && data->batch_id > 0) {

        if (status == STAGE_RUNNING ||
            status == STAGE_COMPLETED) {

            snprintf(
                buffer,
                sizeof(buffer),
                "Batch: %d | T: %.1f | P: %.1f",
                data->batch_id,
                data->temperature,
                data->pressure
            );

        } else {

            snprintf(
                buffer,
                sizeof(buffer),
                "Batch: %d | T: -- | P: --",
                data->batch_id
            );
        }

        drawSmallText(
            x + 15,
            y + 27,
            buffer
        );

        if (status == STAGE_COMPLETED) {

            snprintf(
                buffer,
                sizeof(buffer),
                "Processing Time: %.2f s",
                data->processing_time
            );

        } else {

            snprintf(
                buffer,
                sizeof(buffer),
                "Processing Time: --"
            );
        }

        drawSmallText(
            x + 15,
            y + 7,
            buffer
        );

    } else {

        drawSmallText(
            x + 15,
            y + 27,
            "Waiting for data..."
        );

        drawSmallText(
            x + 15,
            y + 7,
            "Processing Time: --"
        );
    }
}

static void drawArrow(
    float x1, float y1,
    float x2, float y2)
{
    glColor3f(0.45f, 0.65f, 0.58f);
    glLineWidth(3.0f);

    glBegin(GL_LINES);

        glVertex2f(x1, y1);
        glVertex2f(x2, y2);

    glEnd();

    glBegin(GL_TRIANGLES);

        if (x2 > x1) {

            glVertex2f(x2, y2);
            glVertex2f(x2 - 12, y2 + 7);
            glVertex2f(x2 - 12, y2 - 7);

        } else if (x2 < x1) {

            glVertex2f(x2, y2);
            glVertex2f(x2 + 12, y2 + 7);
            glVertex2f(x2 + 12, y2 - 7);

        } else if (y2 < y1) {

            glVertex2f(x2, y2);
            glVertex2f(x2 - 7, y2 + 12);
            glVertex2f(x2 + 7, y2 + 12);
        }

    glEnd();

    glLineWidth(1.0f);
}

void drawPipeline(void)
{
    drawStage(0, 65, 370);
    drawStage(1, 460, 370);
    drawStage(2, 855, 370);

    drawStage(5, 65, 215);
    drawStage(4, 460, 215);
    drawStage(3, 855, 215);

    drawArrow(345, 417, 455, 417);
    drawArrow(740, 417, 850, 417);

    drawArrow(995, 365, 995, 315);

    drawArrow(855, 262, 745, 262);
    drawArrow(460, 262, 350, 262);
}
