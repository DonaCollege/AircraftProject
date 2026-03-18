#include <iostream>
#include "TelemetryReader.h"
#include "../TelementryPacket.h"
#include "ClientSocket.cpp"

// forward declarations
TelemetryPacket buildPacket(int id, double time, double fuel);

int main()
{
    int planeID = rand() % 10000; // simple unique ID

    TelemetryReader reader;

    if (!reader.open("Telem_2023_3_12 14_56_40.txt"))
    {
        std::cout << "Failed to open file\n";
        return 1;
    }

    ClientSocket socket;

    if (!socket.connectToServer("127.0.0.1", 8080))
        return 1;

    double time, fuel;

    while (reader.readNext(time, fuel))
    {
        TelemetryPacket packet = buildPacket(planeID, time, fuel);

        socket.sendPacket(packet);

        std::cout << "Sent: " << time << " " << fuel << "\n";
    }

    socket.closeConnection();

    std::cout << "Flight complete\n";

    return 0;
}