#include <GL/glut.h>
#include <math.h>
#include "animation.h"
#include "graphics.h"

#define PI 3.14159265358979323846f

static float progress = 0.0f;

void updateAnimation(float deltaTime)
{
    progress += deltaTime * 0.8f;
    progress = fmodf(progress, 1.0f);
}

static void drawOlive(float x, float y)
{
    glColor3f(0.48f, 0.65f, 0.18f);

    glBegin(GL_TRIANGLE_FAN);

    glVertex2f(x, y);

    for (int i = 0; i <= 32; i++)
    {
        float angle = 2.0f * PI * i / 32.0f;

        glVertex2f(
            x + cosf(angle) * 7.0f,
            y + sinf(angle) * 4.5f
        );
    }

    glEnd();
}

static void drawFlow(float x1, float y1,
                     float x2, float y2)
{
    for (int i = 0; i < 3; i++)
    {
        float t = fmodf(
            progress + i / 3.0f, 1.0f
        );

        float x = x1 + (x2 - x1) * t;
        float y = y1 + (y2 - y1) * t;

        drawOlive(x, y);
    }
}

void drawAnimation(void){
    if (getStageStatus(0) == STAGE_RUNNING &&
        getStageStatus(1) == STAGE_RUNNING)
    {
        drawFlow(355, 417, 445, 417);
    }

    if (getStageStatus(1) == STAGE_RUNNING &&
        getStageStatus(2) == STAGE_RUNNING)
    {
        drawFlow(750, 417, 840, 417);
    }

    if (getStageStatus(2) == STAGE_RUNNING &&
        getStageStatus(3) == STAGE_RUNNING)
    {
        drawFlow(995, 355, 995, 325);
    }

    if (getStageStatus(3) == STAGE_RUNNING &&
        getStageStatus(4) == STAGE_RUNNING)
    {
        drawFlow(845, 262, 755, 262);
    }

    if (getStageStatus(4) == STAGE_RUNNING &&
        getStageStatus(5) == STAGE_RUNNING)
    {
        drawFlow(450, 262, 360, 262);
    }
}