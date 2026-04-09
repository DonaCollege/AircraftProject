#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <iomanip>

#pragma comment(lib, "Ws2_32.lib")

constexpr int DEFAULT_PORT = 54000;

struct TelemetryPacket
{
    int    planeID = 0;
    int    timestamp = 0;
    double fuelRemaining = 0.0;
};

struct FlightState
{
    int    planeID = 0;
    double initialFuel = 0.0;
    double currentFuel = 0.0;
    int    currentTimestamp = 0;
    bool   firstPacket = true;

    double GetAvgConsumption() const
    {
        if (firstPacket || currentTimestamp == 0) return 0.0;
        return (initialFuel - currentFuel) / static_cast<double>(currentTimestamp);
    }
};

extern std::unordered_map<int, FlightState> g_flights;
extern std::mutex                           g_flightsMutex;
extern std::ofstream                        g_logFile;
extern std::mutex                           g_logMutex;

TelemetryPacket ParsePacket(const std::string& line);
void RegisterFlight(int planeID);
void UpdateAndLog(int planeID, const TelemetryPacket& pkt);
void FinalizeFlight(int planeID);
void ClientHandler(SOCKET clientSocket);