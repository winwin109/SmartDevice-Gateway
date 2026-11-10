#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <jsoncpp/json/json.h>

using namespace std;

class TcpClient
{
public:
    TcpClient(string ser_ips, int ser_port) : ips(ser_ips), port(ser_port)
    {
        runing = true;
        sockfd = -1;
    }

    TcpClient()
    {
        ips = "127.0.0.1";
        port = 6000;
        runing = true;
        sockfd = -1; // 与带参构造函数保持一致，避免析构时 close 未初始化的 fd
    }

    ~TcpClient()
    {
        if (sockfd != -1) // 兜底：防止重复关闭或关闭无效 fd
        {
            close(sockfd);
            sockfd = -1;
        }
    }

    bool Socket_Init();

    void Run();

private:
    string ips; // 服务ip地址
    int port;   // 服务器端口

    int sockfd;
    bool runing; // 客户端是否继续运行
};