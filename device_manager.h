
#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <cstdint>
#include <unordered_map>

struct DeviceInfo
{
    uint32_t device_id;
    int sockfd;
    bool online;
};

class DeviceManager
{
private:
    std::unordered_map<uint32_t, DeviceInfo> devices_;

public:
    bool RegisterDevice(uint32_t device_id, int sockfd);

    bool MarkOffline(uint32_t device_id, int sockfd);

    DeviceInfo *GetDevice(uint32_t device_id);

    void PrintDevices();
};

#endif
