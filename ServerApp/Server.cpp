#include <iostream>
#include <thread>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <sstream>
#include <fstream>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define DEFAULT_PORT 54000
#define BUFFER_SIZE 4096

struct TelemetryPacket {
    int planeID;
    int timestamp;      // relative time (for calculation)
    double fuelRemaining;
    std::string realTime; // display only
};

struct FlightState {
    int planeID;
    double initialFuel = 0;
    double currentFuel = 0;
    int currentTimestamp = 0;
    bool firstPacket = true;

    double getAvgConsumption() const {
        if (firstPacket || currentTimestamp == 0) return 0.0;
        return (initialFuel - currentFuel) / (double)currentTimestamp;
    }
};

std::unordered_map<int, FlightState> g_flights;
std::mutex g_mutex;

std::ofstream logFile("flight_log.csv");

// ================= PARSER =================
bool parsePacket(const std::string& line, TelemetryPacket& pkt) {
    std::istringstream ss(line);
    std::string timeStr;
    char comma;

    if (!(ss >> pkt.planeID >> comma
        >> pkt.timestamp >> comma
        >> pkt.fuelRemaining >> comma
        >> timeStr))
        return false;

    pkt.realTime = timeStr;
    return true;
}

// ================= FLIGHT MANAGER =================
void updateFlight(const TelemetryPacket& pkt) {
    std::lock_guard<std::mutex> lock(g_mutex);

    auto& flight = g_flights[pkt.planeID];
    flight.planeID = pkt.planeID;

    if (flight.firstPacket) {
        flight.initialFuel = pkt.fuelRemaining;
        flight.firstPacket = false;
    }

    flight.currentFuel = pkt.fuelRemaining;
    flight.currentTimestamp = pkt.timestamp;

    double avg = flight.getAvgConsumption();

    std::cout << "Plane " << pkt.planeID
        << " | Time:" << pkt.realTime
        << " | Fuel:" << pkt.fuelRemaining
        << " | Avg:" << avg << std::endl;

    logFile << pkt.planeID << "," << pkt.timestamp << ","
        << pkt.fuelRemaining << "," << avg << "\n";
}

void finalizeFlight(int planeID) {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_flights.find(planeID) != g_flights.end()) {
        double finalAvg = g_flights[planeID].getAvgConsumption();

        std::cout << "[FLIGHT COMPLETE] Plane " << planeID
            << " | Final Avg: " << finalAvg << " gal/s\n";

        logFile << "FINAL," << planeID << "," << finalAvg << "\n";

        g_flights.erase(planeID);
    }
}

// ================= CLIENT HANDLER =================
void handleClient(SOCKET clientSocket) {
    char buffer[BUFFER_SIZE];
    std::string dataBuffer;
    int planeID = -1;

    while (true) {
        int bytes = recv(clientSocket, buffer, BUFFER_SIZE, 0);

        if (bytes <= 0) {
            if (planeID != -1)
                finalizeFlight(planeID);
            break;
        }

        dataBuffer.append(buffer, bytes);

        size_t pos;
        while ((pos = dataBuffer.find('\n')) != std::string::npos) {
            std::string line = dataBuffer.substr(0, pos);
            dataBuffer.erase(0, pos + 1);

            TelemetryPacket pkt;
            if (parsePacket(line, pkt)) {
                planeID = pkt.planeID;
                updateFlight(pkt);
            }
        }
    }

    closesocket(clientSocket);
}

// ================= MAIN =================
int main(int argc, char* argv[]) {
    int port = DEFAULT_PORT;
    if (argc > 1) port = atoi(argv[1]);

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, SOMAXCONN);

    std::cout << "Server started on port " << port << "\n";
    std::cout << "Waiting for clients...\n";

    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        std::thread(handleClient, clientSocket).detach();
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}