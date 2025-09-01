#include "message_queue.h"
#include "cpu_affinity.h"
#include <iostream>
#include <thread>
#include <chrono>

// 高精度时钟获取纳秒时间戳
inline uint64_t getHighResolutionTimestamp() {
    return std::chrono::high_resolution_clock::now().time_since_epoch().count();
}

// Market data structure
#pragma pack(push, 1)
struct MarketData {
    char symbol[16];
    double price;
    int volume;
    long timestamp; // This timestamp will be overwritten by MessageQueue's internal timestamp
};
#pragma pack(pop)

int main() {
    try {
        // 绑定到CPU核心0 (生产者)
        std::cout << "Available CPU cores: " << CPUAffinity::getCPUCount() << std::endl;
        CPUAffinity::bindToCPU(0);
        
        // 尝试设置实时优先级 (需要root权限)
        // CPUAffinity::setRealtimePriority(90);
        
        // Create message queue
        // Name, capacity (number of messages), max_payload_size
        MessageQueue queue("/market_data_queue", 1024, sizeof(MarketData));
        
        // Simulate market data feed
        MarketData data;
        int counter = 0;
        
        while (true) {
            // Prepare sample data
            snprintf(data.symbol, sizeof(data.symbol), "AAPL");
            data.price = 182.72 + (counter % 10) * 0.01;  // Simulate price changes
            data.volume = 1000 + (counter % 500);
            
            // Publish data to message queue
            if (queue.publish(MessageType::MARKET_DATA, &data, sizeof(MarketData))) {
                std::cout << "Produced: " << data.symbol 
                         << " Price: " << data.price
                         << " Volume: " << data.volume 
                         << " Timestamp: (handled by MessageQueue)" << std::endl; // Timestamp is now handled by MessageQueue
                counter++;
            } else {
                std::cout << "Queue full, waiting..." << std::endl;
            }
            
            // 减少睡眠时间以获得更高频率的数据产生
            std::this_thread::sleep_for(std::chrono::microseconds(1000));  // 1ms instead of 100ms
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}