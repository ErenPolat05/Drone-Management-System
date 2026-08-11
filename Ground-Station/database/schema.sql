-- Users table. Auth storage.
CREATE TABLE Users (
    UserID INT IDENTITY(1,1) PRIMARY KEY, -- Auto-inc PK
    Username VARCHAR(50) NOT NULL UNIQUE, -- Unique login
    PasswordHash VARCHAR(256) NOT NULL, -- Encrypted auth
    Role VARCHAR(20) NOT NULL DEFAULT 'Operator', -- Access level
    CreatedAt DATETIME2 NOT NULL DEFAULT SYSDATETIME() -- Creation time
);

-- Telemetry data. High volume storage.
CREATE TABLE TelemetryLogs (
    LogID BIGINT IDENTITY(1,1) PRIMARY KEY, -- Auto-inc PK
    DroneTimestamp BIGINT NOT NULL, -- Drone internal ms
    DroneID INT NOT NULL, -- UAV identifier
    Latitude DECIMAL(10,7) NOT NULL, -- GPS Lat
    Longitude DECIMAL(10,7) NOT NULL, -- GPS Lon
    Altitude DECIMAL(10,7) NOT NULL, -- GPS Alt
    Pitch FLOAT NOT NULL, -- IMU Pitch
    Roll FLOAT NOT NULL, -- IMU Roll
    Yaw FLOAT NOT NULL, -- IMU Yaw
    MotorTemp DECIMAL(5,2) NOT NULL, -- Motor heat
    BatteryTemp DECIMAL(5,2) NOT NULL, -- Battery heat
    StatusCode INT NOT NULL, -- Raw state code
    BatteryPercent INT NOT NULL, -- Power level
    InsertedAt DATETIME2 NOT NULL DEFAULT SYSDATETIME() -- DB write time
);

-- System events. Parsed states.
CREATE TABLE EventLogs (
    EventID BIGINT IDENTITY(1,1) PRIMARY KEY, -- Auto-inc PK
    Timestamp DATETIME2 NOT NULL DEFAULT SYSDATETIME(), -- Event time
    Severity VARCHAR(20) NOT NULL, -- INFO, WARNING, CRITICAL
    Message VARCHAR(500) NOT NULL -- Parsed state desc
);