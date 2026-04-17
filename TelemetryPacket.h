/**
 * @file TelemetryPacket.h
 * @brief Shared data structures used by both the Client and the Server.
 *
 * This header defines the two structs that the FlightMonitor system runs on.
 * TelemetryPacket is what the client sends over TCP. FlightRecord is what
 * the server keeps internally to track each active flight.
 */

#pragma once

#pragma pack(1)

 /**
  * @brief Tracks everything the server knows about one plane's flight.
  *
  * One of these gets created for each plane that connects. The server updates
  * it as packets come in, keeping running totals so the average fuel consumption
  * can be calculated when the flight ends.
  */
struct FlightRecord {
    int    planeID;           ///< Unique ID of the plane this record belongs to.
    double prevFuel;          ///< Fuel level from the last packet, used to calculate how much was burned.
    double totalConsumption;  ///< Total fuel burned so far across all valid readings, in gallons.
    int    readingCount;      ///< Number of valid fuel consumption readings received so far.
    double avgConsumption;    ///< Running average fuel consumption, kept for reference during the flight.
    double finalAvg;          ///< Final average consumption saved when the plane lands.
    bool   isActive;          ///< True while the flight is ongoing, false once the client disconnects.
    char   lastTimestamp[32]; ///< Timestamp from the most recent packet received.
};

/**
 * @brief One packet of telemetry data sent from the Client to the Server over TCP.
 *
 * The client builds one of these for each line it reads from the CSV file
 * and sends it as raw bytes over the network. The server receives the same
 * struct on the other end and reads the fields directly.
 */
struct TelemetryPacket {
    int    planeID;        ///< ID of the plane sending this data.
    char   timestamp[32];  ///< Time this reading was recorded, as a string like "3_3_2023 14:53:21".
    double fuelRemaining;  ///< How much fuel is left in the tank at this moment, in gallons.
};

#pragma pack()