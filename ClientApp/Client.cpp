#include <iostream>
#include <fstream>
#include <string>
#include <atomic>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sstream>
#include <iomanip>
#include <windows.h>


#pragma comment(lib, "ws2_32.lib")

#define PORT 54000

std::atomic<int> idCounter(100);

// Helper: convert HH:MM:SS → seconds
int timeToSeconds(const std::string& timeStr) {
    int h, m, s;
    char colon;
    std::istringstream ss(timeStr);
    ss >> h >> colon >> m >> colon >> s;
    return h * 3600 + m * 60 + s;
}
std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t");
    size_t end = str.find_last_not_of(" \t");
    return (start == std::string::npos) ? "" : str.substr(start, end - start + 1);
}
int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: Client.exe <server_ip> <file>\n";
        return 1;
    }

    std::string serverIP = argv[1];
    std::string filename = argv[2];

    int planeID = GetCurrentProcessId();

    std::cout << "Plane ID: " << planeID << "\n";

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "Error opening file\n";
        return 1;
    }

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);

    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cout << "Connection failed\n";
        return 1;
    }

    std::cout << "Sending data...\n";
    std::string line;

    // Skip header
    getline(file, line);

    while (getline(file, line)) {
        size_t comma1 = line.find(',');
        size_t comma2 = line.find(',', comma1 + 1);

        if (comma1 == std::string::npos || comma2 == std::string::npos)
            continue;

        // Clean fields
        std::string datetime = trim(line.substr(0, comma1));
        std::string fuelStr = trim(line.substr(comma1 + 1, comma2 - comma1 - 1));

        // Extract time (after space)
        size_t spacePos = datetime.find(' ');
        if (spacePos == std::string::npos)
            continue;

        std::string timePart = datetime.substr(spacePos + 1);

        // Convert to seconds
        int timestamp = timeToSeconds(timePart);

        // Convert to RELATIVE time
        static int startTime = -1;
        if (startTime == -1)
            startTime = timestamp;

        int relativeTime = timestamp - startTime;

        // Convert fuel safely
        double fuel = std::stod(fuelStr);

        // Build packet
        std::string packet =
            std::to_string(planeID) + "," +
            std::to_string(relativeTime) + "," +
            std::to_string(fuel) + "," +
            timePart + "\n";

        std::cout << "Sending: " << packet;
        std::string endMsg = "END," + std::to_string(planeID) + "\n";
        send(sock, endMsg.c_str(), endMsg.size(), 0);
        send(sock, packet.c_str(), packet.size(), 0);
    }

    closesocket(sock);
    WSACleanup();

    std::cout << "Flight finished.\n";
    return 0;
}