/**
 * @copyright Copyright (C) 2025 BOKE Inc.
 * @author Yuchen Liang
 * @date 2025-09-01
 * @brief 市场数据生产者，生成并发布市场数据到消息队列。
 *
 * 该文件实现了市场数据生产者，它模拟市场数据源，
 * 将生成的市场数据发布到一个共享内存消息队列。
 * 它还支持通过命令行参数指定生产的消息数量。
 */
#include "market_data.h"
#include "message_queue.h"
#include <chrono>
#include <iostream>
#include <optional> // For std::optional
#include <string>
#include <thread>

/**
 * @brief 打印程序用法信息。
 * @param program_name 程序的名称。
 */
void print_usage(const char *program_name) {
    std::cerr << "Usage: " << program_name << " [-n <count>] [-c <num_consumers>]" << std::endl;
    std::cerr << "  -n, --num    Total number of messages to produce (default: "
                 "infinite)"
              << std::endl;
    std::cerr << "  -c, --consumers Total number of consumers (default: 1)" << std::endl;
}

/**
 * @brief 主函数，实现市场数据生产者的逻辑。
 *
 * 绑定到CPU核心，创建消息队列，生成并发布模拟市场数据。
 * 支持通过命令行参数指定生产的消息数量。
 *
 * @param argc 命令行参数的数量。
 * @param argv 命令行参数数组。
 * @return 程序的退出状态码。
 */
int main(int argc, char *argv[]) {
    // Initialize fmtlog
    fmtlog::setLogLevel(fmtlog::DBG);
    fmtlog::setLogFile("producer.log"); // Log to producer.log
    fmtlog::startPollingThread();
    
    std::optional<long long> total_message_count; // Use long long for potentially large counts
    uint32_t num_consumers = 1;                   // Default to 1 consumer

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-n" || arg == "--num") {
            if (i + 1 < argc) {
                total_message_count = std::stoll(argv[++i]);
            } else {
                std::cerr << "Error: -n/--num requires an argument." << std::endl;
                print_usage(argv[0]);
                return 1;
            }
        } else if (arg == "-c" || arg == "--consumers") {
            if (i + 1 < argc) {
                num_consumers = std::stoul(argv[++i]);
                if (num_consumers == 0) {
                    std::cerr << "Error: num_consumers cannot be zero." << std::endl;
                    print_usage(argv[0]);
                    return 1;
                }
            } else {
                std::cerr << "Error: -c/--consumers requires an argument." << std::endl;
                print_usage(argv[0]);
                return 1;
            }
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Error: Unknown argument: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    try {
        shm_unlink("/market_data_queue");

        // 绑定到CPU核心0 (生产者)
        MQ_ILOG("Available CPU cores: {}", CPUAffinity::getCPUCount());
        CPUAffinity::bindToCPU(0);

        // 尝试设置实时优先级 (需要root权限)
        // CPUAffinity::setRealtimePriority(90);

        // Create message queue
        // Name, capacity (number of messages), max_payload_size
        MessageQueue queue("/market_data_queue",
                           1024,
                           sizeof(MarketData),
                           num_consumers); // Use the parsed num_consumers

        // Simulate market data feed
        MarketData data;
        int counter = 0;
        long long messages_produced = 0;

        while (true) {
            if (total_message_count.has_value() && messages_produced >= total_message_count.value()) {
                MQ_ILOG("Produced {} messages. Exiting.", messages_produced);
                break;
            }
            // Prepare sample data
            snprintf(data.symbol, sizeof(data.symbol), "AAPL");
            data.price = 182.72 + (counter % 10) * 0.01; // Simulate price changes
            data.volume = 1000 + (counter % 500);

            // Produce data to message queue
            if (queue.produce(MessageType::MARKET_DATA, &data, sizeof(MarketData))) {
                MQ_DLOG("Produced: {} Price: {:.2f} Volume: {} Timestamp: (handled by MessageQueue)",
                        data.symbol,
                        data.price,
                        data.volume);
                counter++;
                messages_produced++;
            } else {
                // MQ_WLOG("Queue full, waiting...");
            }

            // 减少睡眠时间以获得更高频率的数据产生
            std::this_thread::sleep_for(std::chrono::microseconds(1000)); // 1ms instead of 100ms
        }
    } catch (const std::exception &e) {
        MQ_ELOG("Error: {}", e.what());
        fmtlog::poll(true); // Flush logs before exit
        return 1;
    }

    fmtlog::poll(true); // Flush logs before exit
    return 0;
}