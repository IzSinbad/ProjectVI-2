#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#include "TelemetryPacket.h"

const char* FILES[4] = {
    "katl-kefd-B737-700.txt",
    "Telem_2023_3_12_14_56_40.txt",
    "Telem_2023_3_12_16_26_4.txt",
    "Telem_czba-cykf-pa28-w2_2023_3_1_12_31_27.txt"
};

int main(int argc, char* argv[]) {

    // Gets the server IP from command line
    const char* serverIP = (argc > 1) ? argv[1] : "127.0.0.1";

    // Gets the Unique ID from process ID
    int uniqueID = (int)GetCurrentProcessId();

    // Picks a random data file
    srand((unsigned)time(NULL) ^ (unsigned)uniqueID);
    const char* fname = FILES[rand() % 4];

    // Initialize Winsock
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    // Creates and connects to socket
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in sa = {};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(5000);
    inet_pton(AF_INET, serverIP, &sa.sin_addr);

    if (connect(s, (sockaddr*)&sa, sizeof(sa)) != 0) {
        printf("Connect failed to %s\n", serverIP);
        WSACleanup();
        return 1;
    }

    printf("Plane %d connected to %s\n", uniqueID, serverIP);

    // Opens the data file
    FILE* f = fopen(fname, "r");
    if (!f) {
        printf("Could not open file: %s\n", fname);
        closesocket(s);
        WSACleanup();
        return 1;
    }

    // Skips the header line
    char line[128];
    fgets(line, sizeof(line), f);

    // Reads and then send loop
    while (fgets(line, sizeof(line), f)) {
        char* comma = strchr(line, ',');
        if (!comma) continue;

        TelemetryPacket pkt = {};
        pkt.planeID = uniqueID;

        // Parse timestamp 
        int tsLen = (int)(comma - line);
        if (tsLen >= 32) tsLen = 31;
        strncpy(pkt.timestamp, line, tsLen);
        pkt.timestamp[tsLen] = '\0';

        // Parse fuel value 
        pkt.fuelRemaining = atof(comma + 1);

        send(s, (char*)&pkt, sizeof(pkt), 0);
        Sleep(1000); 
    }

    // After reaching end of file,  flight complete
    printf("Plane %d flight complete.\n", uniqueID);
    fclose(f);
    closesocket(s);
    WSACleanup();
    return 0;
}