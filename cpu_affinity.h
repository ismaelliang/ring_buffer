#pragma once

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include <unistd.h>
#include <pthread.h>
#include <stdexcept>
#include <iostream>

class CPUAffinity {
public:
    // 绑定当前线程到指定CPU核心
    static bool bindToCPU(int cpu_id) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_id, &cpuset);
        
        int result = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
        if (result != 0) {
            std::cerr << "Failed to bind to CPU " << cpu_id << std::endl;
            return false;
        }
        
        std::cout << "Thread bound to CPU " << cpu_id << std::endl;
        return true;
    }
    
    // 设置进程为实时优先级
    static bool setRealtimePriority(int priority = 99) {
        struct sched_param param;
        param.sched_priority = priority;
        
        if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
            std::cerr << "Failed to set real-time priority (需要root权限)" << std::endl;
            return false;
        }
        
        std::cout << "Set real-time priority: " << priority << std::endl;
        return true;
    }
    
    // 获取CPU核心数量
    static int getCPUCount() {
        return sysconf(_SC_NPROCESSORS_ONLN);
    }
};