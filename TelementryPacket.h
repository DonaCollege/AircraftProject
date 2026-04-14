#pragma once
#include <string>

struct TelemetryPacket
{
    int         planeID;
    std::string timestamp;    // ✅ changed to string - holds "3_3_2023 14:53:21"
    double      fuelRemaining;
    bool        isEnd = false; // ✅ for END signal
};