#pragma once
#include "types.h"

// Native control metadata, NOT a binary-compatible Dolphin TASK or SCE_TASK.
// Only the fields used by the verbatim functions below are provided. There are
// no pretend OSThread, OS semaphore, GX or ordering-table pointer APIs here.
namespace re4::port::original {
constexpr int TASK_NUM = 18;
constexpr int TASK_ISR = 4;
constexpr u8 TASK_NONE = 0, TASK_EXEC = 1, TASK_SLEEP = 2, TASK_RUN = 3;
constexpr u8 TASK_SUSPEND = 0x80;
struct TASK {
    u8 Status = 0;
    u8 Priority = 0;
    u8 Task_no = 0;
    u16 suspend_cnt = 0;
    u16 SleepCtr = 0;
    void* pModel = nullptr;
private:
    u8 kind = 0;
public:
#include "scheduler_methods.inc"
};
extern thread_local TASK Task[TASK_NUM];
extern thread_local TASK* CTASK_MAIN;
extern thread_local TASK* pCTask;
struct cSceSys { int checkCTaskRange(); };
extern thread_local cSceSys SceSys;

u32 GetStackSize(int level);
void TaskSuspend(int level);
void TaskSignal(int level);
u8 TaskStatus(int level);
void SetTaskModelPtr(void* model, TASK* task);
void SceSleep(int ctr);
void SceExit();
// These three platform adapters transfer control to real native stacks.
void TaskSleep(int ctr);
[[noreturn]] void TaskExit();
void SceTaskDelete(TASK* task);
} // namespace re4::port::original
