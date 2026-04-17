/**
 * @file Client.cpp
 * @brief TCP client that reads flight telemetry from a file and streams it to the server.
 *
 * Each time this runs it connects to the FlightMonitor server over TCP and sends
 * one TelemetryPacket every 100ms until the file runs out of data. The plane's
 * unique ID comes from the Windows process ID, so running multiple copies at the
 * same time gives each one a different ID automatically.
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
  * @brief The four telemetry files the client can choose from.
  *
  * Each file contains a different flight's worth of fuel readings.
  * Currently locked to FILES[0] for consistent performance testing.
  */
const char* FILES[4] = {
    "katl-kefd-B737-700.txt",
    "Telem_2023_3_12_14_56_40.txt",
    "Telem_2023_3_12_16_26_4.txt",
    "Telem_czba-cykf-pa28-w2_2023_3_1_12_31_27.txt"
};

/**
 * @brief Entry point for the client.
 *
 * Connects to the FlightMonitor server, opens the telemetry file, and sends
 * one packet per line until EOF. There is a 100ms pause between packets to
 * simulate real-time data coming in during a flight.
 *
 * @param argc Number of command line arguments.
 * @param argv argv[1] is the server IP address. Defaults to 127.0.0.1 if not provided.
 * @return 0 on success, 1 if the connection or file failed to open.
 */
int main(int argc, char* argv[]) {

    /* Use the IP from the command line, or fall back to localhost */
    const char* serverIP = (argc > 1) ? argv[1] : "127.0.0.1";

    /* Use the process ID as the plane's unique ID */
    int uniqueID = (int)GetCurrentProcessId();

    /* Always use katl-kefd-B737-700.txt for consistent testing */
    const char* fname = FILES[0];

    /* Start up Winsock before using any sockets */
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    /* Create a TCP socket and configure the server address */
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in sa = {};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(5000);
    inet_pton(AF_INET, serverIP, &sa.sin_addr);

    /* Try to connect, exit cleanly if it fails */
    if (connect(s, (sockaddr*)&sa, sizeof(sa)) != 0) {
        printf("Connect failed to %s\n", serverIP);
        WSACleanup();
        return 1;
    }

    printf("Plane %d connected to %s\n", uniqueID, serverIP);

    /* Open the telemetry file */
    FILE* f = fopen(fname, "r");
    if (!f) {
        printf("Plane %d: Could not open file: %s\n", uniqueID, fname);
        closesocket(s);
        WSACleanup();
        return 1;
    }
    printf("Plane %d: File opened: %s\n", uniqueID, fname);
    fflush(stdout);

    /* Skip the header row */
    char line[128];
    fgets(line, sizeof(line), f);

    /* Read one line at a time and send it as a packet */
    while (fgets(line, sizeof(line), f)) {

        /* Each line is "timestamp,fuelValue" so find the comma */
        char* comma = strchr(line, ',');
        if (!comma) continue;

        /* Build the packet */
        TelemetryPacket pkt = {};
        pkt.planeID = uniqueID;

        /* Everything before the comma is the timestamp */
        int tsLen = (int)(comma - line);
        if (tsLen >= 32) tsLen = 31;
        strncpy(pkt.timestamp, line, tsLen);
        pkt.timestamp[tsLen] = '\0';

        /* The number after the comma is the fuel value */
        pkt.fuelRemaining = atof(comma + 1);

        /* Send the packet to the server */
        int sent = send(s, (char*)&pkt, sizeof(pkt), 0);
        if (sent < 0) {
            printf("Plane %d: Send failed!\n", uniqueID);
            fflush(stdout);
            break;
        }

        /* Wait 100ms before sending the next packet */
        Sleep(100);
    }

    /* All lines sent, flight is done */
    printf("Plane %d: EOF reached. Flight complete.\n", uniqueID);
    fflush(stdout);
    fclose(f);
    closesocket(s);
    WSACleanup();
    return 0;
}