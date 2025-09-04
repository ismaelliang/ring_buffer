/**
 * @copyright Copyright (C) 2025 BOKE Inc.
 * @author Yuchen Liang
 * @date 2025-09-01
 * @brief 共享内存头部检查工具，用于检查和显示共享内存的配置信息。
 *
 * 该工具可以连接到现有的共享内存，读取并显示其头部信息，
 * 包括队列容量、元素大小、消费者数量和当前状态。
 */
#include "../message_queue.h"
#include <iomanip>
#include <iostream>
#include <string>
#include <sys/stat.h>

/**
 * @brief 打印程序用法信息。
 * @param program_name 程序的名称。
 */
void print_usage(const char *program_name) {
  std::cerr << "Usage: " << program_name << " <shared_memory_name>" << std::endl;
  std::cerr << "  shared_memory_name    Name of the shared memory to check (e.g., /market_data_queue)" << std::endl;
  std::cerr << std::endl;
  std::cerr << "Examples:" << std::endl;
  std::cerr << "  " << program_name << " /market_data_queue" << std::endl;
  std::cerr << "  " << program_name << " /my_custom_queue" << std::endl;
}

/**
 * @brief 显示共享内存头部信息。
 * @param header 指向RingBufferHeader的指针。
 */
void display_header_info(const RingBufferHeader *header) {
  std::cout << "\n=== 共享内存头部信息 ===" << std::endl;
  std::cout << "队列容量 (size):          " << header->size << " 条消息" << std::endl;
  std::cout << "元素大小 (element_size):  " << header->element_size << " 字节" << std::endl;
  std::cout << "消费者数量 (num_consumers): " << header->num_consumers << std::endl;
  std::cout << "生产者位置 (head):        " << header->head.load(std::memory_order_acquire) << std::endl;
  
  std::cout << "\n=== 消费者状态 ===" << std::endl;
  for (uint32_t i = 0; i < header->num_consumers; ++i) {
    uint32_t tail = header->consumer_tails[i].tail.load(std::memory_order_acquire);
    uint32_t head = header->head.load(std::memory_order_acquire);
    
    // 计算队列中的消息数量
    uint32_t queue_size;
    if (head >= tail) {
      queue_size = head - tail;
    } else {
      queue_size = header->size - tail + head;
    }
    
    std::cout << "消费者 " << std::setw(2) << i << ": "
              << "tail=" << std::setw(4) << tail 
              << ", 待消费=" << std::setw(4) << queue_size << " 条消息" << std::endl;
  }
  
  std::cout << "\n=== 内存布局信息 ===" << std::endl;
  size_t header_size = sizeof(RingBufferHeader) + 
                      (header->num_consumers - 1) * sizeof(RingBufferHeader::ConsumerTail);
  size_t data_size = header->size * header->element_size;
  size_t total_size = header_size + data_size;
  
  std::cout << "头部大小:   " << header_size << " 字节" << std::endl;
  std::cout << "数据区大小: " << data_size << " 字节" << std::endl;
  std::cout << "总大小:     " << total_size << " 字节 (" << std::fixed << std::setprecision(2) 
            << total_size / 1024.0 << " KB)" << std::endl;
}

/**
 * @brief 主函数，检查并显示共享内存头部信息。
 *
 * @param argc 命令行参数的数量。
 * @param argv 命令行参数数组。
 * @return 程序的退出状态码。
 */
int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Error: Wrong number of arguments." << std::endl;
    print_usage(argv[0]);
    return 1;
  }
  
  if (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") {
    print_usage(argv[0]);
    return 0;
  }
  
  const char *shm_name = argv[1];
  
  std::cout << "检查共享内存: " << shm_name << std::endl;
  
  try {
    // 尝试打开现有的共享内存
    int fd = shm_open(shm_name, O_RDONLY, 0666);
    if (fd == -1) {
      if (errno == ENOENT) {
        std::cerr << "错误: 共享内存 '" << shm_name << "' 不存在。" << std::endl;
        std::cerr << "请确保生产者或消费者已经创建了该共享内存。" << std::endl;
      } else {
        std::cerr << "错误: 无法打开共享内存 '" << shm_name << "': " 
                  << strerror(errno) << std::endl;
      }
      return 1;
    }
    
    std::cout << "✓ 成功打开共享内存" << std::endl;
    
    // 获取共享内存的大小
    struct stat stat_buf;
    if (fstat(fd, &stat_buf) == -1) {
      std::cerr << "错误: 无法获取共享内存大小: " << strerror(errno) << std::endl;
      close(fd);
      return 1;
    }
    
    std::cout << "✓ 共享内存总大小: " << stat_buf.st_size << " 字节" << std::endl;
    
    // 首先映射最小的头部大小来读取基本信息
    size_t min_header_size = sizeof(RingBufferHeader);
    void *temp_buffer = mmap(NULL, min_header_size, PROT_READ, MAP_SHARED, fd, 0);
    
    if (temp_buffer == MAP_FAILED) {
      std::cerr << "错误: 无法映射共享内存: " << strerror(errno) << std::endl;
      close(fd);
      return 1;
    }
    
    RingBufferHeader *header = static_cast<RingBufferHeader *>(temp_buffer);
    
    // 检查头部是否已初始化
    if (header->size == 0) {
      std::cout << "\n⚠️  共享内存存在但尚未初始化" << std::endl;
      std::cout << "这通常意味着创建者进程还没有完成初始化。" << std::endl;
      munmap(temp_buffer, min_header_size);
      close(fd);
      return 0;
    }
    
    // 获取消费者数量以计算完整的头部大小
    uint32_t num_consumers = header->num_consumers;
    munmap(temp_buffer, min_header_size);
    
    // 重新映射完整的头部（包括所有消费者tail）
    size_t full_header_size = sizeof(RingBufferHeader) + 
                             (num_consumers - 1) * sizeof(RingBufferHeader::ConsumerTail);
    
    if (full_header_size > static_cast<size_t>(stat_buf.st_size)) {
      std::cerr << "错误: 共享内存大小不足，可能已损坏" << std::endl;
      close(fd);
      return 1;
    }
    
    void *full_buffer = mmap(NULL, full_header_size, PROT_READ, MAP_SHARED, fd, 0);
    if (full_buffer == MAP_FAILED) {
      std::cerr << "错误: 无法映射完整头部: " << strerror(errno) << std::endl;
      close(fd);
      return 1;
    }
    
    header = static_cast<RingBufferHeader *>(full_buffer);
    
    std::cout << "✓ 成功读取头部信息" << std::endl;
    
    // 显示头部信息
    display_header_info(header);
    
    // 清理资源
    munmap(full_buffer, full_header_size);
    close(fd);
    
    std::cout << "\n✓ 检查完成" << std::endl;
    
  } catch (const std::exception &e) {
    std::cerr << "错误: " << e.what() << std::endl;
    return 1;
  }
  
  return 0;
}
