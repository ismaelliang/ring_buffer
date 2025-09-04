# 高性能 SPMC 无锁消息队列

一个基于共享内存的无锁 SPMC（Single Producer Multiple Consumer）消息队列实现，专为高频交易和低延迟应用设计。该项目提供了 header-only 的 C++ 库，支持多进程通信，实现了纳秒级的消息传递延迟。

## 核心特性

### SPMC 架构设计
- **单生产者多消费者**: 支持一个生产者同时向多个独立的消费者发送消息
- **无锁实现**: 基于原子操作的环形缓冲区，避免互斥锁开销
- **独立消费进度**: 每个消费者维护独立的读取指针，互不影响
- **消息广播**: 同一消息可被所有消费者独立消费

### 高性能优化
- **共享内存**: 进程间零拷贝通信，减少数据拷贝开销
- **原子操作**: 使用 C++11 原子类型确保线程安全
- **内存对齐**: 64字节缓存行对齐，避免 false sharing
- **CPU 亲和性**: 支持线程绑定特定 CPU 核心
- **实时调度**: 支持 SCHED_FIFO 实时调度策略

### 系统特性
- **Header-only**: 单头文件库，易于集成
- **自动兼容性检查**: 智能检测现有共享内存兼容性
- **灵活配置**: 可配置队列容量、消息大小、消费者数量
- **高精度时间戳**: 纳秒级时间戳用于延迟测量

## 架构设计

### 核心组件

```
┌─────────────────┐    ┌─────────────────────────────┐    ┌─────────────────┐
│     Producer    │───▶│     SharedMemory Queue      │◀───│   Consumer 1    │
│   (Single)      │    │   ┌─────────────────────┐   │    │                 │
└─────────────────┘    │   │   RingBuffer        │   │    └─────────────────┘
                       │   │   - Head (atomic)   │   │                      
                       │   │   - Tail[0]         │   │    ┌─────────────────┐
                       │   │   - Tail[1]         │   │◀───│   Consumer 2    │
                       │   │   - Tail[N]         │   │    │                 │
                       │   └─────────────────────┘   │    └─────────────────┘
                       └─────────────────────────────┘              
                                                           ┌─────────────────┐
                                                      ◀───│   Consumer N    │
                                                           │                 │
                                                           └─────────────────┘
```

### 内存布局

```
共享内存布局:
┌─────────────────────────────────────────────────────────────────┐
│                    RingBufferHeader                             │
│  ┌─────────────┬─────────────┬─────────────┬─────────────────┐  │
│  │ head(atomic)│    size     │ element_size│  num_consumers  │  │
│  └─────────────┴─────────────┴─────────────┴─────────────────┘  │
│  ┌─────────────┬─────────────┬─────────────┬─────────────────┐  │
│  │ tail[0]     │   tail[1]   │   tail[2]   │     ...         │  │
│  │ (atomic)    │  (atomic)   │  (atomic)   │                 │  │
│  └─────────────┴─────────────┴─────────────┴─────────────────┘  │
├─────────────────────────────────────────────────────────────────┤
│                        Message Data                            │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │ Message[0]: [Header][Payload]                           │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │ Message[1]: [Header][Payload]                           │   │
│  ├─────────────────────────────────────────────────────────┤   │
│  │                        ...                              │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### 消息结构

```cpp
// 消息头部（自动添加）
struct MessageHeader {
    MessageType type;      // 消息类型
    uint32_t payload_size; // 有效载荷大小
    uint64_t timestamp;    // 高精度时间戳（纳秒）
    uint64_t sequence_num; // 序列号
};

// 用户自定义消息结构示例
struct MarketData {
    char symbol[16];  // 股票代码
    double price;     // 价格
    int volume;       // 成交量
    long timestamp;   // 用户时间戳
};
```

## API 使用指南

### 1. 定义消息类型

```cpp
#include "message_queue.h"

// 定义你的消息结构
struct MyMessage {
    char symbol[16];
    double price;
    uint32_t volume;
};
```

### 2. 生产者使用

```cpp
#include "message_queue.h"

void producer_example() {
    // 创建消息队列
    // 参数: 队列名称, 容量(消息数), 最大载荷大小, 消费者数量
    MessageQueue queue("/my_queue", 1024, sizeof(MyMessage), 2);
    
    // 准备消息数据
    MyMessage msg;
    strcpy(msg.symbol, "AAPL");
    msg.price = 150.25;
    msg.volume = 1000;
    
    // 发送消息
    if (queue.produce(MessageType::MARKET_DATA, &msg, sizeof(MyMessage))) {
        std::cout << "Message sent successfully" << std::endl;
    } else {
        std::cout << "Queue is full" << std::endl;
    }
}
```

### 3. 消费者使用

```cpp
#include "message_queue.h"

void consumer_example(uint32_t consumer_id) {
    // 连接到现有队列（参数必须匹配）
    MessageQueue queue("/my_queue", 1024, sizeof(MyMessage), 2);
    
    // 准备接收缓冲区
    std::vector<char> buffer(sizeof(MessageHeader) + queue.max_payload_size());
    
    while (true) {
        // 消费消息
        if (queue.consume(buffer.data(), consumer_id)) {
            // 解析消息
            const GenericMessage* generic_msg = 
                reinterpret_cast<const GenericMessage*>(buffer.data());
            const MyMessage* msg = 
                reinterpret_cast<const MyMessage*>(generic_msg->payload);
            
            // 计算延迟
            auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            auto latency = now - generic_msg->header.timestamp;
            
            std::cout << "Consumer " << consumer_id 
                      << " received: " << msg->symbol 
                      << " Price: " << msg->price
                      << " Latency: " << latency << "ns" << std::endl;
        } else {
            // 队列为空，可以短暂休眠或忙等待
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    }
}
```

### 4. 多消费者场景

```cpp
#include <thread>

void multi_consumer_example() {
    const uint32_t NUM_CONSUMERS = 3;
    
    // 启动多个消费者线程
    std::vector<std::thread> consumers;
    for (uint32_t i = 0; i < NUM_CONSUMERS; ++i) {
        consumers.emplace_back([i]() {
            consumer_example(i);  // 每个消费者使用不同的 ID
        });
    }
    
    // 等待所有消费者线程
    for (auto& t : consumers) {
        t.join();
    }
}
```

## 性能优化

### 编译优化

```bash
# 生产环境构建
mkdir build-prod && cd build-prod
cmake -DCMAKE_BUILD_TYPE=PROD ..
make -j$(nproc)
```

**PROD 构建参数**: `-O3 -march=native -mtune=native -DNDEBUG -flto -ffast-math -funroll-loops`

### 运行时优化

```cpp
#include "message_queue.h"

void setup_high_performance() {
    // 1. 绑定CPU核心
    CPUAffinity::bindToCPU(0);  // 生产者绑定核心0
    
    // 2. 设置实时优先级（需要root权限）
    CPUAffinity::setRealtimePriority(99);
    
    // 3. 获取CPU核心数
    int cpu_count = CPUAffinity::getCPUCount();
    std::cout << "Available CPU cores: " << cpu_count << std::endl;
}
```

### 系统级优化

```bash
# 设置CPU性能模式
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# 启用大页内存
echo 1024 | sudo tee /proc/sys/vm/nr_hugepages

# 减少定时器迁移
echo 0 | sudo tee /proc/sys/kernel/timer_migration

# 使用实时调度运行
sudo chrt -f 99 ./consumer &
sudo chrt -f 95 ./producer &

# CPU亲和性绑定
sudo taskset -c 0 ./producer &    # 生产者绑定核心0
sudo taskset -c 1 ./consumer1 &   # 消费者1绑定核心1  
sudo taskset -c 2 ./consumer2 &   # 消费者2绑定核心2
```

## 构建和测试

### 环境要求

- **编译器**: GCC 7+ 或 Clang 6+ (支持 C++17)
- **CMake**: 3.10+
- **操作系统**: Linux (测试环境: Ubuntu 18.04+)
- **依赖**: vcpkg (用于管理 Google Test)

### 安装依赖

```bash
# 1. 安装 vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh

# 2. 设置环境变量
export VCPKG_ROOT=/path/to/vcpkg
export PATH=$VCPKG_ROOT:$PATH

# 3. 安装依赖
vcpkg install gtest
```

### 构建项目

```bash
# Debug 构建（开发调试）
mkdir build-debug && cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# PROD 构建（生产环境）
mkdir build-prod && cd build-prod  
cmake -DCMAKE_BUILD_TYPE=PROD ..
make -j$(nproc)
```

### 运行测试

```bash
# 运行所有测试
make test

# 或直接运行测试程序
./test/ringbuffer_tests
./test/no_create_tests
```

### 运行示例

```bash
# 基本运行
./bin/consumer &
./bin/producer

# 多消费者示例
./bin/consumer -c 3 -id 0 &    # 消费者0
./bin/consumer -c 3 -id 1 &    # 消费者1  
./bin/consumer -c 3 -id 2 &    # 消费者2
./bin/producer -c 3            # 生产者指定3个消费者

# 限制消息数量
./bin/producer -n 10000        # 生产10000条消息
./bin/consumer -n 10000        # 消费10000条消息
```

## 性能指标

### 延迟性能

| 构建类型 | 典型延迟 | 最佳延迟 | P99延迟 | 适用场景 |
|---------|---------|---------|---------|----------|
| Debug   | 10-50μs | 5μs     | 100μs   | 开发调试 |
| PROD    | 0.4-3μs | 389ns   | 10μs    | 生产环境 |

### 实测数据（PROD构建）

```
Consumer 0 received: AAPL Price: 182.78 Volume: 1216 Latency: 389ns
Consumer 1 received: AAPL Price: 182.78 Volume: 1216 Latency: 1170ns  
Consumer 2 received: AAPL Price: 182.78 Volume: 1216 Latency: 1250ns

=== 延迟统计 ===
最小延迟: 389ns (0.39μs)
最大延迟: 8450ns (8.45μs)  
平均延迟: 1825ns (1.83μs)
P50 延迟: 1200ns (1.20μs)
P95 延迟: 4300ns (4.30μs)
P99 延迟: 6800ns (6.80μs)
样本数量: 1000
```

### 吞吐量性能

- **单生产者**: 高达 100万 消息/秒
- **多消费者**: 每个消费者独立处理，总体吞吐量线性扩展
- **内存开销**: 约 64KB（1024消息 × 64字节对齐）

## 高级特性

### 共享内存管理

```cpp
// 检查现有共享内存兼容性
bool compatible = MessageQueue::isHeaderCompatible(
    "/my_queue", 1024, sizeof(MyMessage), 2);

// 强制重新创建共享内存
MessageQueue queue("/my_queue", 1024, sizeof(MyMessage), 2, 
                   true);  // force_recreate = true

// 仅连接模式（不创建新的）  
MessageQueue queue("/my_queue", 1024, sizeof(MyMessage), 2,
                   false,  // force_recreate = false
                   true);  // no_create = true
```

### 消息类型定义

```cpp
// 扩展消息类型
enum class MessageType : uint32_t {
    UNKNOWN = 0,
    MARKET_DATA,
    ORDER_UPDATE, 
    HEARTBEAT,
    TRADE_EXECUTION,  // 新增交易执行
    RISK_UPDATE,      // 新增风险更新
    // 添加更多类型...
};
```

### 错误处理

```cpp
try {
    MessageQueue queue("/my_queue", 1024, sizeof(MyMessage), 2);
    
    // 生产消息
    if (!queue.produce(MessageType::MARKET_DATA, &msg, sizeof(msg))) {
        std::cerr << "Queue is full, message dropped" << std::endl;
    }
    
} catch (const std::invalid_argument& e) {
    std::cerr << "Invalid parameters: " << e.what() << std::endl;
} catch (const std::runtime_error& e) {
    std::cerr << "Runtime error: " << e.what() << std::endl;
}
```

## 最佳实践

### 1. 消费者设计模式

```cpp
// 忙等待模式（最低延迟）
while (running) {
    if (queue.consume(buffer.data(), consumer_id)) {
        process_message(buffer.data());
    } else {
        __asm__ __volatile__("pause" ::: "memory");  // CPU pause指令
    }
}

// 混合模式（平衡延迟和CPU使用率）
while (running) {
    if (queue.consume(buffer.data(), consumer_id)) {
        process_message(buffer.data());
        consecutive_empty = 0;
    } else {
        consecutive_empty++;
        if (consecutive_empty > 1000) {
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        } else {
            __asm__ __volatile__("pause" ::: "memory");
        }
    }
}
```

### 2. 内存和性能优化

```cpp
// 预分配缓冲区，避免运行时分配
class HighPerformanceConsumer {
private:
    std::vector<char> buffer_;
    MessageQueue queue_;
    
public:
    HighPerformanceConsumer() : 
        buffer_(sizeof(MessageHeader) + MAX_PAYLOAD_SIZE),
        queue_("/my_queue", 1024, MAX_PAYLOAD_SIZE, 1) {}
        
    void consume_loop(uint32_t consumer_id) {
        while (running_) {
            if (queue_.consume(buffer_.data(), consumer_id)) {
                process_message_fast(buffer_.data());
            }
        }
    }
};
```

### 3. 监控和诊断

```cpp
// 队列状态监控
void monitor_queue_status(const MessageQueue& queue, uint32_t consumer_id) {
    std::cout << "Queue capacity: " << queue.capacity() << std::endl;
    std::cout << "Current size: " << queue.current_size(consumer_id) << std::endl;
    std::cout << "Is empty: " << queue.empty(consumer_id) << std::endl;
    std::cout << "Is full: " << queue.full() << std::endl;
    std::cout << "Max payload size: " << queue.max_payload_size() << std::endl;
}
```

## 故障排除

### 常见问题

1. **权限错误**: 
   ```bash
   # 确保有足够权限访问共享内存
   sudo chmod 666 /dev/shm/my_queue
   ```

2. **段错误**: 检查缓冲区大小是否足够大
   ```cpp
   // 确保缓冲区足够大
   std::vector<char> buffer(sizeof(MessageHeader) + queue.max_payload_size());
   ```

3. **消费者ID越界**: 确保 consumer_id < num_consumers
   ```cpp
   if (consumer_id >= num_consumers) {
       throw std::out_of_range("consumer_id out of range");
   }
   ```

4. **共享内存不兼容**: 清理现有共享内存
   ```bash
   # 查看现有共享内存
   ls -la /dev/shm/
   
   # 手动清理
   rm /dev/shm/my_queue
   ```

### 性能调优

1. **延迟过高**: 
   - 检查CPU绑定和实时优先级设置
   - 确认没有其他高负载进程干扰
   - 使用忙等待替代睡眠

2. **吞吐量不足**:
   - 增加队列容量
   - 检查消息大小是否合理
   - 优化消息处理逻辑

3. **CPU使用率过高**:
   - 在忙等待中添加适当的睡眠
   - 使用混合等待策略
   - 考虑使用CPU pause指令
