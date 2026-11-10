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

        char *k = strtok(buff, " \t\r\n");   // 支持空格/Tab/回车/换行作为分隔符
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

        char buff[256] = {0};
        while (true) // 同一个客户端可以连续发送多条消息
        {
            memset(buff, 0, sizeof(buff));
            int n = recv(c, buff, sizeof(buff) - 1, 0);
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
            buff[n] = '\0';
            cout << "收到客户端消息: " << buff << endl;
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