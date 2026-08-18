import pyodbc
import logging
import threading

class DatabaseManager:
    def __init__(self, server=r'.\SQLEXPRESS', database='UAV_GroundStation'):
        # Build connection string. Windows Auth.
        self.conn_str = f"DRIVER={{ODBC Driver 17 for SQL Server}};SERVER={server};DATABASE={database};Trusted_Connection=yes;"
        self.conn = None
        self.cursor = None

        # Init buffers. Store incoming high-frequency data.
        self.telemetry_buffer = []
        self.heartbeat_buffer = []

        # Init lock. Prevent thread race conditions during buffer operations.
        self.lock = threading.Lock()

    def connect(self):
        # Connect to MSSQL. Setup cursor.
        try:
            self.conn = pyodbc.connect(self.conn_str)
            self.cursor = self.conn.cursor()
            # Initialize tables and indexes after connect.
            self._init_db()
        except pyodbc.Error as e:
            # Handle error. Hide creds.
            logging.error("DB connection failed.")
            self.disconnect()
            raise RuntimeError("Database connection failed.") from e

    def disconnect(self):
        # Close cursor. Free resources.
        if self.cursor:
            try:
                self.cursor.close()
            except Exception:
                pass
            self.cursor = None

        # Close connection. Safe state.
        if self.conn:
            try:
                self.conn.close()
            except Exception:
                pass
            self.conn = None

    def _init_db(self):
        # Create telemetry table. Use MSSQL optimized types.
        self.cursor.execute('''
            IF OBJECT_ID('telemetry', 'U') IS NULL
            BEGIN
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
                -- Create index. Fast read performance for dashboard.
                CREATE NONCLUSTERED INDEX IX_telemetry_drone_time ON telemetry(drone_id, timestamp);
            END
        ''')

        # Create heartbeat table. Use MSSQL optimized types.
        self.cursor.execute('''
            IF OBJECT_ID('heartbeat', 'U') IS NULL
            BEGIN
                CREATE TABLE heartbeat (
                    id BIGINT IDENTITY(1,1) PRIMARY KEY,
                    timestamp BIGINT NOT NULL,
                    drone_id INT NOT NULL,
                    state INT NOT NULL,
                    inserted_at DATETIME2 DEFAULT SYSDATETIME()
                );
                -- Create index. Fast read performance for dashboard.
                CREATE NONCLUSTERED INDEX IX_heartbeat_drone_time ON heartbeat(drone_id, timestamp);
            END
        ''')

        # Commit schema changes.
        self.conn.commit()

    def insert_telemetry(self, data: dict):
        # Extract values.
        values = (
            data.get("timestamp"),
            data.get("drone_id"),
            data.get("latitude"),
            data.get("longitude"),
            data.get("altitude"),
            data.get("pitch"),
            data.get("roll"),
            data.get("yaw"),
            data.get("motor_temp"),
            data.get("battery_temp"),
            data.get("battery_percent"),
            data.get("status_code")
        )

        # Append to buffer. Thread-safe write.
        with self.lock:
            self.telemetry_buffer.append(values)

    def insert_heartbeat(self, data: dict):
        # Extract values.
        values = (
            data.get("timestamp"),
            data.get("drone_id"),
            data.get("state")
        )

        # Append to buffer. Thread-safe write.
        with self.lock:
            self.heartbeat_buffer.append(values)

    def flush_to_db(self):
        # Copy and clear buffers. Keep lock duration minimal.
        with self.lock:
            t_data = self.telemetry_buffer[:]
            h_data = self.heartbeat_buffer[:]
            self.telemetry_buffer.clear()
            self.heartbeat_buffer.clear()

        # Skip empty flushes.
        if not t_data and not h_data:
            return

        # Guard against disconnected state.
        if not self.conn or not self.cursor:
            logging.error("Flush failed: database not connected.")
            with self.lock:
                self.telemetry_buffer = t_data + self.telemetry_buffer
                self.heartbeat_buffer = h_data + self.heartbeat_buffer
            return

        try:
            # Batch insert telemetry. Eliminate I/O blocking.
            if t_data:
                t_query = '''
                    INSERT INTO telemetry (
                        timestamp, drone_id, latitude, longitude, altitude, 
                        pitch, roll, yaw, motor_temp, battery_temp, 
                        battery_percent, status_code
                    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                '''
                self.cursor.executemany(t_query, t_data)

            # Batch insert heartbeat. Eliminate I/O blocking.
            if h_data:
                h_query = '''
                    INSERT INTO heartbeat (timestamp, drone_id, state)
                    VALUES (?, ?, ?)
                '''
                self.cursor.executemany(h_query, h_data)

            # Commit transaction. Single write operation.
            self.conn.commit()

        except Exception as e:
            # Handle failure. Log error.
            logging.error("Database flush failed.")

            # Rollback failed transaction.
            try:
                self.conn.rollback()
            except Exception:
                pass

            # Prepend failed records. Prevent data loss.
            with self.lock:
                self.telemetry_buffer = t_data + self.telemetry_buffer
                self.heartbeat_buffer = h_data + self.heartbeat_buffer

    def get_user_by_username(self, username):
        # Guard against disconnected state.
        if not self.cursor or not self.conn:
            raise RuntimeError("Database is not connected.")

        # Query user. Param prevents injection.
        query = "SELECT UserID, Username, PasswordHash, Role FROM Users WHERE Username = ?"

        try:
            self.cursor.execute(query, (username,))
            row = self.cursor.fetchone()

            # Map row to dict.
            if row:
                return {
                    'UserID': row[0],
                    'Username': row[1],
                    'PasswordHash': row[2],
                    'Role': row[3]
                }
            return None

        except pyodbc.Error as e:
            # Handle query fail.
            logging.error("Failed to fetch user.")
            raise RuntimeError("Database read failed.") from e