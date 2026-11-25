#ifndef LIVE_DATA_H
#define LIVE_DATA_H

#include "../include/dashboard_ipc.h"

int initLiveData(void);
void pollLiveData(void);
void closeLiveData(void);

const status_msg_t *getLiveStageData(int stageId);

const char *getLatestAlert(void);
int getAlertCount(void);

int getCompletedBatches(void);
int getActiveStages(void);

int getEventCount(void);
const char *getEvent(int index);
int isAlertEvent(int index);

#endif