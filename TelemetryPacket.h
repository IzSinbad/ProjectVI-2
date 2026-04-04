/**
 * @file TelemetryPacket.h
 * @brief Shared data structures used by both the Client and the Server.
 *
 * This header defines the two structs that make up the core of the
 * FlightMonitor system. TelemetryPacket is what the client sends over
 * the network. FlightRecord is what the server keeps internally to
 * track the state of each active flight.
 */

#pragma once

/**
 * @brief Tracks everything the server knows about one airplane's flight.
 *
 * The server creates one of these for each plane that connects and updates
 * it as packets arrive. It stores running totals so the average fuel
 * consumption can be calculated at any point during the flight.
 */
struct FlightRecord {
    int    planeID;           ///< The unique ID of the airplane this record belongs to.
    double prevFuel;          ///< The fuel level from the last packet, used to calculate the next delta.
    double totalConsumption;  ///< Running total of all fuel burned so far (in gallons).
    int    readingCount;      ///< How many valid consumption readings have been received.
    double avgConsumption;    ///< Current running average: totalConsumption divided by readingCount.
    double finalAvg;          ///< The average consumption saved when the flight ends.
    bool   isActive;          ///< True while the flight is in progress, false after the client disconnects.
    char   lastTimestamp[32]; ///< The timestamp string from the most recently received packet.
};

/**
 * @brief One unit of telemetry data sent from a Client to the Server over TCP.
 *
 * The client fills one of these structs for each line it reads from the CSV
 * file and sends the whole struct as raw bytes across the network. The server
 * receives the same struct on the other end and reads the fields directly.
 */
struct TelemetryPacket {
    int    planeID;        ///< Identifies which airplane is sending this data.
    char   timestamp[32];  ///< The time this reading was recorded, as a string (e.g. "3_3_2023 14:53:21").
    double fuelRemaining;  ///< How much fuel is left in the tank at this moment, in gallons.
};
