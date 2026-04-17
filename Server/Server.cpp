/**
 * @file Server.cpp
 * @brief TCP server that receives flight telemetry from multiple clients and
 *        tracks fuel consumption for each active flight in real time.
 *
 * The server listens on port 5000 and spawns a new thread for each client
 * that connects. Each client represents one plane sending telemetry data.
 * The server keeps a shared log of all flights and updates it as packets arrive.
 *
 * Optimizations applied after initial performance testing:
 * 1. std::map replaced with std::unordered_map for O(1) average lookups
 * 2. Global mutex replaced with per-plane mutex to stop threads blocking each other
 * 3. Average fuel consumption now only calculated once at landing, not every packet
 */

#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <mutex>
#include <unordered_map>
#pragma comment(lib, "ws2_32.lib")
#include "../TelemetryPacket.h"

 /**
  * @brief Wraps a FlightRecord with its own mutex for per-plane locking.
  *
  * Before optimization, one global mutex blocked all threads on every packet.
  * Now each plane has its own mutex so threads handling different planes
  * never block each other.
  */
struct FlightRecordWithMutex {
    FlightRecord data;
    std::mutex planeMutex;
};

/**
 * @brief Stores flight records for every plane that has connected, keyed by plane ID.
 *
 * Changed from std::map (O(log N) red-black tree) to std::unordered_map
 * (O(1) hash table). Before this change, std::map operations consumed 99.77%
 * of CPU time under load. After this change that dropped to 2.43%.
 */
std::unordered_map<int, FlightRecordWithMutex> flightLog;

/**
 * @brief Protects flightLog insertions only.
 *
 * Only locked when a new plane first connects. All packet processing
 * uses the individual plane's mutex instead.
 */
std::mutex flightLogMutex;

/**
 * @brief Reads exactly the requested number of bytes from a socket.
 *
 * TCP can deliver data in smaller chunks than requested, so this function
 * keeps calling recv() until all the bytes arrive. Returns false if the
 * connection closed or something went wrong.
 *
 * @param s   Socket to read from.
 * @param buf Buffer to store the received bytes.
 * @param len Exact number of bytes to receive.
 * @return true if all bytes received, false if connection closed or error.
 */
bool recvAll(SOCKET s, char* buf, int len) {
    int received = 0;

    /* Keep looping until we have all the bytes we need */
    while (received < len) {
        int n = recv(s, buf + received, len - received, 0);

        /* 0 or negative means the client disconnected or there was an error */
        if (n <= 0) return false;
        received += n;
    }
    return true;
}

/**
 * @brief Handles all communication with a single connected plane.
 *
 * Runs on its own thread for each client. Reads the first packet to register
 * the plane, then keeps receiving packets until the client disconnects or
 * times out. When the flight ends the final average fuel consumption is
 * calculated once and saved.
 *
 * A 10 second socket timeout is set so threads clean up properly if a client
 * crashes or loses connection without sending a disconnect.
 *
 * @param clientSock Socket connected to this specific plane.
 */
void handleClient(SOCKET clientSock) {

    DWORD timeout = 10000; // 10 seconds
    setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    TelemetryPacket pkt;
    bool firstReading = true;

    /* Read the first packet to find out which plane just connected */
    if (!recvAll(clientSock, (char*)&pkt, sizeof(pkt))) {
        closesocket(clientSock);
        return;
    }

    int id = pkt.planeID;

    /* Register the plane in the flight log, global lock only needed here */
    {
        std::lock_guard<std::mutex> lk(flightLogMutex);
        FlightRecord rec = {};
        rec.planeID = id;
        rec.isActive = true;

        strncpy(rec.lastTimestamp, pkt.timestamp, 31);
        rec.lastTimestamp[31] = '\0';

        rec.prevFuel = pkt.fuelRemaining;

        if (flightLog.count(id) > 0) {
            printf("Plane %d reconnected. Starting new flight record.\n", id);
        }
        flightLog[id].data = rec;
    }

    printf("Plane %d connected.\n", id);

    /* Keep receiving packets until the client disconnects or times out.
       Only locks this plane's own mutex so other planes are never affected. */
    while (recvAll(clientSock, (char*)&pkt, sizeof(pkt))) {
        std::lock_guard<std::mutex> lk(flightLog[id].planeMutex);
        FlightRecord& r = flightLog[id].data;

        /* Save the latest timestamp */
        strncpy(r.lastTimestamp, pkt.timestamp, 31);
        r.lastTimestamp[31] = '\0';

        if (firstReading) {
            r.prevFuel = pkt.fuelRemaining;
            firstReading = false;
        }
        else {
            /* Calculate fuel burned since last reading */
            double cons = r.prevFuel - pkt.fuelRemaining;

            /* Skip negative values, those mean a refuel or bad data */
            if (cons > 0) {
                r.totalConsumption += cons;
                r.readingCount++;
                /* Average is NOT calculated here anymore.
                   It was running on every single packet but only needed at landing.
                   Now calculated once below when the flight ends. */
            }
            r.prevFuel = pkt.fuelRemaining;
        }
    }

    /* Client disconnected or timed out, flight is over.
       Calculate the final average exactly once here. */
    {
        std::lock_guard<std::mutex> lk(flightLog[id].planeMutex);
        FlightRecord& r = flightLog[id].data;

        if (r.readingCount > 0) {
            r.finalAvg = r.totalConsumption / r.readingCount;
        }
        r.isActive = false;

        printf("Plane %d | Fuel: %.2f | Avg: %.4f gal/s\n",
            id, pkt.fuelRemaining, r.finalAvg);

        printf("Plane %d landed. Last timestamp: %s. Avg fuel consumption: %.4f gal/s\n",
            id, r.lastTimestamp, r.finalAvg);
    }

    closesocket(clientSock);
}

/**
 * @brief Entry point for the server.
 *
 * Sets up Winsock, binds to port 5000, and loops forever accepting connections.
 * Each new connection gets its own detached thread running handleClient().
 *
 * @return 0 when the server exits (only happens if killed manually).
 */
int main() {
    /* Start up Winsock */
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    /* Create a TCP socket */
    SOCKET srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    /* Listen on all network interfaces, port 5000 */
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5000);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(srv, (sockaddr*)&addr, sizeof(addr));

    listen(srv, SOMAXCONN);
    printf("Server listening on port 5000...\n");

    /* Accept connections forever, each one gets its own detached thread */
    while (true) {
        SOCKET clientSock = accept(srv, nullptr, nullptr);
        if (clientSock == INVALID_SOCKET) continue;
        std::thread(handleClient, clientSock).detach();
    }

    WSACleanup();
    return 0;
}