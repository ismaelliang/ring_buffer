#include "ring_buffer.h"
#include "cpu_affinity.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <vector> // Add this include
#include <algorithm> // Add this include
#include <cmath> // Add this include for std::ceil

// 高精度时钟获取纳秒时间戳
inline uint64_t getHighResolutionTimestamp() {
    return std::chrono::high_resolution_clock::now().time_since_epoch().count();
}

// 延迟统计结构
struct LatencyStats {
    uint64_t min_latency = UINT64_MAX;
    uint64_t max_latency = 0;
    uint64_t total_latency = 0;
    uint64_t count = 0;
    std::vector<uint64_t> latencies; // Store all latency samples
    
    void update(uint64_t latency) {
        min_latency = std::min(min_latency, latency);
        max_latency = std::max(max_latency, latency);
        total_latency += latency;
        count++;
        latencies.push_back(latency); // Store latency
    }
    
    double getAverageLatency() const {
        return count > 0 ? static_cast<double>(total_latency) / count : 0.0;
    }

    uint64_t getPercentile(double percentile) const {
        if (latencies.empty()) {
            return 0;
        }
        std::vector<uint64_t> sorted_latencies = latencies;
        std::sort(sorted_latencies.begin(), sorted_latencies.end());
        int index = static_cast<int>(std::ceil(percentile / 100.0 * sorted_latencies.size())) - 1;
        if (index < 0) index = 0; // Handle percentile 0 edge case
        if (index >= sorted_latencies.size()) index = sorted_latencies.size() - 1; // Handle percentile 100 edge case
        return sorted_latencies[index];
    }
    
    void printStats() const {
        std::cout << "\n=== 延迟统计 ===\n"
                  << "最小延迟: " << min_latency << "ns (" << min_latency/1000.0 << "μs)\n"
                  << "最大延迟: " << max_latency << "ns (" << max_latency/1000.0 << "μs)\n"
                  << "平均延迟: " << getAverageLatency() << "ns (" << getAverageLatency()/1000.0 << "μs)\n"
                  << "P50 延迟: " << getPercentile(50) << "ns (" << getPercentile(50)/1000.0 << "μs)\n"
                  << "P95 延迟: " << getPercentile(95) << "ns (" << getPercentile(95)/1000.0 << "μs)\n"
                  << "P99 延迟: " << getPercentile(99) << "ns (" << getPercentile(99)/1000.0 << "μs)\n"
                  << "样本数量: " << count << "\n" << std::endl;
    }
};

// Market data structure (same as producer)
#pragma pack(push, 1)
struct MarketData {
    char symbol[16];
    double price;
    int volume;
    long timestamp;
};
#pragma pack(pop)

int main() {
    try {
        // 绑定到CPU核心1 (消费者，与生产者分离)
        std::cout << "Available CPU cores: " << CPUAffinity::getCPUCount() << std::endl;
        CPUAffinity::bindToCPU(1);
        
        // 尝试设置实时优先级 (需要root权限)
        CPUAffinity::setRealtimePriority(95);  // 消费者优先级更高
        
        // Connect to the same ring buffer
        MmapRingBuffer buffer("/market_data_queue", 1024, sizeof(MarketData));
        
        // 延迟统计
        LatencyStats stats;
        
        // Consume market data
        MarketData data;
        int msg_count = 0;
        const int STATS_INTERVAL = 1000;  // 每1000条消息打印一次统计
        
        std::cout << "开始消费数据，使用高精度时钟测量延迟...\n" << std::endl;
        
        while (true) {
            // Try to read data from ring buffer
            if (buffer.pop(&data)) {
                // 使用高精度时钟计算延迟
                auto now = getHighResolutionTimestamp();
                auto latency = now - data.timestamp;
                
                // 更新统计
                stats.update(latency);
                msg_count++;
                
                // 显示接收到的数据和延迟（以微秒为单位显示更直观）
                std::cout << "Received: " << data.symbol 
                         << " Price: " << std::fixed << std::setprecision(2) << data.price
                         << " Volume: " << data.volume
                         << " Latency: " << latency << "ns (" 
                         << std::fixed << std::setprecision(2) << latency/1000.0 << "μs)" << std::endl;
                
                // 定期打印统计信息
                if (msg_count % STATS_INTERVAL == 0) {
                    stats.printStats();
                }
                
            } else {
                // 使用更短的睡眠时间以减少延迟，或者使用忙等待
                // std::this_thread::sleep_for(std::chrono::microseconds(1));
                
                // 使用CPU pause指令减少忙等待的功耗
                __asm__ __volatile__("pause" ::: "memory");
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}