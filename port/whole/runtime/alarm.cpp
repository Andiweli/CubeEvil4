// OS alarms (OSAlarm, M10): the memory card library arms one per command as
// its timeout (CARDBios.c SetupTimeoutAlarm: 100 ms for a page program, 2 s
// for a sector erase) and cancels it when the card's interrupt comes. The
// port runs a due alarm's handler as the decrementer interrupt would, at the
// interrupt points (runtime/exi.cpp re4_exi_deliver: before the CPU idles and
// when a thread goes to sleep, so at least once per field): a timeout of the
// card library fires within a field of its time. The time is OSGetTime (real
// time, the bus clock / 4). The game itself uses no alarms.
#include "dolphin/os.h"
#include "dolphin/os/OSAlarm.h"
#include "re4_runtime.h"

namespace {

OSAlarm* gQueue;  // armed alarms, by fire time
bool gInHandler;  // an alarm handler is running (as runtime/exi.cpp: the guard
                  // covers the handler, not the end of the interrupt)

void unlink(OSAlarm* a)
{
    if (a->prev) {
        a->prev->next = a->next;
    } else if (gQueue == a) {
        gQueue = a->next;
    }
    if (a->next) {
        a->next->prev = a->prev;
    }
    a->prev = a->next = 0;
}

bool armed(OSAlarm* a)
{
    for (OSAlarm* q = gQueue; q; q = q->next) {
        if (q == a) {
            return true;
        }
    }
    return false;
}

void insert(OSAlarm* a, OSTime fire, OSAlarmHandler handler)
{
    if (armed(a)) {
        unlink(a);
    }
    a->fire = fire;
    a->handler = handler;
    OSAlarm** at = &gQueue;
    OSAlarm* prev = 0;
    while (*at && (*at)->fire <= fire) {
        prev = *at;
        at = &(*at)->next;
    }
    a->prev = prev;
    a->next = *at;
    if (*at) {
        (*at)->prev = a;
    }
    *at = a;
}

}  // namespace

extern "C" {

void OSCreateAlarm(OSAlarm* alarm)
{
    alarm->handler = 0;
    alarm->tag = 0;
    alarm->fire = 0;
    alarm->prev = alarm->next = 0;
    alarm->period = 0;
    alarm->start = 0;
}

void OSSetAlarm(OSAlarm* alarm, OSTime tick, OSAlarmHandler handler)
{
    alarm->period = 0;
    insert(alarm, OSGetTime() + tick, handler);
}

void OSSetAbsAlarm(OSAlarm* alarm, OSTime time, OSAlarmHandler handler)
{
    alarm->period = 0;
    insert(alarm, time, handler);
}

void OSSetPeriodicAlarm(OSAlarm* alarm, OSTime start, OSTime period, OSAlarmHandler handler)
{
    alarm->period = period;
    alarm->start = start;
    OSTime now = OSGetTime(), fire = start;
    if (period > 0 && fire < now) {
        fire += ((now - fire) / period + 1) * period;
    }
    insert(alarm, fire, handler);
}

void OSCancelAlarm(OSAlarm* alarm)
{
    if (armed(alarm)) {
        unlink(alarm);
    }
    alarm->handler = 0;
}

void OSSetAlarmTag(OSAlarm* alarm, u32 tag)
{
    alarm->tag = tag;
}

void OSCancelAlarms(u32 tag)
{
    if (tag == 0) {
        return;
    }
    for (OSAlarm* a = gQueue; a;) {
        OSAlarm* next = a->next;
        if (a->tag == tag) {
            OSCancelAlarm(a);
        }
        a = next;
    }
}

BOOL OSCheckAlarmQueue(void)
{
    return TRUE;
}

// The alarms due now run (interrupt context); the time of the next one, or 0.
long long re4_alarm_deliver(void)
{
    if (gInHandler) {
        return gQueue ? gQueue->fire : 0;
    }
    for (int guard = 0; guard < 64 && gQueue && gQueue->fire <= OSGetTime(); guard++) {
        OSAlarm* a = gQueue;
        OSAlarmHandler handler = a->handler;
        unlink(a);
        if (a->period > 0) {
            insert(a, a->fire + a->period, handler);
        }
        if (handler) {
            re4_os_interrupt_begin();
            gInHandler = true;
            handler(a, 0);
            gInHandler = false;
            re4_os_interrupt_end();
        }
    }
    return gQueue ? gQueue->fire : 0;
}

}  // extern "C"
