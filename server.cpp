#include "server.h"
#include "message_parser.h"
bool SerConf::ReadConf(string filename) // 读取配置文件
{
    FILE *fp = fopen(filename.c_str(), "r");
    if (fp == NULL)
    {
        cout << "open file: " << filename << " err!" << endl;
        return false;
    }

    char buff[128] = {0};
    int num = 1; // 第几行
    while (fgets(buff, 127, fp))
    {
        if (strncmp(buff, "#", 1) == 0 || strncmp(buff, "\n", 1) == 0)
        {
            num++;
            continue;
        }

        char *k = strtok(buff, " \t\r\n"); // 支持空格/Tab/回车/换行作为分隔符
        char *v = strtok(NULL, " \t\r\n");
        if (k == NULL || v == NULL)
        {
            cout << "配置文件my.cnf第: " << num++ << "行无法解析:" << buff << endl;
            continue;
        }

        if (strcmp(k, "ip") == 0)
        {
            ips = v;
        }
        else if (strcmp(k, "port") == 0)
        {
            char *end = NULL;
            long p = strtol(v, &end, 10);
            if (end == v || *end != '\0') // 非纯数字（含空串、带字母等）
            {
                cout << "配置文件第 " << num << " 行 port 不合法(非数字): " << v << endl;
                fclose(fp);
                return false;
            }
            if (p < 1 || p > 65535) // 端口合法范围 1~65535
            {
                cout << "配置文件第 " << num << " 行 port 超出合法范围(1-65535): " << v << endl;
                fclose(fp);
                return false;
            }
            port = (int)p;
        }
        else if (strcmp(k, "lismax") == 0)
        {
            lismax = atoi(v);
        }
        else if (strcmp(k, "taskmax") == 0)
        {
            taskmax = atoi(v);
        }

        else
        {
            cout << "配置文件第：" << num << "行,无法解析的k: " << k << endl;
        }

        num++;
    }
    fclose(fp); // 读取结束，关闭文件
    return true;
}
void SerConf::PrintInfo() // 打印读取的配置信息
{
    cout << "----SerConf----" << endl;
    cout << "ip: " << ips << endl;
    cout << "port: " << port << endl;
    cout << "lismax: " << lismax << endl;
    cout << "taskmax: " << taskmax << endl;

    cout << "----end----" << endl;
}
// tcpser类
bool TcpSer::Ser_Init()
{
    if (!create_socket()) // 创建监听套接子
    {
        return false;
    }
    return true; // 成功路径必须返回 true，否则调用者拿到的是未定义值
}
bool TcpSer::create_socket()
{
    m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == m_sockfd)
    {
        cout << "socket 创建失败: " << strerror(errno) << endl;
        return false;
    }

    // 让服务器在重启时能立即重新绑定仍处于 TIME_WAIT 状态的端口，避免服务器重启时因 TIME_WAIT 报 "Address already in use"
    int opt = 1;
    if (setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        cout << "setsockopt 失败: " << strerror(errno) << endl;
        close(m_sockfd); // 释放已创建的套接字
        m_sockfd = -1;
        return false;
    }

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(m_conf.port);
    saddr.sin_addr.s_addr = inet_addr(m_conf.ips.c_str());
    if (bind(m_sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) == -1)
    {
        cout << "bind 失败: " << strerror(errno) << endl;
        close(m_sockfd); // 释放已创建的套接字
        m_sockfd = -1;
        return false;
    }

    if (listen(m_sockfd, m_conf.lismax) == -1)
    {
        cout << "listen 失败: " << strerror(errno) << endl;
        close(m_sockfd); // 释放已创建的套接字
        m_sockfd = -1;
        return false;
    }

    return true;
}
void TcpSer::Run()
{
    while (true) // 循环接受多个客户端
    {
        struct sockaddr_in caddr;
        socklen_t len = sizeof(caddr);
        int c = accept(m_sockfd, (struct sockaddr *)&caddr, &len);
        if (c == -1)
        {
            if (errno == EINTR) // 被信号中断，重试而非当作错误
            {
                continue;
            }
            cout << "accept 失败: " << strerror(errno) << endl;
            break; // 其他错误(如 EMFILE/EBADF)会持续失败，跳出循环避免无限忙循环
        }
        cout << "客户端已连接" << endl;

        MessageParser parser; // 每个客户端连接一个独立的解析器，避免不同连接间的数据互相串扰

        // 当前连接的注册状态：一个 TCP 连接最多只允许注册一个设备 ID
        bool registered = false;
        uint32_t registered_device_id = 0;

        uint8_t buff[1024] = {0};
        while (true) // 同一个客户端可以连续发送多条消息
        {
            ssize_t n = recv(c, buff, sizeof(buff), 0);
            if (n == 0) // 返回 0：客户端正常关闭连接(EOF)
            {
                cout << "客户端关闭连接" << endl;
                break;
            }
            if (n < 0) // 返回负值：接收出错
            {
                if (errno == EINTR) // 被信号中断，重试而非当作错误
                {
                    continue;
                }
                cout << "recv 失败: " << strerror(errno) << endl;
                break;
            }
            parser.Append(buff, static_cast<size_t>(n));

            MessageHeader header;
            vector<uint8_t> payload;
            bool invalid_protocol = false;
            while (true)
            {
                DecodeStatus status = parser.Parse(header, payload);

                if (status == DecodeStatus::COMPLETE)
                {
                    switch (header.type)
                    {
                    case MessageType::DEVICE_REGISTER:
                    {
                        // 一个连接只允许注册一次；重复注册直接拒绝
                        if (registered)
                        {
                            cout << "当前连接已注册设备，拒绝重复注册" << endl;
                        }
                        else if (m_device_manager.RegisterDevice(header.device_id, c))
                        {
                            registered = true;
                            registered_device_id = header.device_id;
                            cout << "设备注册成功，Device ID: " << header.device_id << endl;
                        }
                        else
                        {
                            // 注册失败（如该 ID 已在线），不得覆盖已有 Socket/在线状态
                            cout << "设备注册失败，Device ID: " << header.device_id << endl;
                        }
                        break;
                    }
                    case MessageType::DATA_REPORT:
                    {
                        // 必须先注册，且消息 Device ID 必须等于本连接注册的 Device ID
                        if (!registered)
                        {
                            cout << "当前连接未注册设备，拒绝处理 DATA_REPORT" << endl;
                            break;
                        }
                        if (header.device_id != registered_device_id)
                        {
                            cout << "消息 Device ID 与当前连接注册的设备不一致，拒绝处理 DATA_REPORT" << endl;
                            break;
                        }
                        // DeviceManager 中该设备仍在线，且 sockfd 等于当前连接
                        DeviceInfo *dev = m_device_manager.GetDevice(registered_device_id);
                        if (dev == nullptr || !dev->online || dev->sockfd != c)
                        {
                            cout << "设备状态校验失败，拒绝处理 DATA_REPORT" << endl;
                            break;
                        }
                        // 校验通过，沿用现有打印逻辑
                        cout << "收到 DATA_REPORT，设备ID: " << header.device_id
                             << "，Payload(" << payload.size() << " 字节): ";
                        for (uint8_t byte : payload)
                        {
                            printf("%02x ", static_cast<unsigned int>(byte));
                        }
                        cout << endl;
                        break;
                    }
                    case MessageType::HEARTBEAT:
                    {
                        // 与 DATA_REPORT 做相同的身份/状态校验，暂不实现超时机制
                        if (!registered || header.device_id != registered_device_id)
                        {
                            cout << "心跳身份校验失败，拒绝处理 HEARTBEAT" << endl;
                            break;
                        }
                        DeviceInfo *dev = m_device_manager.GetDevice(registered_device_id);
                        if (dev == nullptr || !dev->online || dev->sockfd != c)
                        {
                            cout << "心跳设备状态校验失败，拒绝处理 HEARTBEAT" << endl;
                            break;
                        }
                        cout << "收到 HEARTBEAT，设备ID: " << header.device_id << endl;
                        break;
                    }
                    case MessageType::CONTROL_CMD:
                    case MessageType::CONTROL_ACK:
                    {
                        // 暂不实现业务处理
                        cout << "暂不处理该消息类型: " << static_cast<int>(header.type) << endl;
                        break;
                    }
                    default:
                    {
                        cout << "未知消息类型: " << static_cast<int>(header.type) << endl;
                        break;
                    }
                    }
                    continue;
                }
                else if (status == DecodeStatus::INCOMPLETE)
                {
                    // 数据不足：跳出解析循环，等待下一次 recv() 补充更多数据
                    break;
                }
                else // DecodeStatus::INVALID
                {
                    // 协议错误：结束当前连接处理，统一走下方的设备下线与关闭逻辑
                    cout << "协议错误，关闭客户端连接" << endl;
                    invalid_protocol = true;
                    break;
                }
            }

            if (invalid_protocol)
            {
                break; // 结束当前客户端处理循环
            }
        }

        // 统一清理：若本连接注册过设备则标记离线，再关闭 socket。
        // 三条退出路径(recv==0 / recv 出错 / 协议错误)都在此处 close，且只 close 一次。
        if (registered)
        {
            m_device_manager.MarkOffline(registered_device_id, c);
        }
        close(c);
    }
}
int main()
{
    SerConf conf;
    if (!conf.ReadConf("my.cnf"))
    {
        exit(1);
    }

    conf.PrintInfo();

    TcpSer ser(conf);
    if (!ser.Ser_Init())
    {
        cout << "服务器初始化失败" << endl;
        exit(1);
    }

    cout << "服务器启动成功，正在监听 " << conf.ips << ":" << conf.port << " ..." << endl;
    ser.Run(); // 进入接收连接、处理消息的业务循环

    return 0;
}