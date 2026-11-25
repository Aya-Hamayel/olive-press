
#include <GL/glut.h>
#include <math.h>
#include "machine_icons.h"

#define PI 3.14159265358979323846f

static void line(float x1, float y1, float x2, float y2) {
    glBegin(GL_LINES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
}

static void circle(float x, float y, float radius) {
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 32; i++) {
        float a = 2.0f * PI * i / 32.0f;
        glVertex2f(x + cosf(a) * radius,
                   y + sinf(a) * radius);
    }
    glEnd();
}

void drawMachineIcon(int stageId, float x, float y) {
    glColor3f(0.45f, 0.80f, 0.63f);
    glLineWidth(2.5f);

    switch (stageId) {
        case 0:
            // Reception container
            line(x - 16, y + 12, x + 16, y + 12);
            line(x - 16, y + 12, x - 12, y - 12);
            line(x + 16, y + 12, x + 12, y - 12);
            line(x - 12, y - 12, x + 12, y - 12);
            circle(x - 6, y + 2, 4);
            circle(x + 5, y + 3, 4);
            break;

        case 1:
            // Cleaning fan
            circle(x, y, 5);
            circle(x, y, 17);
            for (int i = 0; i < 4; i++) {
                float a = i * PI / 2.0f;
                line(x + cosf(a) * 6,
                     y + sinf(a) * 6,
                     x + cosf(a + 0.5f) * 15,
                     y + sinf(a + 0.5f) * 15);
            }
            break;

        case 2:
            // Washing droplets
            for (int i = -1; i <= 1; i++) {
                float dx = x + i * 13;
                line(dx, y + 12, dx - 5, y);
                line(dx, y + 12, dx + 5, y);
                circle(dx, y - 3, 5);
            }
            break;

        case 3:
            // Crushing gears
            circle(x - 9, y, 11);
            circle(x + 11, y, 11);
            circle(x - 9, y, 4);
            circle(x + 11, y, 4);
            break;

        case 4:
            // Malaxation mixer
            line(x - 19, y + 13, x + 19, y + 13);
            line(x - 19, y + 13, x - 14, y - 13);
            line(x + 19, y + 13, x + 14, y - 13);
            line(x - 14, y - 13, x + 14, y - 13);
            line(x, y + 18, x, y - 8);
            line(x - 10, y - 8, x + 10, y - 8);
            break;

        case 5:
            // Hydraulic press
            line(x - 17, y + 15, x + 17, y + 15);
            line(x - 17, y + 15, x - 17, y - 15);
            line(x + 17, y + 15, x + 17, y - 15);
            line(x - 17, y - 15, x + 17, y - 15);
            line(x, y + 15, x, y);
            line(x - 12, y, x + 12, y);
            break;
    }
}
