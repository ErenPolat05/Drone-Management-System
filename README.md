# 🚁 Drone-Management-System

![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)
![Python](https://img.shields.io/badge/Python-3.10%2B-blue?style=for-the-badge&logo=python)
![PySide6](https://img.shields.io/badge/PySide6-Dynamic_UI-41CD52?style=for-the-badge&logo=qt&logoColor=white)
![MSSQL](https://img.shields.io/badge/MSSQL-Database-CC292B?style=for-the-badge&logo=microsoft-sql-server&logoColor=white)
![Security](https://img.shields.io/badge/Security-AES--128--GCM-red?style=for-the-badge)
![Network](https://img.shields.io/badge/Socket-UDP-FF6C37?style=for-the-badge)

**End-to-End Encrypted UAV Telemetry & Tactical Ground Control Station**

A high-performance, real-time drone management architecture integrating C-based embedded datalinks and a Python/PySide6 ground control interface. Engineered with industry-standard **AES-128-GCM** payload encryption, low-latency **UDP socket** communication, and a dynamically structured **MSSQL** relational database to ensure secure, data-driven UI updates.



## 📋 Executive Summary

**The Architecture**

The Drone-Management-System is an end-to-end UAV telemetry and tactical Ground Control Station (GCS) architecture designed to securely transport, process, retain, and visualize flight data. The system acquires onboard sensor data, serializes the telemetry into a defined binary packet structure, and protects the payload using **AES-128-GCM** authenticated encryption. The resulting frames are transmitted over a low-latency **UDP** datalink to a custom Python-based Ground Control Station.

On the ground side, the GCS receives, decrypts, and decodes incoming telemetry in real time. Processed data is continuously delivered to the live **PySide6** interface while also being preserved for persistent storage in **MSSQL**. To reduce telemetry loss during communication or processing failures, the system incorporates **ring-buffer-based buffering and disk flushing mechanisms**. This allows unsent or temporarily unavailable telemetry data to be retained and written to disk instead of being immediately discarded.

The architecture therefore covers the complete telemetry lifecycle: **onboard data acquisition → binary packetization → authenticated encryption → UDP transmission → reception and decoding → real-time visualization → buffered data retention → persistent database storage**.

**The Problem It Solves**

UAV telemetry systems must handle several challenges simultaneously: **secure communication, continuous real-time processing, and reliable data retention**. Unprotected telemetry can be intercepted or modified during transmission, while UDP-based communication can result in packet loss or temporary connectivity failures. Without an appropriate buffering and persistence strategy, telemetry that cannot be delivered immediately may be permanently lost.

This project addresses these challenges by combining **AES-128-GCM authenticated encryption**, **UDP-based telemetry transport**, structured binary packet processing, and a dedicated buffering and persistence architecture. A **ring buffer** provides temporary in-memory retention for telemetry that cannot be immediately forwarded, while **flush mechanisms** move retained data to persistent storage when required. This approach helps protect telemetry against transient communication failures while keeping the live data path focused on the newest incoming flight data.

The result is a system designed to maintain the **confidentiality and integrity of telemetry**, minimize data loss during temporary failures, continuously deliver live flight data to the Ground Control Station, and preserve telemetry for later analysis through persistent storage.


## 🏗️ System Architecture & Data Flow

The system is divided into two primary operational nodes: the **embedded Flight Computer** and the **Python-based Ground Control Station (GCS)**. This separation isolates real-time telemetry acquisition and transmission from ground-side processing, persistence, and graphical visualization.

### 📡 Node 1: Flight Computer (Embedded C)

The onboard flight computer acts as the primary data acquisition and telemetry transmission component.

* **Hardware-Aligned Data Acquisition:** Raw sensor values are validated against defined physical limits before entering the telemetry pipeline. Validated values are assembled into a fixed **40-byte `DroneData` structure** with an explicitly defined field layout and reserved bytes for the intended binary representation.

* **State & Fault Management:** System health information is compacted into a 16-bit `status_code` using bitwise flags. This allows multiple system states or fault conditions to be represented within a single field while minimizing packet overhead.

* **Security & Data Retention:** Telemetry payloads are protected using **AES-128-GCM authenticated encryption** before transmission. The embedded telemetry pipeline also provides a persistence path for data that cannot be immediately delivered, allowing unsent or temporarily unavailable records to be retained rather than discarded.

* **Transmission Pipeline:** `main.c` coordinates telemetry generation, packet construction, encryption, and transmission. Encrypted telemetry is combined with a `TelemetryHeader` and passed through the custom **Ring Buffer** before being transmitted over UDP. The telemetry stream operates at approximately **50 Hz**, while a lightweight heartbeat mechanism provides an independent indication of system and connection status.

<details>
<summary><b>View DroneData Struct Blueprint</b></summary>

```c
typedef struct {
    /* 4-Byte Aligned Fields */
    uint32_t timestamp;         // System timestamp (ms)
    float latitude;             // GPS Latitude coordinate
    float longitude;            // GPS Longitude coordinate
    float altitude;             // GPS Altitude (m)
    float pitch;                // IMU Pitch angle
    float roll;                 // IMU Roll angle
    float yaw;                  // IMU Yaw angle

    /* 2-Byte Aligned Fields */
    uint16_t drone_id;          // Unique UAV identifier
    int16_t motor_temp;         // Motor temperature (Scaled x10)
    int16_t battery_temp;       // Battery temperature (Scaled x10)
    uint16_t status_code;       // Bitwise status/error flags

    /* 1-Byte Aligned Fields & Padding */
    uint8_t battery_percent;    // Battery percentage (0-100)
    uint8_t reserved[3];        // Explicit trailing bytes
} DroneData;
```

</details>

### 🖥️ Node 2: Ground Control Station (Python)

The GCS is responsible for telemetry reception, authentication, decryption, validation, persistence, and real-time visualization.

* **Authentication & Access Control:** The application uses credential-based authentication before granting access to the Ground Control Station interface. Passwords are handled using secure password hashing rather than being stored in plaintext.

* **Packet Validation & Filtering:** The UDP receiver validates incoming frames before passing them to the main processing pipeline. Packet structure, expected fields, and telemetry consistency are checked to prevent malformed or invalid data from propagating through the application.

* **Demultiplexing & Decryption:** The receiver parses the `TelemetryHeader` to distinguish telemetry and heartbeat traffic. Authenticated telemetry payloads are decrypted and decoded, while the bitwise `status_code` is interpreted by a dedicated status parser.

* **Buffered Database Persistence:** To prevent high-frequency telemetry from directly blocking the graphical interface or database operations, processed records are temporarily buffered before being written to **MSSQL**. Database operations are performed in controlled batches with explicit result and error checks.

* **Dynamic Visualization:** Validated telemetry is propagated to the **PySide6** interface through the application's processing and worker architecture. The dashboard updates operational components such as the **Artificial Horizon**, tactical map, telemetry tables, battery indicators, connection status, and event logs without blocking the main Qt event loop.

### 🔄 End-to-End Data Flow

```text
┌───────────────────────┐
│   Flight Computer     │
│      Embedded C       │
└──────────┬────────────┘
           │
           ▼
┌───────────────────────┐
│ Sensor Validation     │
│ + DroneData (40 B)    │
└──────────┬────────────┘
           │
           ▼
┌───────────────────────┐
│ Binary Packetization  │
│ + TelemetryHeader     │
└──────────┬────────────┘
           │
           ▼
┌───────────────────────┐
│ AES-128-GCM           │
│ Authenticated Encrypt.│
└──────────┬────────────┘
           │
           ▼
┌───────────────────────┐
│ Ring Buffer / UDP     │
│ ~50 Hz Telemetry      │
└──────────┬────────────┘
           │
           │ UDP
           ▼
┌───────────────────────┐
│ Ground Station        │
│ Packet Reception      │
└──────────┬────────────┘
           │
           ▼
┌───────────────────────┐
│ Validation / Decrypt  │
│ / Decode              │
└───────┬─────────┬─────┘
        │         │
        ▼         ▼
┌────────────┐ ┌───────────────┐
│ PySide6 UI │ │ MSSQL Buffer  │
│ Real-Time  │ │ + Persistence │
└────────────┘ └───────────────┘
```

## ✨ Core Features

* **Asynchronous Telemetry Processing:** A dedicated `UDPWorker` thread handles incoming telemetry without blocking the main PySide6 event loop, keeping the graphical interface responsive during continuous high-frequency data reception.

* **Custom Real-Time Visualization:** Custom `QPainter`-based widgets provide specialized rendering for the **Artificial Horizon** and **Tactical Map**, allowing the interface to display flight attitude and positional telemetry through purpose-built visual components.

* **Authenticated Telemetry Security:** Telemetry payloads are protected using **AES-128-GCM authenticated encryption**, providing confidentiality and cryptographic integrity for the UAV-to-GCS data path. Ground Control Station credentials are protected using **bcrypt password hashing**.

* **Telemetry Buffering & Data Retention:** A custom **Ring Buffer** provides temporary in-memory retention for telemetry that cannot be immediately forwarded. Flush and persistence mechanisms help prevent temporarily undeliverable telemetry from being discarded.

* **Buffered Database Persistence:** Telemetry records are accumulated in memory and written to **MSSQL** in controlled batches, reducing the frequency of database operations and preventing high-rate telemetry processing from directly blocking the user interface.

* **Fault-Aware Telemetry Flow:** The architecture separates live telemetry delivery from retained telemetry data, allowing the system to prioritize the newest incoming flight data while preserving older unsent records for later persistence or processing.

---

## 💻 Tech Stack & Prerequisites

### Embedded Flight Computer — Node 1

| Component               | Technology                         |
| ----------------------- | ---------------------------------- |
| Language                | C99                                |
| Compiler                | GCC                                |
| Development Environment | Visual Studio Code                 |
| Network                 | UDP / platform-specific socket API |
| Cryptography            | **Mbed TLS — AES-128-GCM**         |
| Data Structures         | Custom Ring Buffer                 |
| Target Platform         | Windows development environment    |

The embedded component primarily relies on the **C standard library**, platform-specific networking APIs, and **Mbed TLS** for authenticated AES-128-GCM encryption.

Standard headers such as `stdint.h`, `string.h`, and `signal.h` are part of the C standard environment and do not require separate installation.

**Mbed TLS** must be available in the development environment when compiling the embedded component.

### Ground Control Station — Node 2

| Component             | Technology           |
| --------------------- | -------------------- |
| Language              | Python 3.10.10       |
| GUI Framework         | PySide6              |
| Database              | Microsoft SQL Server |
| Database Connectivity | pyodbc               |
| Authentication        | bcrypt               |
| Cryptography          | cryptography         |
| UI Icons              | qtawesome            |
| Network               | UDP sockets          |

## 🛠️ Installation & Build Instructions

### 1. Python Environment Setup

The Ground Control Station is developed with **Python 3.10+**. A virtual environment is recommended to isolate project dependencies.

Create the virtual environment:

```bash
python -m venv venv
```

Activate it on Windows:

```bash
venv\Scripts\activate
```

Install the required Python packages:

```bash
pip install -r requirements.txt
```

If a `requirements.txt` file is not provided, install the dependencies manually:

```bash
pip install PySide6 qtawesome pyodbc bcrypt cryptography
```

The Ground Control Station also requires a compatible **Microsoft ODBC Driver for SQL Server** installation.

### 2. Database Configuration

The Ground Control Station uses **Microsoft SQL Server** for user authentication and telemetry persistence.

Before running the application:

1. Ensure that the SQL Server instance is running.
2. Create and initialize the `UAV_GroundStation` database.
3. Apply the project's database schema.
4. Verify that the configured SQL Server instance is accessible through `pyodbc`.
5. Ensure that the application has the required database permissions.

Database connection settings should be configured according to the local SQL Server environment.

### 3. Build the Flight Computer

The embedded component is implemented in **C99** and compiled using **GCC**. The project uses **Mbed TLS** for AES-128-GCM authenticated encryption.

The embedded component is built using the project's `Makefile`:

```bash
cd Flight-Computer
make
```

If the project's Makefile provides a clean target, previously generated build artifacts can be removed with:

```bash
make clean
```

Mbed TLS headers and libraries must be available to the compiler and linker before building the application.

### 4. Start the Ground Control Station

Start the Ground Control Station before the telemetry source so that the UDP receiver is ready:

```bash
cd Ground-Station
python main.py
```

The application initializes the authentication layer, database connection, telemetry processing components, and PySide6 interface.

### 5. Start the Flight Computer

After the Ground Control Station is running, start the embedded telemetry application:

```bash
cd Flight-Computer
./flight_computer.exe
```

The Flight Computer generates telemetry, constructs the binary packet, applies AES-128-GCM protection, and transmits the resulting frames through the UDP datalink.

---

## 🚀 Usage / Quick Start

The normal runtime sequence is:

```text
Flight Computer
      │
      │ AES-128-GCM + UDP
      ▼
Ground Control Station
      │
      ├──► Telemetry Processing
      │
      ├──► PySide6 Live Dashboard
      │
      └──► MSSQL Persistence
```

### Runtime Sequence

1. Start the Ground Control Station.
2. Start the Flight Computer or telemetry simulation.
3. Telemetry is generated and converted into the project's binary packet format.
4. The telemetry payload is protected using AES-128-GCM.
5. The encrypted telemetry is transmitted through UDP.
6. The Ground Control Station receives and validates incoming packets.
7. Authenticated telemetry is decrypted and decoded.
8. Processed data is delivered to the PySide6 interface.
9. Telemetry records are buffered and persisted to MSSQL according to the database persistence workflow.

---

## 📡 Telemetry Protocol & Data Framing

The telemetry system uses a custom binary packet structure for communication between the Flight Computer and the Ground Control Station.

The primary flight-data payload is represented by the `DroneData` structure, while packet-level metadata is handled through the `TelemetryHeader`.

### `DroneData`

The telemetry payload contains the primary flight and system-state information:

| Field             | Type         | Description                 |
| ----------------- | ------------ | --------------------------- |
| `timestamp`       | `uint32_t`   | System timestamp            |
| `latitude`        | `float`      | GPS latitude                |
| `longitude`       | `float`      | GPS longitude               |
| `altitude`        | `float`      | GPS altitude                |
| `pitch`           | `float`      | Aircraft pitch              |
| `roll`            | `float`      | Aircraft roll               |
| `yaw`             | `float`      | Aircraft yaw                |
| `drone_id`        | `uint16_t`   | UAV identifier              |
| `motor_temp`      | `int16_t`    | Motor temperature           |
| `battery_temp`    | `int16_t`    | Battery temperature         |
| `status_code`     | `uint16_t`   | Bitwise system status flags |
| `battery_percent` | `uint8_t`    | Battery level               |
| `reserved`        | `uint8_t[3]` | Reserved bytes              |

### Packet Processing

The telemetry pipeline follows this general sequence:

```text
DroneData
    │
    ▼
TelemetryHeader + Binary Payload
    │
    ▼
AES-128-GCM
Encryption + Authentication
    │
    ▼
UDP Transmission
    │
    ▼
GCS Packet Reception
    │
    ▼
Packet Validation
    │
    ▼
AES-128-GCM Verification & Decryption
    │
    ▼
Binary Decoding
    │
    ├──────────────► PySide6 Visualization
    │
    └──────────────► Buffering / MSSQL Persistence
```

### AES-128-GCM

The telemetry payload is protected using **AES-128-GCM** through **Mbed TLS**.

AES-GCM provides:

* **Confidentiality** through authenticated encryption.
* **Cryptographic integrity** through the authentication tag.
* **Authentication** of the protected telemetry data.

The Ground Control Station verifies the authentication result before accepting the decrypted telemetry for further processing.

Cryptographic authentication is separate from application-level packet validation. Packet validation checks whether the received data conforms to the expected protocol structure, while AES-GCM verifies the authenticity and integrity of the protected payload.

### UDP Transport

UDP is used as the telemetry transport protocol because the system is designed around continuous, low-overhead telemetry delivery.

UDP itself does not provide reliable delivery or retransmission. Reliability and data-retention requirements are therefore addressed at the application level through the project's buffering and persistence mechanisms.

The architecture prioritizes the continuous processing of current telemetry while providing retention mechanisms for data that cannot be immediately forwarded or persisted.

### Heartbeat

A dedicated heartbeat mechanism provides a lightweight indication of system and connection state.

The heartbeat payload contains:

* Timestamp
* UAV identifier
* System state
* Reserved bytes

The heartbeat mechanism operates independently from the main telemetry data path and is used to monitor the operational state of the telemetry connection.

---

## 🧪 Simulation & Testing Environment

The project was developed and tested in a **simulated telemetry environment** rather than on a physical UAV.

A dataset containing **10,000 pre-generated telemetry records in CSV format** is used to reproduce the expected flow of flight data through the software architecture.

The simulated telemetry passes through the same major processing stages used by the system:

```text
10,000 Pre-Generated CSV Records
              │
              ▼
       Telemetry Generation
              │
              ▼
       Binary Packetization
              │
              ▼
        AES-128-GCM
              │
              ▼
         UDP Datalink
              │
              ▼
      GCS Packet Reception
              │
              ▼
     Validation / Decryption
              │
              ▼
        Telemetry Decode
          ┌───┴────┐
          ▼        ▼
     PySide6      Buffer
    Visualization    │
                    ▼
                  MSSQL
```

This simulation environment is used to exercise the telemetry pipeline, including packet generation, authenticated encryption, UDP communication, reception, decoding, buffering, database persistence, and graphical visualization.

The project has **not been validated through physical UAV flight testing or onboard flight-control hardware**. The simulation environment is therefore intended to validate the software architecture and data-processing pipeline rather than claim real-world flight performance.

---

## 👨‍💻 Author & Contact

**Eren Polat**

Computer Engineering Student at **Istanbul Kültür University**

Specializing in:

* Embedded Systems

* Cybersecurity

* Defense Technologies

* **GitHub:** [https://github.com/ErenPolat05]

* **LinkedIn:** [https://www.linkedin.com/in/erenpolat05/]
