#include "protocol.h"
#include <cassert>
#include <iostream>
#include <cassert>
using namespace std;

int main()
{
    // 1. 准备原始消息头和 Payload
    MessageHeader header;
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = MessageType::DEVICE_REGISTER;
    header.device_id = 001;
    header.length = 0;
    std::vector<uint8_t> payload = {};
    // 2. 调用 EncodeMessage，模拟网络字节流
    vector<uint8_t> sendbuff = EncodeMessage(header, payload);
    // 3. 准备解码结果
    MessageHeader deheader;
    vector<uint8_t> depayload;
    // 4. 调用 DecodeMessage
    bool test = DecodeMessage(sendbuff, deheader, depayload);

    cout << "result:" << test << endl;
    cout << sizeof(deheader) << endl;
    cout << depayload.size() << endl;
    return 0;
}
// 测试空包
#if 0
// 测试粘包+半包
int main()
{
    // 1. 准备原始消息头和 Payload
    MessageHeader header1;
    header1.magic = PROTOCOL_MAGIC;
    header1.version = PROTOCOL_VERSION;
    header1.type = MessageType::DEVICE_REGISTER;
    header1.device_id = 001;
    header1.length = 2;
    std::vector<uint8_t> payload1 = {'A', 'B'};

    MessageHeader header2;
    header2.magic = PROTOCOL_MAGIC;
    header2.version = PROTOCOL_VERSION;
    header2.type = MessageType::HEARTBEAT;
    header2.device_id = 002;
    header2.length = 3;
    std::vector<uint8_t> payload2 = {'A', 'B', 'C'};

    // 2. 调用 EncodeMessage，模拟网络字节流
    std::vector<uint8_t> msg1 = EncodeMessage(header1, payload1);
    std::vector<uint8_t> msg2 = EncodeMessage(header2, payload2);

    vector<uint8_t> recv_buf;
    // 先放入第一条完整消息
    recv_buf.insert(recv_buf.end(), msg1.begin(), msg1.end());
    // 再放入第二条消息的前 5 个字节
    recv_buf.insert(recv_buf.end(), msg2.begin(), msg2.begin() + 5);
    // 3. 准备解码结果
    MessageHeader deheader;
    vector<uint8_t> depayload;
    // 4. 调用 DecodeMessage
    bool test = DecodeMessage(recv_buf, deheader, depayload);

    cout << "result:" << test << endl;
    cout << recv_buf.size() << endl;
    return 0;
}

// 测试只有只有半个数据包
int main()
{
    // 1. 准备原始消息头和 Payload
    std::vector<uint8_t> sendbuff = {
        0x53, 0x4D, 0x44, 0x56, 0x01, 0x03, 0x00};
    // 3. 准备解码结果
    MessageHeader deheader;
    vector<uint8_t> depayload;
    // 4. 调用 DecodeMessage
    bool test = DecodeMessage(sendbuff, deheader, depayload);

    cout << "result:" << test << endl;
    cout << sendbuff.size() << endl;
    return 0;
}

//测试正常一个数据包
int main()
{
    // 1. 准备原始消息头和 Payload
    MessageHeader header;
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = MessageType::DEVICE_REGISTER;
    header.device_id = 001;
    header.length = 2;
    std::vector<uint8_t> payload = {'A', 'B'};
    // 2. 调用 EncodeMessage，模拟网络字节流
    vector<uint8_t> sendbuff = EncodeMessage(header, payload);
    // 3. 准备解码结果
    MessageHeader deheader;
    vector<uint8_t> depayload;
    // 4. 调用 DecodeMessage
    bool test = DecodeMessage(sendbuff, deheader, depayload);
    // 5. 使用 assert 检查结果
    assert(test == true);
    assert(deheader.device_id == 001);
    assert(depayload.size() == 2);
    assert(depayload[0] == 'A');
    cout << "Test A passed!" << endl;
    return 0;
}
#endif