#include "client.h"
#include "message_parser.h"
#include <signal.h>

namespace
{
    // 编码并发送一条完整协议消息，循环发送直到全部字节发出；返回是否成功
    bool SendMessage(int sockfd, const MessageHeader &header, const vector<uint8_t> &payload)
    {
        vector<uint8_t> message = EncodeMessage(header, payload);
        if (message.empty())
        {
            cout << "消息编码失败" << endl;
            return false;
        }

        size_t total = message.size();
        size_t sent = 0;
        while (sent < total)
        {
            ssize_t n = send(sockfd, message.data() + sent, total - sent, 0);
            if (n == 0) // 返回 0：连接已关闭，避免死循环
            {
                cout << "发送失败: 连接已关闭" << endl;
                return false;
            }
            if (n < 0)
            {
                if (errno == EINTR) // 被信号中断，重试而非当作错误
                {
                    continue;
                }
                cout << "发送失败: " << strerror(errno) << endl;
                return false;
            }
            sent += (size_t)n;
        }
        return true;
    }
} // namespace

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

    // 连接建立后先发送一条 DEVICE_REGISTER 注册本设备（payload 为空）
    {
        MessageHeader reg_header;
        reg_header.magic = PROTOCOL_MAGIC;
        reg_header.version = PROTOCOL_VERSION;
        reg_header.type = MessageType::DEVICE_REGISTER;
        reg_header.device_id = 100;
        reg_header.length = 0;

        vector<uint8_t> empty_payload;
        if (!SendMessage(sockfd, reg_header, empty_payload))
        {
            cout << "设备注册消息发送失败，结束客户端" << endl;
            return;
        }

        cout << "已发送设备注册消息，Device ID: " << reg_header.device_id << endl;
    }

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

        // 3. 编码并发送
        if (!SendMessage(sockfd, header, payload))
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