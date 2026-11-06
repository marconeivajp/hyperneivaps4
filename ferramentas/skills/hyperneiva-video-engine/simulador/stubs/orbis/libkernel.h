#pragma once
#include <stdint.h>
#include <pthread.h>
#ifdef __cplusplus
extern "C" {
#endif
int32_t sceKernelUsleep(uint32_t us);
uint64_t sceKernelGetProcessTime(void);
typedef pthread_t OrbisPthread;
OrbisPthread scePthreadSelf(void);
int32_t scePthreadSetprio(OrbisPthread t, int prio);
#ifdef __cplusplus
}
#endif
