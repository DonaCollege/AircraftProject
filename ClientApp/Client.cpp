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
#include <iomanip>
#pragma comment(lib, "Ws2_32.lib")

constexpr int DEFAULT_PORT = 54000;

int main(int argc, char* argv[])
{
    // ── Command-line arguments ──────────────────────────────────────────
    if (argc < 2)
    {
        std::cerr << "Usage: ClientApp.exe <server_ip> [port] [telemetry_file]\n";
        std::cerr << "Example: ClientApp.exe 192.168.1.100 54000 Telem_2023_3_12.txt\n";
        return 1;
    }

    std::string serverIP = argv[1];
    int         port = (argc >= 3) ? std::atoi(argv[2]) : DEFAULT_PORT;
    std::string telemFile = (argc >= 4) ? argv[3] : "katl-kefd-B737-700.txt";

    // ── Unique Plane ID using PID ───────────────────────────────────────
    int planeID = static_cast<int>(GetCurrentProcessId());
    std::cout << "=================================\n";
    std::cout << "  Plane ID : " << planeID << "\n";
    std::cout << "  Server   : " << serverIP << ":" << port << "\n";
    std::cout << "  Telem    : " << telemFile << "\n";
    std::cout << "=================================\n";

    // ── Open telemetry file ─────────────────────────────────────────────
    std::ifstream telemStream(telemFile);
    if (!telemStream.is_open())
    {
        std::cerr << "Error: Cannot open '" << telemFile << "'\n";
        std::cerr << "Make sure the .txt file is in the SAME FOLDER as ClientApp.exe\n";
        return 1;
    }
    std::cout << "Telemetry file opened successfully.\n";

    // ── Winsock startup ─────────────────────────────────────────────────
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    // ── Create socket ───────────────────────────────────────────────────
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
    {
        std::cerr << "socket() failed. Error: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    // ── Server address setup ────────────────────────────────────────────
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(static_cast<u_short>(port));

    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr) <= 0)
    {
        std::cerr << "Invalid IP address: " << serverIP << "\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // ── Connect to server ───────────────────────────────────────────────
    std::cout << "Connecting to server...\n";
    if (connect(sock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR)
    {
        std::cerr << "Error: Cannot connect to " << serverIP << ":" << port << "\n";
        std::cerr << "Is ServerApp.exe running?\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }
    std::cout << "Connected! Sending telemetry data...\n";

    // ── Parse and send telemetry data ───────────────────────────────────
    std::string line;
    int         packetCount = 0;
    bool        headerSkipped = false;

    while (std::getline(telemStream, line))
    {
        // Strip carriage return if present
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // Skip empty lines
        if (line.empty()) continue;

        // Skip the header line "FUEL TOTAL QUANTITY,..."
        // It only appears once at the top
        if (!headerSkipped)
        {
            if (line.find("FUEL") != std::string::npos ||
                line.find("fuel") != std::string::npos)
            {
                headerSkipped = true;
                continue;
            }
        }

        // ── Parse line format: " 3_3_2023 14:53:21,4564.466309, " ──────
        // Step 1: trim leading spaces
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        // Step 2: split by comma
        // Format: "3_3_2023 14:53:21,4564.466309,"
        size_t firstComma = line.find(',');
        if (firstComma == std::string::npos) continue;

        std::string timestamp = line.substr(0, firstComma);
        std::string restOfLine = line.substr(firstComma + 1);

        // Step 3: get fuel value (before the trailing comma)
        size_t secondComma = restOfLine.find(',');
        std::string fuelStr = (secondComma != std::string::npos)
            ? restOfLine.substr(0, secondComma)
            : restOfLine;

        // Step 4: trim spaces from fuel string
        size_t fuelStart = fuelStr.find_first_not_of(" \t");
        if (fuelStart == std::string::npos) continue;
        fuelStr = fuelStr.substr(fuelStart);

        // Step 5: convert fuel to double
        double fuel = 0.0;
        try {
            fuel = std::stod(fuelStr);
        }
        catch (...) {
            continue; // skip bad lines
        }

        // Step 6: trim spaces from timestamp
        size_t tsStart = timestamp.find_first_not_of(" \t");
        if (tsStart == std::string::npos) continue;
        timestamp = timestamp.substr(tsStart);

        // ── Build packet: "planeID,timestamp,fuel\n" ───────────────────
        std::ostringstream packet;
        packet << planeID << ","
            << timestamp << ","
            << std::fixed << std::setprecision(6) << fuel << "\n";

        std::string p = packet.str();

        if (send(sock, p.c_str(), (int)p.size(), 0) == SOCKET_ERROR)
        {
            std::cerr << "send() failed. Error: " << WSAGetLastError() << "\n";
            break;
        }

        packetCount++;

        // Show progress every 100 packets
        if (packetCount % 100 == 0)
            std::cout << "Sent " << packetCount << " packets...\n";

        // Small delay so server is not flooded
        Sleep(10);
    }

    // ── Send END OF FLIGHT signal ───────────────────────────────────────
    std::string endMsg = std::to_string(planeID) + ",END\n";
    if (send(sock, endMsg.c_str(), (int)endMsg.size(), 0) == SOCKET_ERROR)
        std::cerr << "Warning: Could not send END signal.\n";
    else
        std::cout << "END signal sent to server.\n";

    // ── Cleanup ─────────────────────────────────────────────────────────
    std::cout << "=================================\n";
    std::cout << "  Flight finished!\n";
    std::cout << "  Plane ID     : " << planeID << "\n";
    std::cout << "  Packets sent : " << packetCount << "\n";
    std::cout << "=================================\n";

    telemStream.close();
    closesocket(sock);
    WSACleanup();

    return 0;
}