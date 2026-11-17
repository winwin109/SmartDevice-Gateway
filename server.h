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
#include <fcntl.h>
#include <pthread.h>
#include <jsoncpp/json/json.h>
#include <mysql/mysql.h>

// mysql 通过 mysql_version.h 定义了宏 PROTOCOL_VERSION，会与 protocol.h 中的
// constexpr uint8_t PROTOCOL_VERSION 冲突，这里在包含 mysql 头之后清除该宏。
#undef PROTOCOL_VERSION

#include <unordered_map>

// 注意：message_parser.h 会间接包含 protocol.h，必须在 #undef PROTOCOL_VERSION 之后引入，
// 否则 mysql 头定义的 PROTOCOL_VERSION 宏会与 protocol.h 中的 constexpr 冲突。
#include "message_parser.h"
#include "device_manager.h"

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
// 每个 TCP 连接一份独立状态。改造为 epoll 后，连接状态必须跨多次事件持续存在，
// 因此将原 Run() 中的栈局部变量（parser / registered / registered_device_id）提升为常驻对象。
struct Connection
{
    int fd = -1;                          // 连接 socket（非阻塞）
    MessageParser parser;                  // 每连接独立的解析器（拥有自己的 recv_buffer_）
    bool registered = false;               // 该连接是否已完成设备注册
    uint32_t registered_device_id = 0;     // 已注册的设备 ID
};

class TcpSer
{
public:
    TcpSer(const SerConf &sc) : m_conf(sc), m_sockfd(-1), m_epfd(-1)
    {
    }

    ~TcpSer();

    bool Ser_Init();
    void Run(); // 进入 epoll 事件循环

private:
    bool create_socket(); // 创建监听套接子（含非阻塞设置）

    // ---- epoll 基础骨架 ----
    void AcceptConnections();                   // 循环 accept4 接受所有就绪连接
    bool HandleClient(int fd, uint32_t events); // 客户端事件入口；返回 true 表示该连接需要关闭
    void CloseConnection(int fd);               // 统一清理单个连接

    // ---- 业务分发 ----
    bool ProcessMessage(Connection &conn, const MessageHeader &header, const std::vector<uint8_t> &payload); // 返回 true 表示需要关闭连接
    void HandleRegister(Connection &conn, const MessageHeader &header);
    void HandleDataReport(Connection &conn, const MessageHeader &header, const std::vector<uint8_t> &payload);
    void HandleHeartbeat(Connection &conn, const MessageHeader &header);

private:
    int m_sockfd;   // 监听套接子
    int m_epfd;     // epoll 实例 fd
    SerConf m_conf; // 配置文件的信息
    DeviceManager m_device_manager;                    // 设备注册/在线状态管理
    std::unordered_map<int, Connection> m_connections; // fd -> 连接对象
};