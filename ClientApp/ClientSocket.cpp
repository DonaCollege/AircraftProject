#include <winsock2.h>
#include <iostream>
#pragma comment(lib, "ws2_32.lib")
#include "../TelementryPacket.h"
#include <WS2tcpip.h>


class ClientSocket
{
private:
    SOCKET sock;

public:
    bool connectToServer(const std::string& ip, int port)
    {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);

        sock = socket(AF_INET, SOCK_STREAM, 0);

        sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_port = htons(port);

        InetPtonA(AF_INET, ip.c_str(), &server.sin_addr);
        if (connect(sock, (sockaddr*)&server, sizeof(server)) < 0)
        {
            std::cout << "Connection failed\n";
            return false;
        }

        return true;
    }

    void sendPacket(const TelemetryPacket& packet)
    {
        send(sock, (char*)&packet, sizeof(packet), 0);
    }

    void closeConnection()
    {
        closesocket(sock);
        WSACleanup();
    }
};