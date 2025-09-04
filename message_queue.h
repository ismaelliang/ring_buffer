/**
 * @file message_queue.h
 * @brief 这是一个 C++ header-only 库，提供了基于共享内存的无锁消息队列。
 *        它支持多进程生产者-消费者模型，并针对低延迟和高吞吐量进行了优化。
 *
 * @section app_usage 应用层使用示例
 *
 * @subsection define_message 1. 定义消息结构
 * 首先，定义您的应用程序特定的消息结构。所有消息都应包含一个 `MessageHeader`
 * 作为前缀。 例如：
 * @code
 * #include "message_queue.h"
 *
 * // 继承 MessageHeader 以确保您的消息包含必要的信息
 * struct MyMarketDataMessage {
 *     MessageHeader header; // 所有消息必须包含此头部
 *     double price;
 *     uint32_t quantity;
 *     char symbol[8];
 * };
 * @endcode
 *
 * @subsection producer_usage 2. 生产者 (Producer) 使用
 * 生产者负责创建并发布消息到消息队列。
 * @code
 * #include "message_queue.h"
 * #include <iostream>
 * #include <thread>
 *
 * void producer_main() {
 *     // 1. 创建或连接到消息队列
 *     // 参数: 队列名称, 队列容量 (消息数量), 最大负载大小
 * (单个消息有效载荷的最大字节数) MessageQueue queue("my_shared_queue", 1024,
 * sizeof(MyMarketDataMessage) - sizeof(MessageHeader));
 *
 *     // 2. 准备消息
 *     MyMarketDataMessage msg;
 *     // 填充您的消息数据，包括消息类型
 *     // msg.header.type 和 msg.header.payload_size 将由 publish 方法自动填充
 *     msg.price = 100.0;
 *     msg.quantity = 10;
 *     strncpy(msg.symbol, "AAPL", 8);
 *
 *     // 3. 发布消息
 *     if (queue.publish(MessageType::MARKET_DATA, &msg.price,
 * sizeof(MyMarketDataMessage) - sizeof(MessageHeader))) { std::cout <<
 * "Producer: Published market data." << std::endl; } else { std::cerr <<
 * "Producer: Queue is full, failed to publish." << std::endl;
 *     }
 * }
 * @endcode
 *
 * @subsection consumer_usage 3. 消费者 (Consumer) 使用
 * 消费者负责从消息队列中订阅并处理消息。
 * @code
 * #include "message_queue.h"
 * #include <iostream>
 * #include <vector>
 *
 * void consumer_main() {
 *     // 1. 创建或连接到消息队列 (名称、容量和最大负载大小必须与生产者一致)
 *     MessageQueue queue("my_shared_queue", 1024, sizeof(MyMarketDataMessage) -
 * sizeof(MessageHeader));
 *
 *     // 2. 准备一个足够大的缓冲区来接收消息
 *     // 缓冲区大小必须至少为 sizeof(MessageHeader) + max_payload_size
 *     std::vector<char> recv_buffer(sizeof(MessageHeader) +
 * queue.max_payload_size());
 *
 *     // 3. 订阅消息
 *     if (queue.subscribe(recv_buffer.data())) {
 *         // 4. 将接收到的通用消息转换为您的特定消息结构
 *         GenericMessage* generic_msg =
 * reinterpret_cast<GenericMessage*>(recv_buffer.data());
 *         // 现在可以访问头部信息
 *         std::cout << "Consumer: Received message type: " <<
 * static_cast<uint32_t>(generic_msg->header.type)
 *                   << ", payload size: " << generic_msg->header.payload_size
 * << std::endl;
 *
 *         // 如果消息类型是您的
 * MyMarketDataMessage，则可以进一步转换和访问其内容 if
 * (generic_msg->header.type == MessageType::MARKET_DATA) { MyMarketDataMessage*
 * market_data = reinterpret_cast<MyMarketDataMessage*>(generic_msg->payload);
 *             // 注意: 实际的 payload 是从 GenericMessage.payload 开始的
 *             // 所以需要偏移 MessageHeader 的大小来获取实际的
 * MyMarketDataMessage 结构
 *             // 另一种方法是直接将 recv_buffer 转换为
 * MyMarketDataMessage*，但这需要确保 MyMarketDataMessage 的布局正确
 *             // 这里我们假设 MyMarketDataMessage 包含了 MessageHeader
 *             market_data =
 * reinterpret_cast<MyMarketDataMessage*>(recv_buffer.data()); std::cout << "
 * Market Data - Price: " << market_data->price
 *                       << ", Quantity: " << market_data->quantity
 *                       << ", Symbol: " << market_data->symbol << std::endl;
 *         }
 *     } else {
 *         std::cout << "Consumer: Queue is empty." << std::endl;
 *     }
 * }
 * @endcode
 *
 * @section compile_optimization 4. 编译优化建议
 * 为了达到预期的低延迟和高吞吐量性能，使用此 header-only 库的外部项目
 * 在编译时应启用以下或类似的编译器优化标志：
 * - `-O3`: 启用最高级别的优化。
 * - `-march=native -mtune=native`: 针对编译机器的 CPU 架构进行优化。
 * - `-DNDEBUG`: 禁用断言和调试代码。
 * - `-flto`: 启用链接时优化 (Link Time Optimization)。
 * - `-ffast-math`: 启用激进的浮点运算优化（使用时需谨慎，可能影响精度）。
 * - `-funroll-loops`: 展开循环以减少分支预测失败和循环开销。
 *
 * 例如，在 CMake 中，可以设置：
 * @code cmake
 * set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -mtune=native -DNDEBUG -flto
 * -ffast-math -funroll-loops") set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
 * target_link_libraries(YOUR_TARGET PRIVATE rt Threads::Threads)
 * @endcode
 * 同时，需要链接 `rt` 库（对于 `shm_open` 等共享内存操作）和 `pthread` 库。
 *
 * 此外，为关键线程设置 CPU 亲和性 (`CPUAffinity::bindToCPU`) 和实时优先级
 * (`CPUAffinity::setRealtimePriority`)
 * 可以在高负载或多核环境中进一步提升性能稳定性。
 */
#pragma once

#include <atomic> // Required for std::atomic
#include <chrono> // For high-resolution timestamps
#include <cstdint>
#include <cstring>  // For memcpy
#include <fcntl.h>  // Required for O_CREAT, O_RDWR
#include <iostream> // For debugging print statements
#include <stdexcept>
#include <string>
#include <sys/mman.h> // Required for shm_open, mmap, munmap, PROT_*, MAP_*
#include <sys/stat.h> // Required for fstat
#include <unistd.h>   // Required for ftruncate, close
#include <vector>     // Required for std::vector

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <pthread.h>
#include <sched.h>

/**
 * @brief 定义不同消息类型的枚举（用户可扩展）。
 */
enum class MessageType : uint32_t {
  UNKNOWN = 0,  /**< 未知消息类型。 */
  MARKET_DATA,  /**< 市场数据消息。 */
  ORDER_UPDATE, /**< 订单更新消息。 */
  HEARTBEAT,    /**< 心跳消息。 */
                // 根据应用需求在此处添加更多消息类型。
};

/**
 * @brief 通用消息头部。
 *        所有消息在缓冲区中都以此头部作为前缀。
 */
struct MessageHeader {
  MessageType type;      ///< 消息类型。
  uint32_t payload_size; ///< 此头部之后实际数据的大小。
  uint64_t timestamp; ///< 消息创建时的高精度时间戳（纳秒）。
  uint64_t sequence_num; ///< 用于排序和追踪的唯一序列号。
};

/**
 * @brief 用于固定大小消息的通用消息结构体。
 *        消息的总大小将是 `sizeof(MessageHeader) + max_payload_size`。
 *        用户通常会将 `payload` 缓冲区转换为其特定的消息结构体。
 */
struct alignas(64) GenericMessage { ///< 为提升性能，按缓存行对齐。
  MessageHeader header;             ///< 消息头部。
  /**
   * @brief 实际消息数据的占位符。
   *        此数组的大小将在创建 `MessageQueue` 时由 `max_payload_size`
   * 参数确定。 这是一个柔性数组（flexible array
   * member）概念，但对于固定大小消息，它只是一个缓冲区。
   */
  char payload[1]; ///< 实际大小在运行时由队列的 `max_payload_size` 确定。
};

/**
 * @brief CPU 亲和性和实时优先级工具类。
 */
class CPUAffinity {
public:
  /**
   * @brief 绑定当前线程到指定的 CPU 核心。
   * @param cpu_id 要绑定的 CPU 核心 ID。
   * @return 如果绑定成功，则为 `true`；否则为 `false`。
   */
  static bool bindToCPU(int cpu_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);

    int result =
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
      std::cerr << "Failed to bind to CPU " << cpu_id << ": " << strerror(errno)
                << std::endl;
      return false;
    }

    std::cout << "Thread bound to CPU " << cpu_id << std::endl;
    return true;
  }

  /**
   * @brief 设置进程的实时优先级。
   * @param priority 要设置的实时优先级，默认为 99。
   * @return 如果设置成功，则为 `true`；否则为 `false`。
   */
  static bool setRealtimePriority(int priority = 99) {
    struct sched_param param;
    param.sched_priority = priority;

    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
      std::cerr << "Failed to set real-time priority (需要root权限): "
                << strerror(errno) << std::endl;
      return false;
    }

    std::cout << "Set real-time priority: " << priority << std::endl;
    return true;
  }

  /**
   * @brief 获取系统中的 CPU 核心数量。
   * @return CPU 核心数量。
   */
  static int getCPUCount() { return sysconf(_SC_NPROCESSORS_ONLN); }
};

/**
 * @brief 环形缓冲区头部信息，按缓存行对齐以防止伪共享。
 */
struct alignas(64) RingBufferHeader {
  std::atomic<uint32_t> head; ///< 写入位置。
  uint32_t size;              ///< 缓冲区总大小（元素数量）。
  uint32_t element_size;      ///< 每个元素的大小。
  uint32_t num_consumers;     ///< 消费者数量
  struct alignas(64) ConsumerTail {
    std::atomic<uint32_t> tail;
  };
  ConsumerTail consumer_tails[1]; ///< 读取位置 (为每个消费者独立维护)。
};

/**
 * @brief 使用 `mmap` 实现共享内存的环形缓冲区类。
 */
class MmapRingBuffer {
public:
  /**
   * @brief 检查现有共享内存的头部是否与预期参数匹配。
   * @param name 共享内存的唯一标识符。
   * @param element_count 期望的元素数量。
   * @param element_size 期望的元素大小。
   * @param num_consumers 期望的消费者数量。
   * @return 如果头部匹配，则返回 true；否则返回 false。
   */
  static bool isHeaderCompatible(const char* name, uint32_t element_count,
                                uint32_t element_size, uint32_t num_consumers) {
    // 尝试打开现有的共享内存
    int fd = shm_open(name, O_RDONLY, 0666);
    if (fd == -1) {
      // 共享内存不存在，需要创建新的
      return false;
    }

    // 计算期望的头部大小和总大小
    size_t expected_header_size = sizeof(RingBufferHeader) + 
                                  (num_consumers - 1) * sizeof(RingBufferHeader::ConsumerTail);
    size_t expected_total_size = expected_header_size + element_count * element_size;

    // 获取现有共享内存的大小
    struct stat shm_stat;
    if (fstat(fd, &shm_stat) == -1) {
      close(fd);
      return false;
    }

    // 如果大小不匹配，直接返回 false
    if (static_cast<size_t>(shm_stat.st_size) != expected_total_size) {
      close(fd);
      return false;
    }

    // 映射内存以读取头部信息
    void* buffer = mmap(NULL, expected_header_size, PROT_READ, MAP_SHARED, fd, 0);
    if (buffer == MAP_FAILED) {
      close(fd);
      return false;
    }

    RingBufferHeader* header = static_cast<RingBufferHeader*>(buffer);
    
    // 检查头部字段是否匹配
    bool is_compatible = (header->size == element_count) &&
                        (header->element_size == element_size) &&
                        (header->num_consumers == num_consumers);

    // 清理资源
    munmap(buffer, expected_header_size);
    close(fd);

    return is_compatible;
  }

  /**
   * @brief 构造函数，创建或连接到一个命名的环形缓冲区。
   * @param name 共享内存的唯一标识符。
   * @param element_count 缓冲区中元素的总数量。
   * @param element_size 每个元素的大小（字节）。
   * @param num_consumers 消费者数量。
   * @param force_recreate 如果为 true，强制重新创建共享内存；如果为 false，在头部兼容时重用现有共享内存。
   */
  MmapRingBuffer(const char *name, uint32_t element_count,
                 uint32_t element_size, uint32_t num_consumers, 
                 bool force_recreate = false)
      : name_(name), num_consumers_(num_consumers) {
    std::cout << "MmapRingBuffer: Creating/attaching to buffer: " << name
              << std::endl;
    std::cout << "  element_count: " << element_count
              << ", element_size: " << element_size << std::endl;
    std::cout << "  num_consumers: " << num_consumers << std::endl;
    std::cout << "  force_recreate: " << force_recreate << std::endl;

    // 检查现有共享内存是否与当前参数兼容
    bool header_compatible = false;
    if (!force_recreate) {
      header_compatible = isHeaderCompatible(name_, element_count, element_size, num_consumers);
      std::cout << "  Header compatibility check result: " << header_compatible << std::endl;
    }

    // 如果需要强制重新创建或头部不兼容，则先删除现有共享内存
    if (force_recreate || !header_compatible) {
      std::cout << "  Unlinking existing shared memory (if any)..." << std::endl;
      shm_unlink(name_);  // 忽略错误，因为共享内存可能不存在
    }

    // 计算所需的总大小。
    // RingBufferHeader 的实际大小，包括了灵活数组成员 consumer_tails
    size_t header_actual_size =
        sizeof(RingBufferHeader) +
        (num_consumers - 1) * sizeof(RingBufferHeader::ConsumerTail);
    total_size_ = header_actual_size + element_count * element_size;
    std::cout << "  Calculated total_size_: " << total_size_ << std::endl;

    // 创建或打开共享内存。
    fd_ = shm_open(name_, O_CREAT | O_RDWR, 0666);
    if (fd_ == -1) {
      throw std::runtime_error(std::string("shm_open failed: ") +
                               strerror(errno));
    }
    std::cout << "  shm_open successful, fd_: " << fd_ << std::endl;

    // 设置共享内存大小。
    if (ftruncate(fd_, total_size_) == -1) {
      close(fd_);
      throw std::runtime_error(std::string("ftruncate failed: ") +
                               strerror(errno));
    }
    std::cout << "  ftruncate successful, size: " << total_size_ << std::endl;

    // 映射内存。
    buffer_ =
        mmap(NULL, total_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (buffer_ == MAP_FAILED) {
      close(fd_);
      throw std::runtime_error(std::string("mmap failed: ") + strerror(errno));
    }
    std::cout << "  mmap successful, buffer_ address: " << buffer_ << std::endl;

    // 初始化头部。
    header_ = static_cast<RingBufferHeader *>(buffer_);
    if (header_->size == 0) { // 首次创建时初始化。
      std::cout << "  Initializing new buffer header." << std::endl;
      header_->head.store(0, std::memory_order_relaxed);
      // 初始化所有消费者的 tail 指针
      for (uint32_t i = 0; i < num_consumers_; ++i) {
        header_->consumer_tails[i].tail.store(0, std::memory_order_relaxed);
      }
      header_->size = element_count;
      header_->element_size = element_size;
      header_->num_consumers = num_consumers_;
    }
    std::cout << "  Header initialized. header_->size: " << header_->size
              << ", header_->element_size: " << header_->element_size
              << std::endl;
  }

  /**
   * @brief 析构函数，处理共享内存清理。
   */
  ~MmapRingBuffer() {
    if (buffer_ && buffer_ != MAP_FAILED) {
      munmap(buffer_, total_size_);
    }
    if (fd_ != -1) {
      close(fd_);
      shm_unlink(name_); ///< 解除共享内存对象的链接。
    }
  }

  /**
   * @brief 禁用拷贝构造函数。
   */
  MmapRingBuffer(const MmapRingBuffer &) = delete;
  /**
   * @brief 禁用赋值运算符。
   */
  MmapRingBuffer &operator=(const MmapRingBuffer &) = delete;

private:
  void *buffer_;             ///< 指向映射内存的指针。
  RingBufferHeader *header_; ///< 环形缓冲区头部。
  int fd_;                   ///< 共享内存文件描述符。
  const char *name_;         ///< 共享内存名称。
  size_t total_size_;        ///< 共享内存总大小。
  uint32_t num_consumers_;   ///< 消费者数量。
public:
  /**
   * @brief 将数据推入环形缓冲区。
   * @param data 指向要写入数据的指针。
   * @return 如果成功推入，则为 `true`；如果缓冲区已满，则为 `false`。
   */
  bool push(const void *data) {
    uint32_t current_head = header_->head.load(std::memory_order_relaxed);
    // 找到所有消费者尾部的最小值，生产者不能覆盖这个最小值。
    uint32_t min_consumer_tail =
        header_->consumer_tails[0].tail.load(std::memory_order_acquire);
    for (uint32_t i = 1; i < header_->num_consumers; ++i) {
      uint32_t consumer_tail =
          header_->consumer_tails[i].tail.load(std::memory_order_acquire);
      if (consumer_tail < min_consumer_tail) {
        min_consumer_tail = consumer_tail;
      }
    }

    uint32_t next_head = (current_head + 1) % header_->size;

    if (next_head == min_consumer_tail) { ///< 缓冲区已满。
      return false;
    }

    // 计算写入位置。
    char *element_ptr = static_cast<char *>(buffer_) +
                        (sizeof(RingBufferHeader) +
                         (num_consumers_ - 1) * sizeof(std::atomic<uint32_t>)) +
                        current_head * header_->element_size;

    // 写入数据。
    memcpy(element_ptr, data, header_->element_size);

    // 以 `release` 内存顺序更新头部指针。
    header_->head.store(next_head, std::memory_order_release);

    return true;
  }

  /**
   * @brief 从环形缓冲区中弹出数据。
   * @param data 指向接收弹出数据的缓冲区的指针。
   * @return 如果成功弹出，则为 `true`；如果缓冲区为空，则为 `false`。
   */
  bool pop(void *data, uint32_t consumer_id) {
    if (consumer_id >= header_->num_consumers) {
      throw std::out_of_range("consumer_id out of range.");
    }

    uint32_t current_tail = header_->consumer_tails[consumer_id].tail.load(
        std::memory_order_relaxed);
    uint32_t current_head = header_->head.load(std::memory_order_acquire);

    if (current_tail == current_head) { ///< 缓冲区为空。
      return false;
    }

    // 计算读取位置。
    const char *element_ptr =
        static_cast<char *>(buffer_) +
        (sizeof(RingBufferHeader) +
         (num_consumers_ - 1) * sizeof(std::atomic<uint32_t>)) +
        current_tail * header_->element_size;

    // 读取数据。
    memcpy(data, element_ptr, header_->element_size);

    // 以 `release` 内存顺序更新尾部指针。
    uint32_t next_tail = (current_tail + 1) % header_->size;
    header_->consumer_tails[consumer_id].tail.store(next_tail,
                                                    std::memory_order_release);

    return true;
  }

  /**
   * @brief 检查缓冲区是否为空。
   * @return 如果缓冲区为空，则为 `true`；否则为 `false`。
   */
  bool empty(uint32_t consumer_id) const {
    if (consumer_id >= header_->num_consumers) {
      throw std::out_of_range("consumer_id out of range.");
    }
    return header_->head.load(std::memory_order_acquire) ==
           header_->consumer_tails[consumer_id].tail.load(
               std::memory_order_acquire);
  }

  /**
   * @brief 检查缓冲区是否已满。
   * @return 如果缓冲区已满，则为 `true`；否则为 `false`。
   */
  bool full() const {
    uint32_t current_head = header_->head.load(std::memory_order_relaxed);
    uint32_t min_consumer_tail =
        header_->consumer_tails[0].tail.load(std::memory_order_acquire);
    for (uint32_t i = 1; i < header_->num_consumers; ++i) {
      uint32_t consumer_tail =
          header_->consumer_tails[i].tail.load(std::memory_order_acquire);
      if (consumer_tail < min_consumer_tail) {
        min_consumer_tail = consumer_tail;
      }
    }
    uint32_t next_head = (current_head + 1) % header_->size;
    return next_head == min_consumer_tail;
  }

  /**
   * @brief 获取缓冲区的容量（元素数量）。
   * @return 缓冲区的容量。
   */
  uint32_t capacity() const { return header_->size; }

  /**
   * @brief 获取缓冲区当前存储的元素数量。
   * @return 缓冲区当前存储的元素数量。
   */
  uint32_t current_size(uint32_t consumer_id) const {
    if (consumer_id >= header_->num_consumers) {
      throw std::out_of_range("consumer_id out of range.");
    }
    uint32_t current_head = header_->head.load(std::memory_order_acquire);
    uint32_t current_tail = header_->consumer_tails[consumer_id].tail.load(
        std::memory_order_acquire);
    if (current_head >= current_tail) {
      return current_head - current_tail;
    } else {
      return header_->size - current_tail + current_head;
    }
  }
};

/**
 * @brief 消息队列类，基于 `MmapRingBuffer` 实现。
 */
class MessageQueue {
public:
  /**
   * @brief 检查现有共享内存的头部是否与预期参数匹配。
   * @param name 共享内存队列的唯一标识符。
   * @param queue_capacity 期望的队列容量。
   * @param max_payload_size 期望的最大有效载荷大小。
   * @param num_consumers 期望的消费者数量。
   * @return 如果头部匹配，则返回 true；否则返回 false。
   */
  static bool isHeaderCompatible(const std::string &name, uint32_t queue_capacity,
                                uint32_t max_payload_size, uint32_t num_consumers) {
    // 尝试打开现有的共享内存
    int fd = shm_open(name.c_str(), O_RDONLY, 0666);
    if (fd == -1) {
      // 共享内存不存在，需要创建新的
      return false;
    }

    // 计算期望的元素大小和总大小
    uint32_t expected_element_size = sizeof(MessageHeader) + max_payload_size;
    size_t expected_header_size = sizeof(RingBufferHeader) + 
                                  (num_consumers - 1) * sizeof(RingBufferHeader::ConsumerTail);
    size_t expected_total_size = expected_header_size + queue_capacity * expected_element_size;

    // 获取现有共享内存的大小
    struct stat shm_stat;
    if (fstat(fd, &shm_stat) == -1) {
      close(fd);
      return false;
    }

    // 如果大小不匹配，直接返回 false
    if (static_cast<size_t>(shm_stat.st_size) != expected_total_size) {
      close(fd);
      return false;
    }

    // 映射内存以读取头部信息
    void* buffer = mmap(NULL, expected_header_size, PROT_READ, MAP_SHARED, fd, 0);
    if (buffer == MAP_FAILED) {
      close(fd);
      return false;
    }

    RingBufferHeader* header = static_cast<RingBufferHeader*>(buffer);
    
    // 检查头部字段是否匹配
    bool is_compatible = (header->size == queue_capacity) &&
                        (header->element_size == expected_element_size) &&
                        (header->num_consumers == num_consumers);

    // 清理资源
    munmap(buffer, expected_header_size);
    close(fd);

    return is_compatible;
  }

  /**
   * @brief 构造函数，用于创建或连接到命名的消息队列。
   * @param name 共享内存队列的唯一标识符。
   * @param queue_capacity 队列可以容纳的 *消息* 最大数量（非字节）。
   * @param max_payload_size 每个消息的用户数据（有效载荷）的最大字节数。
   *                         底层环形缓冲区中实际的元素大小将是
   * `sizeof(MessageHeader) + max_payload_size`。
   * @param num_consumers 消费者数量。
   * @param force_recreate 如果为 true，强制重新创建共享内存；如果为 false，在头部兼容时重用现有共享内存。
   */
  MessageQueue(const std::string &name, uint32_t queue_capacity,
               uint32_t max_payload_size, uint32_t num_consumers, 
               bool force_recreate = false)
      : max_payload_size_(max_payload_size),
        total_message_size_(sizeof(MessageHeader) + max_payload_size),
        buffer_(name.c_str(), queue_capacity, total_message_size_,
                num_consumers, force_recreate), // Pass num_consumers and force_recreate
        next_sequence_num_(0),
        num_consumers_(num_consumers) ///< 在此处初始化 `buffer_`。
  {
    if (max_payload_size_ == 0) {
      throw std::invalid_argument("max_payload_size cannot be zero.");
    }
    if (total_message_size_ >
        0xFFFFFFFF) { ///< 检查 `total_message_size_` 是否溢出（如果它是
                      ///< `uint32_t`）。
      throw std::overflow_error("Total message size exceeds maximum allowed.");
    }
  }

  /**
   * @brief 析构函数（处理共享内存清理）。
   */
  ~MessageQueue() = default;

  /**
   * @brief 将消息发布到队列。
   * @param type 消息的 `MessageType`。
   * @param data 指向用户原始消息有效载荷的指针。
   * @param data_size 用户有效载荷的实际大小。必须 `<= max_payload_size`。
   * @return 如果消息成功发布，则为 `true`；如果队列已满，则为 `false`。
   */
  bool publish(MessageType type, const void *data, uint32_t data_size) {
    if (data_size > max_payload_size_) {
      throw std::invalid_argument(
          "Payload size exceeds max_payload_size set for the queue.");
    }

    std::vector<char> full_message_buffer(total_message_size_);
    GenericMessage *msg =
        reinterpret_cast<GenericMessage *>(full_message_buffer.data());

    // 填充头部。
    msg->header.type = type;
    msg->header.payload_size = data_size;
    msg->header.timestamp = getHighResolutionTimestamp();
    msg->header.sequence_num = next_sequence_num_++; ///< 递增序列号。

    // 拷贝有效载荷数据。
    if (data && data_size > 0) {
      std::memcpy(msg->payload, data, data_size);
    }

    // 将完整消息推入底层环形缓冲区。
    return buffer_.push(msg);
  }

  /**
   * @brief 从队列订阅消息。
   * @param message_buffer 调用者提供的缓冲区，用于接收完整的 `GenericMessage`
   *                       （包括头部和有效载荷）。缓冲区必须足够大，
   *                       以容纳 `sizeof(MessageHeader) + max_payload_size`。
   * @param consumer_id 消费者的唯一标识符（0 到 `num_consumers - 1`）。
   * @return 如果成功检索到消息，则为 `true`；如果队列为空，则为 `false`。
   */
  bool subscribe(void *message_buffer, uint32_t consumer_id) {
    if (!message_buffer) {
      throw std::invalid_argument("Provided message_buffer is null.");
    }
    if (consumer_id >= num_consumers_) {
      throw std::out_of_range(
          "consumer_id out of range for this MessageQueue instance.");
    }
    // 从底层环形缓冲区弹出完整消息。
    return buffer_.pop(message_buffer, consumer_id);
  }

  /**
   * @brief 查询队列状态：是否为空。
   * @param consumer_id 消费者的唯一标识符（0 到 `num_consumers - 1`）。
   * @return 如果队列为空，则为 `true`；否则为 `false`。
   */
  bool empty(uint32_t consumer_id) const {
    if (consumer_id >= num_consumers_) {
      throw std::out_of_range(
          "consumer_id out of range for this MessageQueue instance.");
    }
    return buffer_.empty(consumer_id);
  }

  /**
   * @brief 查询队列状态：是否已满。
   * @return 如果队列已满，则为 `true`；否则为 `false`。
   */
  bool full() const {
    // 队列满是由生产者逻辑决定的，不需要 consumer_id
    return buffer_.full();
  }

  /**
   * @brief 获取队列的容量（消息数量）。
   * @return 队列的容量。
   */
  uint32_t capacity() const { return buffer_.capacity(); }

  /**
   * @brief 获取队列当前存储的消息数量。
   * @param consumer_id 消费者的唯一标识符（0 到 `num_consumers - 1`）。
   * @return 队列当前存储的消息数量。
   */
  uint32_t current_size(uint32_t consumer_id) const {
    if (consumer_id >= num_consumers_) {
      throw std::out_of_range(
          "consumer_id out of range for this MessageQueue instance.");
    }
    return buffer_.current_size(consumer_id);
  }

  /**
   * @brief 获取队列允许的最大有效载荷大小。
   * @return 队列允许的最大有效载荷大小。
   */
  uint32_t max_payload_size() const { return max_payload_size_; }

private:
  /**
   * @brief 辅助函数，获取高精度时间戳。
   * @return 高精度时间戳。
   */
  static uint64_t getHighResolutionTimestamp() {
    return std::chrono::high_resolution_clock::now().time_since_epoch().count();
  }

  uint32_t max_payload_size_; ///< 此队列实例的最大有效载荷大小。
  uint32_t
      total_message_size_; ///< `sizeof(MessageHeader) + max_payload_size`。
  MmapRingBuffer buffer_;  ///< 底层环形缓冲区。
  uint64_t next_sequence_num_; ///< 用于为已发布消息分配唯一序列号（每个实例）。
  uint32_t num_consumers_; ///< 消费者数量。
};