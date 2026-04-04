/**
 * @file Client.cpp
 * @brief TCP client that reads flight telemetry from a CSV file and streams
 *        it to the FlightMonitor server one packet per second.
 *
 * Each time this program runs it picks a random CSV data file, connects to
 * the server over TCP, and sends one TelemetryPacket every second until the
 * file runs out of data. The plane's unique ID is taken from the Windows
 * process ID, so running multiple copies at the same time gives each one
 * a different ID automatically.
 */

#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#include "../TelemetryPacket.h"

/**
 * @brief The four CSV telemetry files the client can choose from.
 *
 * One of these is picked at random each time the program starts.
 * Each file contains a different flight's worth of fuel readings.
 */
const char* FILES[4] = {
    "katl-kefd-B737-700.txt",
    "Telem_2023_3_12_14_56_40.txt",
    "Telem_2023_3_12_16_26_4.txt",
    "Telem_czba-cykf-pa28-w2_2023_3_1_12_31_27.txt"
};

/**
 * @brief Entry point for the client application.
 *
 * Connects to the FlightMonitor server, reads a CSV telemetry file line by
 * line, and sends each line as a TelemetryPacket over TCP. There is a one
 * second pause between packets to simulate real-time data coming in during
 * an actual flight. The program ends once it reaches the last line of the file.
 *
 * @param argc Number of command line arguments.
 * @param argv Command line arguments. argv[1] is the optional server IP address
 *             (defaults to "127.0.0.1" if not provided).
 * @return 0 on success, 1 if the connection or file could not be opened.
 */
int main(int argc, char* argv[]) {

    /* Use the IP address from the command line, or fall back to localhost */
    const char* serverIP = (argc > 1) ? argv[1] : "127.0.0.1";

    /* Use the Windows process ID as this plane's unique ID so that running
       multiple clients at the same time gives each one a different number */
    int uniqueID = (int)GetCurrentProcessId();

    /* Seed the random number generator with the current time XOR the process ID
       so that two clients started at the same second still pick different files */
    srand((unsigned)time(NULL) ^ (unsigned)uniqueID);
    const char* fname = FILES[rand() % 4];

    /* Start up the Windows networking library (required before using sockets on Windows) */
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    /* Create a TCP socket and set up the server address we want to connect to */
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in sa = {};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(5000);
    inet_pton(AF_INET, serverIP, &sa.sin_addr);

    /* Try to connect — if it fails, print an error and exit cleanly */
    if (connect(s, (sockaddr*)&sa, sizeof(sa)) != 0) {
        printf("Connect failed to %s\n", serverIP);
        WSACleanup();
        return 1;
    }

    printf("Plane %d connected to %s\n", uniqueID, serverIP);

    /* Open the randomly selected CSV data file for reading */
    FILE* f = fopen(fname, "r");
    if (!f) {
        printf("Could not open file: %s\n", fname);
        closesocket(s);
        WSACleanup();
        return 1;
    }

    /* Skip the first line because it is a header row, not actual data */
    char line[128];
    fgets(line, sizeof(line), f);

    /* Read one data line at a time and send it as a packet to the server */
    while (fgets(line, sizeof(line), f)) {

        /* Each line is formatted as "timestamp,fuelValue" — find the comma that separates them */
        char* comma = strchr(line, ',');
        if (!comma) continue;

        /* Build a new packet and fill in the plane ID */
        TelemetryPacket pkt = {};
        pkt.planeID = uniqueID;

        /* Copy the timestamp — it is everything before the comma */
        int tsLen = (int)(comma - line);
        if (tsLen >= 32) tsLen = 31;
        strncpy(pkt.timestamp, line, tsLen);
        pkt.timestamp[tsLen] = '\0';

        /* Parse the fuel value — it is the number immediately after the comma */
        pkt.fuelRemaining = atof(comma + 1);

        /* Send the completed packet to the server */
        send(s, (char*)&pkt, sizeof(pkt), 0);

        /* Wait one second before sending the next packet to simulate real-time telemetry */
        Sleep(1000);
    }

    /* All lines have been sent — the flight is complete */
    printf("Plane %d flight complete.\n", uniqueID);
    fclose(f);
    closesocket(s);
    WSACleanup();
    return 0;
}
