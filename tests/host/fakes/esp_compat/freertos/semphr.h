#pragma once
#include "freertos/FreeRTOS.h"
typedef void *SemaphoreHandle_t;
#define pdTRUE 1
#define pdFALSE 0
extern int g_fake_semaphore_take_result;
static inline int xSemaphoreTake(SemaphoreHandle_t sem, TickType_t ticks) { (void)sem; (void)ticks; return g_fake_semaphore_take_result; }
static inline int xSemaphoreGive(SemaphoreHandle_t sem) { (void)sem; return pdTRUE; }
