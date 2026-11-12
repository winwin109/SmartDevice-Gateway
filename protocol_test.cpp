#include "protocol.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
using namespace std;

// 测试 1：空包（length = 0，无 payload）
static void test_empty_packet()
{
    MessageHeader header;
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = MessageType::DEVICE_REGISTER;
    header.device_id = 1;
    header.length = 0;
    vector<uint8_t> payload = {};

    vector<uint8_t> sendbuff = EncodeMessage(header, payload);

    MessageHeader deheader;
    vector<uint8_t> depayload;
    DecodeStatus status = DecodeMessage(sendbuff, deheader, depayload);

    assert(status == DecodeStatus::COMPLETE);
    assert(sendbuff.size() == 0); // 完整消息已消费
    assert(depayload.size() == 0);
    assert(deheader.device_id == 1);
    assert(deheader.type == MessageType::DEVICE_REGISTER);

    cout << "test_empty_packet passed!" << endl;
}

// 测试 2：粘包 + 半包（第一条完整，第二条只有前 5 字节）
static void test_sticky_and_half_packet()
{
    MessageHeader header1;
    header1.magic = PROTOCOL_MAGIC;
    header1.version = PROTOCOL_VERSION;
    header1.type = MessageType::DEVICE_REGISTER;
    header1.device_id = 1;
    header1.length = 2;
    vector<uint8_t> payload1 = {'A', 'B'};

    MessageHeader header2;
    header2.magic = PROTOCOL_MAGIC;
    header2.version = PROTOCOL_VERSION;
    header2.type = MessageType::HEARTBEAT;
    header2.device_id = 2;
    header2.length = 3;
    vector<uint8_t> payload2 = {'A', 'B', 'C'};

    vector<uint8_t> msg1 = EncodeMessage(header1, payload1);
    vector<uint8_t> msg2 = EncodeMessage(header2, payload2);

    vector<uint8_t> recv_buf;
    recv_buf.insert(recv_buf.end(), msg1.begin(), msg1.end());
    recv_buf.insert(recv_buf.end(), msg2.begin(), msg2.begin() + 5);

    MessageHeader deheader;
    vector<uint8_t> depayload;
    DecodeStatus status = DecodeMessage(recv_buf, deheader, depayload);

    assert(status == DecodeStatus::COMPLETE);
    assert(deheader.device_id == 1);
    assert(depayload.size() == 2);
    assert(depayload[0] == 'A' && depayload[1] == 'B');
    assert(recv_buf.size() == 5); // 第二条消息剩余 5 字节半包

    cout << "test_sticky_and_half_packet passed!" << endl;
}

// 测试 3：只有半个消息头（不足 14 字节）
static void test_half_header()
{
    vector<uint8_t> sendbuff = {0x53, 0x4D, 0x44, 0x56, 0x01, 0x03, 0x00};

    MessageHeader deheader;
    vector<uint8_t> depayload;
    DecodeStatus status = DecodeMessage(sendbuff, deheader, depayload);

    assert(status == DecodeStatus::INCOMPLETE);
    assert(sendbuff.size() == 7); // 数据不足，缓冲区保持不变

    cout << "test_half_header passed!" << endl;
}

// 测试 4：正常一个数据包
static void test_normal_packet()
{
    MessageHeader header;
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = MessageType::DEVICE_REGISTER;
    header.device_id = 1;
    header.length = 2;
    vector<uint8_t> payload = {'A', 'B'};

    vector<uint8_t> sendbuff = EncodeMessage(header, payload);

    MessageHeader deheader;
    vector<uint8_t> depayload;
    DecodeStatus status = DecodeMessage(sendbuff, deheader, depayload);

    assert(status == DecodeStatus::COMPLETE);
    assert(deheader.device_id == 1);
    assert(depayload.size() == 2);
    assert(depayload[0] == 'A' && depayload[1] == 'B');
    assert(sendbuff.size() == 0);

    cout << "test_normal_packet passed!" << endl;
}

// 测试 5：非法消息类型
static void test_invalid_type()
{
    MessageHeader header;
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = static_cast<MessageType>(0x7F); // 不属于五种合法类型
    header.device_id = 1;
    header.length = 0;
    vector<uint8_t> payload = {};

    vector<uint8_t> sendbuff = EncodeMessage(header, payload);

    MessageHeader deheader;
    vector<uint8_t> depayload;
    DecodeStatus status = DecodeMessage(sendbuff, deheader, depayload);

    assert(status == DecodeStatus::INVALID);

    cout << "test_invalid_type passed!" << endl;
}

// 测试 6：Payload 不完整（头完整 + 部分 payload）
static void test_incomplete_payload()
{
    MessageHeader header;
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = MessageType::DEVICE_REGISTER;
    header.device_id = 1;
    header.length = 5;
    vector<uint8_t> payload = {'A', 'B', 'C', 'D', 'E'};

    vector<uint8_t> full = EncodeMessage(header, payload);
    assert(full.size() == 19); // 14 字节头 + 5 字节 payload

    // 只放入前 16 字节：14 字节消息头 + 2 字节 payload
    vector<uint8_t> recv_buf(full.begin(), full.begin() + 16);
    vector<uint8_t> snapshot = recv_buf; // 用于校验缓冲区未被消费

    MessageHeader deheader;
    vector<uint8_t> depayload;
    DecodeStatus status = DecodeMessage(recv_buf, deheader, depayload);

    assert(status == DecodeStatus::INCOMPLETE);
    assert(recv_buf.size() == 16); // 缓冲区大小不变
    assert(recv_buf == snapshot);   // 内容不变，未消费任何字节

    cout << "test_incomplete_payload passed!" << endl;
}

// 测试 7：非法 Magic（修改首字节）
static void test_invalid_magic()
{
    MessageHeader header;
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = MessageType::DEVICE_REGISTER;
    header.device_id = 1;
    header.length = 2;
    vector<uint8_t> payload = {'A', 'B'};

    vector<uint8_t> sendbuff = EncodeMessage(header, payload);
    sendbuff[0] = 0x00; // 破坏 Magic 首字节

    vector<uint8_t> snapshot = sendbuff;
    MessageHeader deheader;
    vector<uint8_t> depayload;
    DecodeStatus status = DecodeMessage(sendbuff, deheader, depayload);

    assert(status == DecodeStatus::INVALID);
    assert(sendbuff.size() == snapshot.size());
    assert(sendbuff == snapshot); // 缓冲区内容和大小不变

    cout << "test_invalid_magic passed!" << endl;
}

// 测试 8：非法 Version（偏移 4 处改为 0x02）
static void test_invalid_version()
{
    MessageHeader header;
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = MessageType::DEVICE_REGISTER;
    header.device_id = 1;
    header.length = 2;
    vector<uint8_t> payload = {'A', 'B'};

    vector<uint8_t> sendbuff = EncodeMessage(header, payload);
    sendbuff[4] = 0x02; // 破坏 Version 字段

    vector<uint8_t> snapshot = sendbuff;
    MessageHeader deheader;
    vector<uint8_t> depayload;
    DecodeStatus status = DecodeMessage(sendbuff, deheader, depayload);

    assert(status == DecodeStatus::INVALID);
    assert(sendbuff.size() == snapshot.size());
    assert(sendbuff == snapshot);

    cout << "test_invalid_version passed!" << endl;
}

// 测试 9：超长 Length（4097 > 4096）
static void test_invalid_length()
{
    MessageHeader header;
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = MessageType::DEVICE_REGISTER;
    header.device_id = 1;
    header.length = 2;
    vector<uint8_t> payload = {'A', 'B'};

    vector<uint8_t> sendbuff = EncodeMessage(header, payload);

    // Length 字段偏移 10～13，大端写入 4097 = 0x00001001
    sendbuff[10] = 0x00;
    sendbuff[11] = 0x00;
    sendbuff[12] = 0x10;
    sendbuff[13] = 0x01;

    vector<uint8_t> snapshot = sendbuff;
    MessageHeader deheader;
    vector<uint8_t> depayload;
    DecodeStatus status = DecodeMessage(sendbuff, deheader, depayload);

    assert(status == DecodeStatus::INVALID);
    assert(sendbuff.size() == snapshot.size());
    assert(sendbuff == snapshot);

    cout << "test_invalid_length passed!" << endl;
}

int main()
{
    test_empty_packet();
    test_sticky_and_half_packet();
    test_half_header();
    test_normal_packet();
    test_invalid_type();
    test_incomplete_payload();
    test_invalid_magic();
    test_invalid_version();
    test_invalid_length();

    cout << "All tests passed!" << endl;
    return 0;
}
