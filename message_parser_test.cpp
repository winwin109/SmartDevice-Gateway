#include "message_parser.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
using namespace std;

// 测试 1：一次追加一条完整消息，能否解析
static void test_append_one_complete_message()
{
    MessageParser parser;

    MessageHeader header;
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = MessageType::DEVICE_REGISTER;
    header.device_id = 1;
    header.length = 3;
    vector<uint8_t> payload = {'A', 'B', 'C'};

    vector<uint8_t> msg = EncodeMessage(header, payload);

    // 一次追加整条消息
    parser.Append(msg.data(), msg.size());

    MessageHeader out_header;
    vector<uint8_t> out_payload;
    DecodeStatus status = parser.Parse(out_header, out_payload);

    assert(status == DecodeStatus::COMPLETE);
    assert(out_header.device_id == 1);
    assert(out_header.type == MessageType::DEVICE_REGISTER);
    assert(out_payload.size() == 3);
    assert(out_payload[0] == 'A' && out_payload[1] == 'B' && out_payload[2] == 'C');

    cout << "test_append_one_complete_message passed!" << endl;
}

// 测试 2：分两次追加一条消息，能否解析
static void test_append_one_message_in_two_parts()
{
    MessageParser parser;

    MessageHeader header;
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = MessageType::HEARTBEAT;
    header.device_id = 2;
    header.length = 5;
    vector<uint8_t> payload = {'H', 'E', 'L', 'L', 'O'};

    vector<uint8_t> msg = EncodeMessage(header, payload);

    // 第一次只追加前 10 字节（消息头都不完整）
    parser.Append(msg.data(), 10);

    MessageHeader out_header;
    vector<uint8_t> out_payload;
    DecodeStatus status = parser.Parse(out_header, out_payload);
    assert(status == DecodeStatus::INCOMPLETE);

    // 第二次追加剩余字节
    parser.Append(msg.data() + 10, msg.size() - 10);

    status = parser.Parse(out_header, out_payload);
    assert(status == DecodeStatus::COMPLETE);
    assert(out_header.device_id == 2);
    assert(out_payload.size() == 5);
    assert(out_payload[0] == 'H' && out_payload[4] == 'O');

    cout << "test_append_one_message_in_two_parts passed!" << endl;
}

// 测试 3：一次追加两条完整消息，能否连续解析
static void test_append_two_complete_messages()
{
    MessageParser parser;

    MessageHeader h1;
    h1.magic = PROTOCOL_MAGIC;
    h1.version = PROTOCOL_VERSION;
    h1.type = MessageType::DEVICE_REGISTER;
    h1.device_id = 1;
    h1.length = 2;
    vector<uint8_t> p1 = {'A', 'B'};

    MessageHeader h2;
    h2.magic = PROTOCOL_MAGIC;
    h2.version = PROTOCOL_VERSION;
    h2.type = MessageType::HEARTBEAT;
    h2.device_id = 2;
    h2.length = 3;
    vector<uint8_t> p2 = {'X', 'Y', 'Z'};

    vector<uint8_t> msg1 = EncodeMessage(h1, p1);
    vector<uint8_t> msg2 = EncodeMessage(h2, p2);

    // 一次追加两条完整消息
    vector<uint8_t> buf;
    buf.insert(buf.end(), msg1.begin(), msg1.end());
    buf.insert(buf.end(), msg2.begin(), msg2.end());
    parser.Append(buf.data(), buf.size());

    // 连续解析两条
    MessageHeader out_header;
    vector<uint8_t> out_payload;

    DecodeStatus s1 = parser.Parse(out_header, out_payload);
    assert(s1 == DecodeStatus::COMPLETE);
    assert(out_header.device_id == 1);
    assert(out_payload.size() == 2);
    assert(out_payload[0] == 'A' && out_payload[1] == 'B');

    DecodeStatus s2 = parser.Parse(out_header, out_payload);
    assert(s2 == DecodeStatus::COMPLETE);
    assert(out_header.device_id == 2);
    assert(out_payload.size() == 3);
    assert(out_payload[0] == 'X' && out_payload[2] == 'Z');

    cout << "test_append_two_complete_messages passed!" << endl;
}

// 测试 4：一次追加 A 完整消息 + B 半条消息，能否先解析 A，等 B 补齐后再解析 B
static void test_sticky_complete_and_half()
{
    MessageParser parser;

    MessageHeader ha;
    ha.magic = PROTOCOL_MAGIC;
    ha.version = PROTOCOL_VERSION;
    ha.type = MessageType::DEVICE_REGISTER;
    ha.device_id = 1;
    ha.length = 2;
    vector<uint8_t> pa = {'A', 'B'};

    MessageHeader hb;
    hb.magic = PROTOCOL_MAGIC;
    hb.version = PROTOCOL_VERSION;
    hb.type = MessageType::DATA_REPORT;
    hb.device_id = 2;
    hb.length = 4;
    vector<uint8_t> pb = {'P', 'Q', 'R', 'S'};

    vector<uint8_t> msgA = EncodeMessage(ha, pa);
    vector<uint8_t> msgB = EncodeMessage(hb, pb);

    // 一次追加 A 完整消息 + B 前 5 字节（半条）
    vector<uint8_t> buf;
    buf.insert(buf.end(), msgA.begin(), msgA.end());
    buf.insert(buf.end(), msgB.begin(), msgB.begin() + 5);
    parser.Append(buf.data(), buf.size());

    // 先解析出 A
    MessageHeader out_header;
    vector<uint8_t> out_payload;
    DecodeStatus s = parser.Parse(out_header, out_payload);
    assert(s == DecodeStatus::COMPLETE);
    assert(out_header.device_id == 1);
    assert(out_payload.size() == 2);
    assert(out_payload[0] == 'A' && out_payload[1] == 'B');

    // B 仍不完整
    s = parser.Parse(out_header, out_payload);
    assert(s == DecodeStatus::INCOMPLETE);

    // 补齐 B 剩余字节后，再解析出 B
    parser.Append(msgB.data() + 5, msgB.size() - 5);

    s = parser.Parse(out_header, out_payload);
    assert(s == DecodeStatus::COMPLETE);
    assert(out_header.device_id == 2);
    assert(out_payload.size() == 4);
    assert(out_payload[0] == 'P' && out_payload[3] == 'S');

    cout << "test_sticky_complete_and_half passed!" << endl;
}

// 测试 5：最大 Payload（4096 字节）的完整消息解析
static void test_max_payload()
{
    MessageParser parser;

    MessageHeader header;
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = MessageType::DATA_REPORT;
    header.device_id = 1;
    header.length = MAX_PAYLOAD_LEN;

    vector<uint8_t> payload(MAX_PAYLOAD_LEN);
    for (uint32_t i = 0; i < MAX_PAYLOAD_LEN; ++i)
    {
        payload[i] = static_cast<uint8_t>(i % 256);
    }

    vector<uint8_t> msg = EncodeMessage(header, payload);
    assert(msg.size() == PROTOCOL_HEADER_LEN + MAX_PAYLOAD_LEN);

    parser.Append(msg.data(), msg.size());

    MessageHeader out_header;
    vector<uint8_t> out_payload;
    DecodeStatus status = parser.Parse(out_header, out_payload);

    assert(status == DecodeStatus::COMPLETE);
    assert(out_header.device_id == 1);
    assert(out_header.type == MessageType::DATA_REPORT);
    assert(out_payload.size() == MAX_PAYLOAD_LEN);
    assert(out_payload == payload);

    cout << "test_max_payload passed!" << endl;
}

// 测试 6：非法 Magic —— Parse 返回 INVALID，且非法数据未被消费
static void test_invalid_magic()
{
    MessageParser parser;

    MessageHeader header;
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = MessageType::DEVICE_REGISTER;
    header.device_id = 1;
    header.length = 2;
    vector<uint8_t> payload = {'A', 'B'};

    vector<uint8_t> msg = EncodeMessage(header, payload);
    msg[0] = 0x00; // 破坏 Magic 首字节

    parser.Append(msg.data(), msg.size());

    MessageHeader out_header;
    vector<uint8_t> out_payload;
    DecodeStatus status = parser.Parse(out_header, out_payload);
    assert(status == DecodeStatus::INVALID);

    // 再次解析仍返回 INVALID，说明非法数据仍保留在内部缓冲区、未被消费
    status = parser.Parse(out_header, out_payload);
    assert(status == DecodeStatus::INVALID);

    cout << "test_invalid_magic passed!" << endl;
}

// 测试 7：非法 Type（0x7F）—— Parse 返回 INVALID，且非法数据未被消费
static void test_invalid_type()
{
    MessageParser parser;

    MessageHeader header;
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = MessageType::DEVICE_REGISTER;
    header.device_id = 1;
    header.length = 2;
    vector<uint8_t> payload = {'A', 'B'};

    vector<uint8_t> msg = EncodeMessage(header, payload);
    msg[5] = 0x7F; // 破坏 Type 字段（不属于五种合法类型）

    parser.Append(msg.data(), msg.size());

    MessageHeader out_header;
    vector<uint8_t> out_payload;
    DecodeStatus status = parser.Parse(out_header, out_payload);
    assert(status == DecodeStatus::INVALID);

    // 再次解析仍返回 INVALID，说明非法数据仍保留在内部缓冲区、未被消费
    status = parser.Parse(out_header, out_payload);
    assert(status == DecodeStatus::INVALID);

    cout << "test_invalid_type passed!" << endl;
}

int main()
{
    test_append_one_complete_message();
    test_append_one_message_in_two_parts();
    test_append_two_complete_messages();
    test_sticky_complete_and_half();
    test_max_payload();
    test_invalid_magic();
    test_invalid_type();

    cout << "All tests passed!" << endl;
    return 0;
}