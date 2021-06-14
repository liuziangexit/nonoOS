#ifndef __LIBNO_CONDITION_VARIABLE_H__
#define __LIBNO_CONDITION_VARIABLE_H__

#define USER_CV_ACTION_CREATE 1
#define USER_CV_ACTION_WAIT 2
#define USER_CV_ACTION_TIMEDWAIT 3
#define USER_CV_ACTION_NOTIFY_ONE 4
#define USER_CV_ACTION_NOTIFY_ALL 5

#ifdef LIBNO_USER
#include <stdbool.h>
#include <stdint.h>
uint32_t cv_create();
void cv_wait(uint32_t cv_id, uint32_t mut_id);
bool cv_timedwait(uint32_t cv_id, uint32_t mut_id, uint64_t timeout_ms);
void cv_notify_one(uint32_t cv_id, uint32_t mut_id);
void cv_notify_all(uint32_t cv_id, uint32_t mut_id);
#endif

#endif