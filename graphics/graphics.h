#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "../include/common.h"

typedef stage_state_t StageStatus;

void updateStageStatus(int stageId, StageStatus status);
StageStatus getStageStatus(int stageId);

#endif