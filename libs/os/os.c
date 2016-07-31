#include "base/os.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "event_groups.h"
#include "semphr.h"
#include "task.h"

static inline TickType_t ConvertTimeToTicks(const OSTaskTimeSpan span)
{
    const uint64_t time = span;
    return pdMS_TO_TICKS(time);
}

static OSResult TaskCreate(OSTaskProcedure entryPoint,
    const char* taskName,
    uint16_t stackSize,
    void* taskParameter,
    uint32_t priority,
    OSTaskHandle* taskHandle)
{
    const BaseType_t result = xTaskCreate(entryPoint, taskName, stackSize, taskParameter, priority, taskHandle);
    if (result != pdPASS)
    {
        return OSResultOutOfResources;
    }
    else
    {
        return OSResultSuccess;
    }
}

static void RunScheduller(void)
{
    vTaskStartScheduler();
}

static void TaskSleep(const OSTaskTimeSpan time)
{
    vTaskDelay(ConvertTimeToTicks(time));
}

static void TaskSuspend(OSTaskHandle task)
{
    vTaskSuspend(task);
}

static void TaskResume(OSTaskHandle task)
{
    vTaskResume(task);
}

static OSSemaphoreHandle CreateBinarySemaphore(void)
{
    return xSemaphoreCreateBinary();
}

static OSResult TakeSemaphore(OSSemaphoreHandle semaphore, OSTaskTimeSpan timeout)
{
    const BaseType_t result = xSemaphoreTake(semaphore, ConvertTimeToTicks(timeout));
    if (result != pdPASS)
    {
        return OSResultTimeout;
    }
    else
    {
        return OSResultSuccess;
    }
}

static OSResult GiveSemaphore(OSSemaphoreHandle semaphore)
{
    const BaseType_t result = xSemaphoreGive(semaphore);
    if (result != pdPASS)
    {
        return OSResultInvalidOperation;
    }
    else
    {
        return OSResultSuccess;
    }
}

static OSEventGroupHandle CreateEventGroup(void)
{
    return xEventGroupCreate();
}

static OSEventBits EventGroupSetBits(OSEventGroupHandle eventGroup, const OSEventBits bitsToChange)
{
    return xEventGroupSetBits(eventGroup, bitsToChange);
}

static OSEventBits EventGroupClearBits(OSEventGroupHandle eventGroup, const OSEventBits bitsToChange)
{
    return xEventGroupClearBits(eventGroup, bitsToChange);
}

static OSEventBits EventGroupWaitForBits(OSEventGroupHandle eventGroup,
    const OSEventBits bitsToWaitFor,
    bool waitAll,
    bool autoReset,
    const OSTaskTimeSpan timeout)
{
    return xEventGroupWaitBits(eventGroup,
        bitsToWaitFor,
        autoReset ? bitsToWaitFor : 0,
        waitAll ? pdTRUE : pdFALSE,
        ConvertTimeToTicks(timeout));
}

static void* Alloc(size_t size)
{
    return pvPortMalloc(size);
}

static void Free(void* ptr)
{
    return vPortFree(ptr);
}

OS System;

OSResult OSSetup(void)
{
    System.CreateTask = TaskCreate;
    System.RunScheduler = RunScheduller;
    System.SleepTask = TaskSleep;
    System.SuspendTask = TaskSuspend;
    System.ResumeTask = TaskResume;
    System.CreateBinarySemaphore = CreateBinarySemaphore;
    System.TakeSemaphore = TakeSemaphore;
    System.GiveSemaphore = GiveSemaphore;
    System.CreateEventGroup = CreateEventGroup;
    System.EventGroupSetBits = EventGroupSetBits;
    System.EventGroupClearBits = EventGroupClearBits;
    System.EventGroupWaitForBits = EventGroupWaitForBits;
    System.Alloc = Alloc;
    System.Free = Free;

    return OSResultSuccess;
}