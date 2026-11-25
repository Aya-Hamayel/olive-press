
#include "graphics.h"

#define STAGE_COUNT 6

static StageStatus stageStatuses[STAGE_COUNT] = {
    STAGE_IDLE,
    STAGE_IDLE,
    STAGE_IDLE,
    STAGE_IDLE,
    STAGE_IDLE,
    STAGE_IDLE
};

void updateStageStatus(int stageId, StageStatus status) {
    if (stageId < 0 || stageId >= STAGE_COUNT) {
        return;
    }

    if (status < STAGE_IDLE || status > STAGE_ERROR) {
        return;
    }

    stageStatuses[stageId] = status;
}

StageStatus getStageStatus(int stageId) {
    if (stageId < 0 || stageId >= STAGE_COUNT) {
        return STAGE_ERROR;
    }

    return stageStatuses[stageId];
}
