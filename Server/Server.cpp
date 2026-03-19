#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <mutex>
#include <map>
#pragma comment(lib, "ws2_32.lib")
#include "TelemetryPacket.h"

std::map<int, FlightRecord> flightLog;
std::mutex flightLogMutex;

// It receives exactly 'len' bytes and also handles partial reads
bool recvAll(SOCKET s, char* buf, int len) {
    int received = 0;
    while (received < len) {
        int n = recv(s, buf + received, len - received, 0);
        if (n <= 0) return false;
        received += n;
    }
    return true;
}

void handleClient(SOCKET clientSock) {
    TelemetryPacket pkt;
    bool firstReading = true;

    //Gets ther first packet to register the plane
    if (!recvAll(clientSock, (char*)&pkt, sizeof(pkt))) {
        closesocket(clientSock);
        return;
    }

    int id = pkt.planeID;

    // Creates flight record
    {
        std::lock_guard<std::mutex> lk(flightLogMutex);
        FlightRecord rec = {};
        rec.planeID = id;
        rec.isActive = true;
        // If plane already exists , it resets its stats for a new flight
        if (flightLog.count(id) > 0) {
            printf("Plane %d reconnected. Starting new flight record.\n", id);
        }
        flightLog[id] = rec;
    }

    printf("Plane %d connected.\n", id);

    // Will keep receiving packets until client disconnects
    while (recvAll(clientSock, (char*)&pkt, sizeof(pkt))) {
        std::lock_guard<std::mutex> lk(flightLogMutex);
        FlightRecord& r = flightLog[id];

        if (firstReading) {
            r.prevFuel = pkt.fuelRemaining;
            firstReading = false;
        }
        else {
            double cons = r.prevFuel - pkt.fuelRemaining;
            if (cons > 0) {
                r.totalConsumption += cons;
                r.readingCount++;
                r.avgConsumption = r.totalConsumption / r.readingCount;
            }
            r.prevFuel = pkt.fuelRemaining;
        }
    }
    printf("Plane %d | Fuel: %.2f | Avg: %.4f gal/s\n",
        id, pkt.fuelRemaining, r.avgConsumption);
    // Flight ended
    {
        std::lock_guard<std::mutex> lk(flightLogMutex);
        flightLog[id].finalAvg = flightLog[id].avgConsumption;
        flightLog[id].isActive = false;
    }

    printf("Plane %d landed. Avg fuel consumption: %.4f gal/s\n",
        id, flightLog[id].finalAvg);

    closesocket(clientSock);
}

int main() {
    // Initializing Winsock
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    //Creating socket
    SOCKET srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    //Binding it to port 5000
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5000);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(srv, (sockaddr*)&addr, sizeof(addr));

    // Listen on port 5000
    listen(srv, SOMAXCONN);
    printf("Server listening on port 5000...\n");

    // Accepts loop
    while (true) {
        SOCKET clientSock = accept(srv, nullptr, nullptr);
        if (clientSock == INVALID_SOCKET) continue;
        std::thread(handleClient, clientSock).detach();
    }

    WSACleanup();
    return 0;
}