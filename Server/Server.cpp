/**
 * @file Server.cpp
 * @brief TCP server that receives flight telemetry from multiple airplane clients
 *        and tracks fuel consumption for each active flight in real time.
 *
 * The server listens on port 5000 and spawns a new thread for each client
 * that connects. Each client represents one airplane sending telemetry data.
 * The server keeps a shared log of all flights and updates it as packets arrive.
 */

#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <mutex>
#include <map>
#pragma comment(lib, "ws2_32.lib")
#include "TelemetryPacket.h"

/** @brief Stores flight records for every plane that has connected, keyed by plane ID. */
std::map<int, FlightRecord> flightLog;

/** @brief Protects flightLog from being read and written at the same time by multiple threads. */
std::mutex flightLogMutex;

/**
 * @brief Receives exactly the requested number of bytes from a socket,
 *        handling the case where TCP delivers data in multiple smaller chunks.
 *
 * Normally a single recv() call might only give you part of what you asked for.
 * This function keeps calling recv() in a loop until all the bytes arrive,
 * so the caller always gets a complete packet.
 *
 * @param s   The socket to read from.
 * @param buf The buffer to store the received bytes in.
 * @param len The exact number of bytes we want to receive.
 * @return true  if all bytes were received successfully.
 * @return false if the connection was closed or an error occurred.
 */
bool recvAll(SOCKET s, char* buf, int len) {
    int received = 0;

    /* Keep looping until we have collected every byte we need */
    while (received < len) {
        int n = recv(s, buf + received, len - received, 0);

        /* If recv returns 0 or negative, the client disconnected or something went wrong */
        if (n <= 0) return false;
        received += n;
    }
    return true;
}

/**
 * @brief Handles all communication with a single connected airplane client.
 *
 * This function runs on its own thread for each client. It receives the first
 * packet to register the plane, then keeps receiving packets until the client
 * disconnects. After each packet it updates the plane's fuel consumption stats.
 * When the client disconnects the flight is marked as finished and the final
 * average fuel consumption is saved and printed.
 *
 * @param clientSock The socket connected to this specific airplane client.
 */
void handleClient(SOCKET clientSock) {
    TelemetryPacket pkt;
    bool firstReading = true;

    /* Read the very first packet so we know which plane just connected */
    if (!recvAll(clientSock, (char*)&pkt, sizeof(pkt))) {
        closesocket(clientSock);
        return;
    }

    int id = pkt.planeID;

    /* Create a new flight record for this plane, resetting stats if it reconnects */
    {
        std::lock_guard<std::mutex> lk(flightLogMutex);
        FlightRecord rec = {};
        rec.planeID = id;
        rec.isActive = true;

        /* If this plane ID already exists in the log, let the user know it is starting fresh */
        if (flightLog.count(id) > 0) {
            printf("Plane %d reconnected. Starting new flight record.\n", id);
        }
        flightLog[id] = rec;
    }

    printf("Plane %d connected.\n", id);

    /* Keep receiving packets until the client closes the connection */
    while (recvAll(clientSock, (char*)&pkt, sizeof(pkt))) {
        std::lock_guard<std::mutex> lk(flightLogMutex);
        FlightRecord& r = flightLog[id];

        /* Save the most recent timestamp from this packet so we can display it at landing */
        strncpy(r.lastTimestamp, pkt.timestamp, 31);
        r.lastTimestamp[31] = '\0';

        if (firstReading) {
            /* Store the starting fuel level — we can't calculate consumption yet
               because we need at least two readings to find the difference */
            r.prevFuel = pkt.fuelRemaining;
            firstReading = false;
        }
        else {
            /* Calculate how much fuel was burned since the last reading */
            double cons = r.prevFuel - pkt.fuelRemaining;

            /* Only count positive values — negative would mean a refuel or a bad reading */
            if (cons > 0) {
                r.totalConsumption += cons;
                r.readingCount++;
                r.avgConsumption = r.totalConsumption / r.readingCount;
            }
            r.prevFuel = pkt.fuelRemaining;
        }
    }

    /* recvAll returned false, meaning the client disconnected — flight is over */
    printf("Plane %d | Fuel: %.2f | Avg: %.4f gal/s\n",
        id, pkt.fuelRemaining, r.avgConsumption);

    /* Lock the log and store the final average, then mark the flight as inactive */
    {
        std::lock_guard<std::mutex> lk(flightLogMutex);
        flightLog[id].finalAvg = flightLog[id].avgConsumption;
        flightLog[id].isActive = false;
    }

    printf("Plane %d landed. Last timestamp: %s. Avg fuel consumption: %.4f gal/s\n",
        id, flightLog[id].lastTimestamp, flightLog[id].finalAvg);

    closesocket(clientSock);
}

/**
 * @brief Entry point for the server application.
 *
 * Sets up Winsock, creates a TCP socket, binds it to port 5000, and enters
 * an infinite loop accepting incoming client connections. Each new connection
 * gets its own detached thread running handleClient(), so multiple planes can
 * be tracked at the same time without blocking each other.
 *
 * @return 0 when the server exits (this only happens if it is killed manually).
 */
int main() {
    /* Start up the Windows networking library (required on Windows before using sockets) */
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    /* Create a standard TCP socket */
    SOCKET srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    /* Configure the server address — listen on all network interfaces, port 5000 */
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5000);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(srv, (sockaddr*)&addr, sizeof(addr));

    /* Start listening for incoming connections */
    listen(srv, SOMAXCONN);
    printf("Server listening on port 5000...\n");

    /* Accept connections forever — each one spawns a thread and immediately detaches it
       so the main loop can go back to waiting for the next plane right away */
    while (true) {
        SOCKET clientSock = accept(srv, nullptr, nullptr);
        if (clientSock == INVALID_SOCKET) continue;
        std::thread(handleClient, clientSock).detach();
    }

    WSACleanup();
    return 0;
}
