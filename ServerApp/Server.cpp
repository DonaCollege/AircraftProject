#include "Server.h"

// Global definitions
std::unordered_map<int, FlightState> g_flights;
std::mutex                           g_flightsMutex;
std::ofstream                        g_logFile;
std::mutex                           g_logMutex;

// ── Packet Parser ─────────────────────────────────────────────
TelemetryPacket ParsePacket(const std::string& line)
{
    TelemetryPacket pkt;
    std::istringstream ss(line);
    char comma;
    if (!(ss >> pkt.planeID >> comma >> pkt.timestamp >> comma >> pkt.fuelRemaining))
        pkt.planeID = -1;
    return pkt;
}

// ── Flight Manager ────────────────────────────────────────────
void RegisterFlight(int planeID)
{
    std::lock_guard<std::mutex> lk(g_flightsMutex);
    FlightState fs;
    fs.planeID = planeID;
    g_flights[planeID] = fs;
}

void UpdateAndLog(int planeID, const TelemetryPacket& pkt)
{
    double avg = 0.0;
    {
        std::lock_guard<std::mutex> lk(g_flightsMutex);
        auto it = g_flights.find(planeID);
        if (it == g_flights.end()) return;

        FlightState& fs = it->second;
        if (fs.firstPacket) {
            fs.initialFuel = pkt.fuelRemaining;
            fs.firstPacket = false;
        }
        fs.currentFuel = pkt.fuelRemaining;
        fs.currentTimestamp = pkt.timestamp;
        avg = fs.GetAvgConsumption();
    }

    std::ostringstream msg;
    msg << std::fixed << std::setprecision(4)
        << "Plane " << planeID
        << " | Time:" << pkt.timestamp
        << " | Fuel:" << pkt.fuelRemaining
        << " | Avg Consumption:" << avg << " gal/s";

    {
        std::lock_guard<std::mutex> lk(g_logMutex);
        std::cout << msg.str() << "\n";
        if (g_logFile.is_open()) {
            g_logFile << planeID << "," << pkt.timestamp << ","
                << pkt.fuelRemaining << "," << avg << "\n";
            g_logFile.flush();
        }
    }
}

void FinalizeFlight(int planeID)
{
    double finalAvg = 0.0;
    {
        std::lock_guard<std::mutex> lk(g_flightsMutex);
        auto it = g_flights.find(planeID);
        if (it == g_flights.end()) return;
        finalAvg = it->second.GetAvgConsumption();
        g_flights.erase(it);
    }

    std::ostringstream msg;
    msg << std::fixed << std::setprecision(4)
        << "[FLIGHT COMPLETE] Plane " << planeID
        << " | Final Avg Consumption: " << finalAvg << " gal/s";

    {
        std::lock_guard<std::mutex> lk(g_logMutex);
        std::cout << msg.str() << "\n";
        if (g_logFile.is_open()) {
            g_logFile << planeID << ",FINAL,," << finalAvg << "\n";
            g_logFile.flush();
        }
    }
}

// ── Client Handler (one per thread) ──────────────────────────
void ClientHandler(SOCKET clientSocket)
{
    std::string buffer;
    char        chunk[512];
    int         planeID = -1;
    bool        registered = false;

    while (true)
    {
        int bytesReceived = recv(clientSocket, chunk, sizeof(chunk) - 1, 0);
        if (bytesReceived <= 0) break;

        chunk[bytesReceived] = '\0';
        buffer += chunk;

        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos)
        {
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);
            if (line.empty()) continue;

            TelemetryPacket pkt = ParsePacket(line);
            if (pkt.planeID == -1) continue;

            if (!registered)
            {
                planeID = pkt.planeID;
                RegisterFlight(planeID);
                registered = true;
                std::lock_guard<std::mutex> lk(g_logMutex);
                std::cout << "Client connected: Plane " << planeID << "\n";
            }
            UpdateAndLog(planeID, pkt);
        }
    }

    if (registered) FinalizeFlight(planeID);
    closesocket(clientSocket);
}

// ── main() ───────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    int port = DEFAULT_PORT;
    if (argc >= 2)
    {
        port = std::atoi(argv[1]);
        if (port <= 0 || port > 65535) port = DEFAULT_PORT;
    }

    g_logFile.open("flight_log.csv", std::ios::app);
    if (g_logFile.is_open())
        g_logFile << "PlaneID,Timestamp,FuelRemaining,AvgConsumption\n";

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET)
    {
        std::cerr << "socket() failed.\n";
        WSACleanup();
        return 1;
    }

    int opt = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR,
        reinterpret_cast<char*>(&opt), sizeof(opt));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(static_cast<u_short>(port));

    if (bind(listenSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR)
    {
        std::cerr << "bind() failed: " << WSAGetLastError() << "\n";
        closesocket(listenSocket); WSACleanup(); return 1;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        std::cerr << "listen() failed.\n";
        closesocket(listenSocket); WSACleanup(); return 1;
    }

    std::cout << "Server started\n";
    std::cout << "Waiting for clients... (port " << port << ")\n";

    while (true)
    {
        sockaddr_in clientAddr{};
        int clientAddrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(listenSocket,
            reinterpret_cast<sockaddr*>(&clientAddr),
            &clientAddrLen);
        if (clientSocket == INVALID_SOCKET)
        {
            std::cerr << "[WARN] accept() failed - continuing.\n";
            continue;
        }
        std::thread t(ClientHandler, clientSocket);
        t.detach();
    }

    closesocket(listenSocket);
    WSACleanup();
    return 0;
}