#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <string>
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <jsoncpp/json/json.h>
#include <mysql/mysql.h>

// mysql 通过 mysql_version.h 定义了宏 PROTOCOL_VERSION，会与 protocol.h 中的
// constexpr uint8_t PROTOCOL_VERSION 冲突，这里在包含 mysql 头之后清除该宏。
#undef PROTOCOL_VERSION

using namespace std;
struct SerConf
{
    SerConf()
    {
        ips = "127.0.0.1";
        port = 6000;
        lismax = 128;
        taskmax = 128;
    }

    bool ReadConf(string filename); // 读取配置文件
    void PrintInfo();               // 打印读取的配置信息

    string ips;
    int port;    // 服务器端口（int 才能完整表示 0~65535）
    int lismax;  // 监听队列大小
    int taskmax; // 任务队列大小
};
class TcpSer
{
public:
    TcpSer(const SerConf &sc) : m_conf(sc)
    {
        m_sockfd = -1;
    }

    bool Ser_Init();
    void Run(); // 接收客户端连接并处理消息

private:
    bool create_socket(); // 创建监听套接子

private:
    int m_sockfd;   // 监听套接子
    SerConf m_conf; // 配置文件的信息
};