// re4_cpu_info.h: the processor time a thread takes and the kind of processor core it runs on,
// for the performance lines of the log (the core's "GX core" line, the renderer's "GX" line).
// Header only: the core's runtime and the app's renderer both use it.
//
// Phones mix fast and slow cores (Galaxy A30s: two at 1.8 GHz, six at 1.6 GHz; Retroid: one
// fast, three middle, four slow). A thread that needs a whole core gets the frame rate of the
// core Android gives it; processor time next to the time a picture took tells whether the
// thread computed (time = processor time) or waited (for the GPU, a lock, another thread).
#pragma once

#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

namespace re4cpu {

// processor time of the calling thread in ns
inline long long ThreadNs()
{
    timespec t;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t) != 0) {
        return 0;
    }
    return static_cast<long long>(t.tv_sec) * 1000000000LL + t.tv_nsec;
}

// a number from a file of /sys (0: not readable)
inline unsigned ReadNumber(const char* path)
{
    unsigned value = 0;
    if (FILE* f = fopen(path, "r")) {
        if (fscanf(f, "%u", &value) != 1) {
            value = 0;
        }
        fclose(f);
    }
    return value;
}

// current clock of processor core `cpu` in kHz (0: not readable)
inline unsigned CurrentKHz(int cpu)
{
    char path[96];
    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", cpu);
    return ReadNumber(path);
}

struct Layout {
    static constexpr int kMax = 32;
    int count = 0;
    unsigned maxKHz[kMax] = {};
    unsigned slowest = 0, fastest = 0;  // highest clocks of the slowest / fastest kind

    // 0 the slowest kind of core, 2 the fastest, 1 in between; 2 when all are alike or the
    // clocks are not readable
    int Kind(int cpu) const
    {
        if (cpu < 0 || cpu >= count || !maxKHz[cpu] || slowest == fastest) {
            return 2;
        }
        return maxKHz[cpu] == fastest ? 2 : maxKHz[cpu] == slowest ? 0 : 1;
    }

    // "8 cores: 0-5 at most 1586 MHz, 6-7 at most 1794 MHz"
    void Describe(char* out, size_t size) const
    {
        int n = snprintf(out, size, "%d cores", count);
        if (!fastest) {
            snprintf(out + n, n < static_cast<int>(size) ? size - n : 0, ", clocks not readable");
            return;
        }
        for (int i = 0; i < count && n < static_cast<int>(size);) {
            int j = i;
            while (j + 1 < count && maxKHz[j + 1] == maxKHz[i]) {
                j++;
            }
            if (j > i) {
                n += snprintf(out + n, size - n, "%s %d-%d at most %u MHz", i ? "," : ":", i, j, maxKHz[i] / 1000);
            } else {
                n += snprintf(out + n, size - n, "%s %d at most %u MHz", i ? "," : ":", i, maxKHz[i] / 1000);
            }
            i = j + 1;
        }
    }
};

inline const Layout& GetLayout()
{
    static const Layout layout = [] {
        Layout l;
        char path[96];
        for (int cpu = 0; cpu < Layout::kMax; cpu++) {
            snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d", cpu);
            if (access(path, F_OK) != 0) {
                break;
            }
            snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", cpu);
            l.maxKHz[cpu] = ReadNumber(path);
            l.count = cpu + 1;
        }
        for (int cpu = 0; cpu < l.count; cpu++) {
            const unsigned khz = l.maxKHz[cpu];
            if (khz && (!l.slowest || khz < l.slowest)) {
                l.slowest = khz;
            }
            if (khz > l.fastest) {
                l.fastest = khz;
            }
        }
        return l;
    }();
    return layout;
}

// Per picture: processor time and the kind of core the thread was on when the picture ended.
struct ThreadUse {
    long long cpuNs = 0;
    unsigned onKind[3] = {};
    unsigned pictures = 0;
    long long lastNs = 0;
    int lastCpu = -1;

    // at the end of a picture of the calling thread
    void Picture()
    {
        const long long now = ThreadNs();
        if (lastNs) {
            cpuNs += now - lastNs;
            pictures++;
            lastCpu = sched_getcpu();
            onKind[GetLayout().Kind(lastCpu)]++;
        }
        lastNs = now;
    }

    // "11.2 ms of processor time each, on fast/middle/slow cores 80/0/20 %, last on core 6 at
    // 1794 MHz"; then counts from 0
    void Describe(char* out, size_t size)
    {
        const Layout& l = GetLayout();
        const double n = pictures ? pictures : 1;
        int len = snprintf(out, size, "%.1f ms of processor time each", cpuNs / 1e6 / n);
        if (l.slowest != l.fastest && len < static_cast<int>(size)) {
            len += snprintf(out + len, size - len, ", on fast/middle/slow cores %.0f/%.0f/%.0f %%",
                            onKind[2] * 100.0 / n, onKind[1] * 100.0 / n, onKind[0] * 100.0 / n);
        }
        if (lastCpu >= 0 && len < static_cast<int>(size)) {
            const unsigned khz = CurrentKHz(lastCpu);
            if (khz) {
                snprintf(out + len, size - len, ", last on core %d at %u MHz", lastCpu, khz / 1000);
            } else {
                snprintf(out + len, size - len, ", last on core %d", lastCpu);
            }
        }
        cpuNs = 0;
        pictures = 0;
        onKind[0] = onKind[1] = onKind[2] = 0;
    }
};

}  // namespace re4cpu
