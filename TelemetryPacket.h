// TelemetryPacket.h
#pragma once
struct FlightRecord {
    int    planeID;
    // Last fuel reading in gallons
    double prevFuel;
    // Sum of all consumption values
    double totalConsumption;
    // Number of valid readings received
    int    readingCount;
    // Running average = totalConsumption / readingCount
    double avgConsumption;
    // Stored when the flight ends
    double finalAvg;
    // This will remain true while the flight is in progress
    bool   isActive;
    // Most recent timestamp received from this plane
    char   lastTimestamp[32];
};

struct TelemetryPacket {
    int    planeID;            
    char   timestamp[32];      
    double fuelRemaining;  
};
