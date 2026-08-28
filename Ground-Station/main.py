import sys
import socket
import time
import logging
from PySide6.QtWidgets import QApplication, QTableWidgetItem
from PySide6.QtCore import QThread, Signal, QObject

from ui.login_window import LoginWindow
from ui.main_menu import MainMenuWindow
from auth.authenticator import Authenticator
from database.db_manager import DatabaseManager
from security.header_parser import HeaderParser
from security.data_processor import DataProcessor
from models.status_parser import StatusCodeParser

class UDPWorker(QThread):
    # Core communication signals mapped to UI and data handlers
    hud_update = Signal(bool, str, bool, int)
    event_update = Signal(str, str, str)
    footer_update = Signal(str, int, str)
    telemetry_log_update = Signal(list)
    attitude_map_update = Signal(float, float, float, float, float)

    def __init__(self, db_manager, header_parser, data_processor):
        super().__init__()
        self.db = db_manager
        self.header_parser = header_parser
        self.data_processor = data_processor
        
        self._is_running = False
        self.sock = None
        self.last_flush = time.time()
        self.host_port = "127.0.0.1:8080"
        
        # Packet counter used for UI rate-limiting (throttling) to prevent CPU spikes
        self.packet_counter = 0
        # State tracker to prevent event log spamming via debounce logic
        self.last_logged_events = {}

    def run(self):
        self._is_running = True
        print("[INFO] UDP Worker thread started. Listening for telemetry stream...")
        
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.sock.setsockopt(socket.SOL_SOCKET, SO_REUSEADDR := socket.SO_REUSEADDR, 1)
            self.sock.bind(('127.0.0.1', 8080))
            self.sock.settimeout(1.0)
        except OSError as e:
            logging.error(f"UDP bind failed: {e}")
            print(f"[ERROR] UDP bind failed: {e}")
            self._is_running = False
            return

        while self._is_running:
            try:
                data, addr = self.sock.recvfrom(2048)
                self._process_packet(data)
            except socket.timeout:
                self.hud_update.emit(False, self.host_port, False, 0)
                self._check_flush()
            except OSError:
                break
                
        if self.sock:
            try:
                self.sock.close()
            except OSError:
                pass
        print("[INFO] UDP Worker socket closed safely.")

    def _process_packet(self, data):
        valid, msg, header, payload = self.header_parser.parse(data)
        if not valid:
            return

        packet_type = header.get("packet_type")
        processed = self.data_processor.process(packet_type, payload)
        if not processed:
            return

        self.packet_counter += 1

        if packet_type == 1:
            # 1. DATABASE BUFFER: All telemetry packets are buffered for batch insertion
            self.db.insert_telemetry(processed)

            # Extract spatial data for Artificial Horizon and Tactical Map widgets
            pitch = processed.get("pitch", 0.0)
            roll = processed.get("roll", 0.0)
            lat = processed.get("latitude", 0.0)
            lon = processed.get("longitude", 0.0)
            yaw = processed.get("yaw", 0.0)

            self.attitude_map_update.emit(pitch, roll, lat, lon, yaw)

            status_code = processed.get("status_code", 0)
            status_info = StatusCodeParser.parse(status_code)
            severity = status_info.get("severity", "UNKNOWN")
            
            batt = processed.get("battery_percent", 0)
            gps_fix = processed.get("latitude", 0.0) != 0.0 or processed.get("longitude", 0.0) != 0.0
            
            # Real-time HUD update for critical operational parameters
            self.hud_update.emit(True, self.host_port, gps_fix, int(batt))
            
            # Handle active telemetry alerts with bitwise flag processing
            if severity != "NORMAL":
                ts_str = time.strftime("%H:%M:%S")
                current_time = time.time()
                
                alerts_list = status_info.get("alerts", [])
                
                if alerts_list:
                    # Database persistence: Store every individual event securely
                    for alert in alerts_list:
                        if hasattr(self.db, "insert_event"):
                            self.db.insert_event(severity, alert)
                    
                    # UI Optimization: Combine concurrent bitwise flags into a single clean log line
                    combined_message = " | ".join(alerts_list)
                    
                    event_key = (severity, combined_message)
                    last_time = self.last_logged_events.get(event_key, 0)
                    
                    # Rate-limit/Debounce filter: Prevent UI log flooding while maintaining visibility
                    if current_time - last_time >= 0.5:
                        self.last_logged_events[event_key] = current_time
                        self.event_update.emit(ts_str, severity, combined_message)

            # 2. UI RATE-LIMITING (THROTTLE): Update flight logs table every 25 packets to save CPU cycles
            if self.packet_counter % 25 == 0:
                log_row = [
                    str(processed.get("timestamp")), str(processed.get("drone_id")),
                    str(processed.get("latitude")), str(processed.get("longitude")),
                    str(processed.get("altitude")), str(processed.get("pitch")),
                    str(processed.get("roll")), str(processed.get("yaw")),
                    f"{batt}%", severity
                ]
                self.telemetry_log_update.emit(log_row)

            self._check_flush()
            
            buf_size = len(self.db.telemetry_buffer) + len(self.db.heartbeat_buffer)
            self.footer_update.emit("CONNECTED", buf_size, time.strftime("%H:%M:%S"))

        elif packet_type == 2:
            self.db.insert_heartbeat(processed)
            self._check_flush()

    def _check_flush(self):
        now = time.time()
        # Flush cached database buffers every 1 second
        if now - self.last_flush >= 1.0:
            self.db.flush_to_db()
            self.last_flush = now

    def stop(self):
        print("[INFO] Stopping UDP Worker thread...")
        self._is_running = False
        if self.sock:
            try:
                self.sock.close()
            except OSError:
                pass


class AppController(QObject):    
    def __init__(self):
        super().__init__()
        print("[INFO] Initializing Ground Control Station Application...")
        self.auth_db = DatabaseManager()
        self.auth_db.connect()
        
        self.authenticator = Authenticator(self.auth_db)
        self.login_window = LoginWindow(self.authenticator)
        
        self.login_window.login_success.connect(self._on_login_success)
        self.login_window.show()

    def _on_login_success(self, role: str):
        print(f"[INFO] Login successful! Assigned Role: {role}")
        self.login_window.hide()
        
        self.db = DatabaseManager()
        self.db.connect()
        self.header_parser = HeaderParser()
        self.data_processor = DataProcessor()
        
        self.main_menu = MainMenuWindow(username="OPERATOR", role=role)
        
        # Bind window lifecycle events for secure termination
        self.main_menu.closeEvent = self._on_main_window_close
        self.main_menu.logout_requested.connect(self._handle_logout)
        
        self.main_menu.show()
        
        # Initialize and configure background telemetry worker thread
        self.worker = UDPWorker(self.db, self.header_parser, self.data_processor)
        
        self.worker.hud_update.connect(self.main_menu.update_hud)
        self.worker.event_update.connect(self.main_menu.add_event_log)
        self.worker.footer_update.connect(self.main_menu.update_footer)
        self.worker.telemetry_log_update.connect(self._append_flight_log_row)
        self.worker.attitude_map_update.connect(self.main_menu.update_attitude_and_map)
        
        self.worker.start()

    def _append_flight_log_row(self, row_data):
        # Safely append incoming flight telemetry data to the UI table widget
        if hasattr(self.main_menu, "tbl_logs"):
            tbl = self.main_menu.tbl_logs
            row = tbl.rowCount()
            tbl.insertRow(row)
            for col_idx, val in enumerate(row_data):
                tbl.setItem(row, col_idx, QTableWidgetItem(val))
            tbl.scrollToBottom()

    def _handle_logout(self):
        print("[INFO] Secure logout requested by operator.")
        self._shutdown()

    def _on_main_window_close(self, event):
        print("[INFO] Main application window close event triggered.")
        self._shutdown()
        event.accept()

    def _shutdown(self):
        print("[INFO] Beginning safe system shutdown sequence...")
        if hasattr(self, 'worker') and self.worker.isRunning():
            self.worker.stop()
            self.worker.wait()
            print("[INFO] UDP Worker thread successfully joined and terminated.")
        
        if hasattr(self, 'db') and self.db:
            try:
                self.db.flush_to_db()
                self.db.disconnect()
                print("[INFO] Telemetry database connection closed safely.")
            except Exception as e:
                print(f"[WARNING] Error closing telemetry database: {e}")
            
        if hasattr(self, 'auth_db') and self.auth_db:
            try:
                self.auth_db.disconnect()
                print("[INFO] Auth database connection closed safely.")
            except Exception as e:
                print(f"[WARNING] Error closing auth database: {e}")
            
        print("[INFO] Terminal closed. System terminated securely.")
        QApplication.quit()

if __name__ == '__main__':
    app = QApplication(sys.argv)
    controller = AppController()
    sys.exit(app.exec())