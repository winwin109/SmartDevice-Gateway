#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <vector>
// ============================================================
// 协议常量定义
// ============================================================

// Magic Number：数据包起始标识，用于快速识别是否为合法协议包
// 取值 0x534D4456，对应 ASCII "SMDV"（Smart Device）
constexpr uint32_t PROTOCOL_MAGIC = 0x534D4456;

// 协议版本号：当前为第 1 版
constexpr uint8_t PROTOCOL_VERSION = 0x01;

// 最大 Payload 长度（字节）：单个消息的负载部分最大允许 4KB
constexpr uint32_t MAX_PAYLOAD_LEN = 4096;

// 线上消息头的固定字节长度：magic(4) + version(1) + type(1) + device_id(4) + length(4)
// 注意：这不是 sizeof(MessageHeader)，因为结构体可能因内存对齐而更大
constexpr uint32_t PROTOCOL_HEADER_LEN = 14;

// ============================================================
// 消息类型定义
// ============================================================
enum class MessageType : uint8_t
{
    DEVICE_REGISTER = 0x01, // 设备注册
    HEARTBEAT = 0x02,       // 心跳
    DATA_REPORT = 0x03,     // 数据上报
    CONTROL_CMD = 0x04,     // 控制命令
    CONTROL_ACK = 0x05      // 控制应答
};

enum class DecodeStatus
{
    COMPLETE,   // 成功解析
    INCOMPLETE, // 数据不足，继续等待
    INVALID     // 协议数据非法
};

// ============================================================
// 消息头结构体定义（不包含 Payload）
// ============================================================
struct MessageHeader
{
    uint32_t magic;     // 魔法数字，固定为 PROTOCOL_MAGIC
    uint8_t version;    // 协议版本号，固定为 PROTOCOL_VERSION
    MessageType type;   // 消息类型，见 MessageType 枚举
    uint32_t device_id; // 设备 ID，唯一标识发送方设备
    uint32_t length;    // Payload 长度（字节），不能超过 MAX_PAYLOAD_LEN
};

std::vector<uint8_t> EncodeMessage(
    const MessageHeader &header,
    const std::vector<uint8_t> &payload);

// 解码函数：从接收缓冲区中尝试提取一条完整消息。
// recv_buf 为输入输出：成功解析后，已消费的字节会从缓冲区头部移除，剩余字节留给下一条消息。
// header / payload 输出解析结果。
// 返回值见 DecodeStatus：
//   COMPLETE   —— 成功提取一条完整消息
//   INCOMPLETE —— 数据不足，需继续接收更多数据
//   INVALID    —— 协议数据非法（magic / version / length 校验失败）
DecodeStatus DecodeMessage(
    std::vector<uint8_t> &recv_buf,
    MessageHeader &header,
    std::vector<uint8_t> &payload);
#endif