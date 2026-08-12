import pyodbc
import logging

class DatabaseManager:
    def __init__(self, server=r'.\SQLEXPRESS', database='UAV_GroundStation'):
        # Build conn string. Windows Auth.
        self.conn_str = f"DRIVER={{ODBC Driver 17 for SQL Server}};SERVER={server};DATABASE={database};Trusted_Connection=yes;"
        self.conn = None
        self.cursor = None

    def connect(self):
        # Connect to DB. Setup cursor.
        try:
            self.conn = pyodbc.connect(self.conn_str)
            self.cursor = self.conn.cursor()
        except pyodbc.Error as e:
            # Handle error. Hide creds.
            logging.error("DB connection failed.")
            raise RuntimeError("Database connection failed.") from e

    def disconnect(self):
        # Close cursor. Free resources.
        if self.cursor:
            try:
                self.cursor.close()
            except pyodbc.Error:
                pass
            self.cursor = None
            
        # Close connection. Safe state.
        if self.conn:
            try:
                self.conn.close()
            except pyodbc.Error:
                pass
            self.conn = None

    def get_user_by_username(self, username):
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