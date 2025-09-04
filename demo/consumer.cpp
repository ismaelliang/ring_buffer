/**
 * @copyright Copyright (C) 2025 BOKE Inc.
 * @author Yuchen Liang
 * @date 2025-09-01
 * @brief 市场数据消费者，从消息队列订阅并处理市场数据。
 *
 * 该文件实现了市场数据消费者，它连接到一个共享内存消息队列，
 * 订阅市场数据，计算并打印消息延迟统计。
 * 它还支持通过命令行参数指定消费的消息数量。
 */
#include "market_data.h"
#include "message_queue.h"
#include <algorithm>
#include <chrono>
#include <cmath> // Add this include for std::ceil
#include <iomanip>
#include <iostream>
#include <optional> // For std::optional
#include <string>   // For std::string and std::stoll
#include <thread>
#include <vector>

/**
 * @brief 延迟统计结构体。
 *
 * 用于收集和计算消息处理的延迟统计信息，包括最小、最大、平均和百分位延迟。
 */
struct LatencyStats {
    uint64_t min_latency = UINT64_MAX; ///< 最小延迟（纳秒）。
    uint64_t max_latency = 0;          ///< 最大延迟（纳秒）。
    uint64_t total_latency = 0;        ///< 总延迟（纳秒）。
    uint64_t count = 0;                ///< 统计的样本数量。
    std::vector<uint64_t> latencies;   ///< 存储所有延迟样本的向量。

    /**
     * @brief 更新延迟统计。
     * @param latency 当前消息的延迟（纳秒）。
     */
    void update(uint64_t latency) {
        min_latency = std::min(min_latency, latency);
        max_latency = std::max(max_latency, latency);
        total_latency += latency;
        count++;
        latencies.push_back(latency); // Store latency
    }

    /**
     * @brief 获取平均延迟。
     * @return 平均延迟（纳秒），如果无样本则返回0.0。
     */
    double getAverageLatency() const {
        return count > 0 ? static_cast<double>(total_latency) / count : 0.0;
    }

    /**
     * @brief 获取指定百分位数的延迟。
     * @param percentile 百分数 (例如 50 表示 P50)。
     * @return 指定百分位数的延迟（纳秒）。
     */
    uint64_t getPercentile(double percentile) const {
        if (latencies.empty()) {
            return 0;
        }
        std::vector<uint64_t> sorted_latencies = latencies;
        std::sort(sorted_latencies.begin(), sorted_latencies.end());
        int index = static_cast<int>(std::ceil(percentile / 100.0 * sorted_latencies.size())) - 1;
        if (index < 0)
            index = 0; // Handle percentile 0 edge case
        if (static_cast<std::vector<uint64_t>::size_type>(index) >= sorted_latencies.size())
            index = sorted_latencies.size() - 1; // Handle percentile 100 edge case
        return sorted_latencies[index];
    }

    void reset() {
        min_latency = UINT64_MAX;
        max_latency = 0;
        total_latency = 0;
        count = 0;
        latencies.clear();
    }

    /**
     * @brief 打印延迟统计信息。
     *
     * 将最小、最大、平均、P50、P95、P99延迟以及样本数量输出到控制台。
     */
    void printStats() const {
        std::cout << "\n=== 延迟统计 ===\n"
                  << "最小延迟: " << min_latency << "ns (" << min_latency / 1000.0 << "μs)\n"
                  << "最大延迟: " << max_latency << "ns (" << max_latency / 1000.0 << "μs)\n"
                  << "平均延迟: " << getAverageLatency() << "ns (" << getAverageLatency() / 1000.0 << "μs)\n"
                  << "P50 延迟: " << getPercentile(50) << "ns (" << getPercentile(50) / 1000.0 << "μs)\n"
                  << "P95 延迟: " << getPercentile(95) << "ns (" << getPercentile(95) / 1000.0 << "μs)\n"
                  << "P99 延迟: " << getPercentile(99) << "ns (" << getPercentile(99) / 1000.0 << "μs)\n"
                  << "样本数量: " << count << "\n"
                  << std::endl;
    }
};

/**
 * @brief 打印程序用法信息。
 * @param program_name 程序的名称。
 */
void print_usage(const char *program_name) {
    std::cerr << "Usage: " << program_name << " [-n <count>] [-c <num_consumers>] [-id <consumer_id>]" << std::endl;
    std::cerr << "  -n, --num    Total number of messages to consume (default: "
                 "infinite)"
              << std::endl;
    std::cerr << "  -c, --consumers Total number of consumers (default: 1)" << std::endl;
    std::cerr << "  -id, --consumer_id Unique ID for this consumer (0 to "
                 "num_consumers-1, default: 0)"
              << std::endl;
}

/**
 * @brief 主函数，实现市场数据消费者的逻辑。
 *
 * 连接到消息队列，订阅市场数据，计算并打印延迟统计。
 * 支持通过命令行参数指定消费的消息数量。
 *
 * @param argc 命令行参数的数量。
 * @param argv 命令行参数数组。
 * @return 程序的退出状态码。
 */
int main(int argc, char *argv[]) {
    // Initialize fmtlog
    fmtlog::setLogLevel(fmtlog::DBG);
    std::optional<long long> total_message_count; // Use long long for potentially large counts
    uint32_t num_consumers = 1;                   // Default to 1 consumer
    uint32_t consumer_id = 0;                     // Default consumer ID

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
        } else if (arg == "-id" || arg == "--consumer_id") {
            if (i + 1 < argc) {
                consumer_id = std::stoul(argv[++i]);
            } else {
                std::cerr << "Error: -id/--consumer_id requires an argument." << std::endl;
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

    // Set log file with consumer ID suffix
    std::string log_filename = "consumer_" + std::to_string(consumer_id) + ".log";
    fmtlog::setLogFile(log_filename.c_str());
    fmtlog::startPollingThread();

    if (consumer_id >= num_consumers) {
        std::cerr << "Error: consumer_id (" << consumer_id << ") must be less than num_consumers (" << num_consumers
                  << ")." << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    try {
        // 绑定到CPU核心1 (消费者，与生产者分离)
        MQ_ILOG("Available CPU cores: {}", CPUAffinity::getCPUCount());
        int target_cpu = consumer_id + 1; // Start from CPU 1, assuming producer uses CPU 0
        if (target_cpu >= CPUAffinity::getCPUCount()) {
            MQ_ELOG("Error: Not enough CPU cores to bind consumer {} to core {}. Available cores: {}",
                    consumer_id,
                    target_cpu,
                    CPUAffinity::getCPUCount());
            return 1;
        }
        CPUAffinity::bindToCPU(target_cpu);

        // 尝试设置实时优先级 (需要root权限)
        // CPUAffinity::setRealtimePriority(95);  // 消费者优先级更高

        // Connect to the message queue
        // Name, capacity (number of messages), max_payload_size
        // The max_payload_size should match what the producer used
        MessageQueue queue("/market_data_queue", 1024, sizeof(MarketData), num_consumers);

        // Allocate buffer for receiving messages (header + payload)
        std::vector<char> received_message_buffer(sizeof(MessageHeader) + queue.max_payload_size());

        // 延迟统计
        LatencyStats stats;

        // Consume market data
        long long messages_consumed = 0; // Use long long for potentially large counts
        int msg_count = 0;
        const int STATS_INTERVAL = 1000; // 每1000条消息打印一次统计

        MQ_ILOG("开始消费数据，使用高精度时钟测量延迟...");

        while (true) {
            if (total_message_count.has_value() && messages_consumed >= total_message_count.value()) {
                MQ_ILOG("Consumed {} messages. Exiting.", messages_consumed);
                break;
            }

            // Try to read data from message queue
            if (queue.consume(received_message_buffer.data(), consumer_id)) {
                // Interpret the received buffer as a GenericMessage
                const GenericMessage *received_msg =
                    reinterpret_cast<const GenericMessage *>(received_message_buffer.data());

                // Cast the payload to MarketData
                const MarketData *received_data = reinterpret_cast<const MarketData *>(received_msg->payload);

                // 使用高精度时钟计算延迟，使用消息头中的时间戳
                auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
                auto latency = now - received_msg->header.timestamp;

                // 更新统计
                stats.update(latency);
                msg_count++;
                messages_consumed++; // Increment consumed count

                // 显示接收到的数据和延迟（以微秒为单位显示更直观）
                MQ_DLOG("Received: {} Price: {:.2f} Volume: {} Latency: {}ns ({:.2f}μs)",
                        received_data->symbol,
                        received_data->price,
                        received_data->volume,
                        latency,
                        latency / 1000.0);

                // 定期打印统计信息
                if (msg_count % STATS_INTERVAL == 0) {
                    stats.printStats();
                    stats.reset(); // Reset statistics after printing
                }
            } else if (queue.empty(consumer_id)) {
                // 使用更短的睡眠时间以减少延迟，或者使用忙等待
                // std::this_thread::sleep_for(std::chrono::microseconds(1));

                // 使用CPU pause指令减少忙等待的功耗
                __asm__ __volatile__("pause" ::: "memory");
            }
            // 如果队列不空但当前消费者读不到（被其他消费者抢先），则继续循环
        }
    } catch (const std::exception &e) {
        MQ_ELOG("Error: {}", e.what());
        fmtlog::poll(true); // Flush logs before exit
        return 1;
    }

    fmtlog::poll(true); // Flush logs before exit
    return 0;
}