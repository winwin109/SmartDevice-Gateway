#include "device_manager.h"

#include <cassert>
#include <cstdint>
#include <iostream>

namespace {

// 各测试场景共享同一个 DeviceManager，以复现从注册到重连的完整状态流转。
// 每个测试函数只负责自己场景内的断言，并打印清晰的测试名称与通过结果。

void TestRegisterNewDevice(DeviceManager &manager)
{
    std::cout << "测试1：注册新设备 ...\n";

    bool ok = manager.RegisterDevice(1001, 5);
    assert(ok == true);

    DeviceInfo *dev = manager.GetDevice(1001);
    assert(dev != nullptr);
    assert(dev->device_id == 1001);
    assert(dev->sockfd == 5);
    assert(dev->online == true);

    std::cout << "测试1：注册新设备 通过\n";
}

void TestDuplicateRegisterOnline(DeviceManager &manager)
{
    std::cout << "测试2：重复注册在线设备 ...\n";

    bool ok = manager.RegisterDevice(1001, 6);
    assert(ok == false);

    DeviceInfo *dev = manager.GetDevice(1001);
    assert(dev != nullptr);
    assert(dev->sockfd == 5);
    assert(dev->online == true);

    std::cout << "测试2：重复注册在线设备 通过\n";
}

void TestMarkOfflineWrongSocket(DeviceManager &manager)
{
    std::cout << "测试3：错误 Socket 标记离线 ...\n";

    bool ok = manager.MarkOffline(1001, 6);
    assert(ok == false);

    DeviceInfo *dev = manager.GetDevice(1001);
    assert(dev != nullptr);
    assert(dev->sockfd == 5);
    assert(dev->online == true);

    std::cout << "测试3：错误 Socket 标记离线 通过\n";
}

void TestMarkOfflineCorrectSocket(DeviceManager &manager)
{
    std::cout << "测试4：正确 Socket 标记离线 ...\n";

    bool ok = manager.MarkOffline(1001, 5);
    assert(ok == true);

    DeviceInfo *dev = manager.GetDevice(1001);
    assert(dev != nullptr);
    assert(dev->online == false);
    assert(dev->sockfd == -1);

    std::cout << "测试4：正确 Socket 标记离线 通过\n";
}

void TestMarkOfflineAgain(DeviceManager &manager)
{
    std::cout << "测试5：重复标记离线 ...\n";

    bool ok = manager.MarkOffline(1001, 5);
    assert(ok == false);

    DeviceInfo *dev = manager.GetDevice(1001);
    assert(dev != nullptr);
    assert(dev->online == false);
    assert(dev->sockfd == -1);

    std::cout << "测试5：重复标记离线 通过\n";
}

void TestReconnectOfflineDevice(DeviceManager &manager)
{
    std::cout << "测试6：离线设备重新注册 ...\n";

    bool ok = manager.RegisterDevice(1001, 9);
    assert(ok == true);

    DeviceInfo *dev = manager.GetDevice(1001);
    assert(dev != nullptr);
    assert(dev->sockfd == 9);
    assert(dev->online == true);

    std::cout << "测试6：离线设备重新注册 通过\n";
}

void TestQueryNonexistentDevice(DeviceManager &manager)
{
    std::cout << "测试7：查询不存在的设备 ...\n";

    DeviceInfo *dev = manager.GetDevice(9999);
    assert(dev == nullptr);

    std::cout << "测试7：查询不存在的设备 通过\n";
}

void TestRegisterMultipleDevices(DeviceManager &manager)
{
    std::cout << "测试8：注册多个不同设备 ...\n";

    bool ok1 = manager.RegisterDevice(1002, 10);
    bool ok2 = manager.RegisterDevice(1003, 11);
    assert(ok1 == true);
    assert(ok2 == true);

    DeviceInfo *dev1 = manager.GetDevice(1001);
    DeviceInfo *dev2 = manager.GetDevice(1002);
    DeviceInfo *dev3 = manager.GetDevice(1003);

    assert(dev1 != nullptr);
    assert(dev2 != nullptr);
    assert(dev3 != nullptr);

    assert(dev1->sockfd == 9 && dev1->online == true);
    assert(dev2->sockfd == 10 && dev2->online == true);
    assert(dev3->sockfd == 11 && dev3->online == true);

    std::cout << "测试8：注册多个不同设备 通过\n";
}

void TestPrintDevices(DeviceManager &manager)
{
    std::cout << "测试9：打印设备列表 ...\n";

    manager.PrintDevices();

    // 打印不应改变设备记录
    assert(manager.GetDevice(1001) != nullptr);
    assert(manager.GetDevice(1002) != nullptr);
    assert(manager.GetDevice(1003) != nullptr);

    std::cout << "测试9：打印设备列表 通过\n";
}

} // namespace

int main()
{
    DeviceManager manager;

    TestRegisterNewDevice(manager);
    TestDuplicateRegisterOnline(manager);
    TestMarkOfflineWrongSocket(manager);
    TestMarkOfflineCorrectSocket(manager);
    TestMarkOfflineAgain(manager);
    TestReconnectOfflineDevice(manager);
    TestQueryNonexistentDevice(manager);
    TestRegisterMultipleDevices(manager);
    TestPrintDevices(manager);

    std::cout << "\n全部 9 个测试场景全部通过\n";
    return 0;
}
