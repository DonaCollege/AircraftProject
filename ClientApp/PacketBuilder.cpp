#include "../TelementryPacket.h"
#include "TelemetryReader.h"

TelemetryPacket buildPacket(int id, double time, double fuel)
{
    TelemetryPacket packet;
    packet.planeID = id;
    packet.timestamp = time;
    packet.fuelRemaining = fuel;
    return packet;
}