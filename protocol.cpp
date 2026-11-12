#include "protocol.h"

#include <arpa/inet.h> // htonl / ntohl
#include <cstdint>
#include <cstring> // memcpy
#include <iostream>
#include <vector>

// 将消息头和 Payload 编码为完整的网络字节流
std::vector<uint8_t> EncodeMessage(const MessageHeader &header, const std::vector<uint8_t> &payload)
{

    // 检查 Payload 长度合法性
    if (payload.size() > MAX_PAYLOAD_LEN)
    {
        std::cerr << "EncodeMessage: payload 长度 " << payload.size()
                  << " 超过最大限制 " << MAX_PAYLOAD_LEN << std::endl;
        return std::vector<uint8_t>();
    }

    // length 字段必须与实际 Payload 大小一致，否则收发双方会解析错位
    if (header.length != payload.size())
    {
        std::cerr << "EncodeMessage: header.length(" << header.length
                  << ") 与 payload 大小(" << payload.size() << ") 不一致" << std::endl;
        return std::vector<uint8_t>();
    }

    // 将消息头中的三个 uint32_t 字段转换为网络字节序
    uint32_t magic_net = htonl(header.magic);
    uint32_t device_id_net = htonl(header.device_id);
    uint32_t length_net = htonl(header.length);

    // 按照协议字段顺序组织数据：magic | version | type | device_id | length | payload
    std::vector<uint8_t> buf;
    buf.reserve(PROTOCOL_HEADER_LEN + payload.size());

    const uint8_t *p = nullptr;

    // magic（4 字节）
    p = reinterpret_cast<const uint8_t *>(&magic_net);
    buf.insert(buf.end(), p, p + sizeof(magic_net));

    // version（1 字节）
    buf.push_back(header.version);

    // type（1 字节）
    buf.push_back(static_cast<uint8_t>(header.type));

    // device_id（4 字节）
    p = reinterpret_cast<const uint8_t *>(&device_id_net);
    buf.insert(buf.end(), p, p + sizeof(device_id_net));

    // length（4 字节）
    p = reinterpret_cast<const uint8_t *>(&length_net);
    buf.insert(buf.end(), p, p + sizeof(length_net));

    // payload
    buf.insert(buf.end(), payload.begin(), payload.end());

    return buf;
}

namespace
{
    // 从大端字节序的 4 字节中读出主机字节序的 uint32_t。
    // 用 memcpy 而非直接解引用指针，避免未对齐访问与严格别名问题。
    uint32_t read_u32_be(const uint8_t *p)
    {
        uint32_t v = 0;
        memcpy(&v, p, sizeof(v));
        return ntohl(v);
    }
} // namespace

// 从接收缓冲区中尝试提取一条完整消息（处理 TCP 粘包/拆包）
DecodeStatus DecodeMessage(std::vector<uint8_t> &recv_buf, MessageHeader &header, std::vector<uint8_t> &payload)
{
    // 1. 判断是否已有完整的消息头（14 字节）
    if (recv_buf.size() < PROTOCOL_HEADER_LEN)
    {
        return DecodeStatus::INCOMPLETE; // 数据不足，继续等待
    }

    // 2. 检查 Magic Number
    uint32_t magic = read_u32_be(recv_buf.data() + 0);
    if (magic != PROTOCOL_MAGIC)
    {
        std::cerr << "DecodeMessage: magic 非法 (0x" << std::hex << magic << std::dec << ")" << std::endl;
        return DecodeStatus::INVALID;
    }

    // 3. 检查协议版本号
    uint8_t version = recv_buf[4];
    if (version != PROTOCOL_VERSION)
    {
        std::cerr << "DecodeMessage: version 非法 (" << static_cast<int>(version) << ")" << std::endl;
        return DecodeStatus::INVALID;
    }

    // 3.5 校验消息类型：只允许协议定义的五种类型
    uint8_t type = recv_buf[5];
    switch (static_cast<MessageType>(type))
    {
    case MessageType::DEVICE_REGISTER:
    case MessageType::HEARTBEAT:
    case MessageType::DATA_REPORT:
    case MessageType::CONTROL_CMD:
    case MessageType::CONTROL_ACK:
        break;
    default:
        std::cerr << "DecodeMessage: type 非法 (" << static_cast<int>(type) << ")" << std::endl;
        return DecodeStatus::INVALID;
    }

    // 4. 解析 length 字段（偏移 10 起的 4 字节，网络字节序）
    uint32_t length = read_u32_be(recv_buf.data() + 10);

    // length 合法性检查：防止脏数据/数据错位导致 length 异常巨大而无限等待
    if (length > MAX_PAYLOAD_LEN)
    {
        std::cerr << "DecodeMessage: 解析出的 length(" << length
                  << ") 超过最大限制 " << MAX_PAYLOAD_LEN << std::endl;
        return DecodeStatus::INVALID;
    }

    // 5. 判断是否有完整消息（头 + payload）
    size_t total_len = PROTOCOL_HEADER_LEN + length;
    if (recv_buf.size() < total_len)
    {
        return DecodeStatus::INCOMPLETE; // 数据不足，继续等待
    }

    // 6. 数据已完整，填充消息头各字段（magic/version 已在前面校验过，直接复用）
    header.magic = magic;
    header.version = version;
    header.type = static_cast<MessageType>(type);
    header.device_id = read_u32_be(recv_buf.data() + 6);
    header.length = length;

    // 7. 提取 payload
    payload.assign(recv_buf.begin() + PROTOCOL_HEADER_LEN, recv_buf.begin() + total_len);

    // 8. 从缓冲区移除已消费的字节，剩余字节留给下一条消息
    recv_buf.erase(recv_buf.begin(), recv_buf.begin() + total_len);

    return DecodeStatus::COMPLETE;
}
