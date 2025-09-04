# 高性能无锁环形队列

基于共享内存的无锁环形队列实现，专为高频交易(HFT)和低延迟应用设计。

## 项目特性

### 核心技术
- **无锁设计**: 使用原子操作避免互斥锁开销
- **共享内存**: 进程间零拷贝通信
- **高精度时钟**: 纳秒级延迟测量
- **CPU亲和性**: 绑定特定CPU核心减少缓存失效

### 性能优化
- **实时调度**: SCHED_FIFO调度策略
- **忙等待**: 消费者忙轮询避免睡眠开销
- **编译优化**: 多级优化方案支持不同使用场景
- **内存对齐**: 避免false sharing

## 依赖管理

本项目使用 vcpkg 管理依赖项，包括 Google Test 测试框架。

### 安装 vcpkg

1. 克隆 vcpkg 仓库：
```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
```

2. 设置环境变量：
```bash
export VCPKG_ROOT=/path/to/vcpkg
export PATH=$VCPKG_ROOT:$PATH
```

3. 安装项目依赖：
```bash
# 在项目根目录执行
vcpkg install
```

## 构建系统

本项目使用CMake构建系统，支持三种优化级别：

### Debug构建 (开发调试)
```bash
mkdir build-debug && cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```
**编译选项**: `-g -O0 -DDEBUG -Wall -Wextra -Wpedantic`
- 包含调试符号
- 禁用优化便于调试
- 启用所有编译警告

### PROD构建 (生产环境与极致性能)
```bash
mkdir build-prod && cd build-prod
cmake -DCMAKE_BUILD_TYPE=PROD ..
make -j$(nproc)
```
**编译选项**: `-O3 -march=native -mtune=native -DNDEBUG -flto -ffast-math -funroll-loops`
- 最高级别优化(-O3)
- 针对当前CPU架构优化
- 链接时优化(LTO)
- 快速数学运算
- 循环展开优化，减少分支跳转，提升指令级并行度

## 运行方式

### 基本运行
```bash
# 终端1: 启动消费者
./consumer

# 终端2: 启动生产者  
./producer
```

### 高性能运行 (需要root权限)
```bash
# 终端1: 以最高实时优先级运行消费者
sudo chrt -f 99 ./consumer

# 终端2: 以高实时优先级运行生产者
sudo chrt -f 95 ./producer
```

### CPU亲和性绑定
```bash
# 绑定到特定CPU核心
sudo taskset -c 0 ./producer &   # 生产者绑定核心0
sudo taskset -c 1 ./consumer &   # 消费者绑定核心1
```

## 测试

项目使用 Google Test 框架进行单元测试。

### 构建和运行测试
```bash
# 确保已安装 vcpkg 和依赖项
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# 运行所有测试
make test

# 或者直接运行测试可执行文件
./header_compatibility_test
```

### 测试覆盖范围
- 头部兼容性检查
- 共享内存重用逻辑
- 强制重建功能
- 不同参数的兼容性验证
- MmapRingBuffer 和 MessageQueue 的一致性测试

## 性能基准

### 延迟测试结果
| 构建类型 | 典型延迟 | 最佳延迟 | 最差延迟 | 适用场景 |
|---------|---------|---------|---------|----------|
| Debug | 10-50μs | 5μs | 100μs | 开发调试 |
| PROD | 0.4-3μs | 389ns | 10μs | 生产环境 / HFT交易 |

### 实测数据 (PROD构建)
```
Received: AAPL Price: 182.78 Volume: 1216 Latency: 389ns (0.39μs)
Received: AAPL Price: 182.79 Volume: 1217 Latency: 1170ns (1.17μs)
Received: AAPL Price: 182.80 Volume: 1218 Latency: 4535ns (4.54μs)
```

## 架构设计

### 组件说明
- **message_queue.h**: 无锁环形队列核心实现与消息结构定义
- **market_data.h**: 市场数据结构定义，用于示例应用
- **demo/producer.cpp**: 市场数据生产者，模拟市场数据生成并发布到队列
- **demo/consumer.cpp**: 市场数据消费者，从队列订阅数据并计算端到端延迟
- **cpu_affinity.h**: CPU亲和性绑定工具，用于优化高性能应用

### 数据流
```
Producer → 共享内存环形队列 → Consumer
    ↓              ↓              ↓
  时间戳          原子操作        延迟计算
```

## 系统要求

### 硬件要求
- **CPU**: 多核处理器 (推荐4核以上)
- **内存**: 最少4GB (推荐8GB以上)
- **架构**: x86_64

### 软件要求
- **编译器**: GCC 7+ 或 Clang 6+ (支持C++17)
- **CMake**: 3.10+
- **操作系统**: Linux (测试环境: Ubuntu 18.04+)

### 运行时权限
- **普通权限**: 基本功能正常
- **Root权限**: 启用实时调度和CPU绑定(推荐)

## 使用建议

### 开发阶段
1. 使用Debug构建进行功能开发
2. 使用GDB等工具调试: `gdb ./consumer`
3. 启用所有编译警告确保代码质量

### 测试阶段  
1. 使用Release构建测试性能
2. 监控系统资源使用情况
3. 验证在不同负载下的稳定性

### 生产部署
1. 使用ULTRAFAST构建获得最佳性能
2. 配置实时调度和CPU亲和性
3. 监控延迟分布和尾延迟

## 进阶优化

### 系统级优化
```bash
# 禁用CPU频率调节
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# 启用大页内存
echo 1024 | sudo tee /proc/sys/vm/nr_hugepages

# 减少中断延迟
echo 0 | sudo tee /proc/sys/kernel/timer_migration
```

### 编译器优化
- **PGO**: Profile-Guided Optimization
- **自定义优化**: 根据具体CPU型号调优
- **静态链接**: 减少动态库加载开销

## 故障排除

### 常见问题
1. **权限不足**: 使用sudo运行或调整用户权限
2. **CPU核心不足**: 确保至少有2个CPU核心
3. **内存不足**: 检查可用内存和共享内存限制
4. **编译错误**: 确认编译器版本和C++17支持

### 性能调优
1. **延迟过高**: 检查系统负载和其他进程干扰
2. **吞吐量不足**: 调整环形队列大小
3. **CPU使用率高**: 考虑使用混合轮询策略
