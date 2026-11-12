#include "message_parser.h"

void MessageParser::Append(const uint8_t *data, size_t len)
{
    // 把新收到的数据追加到 recv_buffer_
    if (data == nullptr || len == 0)
    {
        return;
    }
    recv_buffer_.insert(recv_buffer_.end(), data, data + len);
}

DecodeStatus MessageParser::Parse(
    MessageHeader &header,
    std::vector<uint8_t> &payload)
{
    // 清空上次的解析结果，避免在 INCOMPLETE / INVALID 时残留旧数据
    payload.clear();

    // 交给底层 DecodeMessage 处理粘包/半包：
    // COMPLETE   —— 已解析出一条完整消息，并从 recv_buffer_ 头部消费对应字节
    // INCOMPLETE —— 数据不足，等待更多数据
    // INVALID    —— 协议数据非法
    return DecodeMessage(recv_buffer_, header, payload);
}