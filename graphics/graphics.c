#include <GL/glut.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "ui_components.h"
#include "graphics.h"
#include "animation.h"
#include "live_data.h"

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 850

static int lastTime = 0;
static int totalBatches = 0;

static void loadProductionConfig(const char *configPath)
{
    FILE *file = fopen(configPath, "r");

    if (file == NULL)
        return;

    char line[256];
    float container = 0.0f;
    float batch = 0.0f;

    while (fgets(line, sizeof(line), file)) {

        float value;

        if (sscanf(line, " container_kg = %f",
                   &value) == 1) {
            container = value;
        }

        if (sscanf(line, " batch_size_kg = %f",
                   &value) == 1) {
            batch = value;
        }
    }

    fclose(file);

    if (isfinite(container) && isfinite(batch) &&
        container > 0 && batch > 0) {

        double count = ceil((double)container / batch);

        if (count <= 2147483647.0)
            totalBatches = (int)count;
    }
}

static void drawColoredText(
    float x, float y,
    const char *text,
    void *font,
    float r, float g, float b)
{
    glColor3f(r, g, b);
    glRasterPos2f(x, y);

    while (*text) {
        glutBitmapCharacter(
            font, (unsigned char)*text++);
    }
}

static void drawStatCard(
    float x, const char *label,
    int value, float r, float g, float b)
{
    char buffer[32];

    drawRectangle(x, 670, 265, 80,
                  0.09f, 0.15f, 0.19f);

    drawRectangle(x, 670, 4, 80, r, g, b);

    drawColoredText(
        x + 18, 724, label,
        GLUT_BITMAP_HELVETICA_12,
        0.62f, 0.72f, 0.77f);

    if (value < 0)
        snprintf(buffer, sizeof(buffer), "--");
    else
        snprintf(buffer, sizeof(buffer), "%02d", value);

    drawColoredText(
        x + 18, 690, buffer,
        GLUT_BITMAP_TIMES_ROMAN_24,
        r, g, b);
}

static void drawStatistics(void)
{
    drawStatCard(
        40, "TOTAL BATCHES",
        totalBatches > 0 ? totalBatches : -1,
        0.90f, 0.75f, 0.45f);

    drawStatCard(
        325, "COMPLETED",
        getCompletedBatches(),
        0.30f, 0.82f, 0.56f);

    drawStatCard(
        610, "ACTIVE STAGES",
        getActiveStages(),
        0.35f, 0.65f, 0.95f);

    drawStatCard(
        895, "QC ALERTS",
        getAlertCount(),
        0.90f, 0.65f, 0.30f);
}

static void drawProgress(void)
{
    float progress = 0.0f;

    if (totalBatches > 0) {
        progress =
            (float)getCompletedBatches()
            / totalBatches;
    }

    if (progress > 1.0f)
        progress = 1.0f;

    char buffer[80];

    if (totalBatches > 0) {
        snprintf(buffer, sizeof(buffer),
                 "PRODUCTION PROGRESS: %.1f%%",
                 progress * 100.0f);
    } else {
        snprintf(buffer, sizeof(buffer),
                 "PRODUCTION PROGRESS: N/A");
    }

    drawColoredText(
        40, 640, buffer,
        GLUT_BITMAP_HELVETICA_12,
        0.75f, 0.82f, 0.86f);

    drawRectangle(
        40, 615, 1120, 12,
        0.15f, 0.22f, 0.26f);

    drawRectangle(
        40, 615, 1120 * progress, 12,
        0.25f, 0.75f, 0.48f);
}

static void drawEventPanel(void)
{
    drawRectangle(
        40, 35, 1120, 140,
        0.075f, 0.12f, 0.16f);

    drawRectangle(
        40, 35, 4, 140,
        0.83f, 0.63f, 0.29f);

    drawColoredText(
        60, 150, "SYSTEM EVENT LOG",
        GLUT_BITMAP_HELVETICA_18,
        0.90f, 0.75f, 0.45f);

    if (getEventCount() == 0) {

        drawColoredText(
            60, 120, "Waiting for production events...",
            GLUT_BITMAP_HELVETICA_12,
            0.65f, 0.72f, 0.76f);

        return;
    }

    int count = getEventCount();

    if (count > 4)
        count = 4;

    for (int i = 0; i < count; i++) {

        int alert = isAlertEvent(i);

        drawColoredText(
            60, 122 - i * 23,
            getEvent(i),
            GLUT_BITMAP_HELVETICA_12,
            alert ? 0.95f : 0.70f,
            alert ? 0.68f : 0.82f,
            alert ? 0.25f : 0.85f);
    }
}

void animationTimer(int value)
{
    (void)value;

    int currentTime =
        glutGet(GLUT_ELAPSED_TIME);

    float deltaTime =
        (currentTime - lastTime) / 1000.0f;

    lastTime = currentTime;

    pollLiveData();
    updateAnimation(deltaTime);

    glutPostRedisplay();
    glutTimerFunc(16, animationTimer, 0);
}

void display(void)
{
    glClearColor(
        0.035f, 0.06f, 0.085f, 1.0f);

    glClear(GL_COLOR_BUFFER_BIT);

    drawRectangle(
        0, 775, 1200, 75,
        0.065f, 0.11f, 0.15f);

    drawColoredText(
        40, 817,
        "OLIVE PRESS CONTROL SYSTEM",
        GLUT_BITMAP_HELVETICA_18,
        0.95f, 0.97f, 0.98f);

    drawColoredText(
        40, 790,
        "ENCS4330 - REAL-TIME APPLICATIONS",
        GLUT_BITMAP_HELVETICA_12,
        0.58f, 0.70f, 0.76f);

    drawRectangle(
        940, 795, 220, 35,
        0.08f, 0.25f, 0.18f);

    drawColoredText(
        970, 807, "LIVE MONITORING",
        GLUT_BITMAP_HELVETICA_18,
        0.35f, 0.85f, 0.58f);

    drawStatistics();
    drawProgress();

    drawColoredText(
        40, 585, "PRODUCTION PIPELINE",
        GLUT_BITMAP_HELVETICA_18,
        0.90f, 0.75f, 0.45f);

    drawRectangle(
        40, 190, 1120, 365,
        0.065f, 0.11f, 0.15f);

    drawPipeline();
    drawAnimation();

    drawEventPanel();

    drawColoredText(
        40, 12,
        "OPENGL DASHBOARD | FIFO IPC | QUALITY CONTROL",
        GLUT_BITMAP_HELVETICA_12,
        0.48f, 0.60f, 0.67f);

    glutSwapBuffers();
}

static void reshape(int width, int height)
{
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluOrtho2D(
        0, WINDOW_WIDTH,
        0, WINDOW_HEIGHT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

int main(int argc, char **argv)
{
    loadProductionConfig(argc > 1 ? argv[1] : "config.txt");

    glutInit(&argc, argv);

    glutInitDisplayMode(
        GLUT_DOUBLE | GLUT_RGB);

    glutInitWindowSize(
        WINDOW_WIDTH, WINDOW_HEIGHT);

    glutCreateWindow(
        "Olive Press - Real-Time Dashboard");

    reshape(WINDOW_WIDTH, WINDOW_HEIGHT);

    if (initLiveData() != 0)
        return 1;

    atexit(closeLiveData);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);

    lastTime = glutGet(GLUT_ELAPSED_TIME);

    glutTimerFunc(16, animationTimer, 0);

    glutMainLoop();

    return 0;
}