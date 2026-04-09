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
#include <atomic>
#include <iomanip>

#pragma comment(lib, "Ws2_32.lib")

static std::atomic<int> g_idCounter{ 100 };
constexpr int DEFAULT_PORT = 54000;

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: ClientApp.exe <server_ip> [port] [telemetry_file]\n";
        std::cerr << "Example: ClientApp.exe 127.0.0.1 54000 katl-kefd-B737-700.txt\n";
        return 1;
    }

    std::string serverIP = argv[1];
    int         port = (argc >= 3) ? std::atoi(argv[2]) : DEFAULT_PORT;
    std::string telemFile = (argc >= 4) ? argv[3] : "katl-kefd-B737-700.txt";

    int planeID = g_idCounter.fetch_add(1) + (GetCurrentProcessId() % 10000) * 10;
    std::cout << "Plane ID: " << planeID << "\n";

    std::ifstream telemStream(telemFile);
    if (!telemStream.is_open())
    {
        std::cerr << "Error: Cannot open '" << telemFile << "'\n";
        std::cerr << "Make sure the .txt file is in the SAME FOLDER as ClientApp.exe\n";
        return 1;
    }
    std::cout << "Reading telemetry file...\n";

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
    {
        std::cerr << "socket() failed.\n";
        WSACleanup(); return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(static_cast<u_short>(port));
    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr) <= 0)
    {
        std::cerr << "Invalid IP address: " << serverIP << "\n";
        closesocket(sock); WSACleanup(); return 1;
    }

    if (connect(sock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR)
    {
        std::cerr << "Error: Cannot connect to " << serverIP << ":" << port << "\n";
        std::cerr << "Is ServerApp.exe running?\n";
        closesocket(sock); WSACleanup(); return 1;
    }

    std::cout << "Sending data to server...\n";

    std::string line;
    while (std::getline(telemStream, line))
    {
        if (line.empty()) continue;
        if (line[0] < '0' || line[0] > '9') continue;

        std::istringstream ss(line);
        int timestamp = 0; double fuel = 0.0; char comma;
        if (!(ss >> timestamp >> comma >> fuel)) continue;

        std::ostringstream packet;
        packet << planeID << "," << timestamp << ","
            << std::fixed << std::setprecision(2) << fuel << "\n";

        std::string p = packet.str();
        if (send(sock, p.c_str(), (int)p.size(), 0) == SOCKET_ERROR)
        {
            std::cerr << "send() failed.\n";
            break;
        }
    }

    closesocket(sock);
    WSACleanup();
    std::cout << "Flight finished.\n";
    return 0;
}