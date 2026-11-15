#include "device_manager.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
using namespace std;

bool DeviceManager::RegisterDevice(uint32_t device_id, int sockfd)
{
    auto it = devices_.find(device_id);

    // 新设备
    if (it == devices_.end())
    {
        devices_.emplace(device_id, DeviceInfo{device_id, sockfd, true});
        return true;
    }

    // 已存在且在线
    if (it->second.online)
    {
        cout << "此ID已经注册使用" << endl;
        return false;
    }

    // 已存在但离线，允许重连
    it->second.sockfd = sockfd;
    it->second.online = true;
    return true;
}

bool DeviceManager::MarkOffline(uint32_t device_id, int sockfd)
{
    auto it = devices_.find(device_id);

    if (it == devices_.end())
    {
        cout << "当前设备不存在" << endl;
        return false;
    }

    if (it->second.sockfd != sockfd)
    {
        cout << "传入的sockfd与此设备不匹配" << endl;
        return false;
    }

    it->second.online = false;
    it->second.sockfd = -1;
    return true;
}

DeviceInfo *DeviceManager::GetDevice(uint32_t device_id)
{
    auto it = devices_.find(device_id);

    if (it != devices_.end())
    {
        return &it->second;
    }
    return nullptr;
}

void DeviceManager::PrintDevices()
{
    for (const auto &item : devices_)
    {
        // item.first：设备 ID
        // item.second：DeviceInfo
        const DeviceInfo &info = item.second;
        cout << "Device ID: " << info.device_id
             << ", Socket: " << info.sockfd
             << ", " << (info.online ? "Online" : "Offline") << endl;
    }
}
