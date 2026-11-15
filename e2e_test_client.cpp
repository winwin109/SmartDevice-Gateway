// 端到端测试客户端：验证 DeviceManager 接入 TcpSer 后的业务边界。
// 可构造并发送指定类型、指定 Device ID 的协议消息，不覆盖 client.cpp。
// 服务端无注册 ACK：客户端仅能直接断言“发送成功/连接被关闭”，其余需看服务端日志。
// 使用始终执行的 CHECK 机制代替 assert，避免带副作用的表达式被 NDEBUG 移除。

#include "protocol.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

using namespace std;

// 始终执行的检查：先求值 expr，再判断；失败时打印位置并退出。
#define CHECK(expr)                                                        \
    do                                                                     \
    {                                                                      \
        bool check_result = (expr);                                        \
        if (check_result)                                                  \
        {                                                                  \
            cout << "  CHECK 通过: " #expr << endl;                        \
        }                                                                  \
        else                                                               \
        {                                                                  \
            cerr << "CHECK 失败: " #expr << " @ " << __FILE__ << ":" << __LINE__ << endl; \
            exit(1);                                                       \
        }                                                                  \
    } while (0)

namespace {
const char *SERVER_IP = "127.0.0.1";
const int SERVER_PORT = 6000;
const uint32_t DEVICE_100 = 100;
const uint32_t DEVICE_200 = 200;

int Connect()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
    {
        cerr << "socket 失败: " << strerror(errno) << endl;
        return -1;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        cerr << "connect 失败: " << strerror(errno) << endl;
        close(fd);
        return -1;
    }
    return fd;
}

void SetRecvTimeout(int fd, int seconds)
{
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

bool SendMessage(int fd, MessageType type, uint32_t device_id, const vector<uint8_t> &payload)
{
    MessageHeader header;
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = type;
    header.device_id = device_id;
    header.length = payload.size();

    vector<uint8_t> msg = EncodeMessage(header, payload);
    if (msg.empty())
    {
        cerr << "EncodeMessage 失败" << endl;
        return false;
    }
    size_t total = msg.size();
    size_t sent = 0;
    while (sent < total)
    {
        ssize_t n = send(fd, msg.data() + sent, total - sent, 0);
        if (n < 0)
        {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool SendRaw(int fd, const uint8_t *data, size_t len)
{
    size_t sent = 0;
    while (sent < len)
    {
        ssize_t n = send(fd, data + sent, len - sent, 0);
        if (n < 0)
        {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool ExpectClosed(int fd)
{
    uint8_t buf[64];
    return recv(fd, buf, sizeof(buf), 0) == 0; // 0 == EOF：对端已关闭
}
} // namespace

int main()
{
    signal(SIGPIPE, SIG_IGN);
    cout << "===== DeviceManager 接入端到端测试开始 =====" << endl;

    // 场景1 & 2：注册 100 → 重复注册(拒绝) → DATA_REPORT(100) 验证首次注册仍有效
    {
        cout << "\n[场景1] 注册设备 100 ..." << endl;
        int fd = Connect();
        CHECK(fd != -1);
        CHECK(SendMessage(fd, MessageType::DEVICE_REGISTER, DEVICE_100, {}));
        cout << "  (服务端应: 设备注册成功，需看日志)" << endl;

        cout << "[场景2] 同一连接重复注册 100，再发送 DATA_REPORT(100) ..." << endl;
        CHECK(SendMessage(fd, MessageType::DEVICE_REGISTER, DEVICE_100, {}));
        cout << "  (服务端应: 拒绝重复注册，需看日志)" << endl;
        {
            vector<uint8_t> payload = {'o', 'k'};
            CHECK(SendMessage(fd, MessageType::DATA_REPORT, DEVICE_100, payload));
        }
        cout << "  (服务端应: 收到 DATA_REPORT(100)，说明首次注册仍有效，需看日志)" << endl;
        close(fd);
    }

    // 场景3：同一连接尝试注册两个不同设备 ID
    {
        cout << "\n[场景3] 同一连接先注册 100，再尝试注册 200，再发送 DATA_REPORT(200) ..." << endl;
        int fd = Connect();
        CHECK(fd != -1);
        CHECK(SendMessage(fd, MessageType::DEVICE_REGISTER, DEVICE_100, {}));
        cout << "  (服务端应: 注册 100 成功)" << endl;
        CHECK(SendMessage(fd, MessageType::DEVICE_REGISTER, DEVICE_200, {}));
        cout << "  (服务端应: 拒绝，当前连接已注册设备)" << endl;
        {
            vector<uint8_t> payload = {0x03, 0x04};
            CHECK(SendMessage(fd, MessageType::DATA_REPORT, DEVICE_200, payload));
        }
        cout << "  (服务端应: 拒绝，Device ID 与已注册设备不一致)" << endl;
        close(fd);
    }

    // 场景4：未注册连接发送 DATA_REPORT
    {
        cout << "\n[场景4] 未注册连接发送 DATA_REPORT(100) ..." << endl;
        int fd = Connect();
        CHECK(fd != -1);
        vector<uint8_t> payload = {0xAA, 0xBB};
        CHECK(SendMessage(fd, MessageType::DATA_REPORT, DEVICE_100, payload));
        cout << "  (服务端应: 拒绝，未注册设备，需看日志)" << endl;
        close(fd);
    }

    // 场景5：已注册 100，但发送 device_id=200
    {
        cout << "\n[场景5] 已注册 100，发送 device_id=200 的 DATA_REPORT ..." << endl;
        int fd = Connect();
        CHECK(fd != -1);
        CHECK(SendMessage(fd, MessageType::DEVICE_REGISTER, DEVICE_100, {}));
        vector<uint8_t> payload = {0x11, 0x22};
        CHECK(SendMessage(fd, MessageType::DATA_REPORT, DEVICE_200, payload));
        cout << "  (服务端应: 拒绝，Device ID 不一致，需看日志)" << endl;
        close(fd);
    }

    // 场景6：已注册 100，发送正常 DATA_REPORT(100)；场景7：随后断开
    {
        cout << "\n[场景6] 已注册 100，发送正常 DATA_REPORT(100) ..." << endl;
        int fd = Connect();
        CHECK(fd != -1);
        CHECK(SendMessage(fd, MessageType::DEVICE_REGISTER, DEVICE_100, {}));
        vector<uint8_t> payload = {'h', 'e', 'l', 'l', 'o'};
        CHECK(SendMessage(fd, MessageType::DATA_REPORT, DEVICE_100, payload));
        cout << "  (服务端应: 收到 DATA_REPORT，需看日志)" << endl;

        cout << "\n[场景7] 断开连接，设备 100 应变为 OFFLINE ..." << endl;
        close(fd);
        cout << "  (OFFLINE 由场景8重注册成功间接验证)" << endl;
    }

    // 场景8：重连并重注册；场景9：注册后发送非法协议
    {
        cout << "\n[场景8] 设备 100 重新连接并重新注册 ..." << endl;
        int fd = Connect();
        CHECK(fd != -1);
        CHECK(SendMessage(fd, MessageType::DEVICE_REGISTER, DEVICE_100, {}));
        cout << "  (若服务端注册成功，说明场景7已正确下线)" << endl;

        cout << "\n[场景9] 注册后发送非法协议，服务端应关闭连接并完成下线清理 ..." << endl;
        SetRecvTimeout(fd, 5);
        uint8_t bad[14];
        memset(bad, 0xFF, sizeof(bad));
        CHECK(SendRaw(fd, bad, sizeof(bad)));
        CHECK(ExpectClosed(fd)); // 直接断言：服务端关闭连接
        cout << "  (下线清理由下方重注册间接验证)" << endl;
        close(fd);

        cout << "\n[场景9-验证] 重连并再次注册 100，验证下线清理 ..." << endl;
        int fd2 = Connect();
        CHECK(fd2 != -1);
        CHECK(SendMessage(fd2, MessageType::DEVICE_REGISTER, DEVICE_100, {}));
        cout << "  (服务端应注册成功，说明场景9已完成下线清理)" << endl;
        close(fd2);
    }

    cout << "\n===== 客户端侧全部场景执行完毕 =====" << endl;
    cout << "结论说明: CHECK 直接断言客户端侧的“发送成功/连接被关闭”；服务端注册/拒绝/下线行为需结合服务端日志确认。" << endl;
    return 0;
}
