// 启用 GNU 扩展：accept4 / epoll_create1 / SOCK_NONBLOCK / SOCK_CLOEXEC / EPOLLRDHUP
// 必须在任何系统头文件之前定义（若编译器已定义则跳过，避免重定义警告）。
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "server.h"
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
    if (!create_socket()) // 创建监听套接子（非阻塞）
    {
        return false;
    }

    m_epfd = epoll_create1(EPOLL_CLOEXEC); // 创建 epoll 实例
    if (m_epfd == -1)
    {
        cout << "epoll_create1 失败: " << strerror(errno) << endl;
        close(m_sockfd); // 释放已创建的监听 socket
        m_sockfd = -1;
        return false;
    }

    // 将监听 socket 注册到 epoll（ET 模式）
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = m_sockfd;
    if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_sockfd, &ev) == -1)
    {
        cout << "epoll_ctl 注册监听 socket 失败: " << strerror(errno) << endl;
        close(m_epfd);
        close(m_sockfd);
        m_epfd = -1;
        m_sockfd = -1;
        return false;
    }

    return true;
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

    // ET 模式下监听 socket 必须非阻塞：否则 accept 在就绪连接耗尽后会阻塞整个事件循环
    int flags = fcntl(m_sockfd, F_GETFL, 0);
    if (flags == -1 || fcntl(m_sockfd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        cout << "设置监听 socket 非阻塞失败: " << strerror(errno) << endl;
        close(m_sockfd);
        m_sockfd = -1;
        return false;
    }

    return true;
}
TcpSer::~TcpSer()
{
    // 关闭所有仍存活的客户端连接。
    // CloseConnection 内部会注销 epoll、按需下线设备、close 并 erase，
    // 因此这里循环取 m_connections.begin() 直到容器为空，避免遍历中 erase 引发迭代器失效。
    while (!m_connections.empty())
    {
        CloseConnection(m_connections.begin()->first);
    }

    // 关闭 epoll 实例与监听 socket（客户端 fd 已在上方循环中关闭）
    if (m_epfd != -1)
    {
        close(m_epfd);
        m_epfd = -1;
    }
    if (m_sockfd != -1)
    {
        close(m_sockfd);
        m_sockfd = -1;
    }
}

void TcpSer::Run()
{
    const int MAX_EVENTS = 128; // 事件数组大小（可用 m_conf.taskmax，但需保证 >= 1）
    struct epoll_event events[MAX_EVENTS];

    while (true)
    {
        int nfds = epoll_wait(m_epfd, events, MAX_EVENTS, -1);
        if (nfds == -1)
        {
            if (errno == EINTR)
            {
                continue; // 被信号中断，重试
            }
            cout << "epoll_wait 失败: " << strerror(errno) << endl;
            break; // 出错退出事件循环
        }

        for (int i = 0; i < nfds; ++i)
        {
            int fd = events[i].data.fd;
            uint32_t ev = events[i].events;

            if (fd == m_sockfd)
            {
                AcceptConnections(); // 监听 socket 就绪，接受所有新连接
            }
            else
            {
                // 客户端连接事件；HandleClient 返回 true 时由 Run 统一清理
                if (HandleClient(fd, ev))
                {
                    CloseConnection(fd);
                }
            }
        }
    }
}

void TcpSer::AcceptConnections()
{
    // ET 模式必须循环 accept，直到 EAGAIN/EWOULDBLOCK 表示就绪连接已全部接受
    while (true)
    {
        struct sockaddr_in caddr;
        socklen_t len = sizeof(caddr);
        int c = accept4(m_sockfd, (struct sockaddr *)&caddr, &len,
                        SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (c == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break; // 已接受完所有就绪连接
            }
            if (errno == EINTR)
            {
                continue; // 被信号中断，重试
            }
            // EMFILE/ENFILE 等：记录后退出本轮，避免忙循环
            cout << "accept4 失败: " << strerror(errno) << endl;
            break;
        }

        // 为每个连接创建独立 Connection（含独立 MessageParser）
        Connection conn;
        conn.fd = c;
        m_connections.emplace(c, conn);

        struct epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
        ev.data.fd = c;
        if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, c, &ev) == -1)
        {
            cout << "epoll_ctl 注册客户端连接失败: " << strerror(errno) << endl;
            CloseConnection(c); // 注册失败，清理该连接
            continue;
        }

        cout << "客户端已连接, fd: " << c << endl;
    }
}

bool TcpSer::HandleClient(int fd, uint32_t events)
{
    auto it = m_connections.find(fd);
    if (it == m_connections.end())
    {
        // 本批事件中该连接可能已被清理（如 fd 复用），跳过，不访问无效对象
        return false;
    }
    Connection &conn = it->second;

    // 1. EPOLLERR：检查实际 socket 错误，不能与普通可读事件混为一谈
    if (events & EPOLLERR)
    {
        int sock_err = 0;
        socklen_t len = sizeof(sock_err);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &sock_err, &len) == 0 && sock_err != 0)
        {
            // 存在不可恢复错误，标记需要关闭
            cout << "连接发生错误, fd: " << fd
                 << ", errno: " << sock_err << " (" << strerror(sock_err) << ")" << endl;
            return true;
        }
        // SO_ERROR == 0（或 getsockopt 失败）：无实际错误，继续尝试读取
    }

    // 2. 可读 / 半关闭 / 挂起事件：优先读取已到达的数据，ET 下必须读到 EAGAIN
    if (events & (EPOLLIN | EPOLLRDHUP | EPOLLHUP))
    {
        uint8_t buff[1024];
        while (true)
        {
            ssize_t n = recv(fd, buff, sizeof(buff), 0);
            if (n > 0)
            {
                // 数据追加到该连接自己的 MessageParser
                conn.parser.Append(buff, static_cast<size_t>(n));

                // 解析循环：直到没有完整消息
                MessageHeader header;
                vector<uint8_t> payload;
                while (true)
                {
                    DecodeStatus status = conn.parser.Parse(header, payload);
                    if (status == DecodeStatus::COMPLETE)
                    {
                        // 交给业务分发；返回 true 表示需要关闭连接（本批业务不触发）
                        if (ProcessMessage(conn, header, payload))
                        {
                            return true;
                        }
                        continue; // 继续解析同一批次中的下一条
                    }
                    else if (status == DecodeStatus::INCOMPLETE)
                    {
                        break; // 数据不足，等待更多数据
                    }
                    else // DecodeStatus::INVALID
                    {
                        cout << "协议错误, fd: " << fd << "，关闭连接" << endl;
                        return true; // 标记需要关闭
                    }
                }
                continue; // 继续 recv，直到 EAGAIN
            }
            else if (n == 0)
            {
                // 对端关闭发送方向(EOF)：已读尽，标记需要关闭
                cout << "对端关闭连接(EOF), fd: " << fd << endl;
                return true;
            }
            else // n < 0
            {
                if (errno == EINTR)
                {
                    continue; // 被信号中断，重试
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    break; // 本轮数据已读尽，保留连接
                }
                // 其他错误：ECONNRESET 等，标记需要关闭
                cout << "recv 失败, fd: " << fd << ": " << strerror(errno) << endl;
                return true;
            }
        }
    }

    return false; // 保留连接
}

bool TcpSer::ProcessMessage(Connection &conn, const MessageHeader &header, const std::vector<uint8_t> &payload)
{
    switch (header.type)
    {
    case MessageType::DEVICE_REGISTER:
        HandleRegister(conn, header);
        break;
    case MessageType::DATA_REPORT:
        HandleDataReport(conn, header, payload);
        break;
    case MessageType::HEARTBEAT:
        HandleHeartbeat(conn, header);
        break;
    case MessageType::CONTROL_CMD:
    case MessageType::CONTROL_ACK:
        // 合法但暂不支持的消息类型：仅日志，不误判为协议错误，也不断开
        cout << "暂不处理该消息类型: " << static_cast<int>(header.type) << endl;
        break;
    default:
        cout << "未知消息类型: " << static_cast<int>(header.type) << endl;
        break;
    }
    return false; // 本批业务不触发关闭
}

void TcpSer::HandleRegister(Connection &conn, const MessageHeader &header)
{
    // 一个连接只允许注册一次；重复注册直接拒绝
    if (conn.registered)
    {
        cout << "当前连接已注册设备，拒绝重复注册" << endl;
        return;
    }

    if (m_device_manager.RegisterDevice(header.device_id, conn.fd))
    {
        conn.registered = true;
        conn.registered_device_id = header.device_id;
        cout << "设备注册成功，Device ID: " << header.device_id << endl;
        m_device_manager.PrintDevices(); // 注册成功后展示设备列表
    }
    else
    {
        // 注册失败（如该 ID 已在线），不得覆盖已有 Socket/在线状态，也不得把 conn 标记为已注册
        cout << "设备注册失败，Device ID: " << header.device_id << endl;
    }
}

void TcpSer::HandleDataReport(Connection &conn, const MessageHeader &header, const std::vector<uint8_t> &payload)
{
    // 必须先注册，且消息 Device ID 必须等于本连接注册的 Device ID
    if (!conn.registered)
    {
        cout << "当前连接未注册设备，拒绝处理 DATA_REPORT" << endl;
        return;
    }
    if (header.device_id != conn.registered_device_id)
    {
        cout << "消息 Device ID 与当前连接注册的设备不一致，拒绝处理 DATA_REPORT" << endl;
        return;
    }
    // DeviceManager 中该设备仍在线，且 sockfd 等于当前连接
    DeviceInfo *dev = m_device_manager.GetDevice(conn.registered_device_id);
    if (dev == nullptr || !dev->online || dev->sockfd != conn.fd)
    {
        cout << "设备状态校验失败，拒绝处理 DATA_REPORT" << endl;
        return;
    }

    // 校验通过。注意：payload 不保证以 '\0' 结尾，按字节打印十六进制，避免越界。
    cout << "收到 DATA_REPORT，设备ID: " << header.device_id
         << "，Payload(" << payload.size() << " 字节): ";
    for (uint8_t byte : payload)
    {
        printf("%02x ", static_cast<unsigned int>(byte));
    }
    cout << endl;
}

void TcpSer::HandleHeartbeat(Connection &conn, const MessageHeader &header)
{
    // 与 DATA_REPORT 做相同的身份/状态校验，暂不实现超时机制
    if (!conn.registered || header.device_id != conn.registered_device_id)
    {
        cout << "心跳身份校验失败，拒绝处理 HEARTBEAT" << endl;
        return;
    }
    DeviceInfo *dev = m_device_manager.GetDevice(conn.registered_device_id);
    if (dev == nullptr || !dev->online || dev->sockfd != conn.fd)
    {
        cout << "心跳设备状态校验失败，拒绝处理 HEARTBEAT" << endl;
        return;
    }
    cout << "收到 HEARTBEAT，设备ID: " << header.device_id << endl;
}

void TcpSer::CloseConnection(int fd)
{
    auto it = m_connections.find(fd);
    if (it == m_connections.end())
    {
        return; // 已清理，幂等返回
    }
    Connection &conn = it->second;

    // 1. 从 epoll 注销（先于 close，避免 fd 复用后残留旧事件）
    epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, nullptr);

    // 2. 若该连接已注册设备，标记离线（复用 DeviceManager 现有逻辑）
    if (conn.registered)
    {
        m_device_manager.MarkOffline(conn.registered_device_id, fd);
    }

    // 3. 关闭 socket
    close(fd);

    // 4. 从容器移除。注意：erase 之后 conn 引用失效，此后不得再访问 conn。
    m_connections.erase(it);

    cout << "连接关闭, fd: " << fd << endl;
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