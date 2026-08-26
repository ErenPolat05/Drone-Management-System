-- Users table. Auth storage.
CREATE TABLE Users (
    UserID INT IDENTITY(1,1) PRIMARY KEY, -- Auto-inc PK
    Username VARCHAR(50) NOT NULL UNIQUE, -- Unique login
    PasswordHash VARCHAR(256) NOT NULL, -- Encrypted auth
    Role VARCHAR(20) NOT NULL DEFAULT 'Operator', -- Access level
    CreatedAt DATETIME2 NOT NULL DEFAULT SYSDATETIME() -- Creation time
);

-- Telemetry data. High volume storage.
CREATE TABLE telemetry (
                    id BIGINT IDENTITY(1,1) PRIMARY KEY,
                    timestamp BIGINT NOT NULL,
                    drone_id INT NOT NULL,
                    latitude DECIMAL(9,6) NOT NULL,
                    longitude DECIMAL(9,6) NOT NULL,
                    altitude REAL NOT NULL,
                    pitch REAL NOT NULL,
                    roll REAL NOT NULL,
                    yaw REAL NOT NULL,
                    motor_temp DECIMAL(5,2) NOT NULL,
                    battery_temp DECIMAL(5,2) NOT NULL,
                    battery_percent INT NOT NULL,
                    status_code INT NOT NULL,
                    inserted_at DATETIME2 DEFAULT SYSDATETIME()
                );

-- Heartbeat data. 1 hz storage
CREATE TABLE heartbeat (
                    id BIGINT IDENTITY(1,1) PRIMARY KEY,
                    timestamp BIGINT NOT NULL,
                    drone_id INT NOT NULL,
                    state INT NOT NULL,
                    inserted_at DATETIME2 DEFAULT SYSDATETIME()
                );

-- System events. Parsed states.
CREATE TABLE EventLogs (
    EventID BIGINT IDENTITY(1,1) PRIMARY KEY, -- Auto-inc PK
    Timestamp DATETIME2 NOT NULL DEFAULT SYSDATETIME(), -- Event time
    Severity VARCHAR(20) NOT NULL, -- INFO, WARNING, CRITICAL
    Message VARCHAR(500) NOT NULL -- Parsed state desc
);