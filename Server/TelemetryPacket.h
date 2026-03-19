#pragma once
// TelemetryPacket.h
#pragma once
struct FlightRecord {
    int    planeID;
    // Last fuel reading in gallons
    double prevFuel;           
    // Sum of all consumption 
    double totalConsumption;   
    // Number of valid readings received
    int    readingCount;       
    // Running average = totalConsumption / readingCount
    double avgConsumption;     
    // Stored when the flight ends 
    double finalAvg;           
    //This will stay true while flight is in progress
    bool   isActive;           
};

struct TelemetryPacket {
    int    planeID;            
    char   timestamp[32];      
    double fuelRemaining;      
};