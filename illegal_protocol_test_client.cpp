// illegal_protocol_test_client.cpp
// 临时非法协议测试客户端：
// 构造一条 14 字节协议头消息（Magic / Version / DeviceID / Length 均合法，Type 非法 = 0x7F），
// 通过 TCP 连接服务端并发送，然后观察服务端是否识别协议错误并关闭本连接。
// 说明：本文件为独立测试代码，不覆盖、不修改现有 client.cpp 及任何业务实现。

#include "protocol.h"

#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

using namespace std;

// 与 my.cnf / server.h 默认配置保持一致
static const char *SERVER_IP = "127.0.0.1";
static const int SERVER_PORT = 6000;

// 等待服务端关闭连接的超时时间（毫秒）
static const int CLOSE_TIMEOUT_MS = 5000;

int main()
{
    // 1. 构造非法协议消息：14 字节消息头，Type = 0x7F
    MessageHeader header;
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = static_cast<MessageType>(0x7F); // 非法类型，不属于五种合法类型
    header.device_id = 12345;                     // 合法设备 ID
    header.length = 0;                            // 无 payload，整条消息恰好 14 字节

    vector<uint8_t> payload = {};
    vector<uint8_t> msg = EncodeMessage(header, payload);
    if (msg.size() != PROTOCOL_HEADER_LEN)
    {
        cerr << "构造失败: 消息长度应为 " << PROTOCOL_HEADER_LEN
             << "，实际为 " << msg.size() << endl;
        return 1;
    }

    cout << "构造的非法消息(" << msg.size() << " 字节): ";
    for (uint8_t byte : msg)
    {
        printf("%02x ", static_cast<unsigned int>(byte));
    }
    cout << endl;

    // 2. 建立 TCP 连接
    signal(SIGPIPE, SIG_IGN); // 避免服务端先关闭后，再 send 触发 SIGPIPE 杀死进程

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        cerr << "socket 创建失败: " << strerror(errno) << endl;
        return 1;
    }

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(SERVER_PORT);
    saddr.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (connect(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1)
    {
        cerr << "连接服务端失败: " << strerror(errno) << endl;
        close(sockfd);
        return 1;
    }
    cout << "已连接服务端 " << SERVER_IP << ":" << SERVER_PORT << endl;

    // 3. 发送 14 字节非法消息（循环发送，防止 TCP 单次只发出部分字节）
    size_t total = msg.size();
    size_t sent = 0;
    while (sent < total)
    {
        ssize_t n = send(sockfd, msg.data() + sent, total - sent, 0);
        if (n == 0)
        {
            cerr << "发送失败: 连接已关闭" << endl;
            close(sockfd);
            return 1;
        }
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            cerr << "发送失败: " << strerror(errno) << endl;
            close(sockfd);
            return 1;
        }
        sent += static_cast<size_t>(n);
    }
    cout << "已发送 " << total << " 字节非法消息" << endl;

    // 4. 观察服务端是否关闭连接
    //    服务端在 INVALID 分支只 close 连接、不发送任何数据，
    //    因此通过 poll 等待可读/HUP 事件，再用 recv 返回 0（EOF）判定对端已关闭。
    struct pollfd pfd;
    pfd.fd = sockfd;
    pfd.events = POLLIN | POLLHUP | POLLERR;
    pfd.revents = 0;

    int ret = poll(&pfd, 1, CLOSE_TIMEOUT_MS);
    if (ret == 0)
    {
        cerr << "测试失败: " << CLOSE_TIMEOUT_MS << " 毫秒内服务端未关闭连接" << endl;
        close(sockfd);
        return 1;
    }
    if (ret < 0)
    {
        cerr << "poll 失败: " << strerror(errno) << endl;
        close(sockfd);
        return 1;
    }

    bool closed = false;
    if (pfd.revents & (POLLHUP | POLLERR))
    {
        // 对端已关闭连接
        closed = true;
    }
    else if (pfd.revents & POLLIN)
    {
        char buf[64];
        ssize_t n = recv(sockfd, buf, sizeof(buf), 0);
        if (n == 0)
        {
            closed = true; // EOF：服务端已关闭连接
        }
        else if (n < 0)
        {
            cerr << "recv 失败: " << strerror(errno) << endl;
            close(sockfd);
            return 1;
        }
        else
        {
            cerr << "测试失败: 意外收到 " << n << " 字节数据" << endl;
            close(sockfd);
            return 1;
        }
    }

    close(sockfd);

    if (closed)
    {
        cout << "测试通过: 服务端识别协议错误并关闭了客户端连接" << endl;
        return 0;
    }

    cerr << "测试失败: 未能确认服务端关闭连接" << endl;
    return 1;
}
