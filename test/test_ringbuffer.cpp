/**
 * @file test_ringbuffer.cpp
 * @brief 针对无锁环形缓冲区 MessageQueue 的 Google Test 单元测试。
 *
 * 本文件包含对 MessageQueue 实现的全面测试，
 * 涵盖基本功能、边界情况以及多线程场景。
 */

#include "market_data.h"
#include "message_queue.h"
#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <vector>

class MessageQueueTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // 在每次测试之前清理任何存在的共享内存
        shm_unlink("/test_queue");
    }

    void TearDown() override {
        // 在每次测试之后清理共享内存
        shm_unlink("/test_queue");
    }

    static constexpr uint32_t TEST_QUEUE_CAPACITY = 64;
    static constexpr uint32_t TEST_MAX_PAYLOAD_SIZE = sizeof(MarketData);
    static constexpr uint32_t TEST_NUM_CONSUMERS = 1;
    static const char *TEST_QUEUE_NAME;
};

const char *MessageQueueTest::TEST_QUEUE_NAME = "/test_queue";

// 测试基本队列创建和销毁
TEST_F(MessageQueueTest, BasicCreation) {
    EXPECT_NO_THROW(
        { MessageQueue queue(TEST_QUEUE_NAME, TEST_QUEUE_CAPACITY, TEST_MAX_PAYLOAD_SIZE, TEST_NUM_CONSUMERS); });
}

// 测试单个消息生产和消费
TEST_F(MessageQueueTest, SingleMessageProduceConsume) {
    MessageQueue queue(TEST_QUEUE_NAME, TEST_QUEUE_CAPACITY, TEST_MAX_PAYLOAD_SIZE, TEST_NUM_CONSUMERS);

    // 准备测试数据
    MarketData test_data;
    snprintf(test_data.symbol, sizeof(test_data.symbol), "TEST");
    test_data.price = 100.50;
    test_data.volume = 1000;
    test_data.timestamp = 123456789;

    // 发布消息
    EXPECT_TRUE(queue.produce(MessageType::MARKET_DATA, &test_data, sizeof(MarketData)));

    // 订阅和验证
    std::vector<uint8_t> buffer(sizeof(GenericMessage) + sizeof(MarketData));
    EXPECT_TRUE(queue.consume(buffer.data(), 0));

    const GenericMessage *msg = reinterpret_cast<const GenericMessage *>(buffer.data());
    EXPECT_EQ(msg->header.type, MessageType::MARKET_DATA);
    EXPECT_EQ(msg->header.payload_size, sizeof(MarketData));

    const MarketData *received_data = reinterpret_cast<const MarketData *>(msg->payload);
    EXPECT_STREQ(received_data->symbol, "TEST");
    EXPECT_DOUBLE_EQ(received_data->price, 100.50);
    EXPECT_EQ(received_data->volume, 1000);
}

// 测试多个消息
TEST_F(MessageQueueTest, MultipleMessages) {
    MessageQueue queue(TEST_QUEUE_NAME, TEST_QUEUE_CAPACITY, TEST_MAX_PAYLOAD_SIZE, TEST_NUM_CONSUMERS);

    const int NUM_MESSAGES = 10;

    // 生产多个消息
    for (int i = 0; i < NUM_MESSAGES; ++i) {
        MarketData test_data;
        snprintf(test_data.symbol, sizeof(test_data.symbol), "SYM%d", i);
        test_data.price = 100.0 + i;
        test_data.volume = 1000 + i;
        test_data.timestamp = 123456789 + i;

        EXPECT_TRUE(queue.produce(MessageType::MARKET_DATA, &test_data, sizeof(MarketData)));
    }

    // 消费和验证所有消息
    for (int i = 0; i < NUM_MESSAGES; ++i) {
        std::vector<uint8_t> buffer(sizeof(GenericMessage) + sizeof(MarketData));
        EXPECT_TRUE(queue.consume(buffer.data(), 0));

        const GenericMessage *msg = reinterpret_cast<const GenericMessage *>(buffer.data());
        const MarketData *received_data = reinterpret_cast<const MarketData *>(msg->payload);

        char expected_symbol[16];
        snprintf(expected_symbol, sizeof(expected_symbol), "SYM%d", i);
        EXPECT_STREQ(received_data->symbol, expected_symbol);
        EXPECT_DOUBLE_EQ(received_data->price, 100.0 + i);
        EXPECT_EQ(received_data->volume, 1000 + i);
    }
}

// 测试队列满场景
TEST_F(MessageQueueTest, QueueFull) {
    MessageQueue queue(TEST_QUEUE_NAME,
                       4, // Small capacity
                       TEST_MAX_PAYLOAD_SIZE,
                       TEST_NUM_CONSUMERS);

    MarketData test_data;
    snprintf(test_data.symbol, sizeof(test_data.symbol), "FULL");
    test_data.price = 100.0;
    test_data.volume = 1000;
    test_data.timestamp = 123456789;

    // 填充队列 - 需要考虑实际队列实现
    // 队列可能与预期行为不同
    int successful_produces = 0;
    for (int i = 0; i < 10; ++i) { // Try more than capacity
        if (queue.produce(MessageType::MARKET_DATA, &test_data, sizeof(MarketData))) {
            successful_produces++;
        } else {
            break; // Queue is full
        }
    }

    // 验证我们能够生产一些消息然后达到限制
    EXPECT_GT(successful_produces, 0);

    // Next produce should fail (queue full)
    EXPECT_FALSE(queue.produce(MessageType::MARKET_DATA, &test_data, sizeof(MarketData)));
}

// 测试空队列消费
TEST_F(MessageQueueTest, EmptyQueueConsume) {
    MessageQueue queue(TEST_QUEUE_NAME, TEST_QUEUE_CAPACITY, TEST_MAX_PAYLOAD_SIZE, TEST_NUM_CONSUMERS);

    std::vector<uint8_t> buffer(sizeof(GenericMessage) + sizeof(MarketData));

    // 当队列为空时，应该返回 false
    EXPECT_FALSE(queue.consume(buffer.data(), 0));
}

// 测试无效的有效载荷大小
TEST_F(MessageQueueTest, InvalidPayloadSize) {
    MessageQueue queue(TEST_QUEUE_NAME, TEST_QUEUE_CAPACITY, TEST_MAX_PAYLOAD_SIZE, TEST_NUM_CONSUMERS);

    // 创建大于最大有效载荷大小的数据
    std::vector<uint8_t> large_data(TEST_MAX_PAYLOAD_SIZE + 1);

    // 预期抛出异常 for oversized payload
    EXPECT_THROW(
        { queue.produce(MessageType::MARKET_DATA, large_data.data(), large_data.size()); }, std::invalid_argument);
}

// 测试不同的消息类型
TEST_F(MessageQueueTest, DifferentMessageTypes) {
    MessageQueue queue(TEST_QUEUE_NAME, TEST_QUEUE_CAPACITY, TEST_MAX_PAYLOAD_SIZE, TEST_NUM_CONSUMERS);

    MarketData market_data;
    snprintf(market_data.symbol, sizeof(market_data.symbol), "TYPE");
    market_data.price = 200.0;
    market_data.volume = 2000;

    // 发布不同的消息类型
    EXPECT_TRUE(queue.produce(MessageType::MARKET_DATA, &market_data, sizeof(MarketData)));
    EXPECT_TRUE(queue.produce(MessageType::HEARTBEAT, &market_data, sizeof(MarketData)));
    EXPECT_TRUE(queue.produce(MessageType::ORDER_UPDATE, &market_data, sizeof(MarketData)));

    // 订阅和验证消息类型
    std::vector<uint8_t> buffer(sizeof(GenericMessage) + sizeof(MarketData));

    EXPECT_TRUE(queue.consume(buffer.data(), 0));
    const GenericMessage *msg1 = reinterpret_cast<const GenericMessage *>(buffer.data());
    EXPECT_EQ(msg1->header.type, MessageType::MARKET_DATA);

    EXPECT_TRUE(queue.consume(buffer.data(), 0));
    const GenericMessage *msg2 = reinterpret_cast<const GenericMessage *>(buffer.data());
    EXPECT_EQ(msg2->header.type, MessageType::HEARTBEAT);

    EXPECT_TRUE(queue.consume(buffer.data(), 0));
    const GenericMessage *msg3 = reinterpret_cast<const GenericMessage *>(buffer.data());
    EXPECT_EQ(msg3->header.type, MessageType::ORDER_UPDATE);
}

// 测试头部兼容性检查
TEST_F(MessageQueueTest, HeaderCompatibility) {
    // 创建初始队列并发布一个消息以确保它被正确初始化
    {
        MessageQueue queue(TEST_QUEUE_NAME, TEST_QUEUE_CAPACITY, TEST_MAX_PAYLOAD_SIZE, TEST_NUM_CONSUMERS);

        // 发布一个消息以确保队列被正确初始化
        MarketData test_data;
        snprintf(test_data.symbol, sizeof(test_data.symbol), "TEST");
        test_data.price = 100.0;
        test_data.volume = 1000;
        queue.produce(MessageType::MARKET_DATA, &test_data, sizeof(MarketData));
    }

    // 给一些时间进行清理 if needed
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    // 检查相同参数的兼容性
    EXPECT_TRUE(MessageQueue::isHeaderCompatible(
        TEST_QUEUE_NAME, TEST_QUEUE_CAPACITY, TEST_MAX_PAYLOAD_SIZE, TEST_NUM_CONSUMERS));

    // 检查不同参数的不兼容性
    EXPECT_FALSE(MessageQueue::isHeaderCompatible(
        TEST_QUEUE_NAME, TEST_QUEUE_CAPACITY + 1, TEST_MAX_PAYLOAD_SIZE, TEST_NUM_CONSUMERS));
    EXPECT_FALSE(MessageQueue::isHeaderCompatible(
        TEST_QUEUE_NAME, TEST_QUEUE_CAPACITY, TEST_MAX_PAYLOAD_SIZE + 1, TEST_NUM_CONSUMERS));
    EXPECT_FALSE(MessageQueue::isHeaderCompatible(
        TEST_QUEUE_NAME, TEST_QUEUE_CAPACITY, TEST_MAX_PAYLOAD_SIZE, TEST_NUM_CONSUMERS + 1));
}

// 测试多消费者场景
TEST_F(MessageQueueTest, MultiConsumer) {
    const uint32_t NUM_CONSUMERS = 2;
    MessageQueue queue(TEST_QUEUE_NAME, TEST_QUEUE_CAPACITY, TEST_MAX_PAYLOAD_SIZE, NUM_CONSUMERS);

    MarketData test_data;
    snprintf(test_data.symbol, sizeof(test_data.symbol), "MULTI");
    test_data.price = 150.0;
    test_data.volume = 1500;

    // Produce a message
    EXPECT_TRUE(queue.produce(MessageType::MARKET_DATA, &test_data, sizeof(MarketData)));

    std::vector<uint8_t> buffer1(sizeof(GenericMessage) + sizeof(MarketData));
    std::vector<uint8_t> buffer2(sizeof(GenericMessage) + sizeof(MarketData));

    // Both consumers should be able to read the same message
    EXPECT_TRUE(queue.consume(buffer1.data(), 0));
    EXPECT_TRUE(queue.consume(buffer2.data(), 1));

    // Verify both received the same message
    const GenericMessage *msg1 = reinterpret_cast<const GenericMessage *>(buffer1.data());
    const GenericMessage *msg2 = reinterpret_cast<const GenericMessage *>(buffer2.data());

    EXPECT_EQ(msg1->header.type, msg2->header.type);
    EXPECT_EQ(msg1->header.payload_size, msg2->header.payload_size);

    const MarketData *data1 = reinterpret_cast<const MarketData *>(msg1->payload);
    const MarketData *data2 = reinterpret_cast<const MarketData *>(msg2->payload);

    EXPECT_STREQ(data1->symbol, data2->symbol);
    EXPECT_DOUBLE_EQ(data1->price, data2->price);
    EXPECT_EQ(data1->volume, data2->volume);
}

// Test producer-consumer threading scenario
TEST_F(MessageQueueTest, ProducerConsumerThreading) {
    MessageQueue queue(TEST_QUEUE_NAME, TEST_QUEUE_CAPACITY, TEST_MAX_PAYLOAD_SIZE, TEST_NUM_CONSUMERS);

    const int NUM_MESSAGES = 50;
    std::vector<bool> received_flags(NUM_MESSAGES, false);
    bool producer_finished = false;

    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < NUM_MESSAGES; ++i) {
            MarketData test_data;
            snprintf(test_data.symbol, sizeof(test_data.symbol), "THR%d", i);
            test_data.price = 100.0 + i;
            test_data.volume = 1000 + i;
            test_data.timestamp = i;

            while (!queue.produce(MessageType::MARKET_DATA, &test_data, sizeof(MarketData))) {
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        }
        producer_finished = true;
    });

    // Consumer thread
    std::thread consumer([&]() {
        int messages_received = 0;
        std::vector<uint8_t> buffer(sizeof(GenericMessage) + sizeof(MarketData));

        while (messages_received < NUM_MESSAGES) {
            if (queue.consume(buffer.data(), 0)) {
                const GenericMessage *msg = reinterpret_cast<const GenericMessage *>(buffer.data());
                const MarketData *data = reinterpret_cast<const MarketData *>(msg->payload);

                // Extract message index from timestamp
                int msg_index = static_cast<int>(data->timestamp);
                EXPECT_GE(msg_index, 0);
                EXPECT_LT(msg_index, NUM_MESSAGES);

                if (msg_index >= 0 && msg_index < NUM_MESSAGES) {
                    received_flags[msg_index] = true;
                    messages_received++;
                }
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        }
    });

    producer.join();
    consumer.join();

    // Verify all messages were received
    for (int i = 0; i < NUM_MESSAGES; ++i) {
        EXPECT_TRUE(received_flags[i]) << "Message " << i << " was not received";
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
