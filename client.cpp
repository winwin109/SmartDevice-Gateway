#include "client.h"
#include "message_parser.h"
#include <signal.h>

bool TcpClient::Socket_Init()
{
    // 忽略 SIGPIPE：若服务端已关闭连接，客户端再次 send() 时默认会收到 SIGPIPE
    // 并导致整个进程被终止。改为忽略该信号后，send() 会返回 -1 且 errno=EPIPE，
    // 从而可以在代码里按错误处理，而不是被信号直接杀掉。
    signal(SIGPIPE, SIG_IGN);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sockfd)
    {
        return false;
    }

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = inet_addr(ips.c_str());

    int res = connect(sockfd, (struct sockaddr *)&saddr, sizeof(saddr));
    if (res == -1)
    {
        cout << "connect server err: " << strerror(errno) << endl;
        close(sockfd); // 连接失败，释放已创建的套接字
        sockfd = -1;
        return false;
    }

    return true;
}

void TcpClient::Run()
{
    cout << "已连接服务器，输入消息发送（输入 exit 退出）" << endl;

    string line;
    while (runing)
    {
        cout << ">> " << flush;
        getline(cin, line);
        if (line.empty())
        {
            continue;
        }

        if (line == "exit" || line == "quit") // 输入 exit 退出（不发往服务器）
        {
            cout << "客户端退出" << endl;
            break;
        }

        // 1. 将用户输入转换为 Payload
        vector<uint8_t> payload(line.begin(), line.end());

        // 2. 构造协议头
        MessageHeader header;
        header.magic = PROTOCOL_MAGIC;
        header.version = PROTOCOL_VERSION;
        header.type = MessageType::DATA_REPORT;
        header.device_id = 100;
        header.length = payload.size();

        // 3. 编码完整协议消息
        vector<uint8_t> message = EncodeMessage(header, payload);
        // 循环发送，防止 send 只发送部分数据（TCP 是字节流，单次 send 可能只发出部分字节）
        size_t total = message.size(); // 发送总长度 = 编码后的完整协议消息字节数
        size_t sent = 0;               // 已发送字节数
        bool send_ok = true;
        while (sent < total)
        {
            // 起始地址：编码消息缓冲区 + 已发送字节数；剩余长度 = total - sent
            ssize_t n = send(sockfd, message.data() + sent, total - sent, 0);
            if (n == 0) // 返回 0：连接已关闭，避免死循环
            {
                cout << "发送失败: 连接已关闭" << endl;
                send_ok = false;
                break;
            }
            if (n < 0)
            {
                if (errno == EINTR) // 被信号中断，重试而非当作错误
                {
                    continue;
                }
                cout << "发送失败: " << strerror(errno) << endl;
                send_ok = false;
                break;
            }
            sent += (size_t)n;
        }

        if (!send_ok)
        {
            break;
        }
        cout << "已发送: " << line << endl;
    }

    // 退出时显式关闭套接字，释放资源
    if (sockfd != -1)
    {
        close(sockfd);
        sockfd = -1;
    }
}

int main()
{
    TcpClient cli;
    if (!cli.Socket_Init())
    {
        cout << "连接服务器失败" << endl;
        return 1; // 用 return 而非 exit，确保析构函数被调用、资源得到释放
    }

    cout << "连接服务器成功" << endl;
    cli.Run(); // 进入交互式发送循环
    return 0;
}