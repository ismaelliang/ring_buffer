/**
 * @file test_no_create.cpp
 * @brief 针对 no_create 选项功能的 Google Test 单元测试。
 *
 * 本文件包含对 MessageQueue 的 no_create 选项的全面测试，
 * 涉及多种场景，包括共享内存不存在、已存在且兼容的共享内存、
 * 不兼容的共享内存，以及参数校验等情况。
 *
 * 当参数 no_create 为 true 时，预期打开一个已存在的 shm,
 * 且传入的 容量、元素大小、消费者数量 与该 shm 的头部信息一致。
 * 如果一致则正常返回。
 * 如果不一致，则抛出异常。
 */

#include "market_data.h"
#include "message_queue.h"
#include <gtest/gtest.h>
#include <stdexcept>

class NoCreateTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // 在每次测试之前清理任何存在的共享内存
        shm_unlink(queue_name.c_str());
    }

    void TearDown() override {
        // 在每次测试之后清理共享内存
        shm_unlink(queue_name.c_str());
    }

    const std::string queue_name = "test_no_create_queue";
    static constexpr uint32_t queue_capacity = 1024;
    static constexpr uint32_t max_payload_size = 256;
    static constexpr uint32_t num_consumers = 2;
};

// 当共享内存不存在时，测试 no_create=true
// 预期抛出 std::runtime_error
TEST_F(NoCreateTest, NoCreateWithNonExistentSharedMemory) {
    EXPECT_THROW(
        { MessageQueue mq(queue_name, queue_capacity, max_payload_size, num_consumers, false, true); },
        std::runtime_error);
}

// 正常创建一个消息队列，然后连接 with no_create=true
// 预期不抛出异常 且容量相同
TEST_F(NoCreateTest, NoCreateWithExistingCompatibleSharedMemory) {
    MessageQueue mq1(queue_name, queue_capacity, max_payload_size, num_consumers);
    EXPECT_EQ(mq1.capacity(), queue_capacity);

    // 现在连接 with no_create=true - 应该成功
    EXPECT_NO_THROW({
        MessageQueue mq2(queue_name, queue_capacity, max_payload_size, num_consumers, false, true);
        EXPECT_EQ(mq2.capacity(), queue_capacity);
    });
}

// 当队列容量不兼容时，测试 no_create=true
// 预期抛出 std::runtime_error
TEST_F(NoCreateTest, NoCreateWithIncompatibleCapacity) {
    MessageQueue mq1(queue_name, queue_capacity, max_payload_size, num_consumers);

    // 尝试连接 with 不同的容量
    EXPECT_THROW(
        { MessageQueue mq2(queue_name, queue_capacity + 100, max_payload_size, num_consumers, false, true); },
        std::runtime_error);
}

// 当有效载荷大小不兼容时，测试 no_create=true
// 预期抛出 std::runtime_error
TEST_F(NoCreateTest, NoCreateWithIncompatiblePayloadSize) {
    // 创建初始队列
    MessageQueue mq1(queue_name, queue_capacity, max_payload_size, num_consumers);

    // 尝试连接 with 不同的有效载荷大小
    EXPECT_THROW(
        { MessageQueue mq2(queue_name, queue_capacity, max_payload_size + 100, num_consumers, false, true); },
        std::runtime_error);
}

// 当消费者数量不兼容时，测试 no_create=true
// 预期抛出 std::runtime_error
TEST_F(NoCreateTest, NoCreateWithIncompatibleConsumerCount) {
    MessageQueue mq1(queue_name, queue_capacity, max_payload_size, num_consumers);

    // 尝试连接 with 不同的消费者数量
    EXPECT_THROW(
        { MessageQueue mq2(queue_name, queue_capacity, max_payload_size, num_consumers + 1, false, true); },
        std::runtime_error);
}

// 当 force_recreate=true 和 no_create=true 时，测试
// 预期抛出 std::invalid_argument
TEST_F(NoCreateTest, ConflictingParameters) {
    EXPECT_THROW(
        { MessageQueue mq(queue_name, queue_capacity, max_payload_size, num_consumers, true, true); },
        std::invalid_argument);
}

// 测试基础功能 with no_create=true
TEST_F(NoCreateTest, BasicFunctionalityWithNoCreate) {
    MessageQueue mq1(queue_name, queue_capacity, max_payload_size, num_consumers);

    // 连接 with no_create=true 并测试功能
    MessageQueue mq2(queue_name, queue_capacity, max_payload_size, num_consumers, false, true);

    // 测试发布
    const char *test_data = "Hello, no_create!";
    EXPECT_TRUE(mq2.produce(MessageType::HEARTBEAT, test_data, strlen(test_data)));

    // 测试消费
    std::vector<char> buffer(sizeof(MessageHeader) + mq2.max_payload_size());
    EXPECT_TRUE(mq2.consume(buffer.data(), 0));

    GenericMessage *msg = reinterpret_cast<GenericMessage *>(buffer.data());
    EXPECT_EQ(msg->header.type, MessageType::HEARTBEAT);
    EXPECT_EQ(msg->header.payload_size, strlen(test_data));

    // 验证 payload 内容
    std::string received_data(reinterpret_cast<const char *>(msg->payload), msg->header.payload_size);
    EXPECT_EQ(received_data, "Hello, no_create!");
}

// 测试多个连接  with no_create=true
TEST_F(NoCreateTest, MultipleNoCreateConnections) {
    MessageQueue mq_initial(queue_name, queue_capacity, max_payload_size, num_consumers);

    // 创建多个连接 with no_create=true
    EXPECT_NO_THROW({
        MessageQueue mq1(queue_name, queue_capacity, max_payload_size, num_consumers, false, true);
        MessageQueue mq2(queue_name, queue_capacity, max_payload_size, num_consumers, false, true);

        // 预期容量相同
        EXPECT_EQ(mq1.capacity(), mq2.capacity());
        EXPECT_EQ(mq1.capacity(), queue_capacity);
    });
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
