#ifndef MESSAGE_PARSER_H
#define MESSAGE_PARSER_H

#include "protocol.h"

class MessageParser
{
private:
    std::vector<uint8_t> recv_buffer_;

public:
    void Append(const uint8_t *data, size_t len);

    DecodeStatus Parse(
        MessageHeader &header,
        std::vector<uint8_t> &payload);
};

#endif