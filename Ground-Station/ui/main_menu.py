import sys
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QPushButton, QStackedWidget, QTableWidget, QTableWidgetItem,
    QProgressBar, QHeaderView, QFrame, QButtonGroup, QAbstractItemView
)
from PySide6.QtCore import Qt, QTimer, Signal, QTime ,QSize , QPointF
from PySide6.QtGui import QColor , QPainter , QPen , QBrush , QPolygonF
import qtawesome as qta

class ArtificialHorizonWidget(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.roll = 0.0
        self.pitch = 0.0
        self.setAutoFillBackground(True)
        p = self.palette()
        p.setColor(self.backgroundRole(), QColor("#1E1E1E"))
        self.setPalette(p)

    def update_attitude(self, pitch: float, roll: float):
        self.pitch = max(-45.0, min(45.0, float(pitch)))
        self.roll = max(-90.0, min(90.0, float(roll)))
        self.update()

    def paintEvent(self, event):
        with QPainter(self) as painter:
            painter.setRenderHint(QPainter.Antialiasing)

            w = self.width()
            h = self.height()
            cx = w / 2.0
            cy = h / 2.0

            painter.fillRect(self.rect(), QColor("#1E1E1E"))

            painter.save()
            painter.translate(cx, cy)
            painter.rotate(-self.roll)

            pitch_offset = self.pitch * 3.0
            box_size = max(w, h) * 3

            # Sky
            painter.setBrush(QBrush(QColor("#2980B9")))
            painter.setPen(Qt.NoPen)
            painter.drawRect(int(-box_size), int(-box_size + pitch_offset), int(box_size * 2), int(box_size))

            # Dirt
            painter.setBrush(QBrush(QColor("#795548")))
            painter.drawRect(int(-box_size), int(pitch_offset), int(box_size * 2), int(box_size))

            # Horizon Line
            painter.setPen(QPen(QColor("#FFFFFF"), 2))
            painter.drawLine(int(-box_size), int(pitch_offset), int(box_size), int(pitch_offset))

            painter.restore()

            # Sight
            painter.setPen(QPen(QColor("#F1C40F"), 2))
            painter.drawLine(int(cx - 40), int(cy), int(cx - 15), int(cy))
            painter.drawLine(int(cx - 15), int(cy), int(cx - 15), int(cy + 8))
            painter.drawLine(int(cx + 15), int(cy), int(cx + 40), int(cy))
            painter.drawLine(int(cx + 15), int(cy), int(cx + 15), int(cy + 8))
            
            painter.setBrush(QBrush(QColor("#F1C40F")))
            painter.drawEllipse(int(cx - 2), int(cy - 2), 4, 4)

class TacticalMapWidget(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.lat = 41.010000
        self.lon = 28.970000
        self.yaw = 0.0
        self.trail_points = []

    def update_position(self, lat: float, lon: float, yaw: float):
        self.lat = float(lat)
        self.lon = float(lon)
        self.yaw = float(yaw)
        
        if not self.trail_points or abs(self.trail_points[-1][0] - lat) > 0.000005 or abs(self.trail_points[-1][1] - lon) > 0.000005:
            self.trail_points.append((lat, lon))
            if len(self.trail_points) > 100:
                self.trail_points.pop(0)
                
        self.update()

    def paintEvent(self, event):
        with QPainter(self) as painter:
            painter.setRenderHint(QPainter.Antialiasing)

            width = self.width()
            height = self.height()
            cx = width / 2.0
            cy = height / 2.0

            painter.fillRect(self.rect(), QColor("#161A1D"))

            # Grid Lines
            painter.setPen(QPen(QColor("#223322"), 1, Qt.DashLine))
            step = 50
            x = int(cx % step)
            while x < width:
                painter.drawLine(x, 0, x, height)
                x += step
            y = int(cy % step)
            while y < height:
                painter.drawLine(0, y, width, y)
                y += step

            # Radar Rings
            painter.setPen(QPen(QColor(46, 204, 113, 40), 1))
            painter.setBrush(Qt.NoBrush)
            painter.drawEllipse(int(cx - 100), int(cy - 100), 200, 200)
            painter.drawEllipse(int(cx - 200), int(cy - 200), 400, 400)

            # Motion Trail
            if len(self.trail_points) > 1:
                painter.setPen(QPen(QColor("#2ECC71"), 2, Qt.SolidLine))
                for i in range(len(self.trail_points) - 1):
                    p1_x = cx + (self.trail_points[i][1] - self.lon) * 500000
                    p1_y = cy - (self.trail_points[i][0] - self.lat) * 500000
                    p2_x = cx + (self.trail_points[i+1][1] - self.lon) * 500000
                    p2_y = cy - (self.trail_points[i+1][0] - self.lat) * 500000
                    painter.drawLine(QPointF(p1_x, p1_y), QPointF(p2_x, p2_y))

            # Drone Icon (Rotates according to yaw angle)
            painter.save()
            painter.translate(cx, cy)
            painter.rotate(self.yaw)
            painter.setBrush(QBrush(QColor("#E74C3C")))
            painter.setPen(Qt.NoPen)
            arrow = QPolygonF([QPointF(0, -14), QPointF(-7, 10), QPointF(7, 10)])
            painter.drawPolygon(arrow)
            painter.restore()

            # Texts
            painter.setPen(QPen(QColor("#2ECC71")))
            painter.drawText(15, 25, f"LAT: {self.lat:.6f}")
            painter.drawText(15, 45, f"LON: {self.lon:.6f}")
            painter.drawText(15, 65, f"YAW: {self.yaw:.1f}°")

            painter.setPen(QPen(QColor("#333333"), 2))
            painter.drawRect(0, 0, width - 1, height - 1)
        
class MainMenuWindow(QMainWindow):
    # Expose UI events. Boundary for future Orchestrator integration.
    logout_requested = Signal()
    emergency_kill_requested = Signal()

    def __init__(self, username: str = "J. DOE", role: str = "MISSION COMMANDER"):
        super().__init__()

        # Store auth context.
        self.username = username
        self.role = role

        # Build tactical UI.
        self._setup_window()
        self._setup_styles()
        self._setup_ui()

        # Init clock timer. Non-blocking UI update.
        self.clock_timer = QTimer(self)
        self.clock_timer.timeout.connect(self._update_clock)
        self.clock_timer.start(1000)
        self._update_clock()

    def _setup_window(self):
        # Configure window bounds. Responsive defense standard.
        self.setWindowTitle("UAV Ground Control Station")
        self.setMinimumSize(1280, 720)
        self.resize(1440, 900)

    def _setup_styles(self):
        # Apply global tactical QSS. Use strict color palette.
        self.setStyleSheet("""
            QMainWindow {
                background-color: #0A0A0A;
            }
            QWidget {
                color: #E8E8E8;
                font-family: 'Segoe UI', Arial, sans-serif;
            }
            QFrame {
                border: none;
            }
            
            /* Sidebar Styling */
            #SidebarFrame {
                background-color: #101315;
                border-right: 1px solid #263238;
            }
            #UserLabel {
                font-size: 16px;
                font-weight: bold;
                color: #E8E8E8;
            }
            #RoleLabel {
                font-size: 12px;
                color: #4A90E2;
                margin-bottom: 20px;
            }
            QPushButton[cssClass="NavButton"] {
                background-color: transparent;
                color: #9AA4AA;
                text-align: left;
                padding: 12px 20px;
                font-size: 14px;
                font-weight: bold;
                border: none;
                border-left: 4px solid transparent;
            }
            QPushButton[cssClass="NavButton"]:hover {
                background-color: #1A1D20;
                color: #E8E8E8;
            }
            QPushButton[cssClass="NavButton"]:checked {
                background-color: #1A1D20;
                color: #4A90E2;
                border-left: 4px solid #4A90E2;
            }
            QPushButton#LogoutButton {
                background-color: transparent;
                color: #E74C3C;
                text-align: left;
                padding: 12px 20px;
                font-size: 14px;
                font-weight: bold;
                border: none;
            }
            QPushButton#LogoutButton:hover {
                background-color: #E74C3C;
                color: #0A0A0A;
            }

            /* HUD Styling */
            #HudFrame {
                background-color: #1A1D20;
                border-bottom: 1px solid #263238;
            }
            QLabel[cssClass="HudData"] {
                font-family: Consolas, 'Cascadia Mono', 'Courier New', monospace;
                font-size: 14px;
                font-weight: bold;
            }

            /* Battery Progress Bar */
            QProgressBar {
                background-color: #121212;
                border: 1px solid #263238;
                border-radius: 2px;
                text-align: center;
                color: #E8E8E8;
                font-size: 12px;
                font-weight: bold;
                min-width: 150px;
                max-width: 150px;
                min-height: 18px;
                max-height: 18px;
            }
            QProgressBar::chunk {
                background-color: #4A90E2;
            }

            /* Dashboard Panels */
            QFrame[cssClass="DashPanel"] {
                background-color: #1A1D20;
                border: 1px solid #263238;
                border-radius: 2px;
            }
            QLabel[cssClass="PanelTitle"] {
                font-size: 16px;
                font-weight: bold;
                color: #9AA4AA;
            }
            QLabel[cssClass="PanelSub"] {
                font-size: 12px;
                color: #596168;
            }
            QPushButton#KillSwitch {
                background-color: #E74C3C;
                color: #FFFFFF;
                font-size: 24px;
                font-weight: bold;
                border: 2px solid #8B0000;
                border-radius: 4px;
                padding: 20px;
            }
            QPushButton#KillSwitch:hover {
                background-color: #C0392B;
            }
            QPushButton#KillSwitch:pressed {
                background-color: #922B21;
            }

            /* Tables */
            QTableWidget {
                background-color: #0F1214;
                alternate-background-color: #14181B;
                color: #E8E8E8;
                gridline-color: #263238;
                border: 1px solid #263238;
                border-bottom: 2px solid #263238;
                font-family: Consolas, 'Cascadia Mono', 'Courier New', monospace;
                font-size: 13px;
            }
            QHeaderView::section {
                background-color: #1A1D20;
                color: #9AA4AA;
                padding: 6px;
                border: 1px solid #263238;
                font-family: 'Segoe UI', Arial, sans-serif;
                font-weight: bold;
            }
            QTableWidget::item:selected {
                background-color: #2C3E50;
            }

            /* Footer */
            #FooterFrame {
                background-color: #1A1D20;
                border-top: 1px solid #263238;
            }
            QLabel[cssClass="FooterText"] {
                font-family: Consolas, 'Cascadia Mono', 'Courier New', monospace;
                font-size: 12px;
                color: #9AA4AA;
            }
        """)

    def _setup_ui(self):
        # Establish main container.
        self.central_widget = QWidget()
        self.setCentralWidget(self.central_widget)

        # Base horizontal layout. Sidebar left, content right.
        self.main_layout = QHBoxLayout(self.central_widget)
        self.main_layout.setContentsMargins(0, 0, 0, 0)
        self.main_layout.setSpacing(0)

        # Generate structural sections.
        self._create_sidebar()
        
        self.content_widget = QWidget()
        self.content_layout = QVBoxLayout(self.content_widget)
        self.content_layout.setContentsMargins(0, 0, 0, 0)
        self.content_layout.setSpacing(0)
        
        self._create_top_hud()
        self._create_stacked_widget()
        self._create_status_bar()

        # Assemble main window.
        self.main_layout.addWidget(self.sidebar_frame)
        self.main_layout.addWidget(self.content_widget)

    def _create_sidebar(self):
        # Fixed-width navigation container.
        self.sidebar_frame = QFrame()
        self.sidebar_frame.setObjectName("SidebarFrame")
        self.sidebar_frame.setFixedWidth(240)
        
        layout = QVBoxLayout(self.sidebar_frame)
        layout.setContentsMargins(0, 30, 0, 20)
        layout.setSpacing(5)

        # Profile visual
        lbl_profile_icon = QPushButton()
        lbl_profile_icon.setIcon(
            qta.icon('fa5s.user-shield', color='#4A90E2')
        )
        lbl_profile_icon.setIconSize(QSize(48, 48))
        lbl_profile_icon.setEnabled(False)
        lbl_profile_icon.setStyleSheet(
            "background: transparent; border: none;"
        )

        lbl_user = QLabel(self.username)
        lbl_user.setObjectName("UserLabel")
        lbl_user.setAlignment(Qt.AlignmentFlag.AlignCenter)
        
        lbl_role = QLabel(f"ROLE: {self.role}")
        lbl_role.setObjectName("RoleLabel")
        lbl_role.setAlignment(Qt.AlignmentFlag.AlignCenter)
        
        layout.addWidget(lbl_profile_icon)
        layout.addWidget(lbl_user)
        layout.addWidget(lbl_role)
        layout.addSpacing(20)

        # Navigation group. Enforce single active state.
        self.nav_group = QButtonGroup(self)
        self.nav_group.setExclusive(True)
        self.nav_group.idClicked.connect(self._change_page)

        nav_items = [
            ("LIVE DASHBOARD", 'fa5s.tachometer-alt'),
            ("FLIGHT LOGS", 'fa5s.database'),
            ("EVENT MONITOR", 'fa5s.exclamation-triangle'),
        ]

        for idx, (text, icon_name) in enumerate(nav_items):
            btn = QPushButton(f"  {text}")
            btn.setIcon(qta.icon(icon_name, color='#9AA4AA'))
            btn.setProperty("cssClass", "NavButton")
            btn.setCheckable(True)
            self.nav_group.addButton(btn, idx)
            layout.addWidget(btn)

        # Set initial nav state.
        self.nav_group.button(0).setChecked(True)

        layout.addStretch()

        # Secure logout action.
        self.btn_logout = QPushButton("  SECURE LOGOUT")
        self.btn_logout.setIcon(qta.icon('fa5s.power-off', color='#E74C3C'))
        self.btn_logout.setObjectName("LogoutButton")
        self.btn_logout.clicked.connect(self.logout_requested.emit)
        layout.addWidget(self.btn_logout)

    def _create_top_hud(self):
        # Fixed-height operational header.
        self.hud_frame = QFrame()
        self.hud_frame.setObjectName("HudFrame")
        self.hud_frame.setFixedHeight(60)
        
        layout = QHBoxLayout(self.hud_frame)
        layout.setContentsMargins(20, 10, 20, 10)
        layout.setSpacing(30)

        # UDP status element.
        self.lbl_udp_icon = QLabel()
        self.lbl_udp_icon.setPixmap(qta.icon('fa5s.satellite-dish', color='#E74C3C').pixmap(18, 18))
        self.lbl_udp_text = QLabel("UDP: DISCONNECTED (Port: ----)")
        self.lbl_udp_text.setProperty("cssClass", "HudData")
        self.lbl_udp_text.setStyleSheet("color: #E74C3C;")
        
        udp_layout = QHBoxLayout()
        udp_layout.setSpacing(8)
        udp_layout.addWidget(self.lbl_udp_icon)
        udp_layout.addWidget(self.lbl_udp_text)

        # GPS status element.
        self.lbl_gps_icon = QLabel()
        self.lbl_gps_icon.setPixmap(qta.icon('fa5s.map-marker-alt', color='#95A5A6').pixmap(18, 18))
        self.lbl_gps_text = QLabel("GPS: NO FIX")
        self.lbl_gps_text.setProperty("cssClass", "HudData")
        self.lbl_gps_text.setStyleSheet("color: #95A5A6;")
        
        gps_layout = QHBoxLayout()
        gps_layout.setSpacing(8)
        gps_layout.addWidget(self.lbl_gps_icon)
        gps_layout.addWidget(self.lbl_gps_text)

        # Battery element. Tight pack required.
        batt_layout = QHBoxLayout()
        batt_layout.setContentsMargins(0, 0, 0, 0)
        batt_layout.setSpacing(8)
        
        lbl_batt_text = QLabel("BATTERY")
        lbl_batt_text.setProperty("cssClass", "HudData")
        
        lbl_batt_icon = QLabel()
        lbl_batt_icon.setPixmap(qta.icon('fa5s.battery-half', color='#E8E8E8').pixmap(18, 18))
        
        self.bar_battery = QProgressBar()
        self.bar_battery.setRange(0, 100)
        self.bar_battery.setValue(0)
       
        self.bar_battery.setFixedWidth(150) 
        
        batt_layout.addWidget(lbl_batt_text)
        batt_layout.addWidget(lbl_batt_icon)
        batt_layout.addWidget(self.bar_battery)

        # Clock element.
        self.lbl_clock_icon = QLabel()
        self.lbl_clock_icon.setPixmap(qta.icon('fa5s.clock', color='#E8E8E8').pixmap(18, 18))
        self.lbl_clock_text = QLabel("00:00:00")
        self.lbl_clock_text.setProperty("cssClass", "HudData")
        
        clock_layout = QHBoxLayout()
        clock_layout.setSpacing(8)
        clock_layout.addWidget(self.lbl_clock_icon)
        clock_layout.addWidget(self.lbl_clock_text)

        # Assemble HUD.
        layout.addLayout(udp_layout)
        layout.addLayout(gps_layout)
        layout.addStretch()
        layout.addLayout(batt_layout)
        layout.addStretch() 
        layout.addLayout(clock_layout)
        
        self.content_layout.addWidget(self.hud_frame)

    def _create_stacked_widget(self):
        # Central routing container.
        self.stack = QStackedWidget()
        
        self.page_dash = self._create_dashboard_page()
        self.page_logs = self._create_flight_logs_page()
        self.page_events = self._create_event_monitor_page()
        
        # Enforce insertion order matching nav group IDs.
        self.stack.addWidget(self.page_dash)
        self.stack.addWidget(self.page_logs)
        self.stack.addWidget(self.page_events)
        
        self.content_layout.addWidget(self.stack)

    def _create_dashboard_page(self) -> QWidget:
        # Main live telemetry interface.
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setContentsMargins(20, 20, 20, 20)
        layout.setSpacing(20)
        
        top_layout = QHBoxLayout()
        top_layout.setSpacing(20)
        
       # Add Artificial Horizon Widget directly without a QFrame wrapper
        self.horizon_widget = ArtificialHorizonWidget()
        
        # Add Tactical Map Widget directly
        self.tactical_map_widget = TacticalMapWidget()
        
        # Spatial ratio (1:2)
        top_layout.addWidget(self.horizon_widget, 1)
        top_layout.addWidget(self.tactical_map_widget, 2)
        
        # Emergency safety override.
        self.btn_kill = QPushButton("⚠ EMERGENCY KILL SWITCH ⚠")
        self.btn_kill.setObjectName("KillSwitch")
        self.btn_kill.clicked.append(self.emergency_kill_requested.emit) if hasattr(self.btn_kill.clicked, 'append') else self.btn_kill.clicked.connect(self.emergency_kill_requested.emit)
        
        layout.addLayout(top_layout, 1)
        layout.addWidget(self.btn_kill)
        
        return page

    def _create_flight_logs_page(self) -> QWidget:
        # History table container.
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setContentsMargins(20, 20, 20, 20)
        
        self.tbl_logs = QTableWidget(0, 10)
        headers = ["Timestamp", "Drone ID", "Lat", "Lon", "Alt", 
                   "Pitch", "Roll", "Yaw", "Battery", "Status"]
        self.tbl_logs.setHorizontalHeaderLabels(headers)
        
        # Enforce read-only and zebra stripes.
        self.tbl_logs.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.tbl_logs.setAlternatingRowColors(True)
        self.tbl_logs.verticalHeader().setVisible(False)
        self.tbl_logs.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        
        # Stretch all columns equally.
        header = self.tbl_logs.horizontalHeader()
        header.setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        
        layout.addWidget(self.tbl_logs)
        return page

    def _create_event_monitor_page(self) -> QWidget:
        # Alerts and states table.
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setContentsMargins(20, 20, 20, 20)
        
        self.tbl_events = QTableWidget(0, 3)
        headers = ["Timestamp", "Severity", "Message"]
        self.tbl_events.setHorizontalHeaderLabels(headers)
        
        # Enforce read-only and zebra stripes.
        self.tbl_events.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.tbl_events.setAlternatingRowColors(True)
        self.tbl_events.verticalHeader().setVisible(False)
        self.tbl_events.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        
        # Size content dynamically. Maximize message width.
        header = self.tbl_events.horizontalHeader()
        header.setSectionResizeMode(0, QHeaderView.ResizeMode.ResizeToContents)
        header.setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)
        header.setSectionResizeMode(2, QHeaderView.ResizeMode.Stretch)
        
        layout.addWidget(self.tbl_events)
        return page

    def _create_settings_page(self) -> QWidget:
        # Config placeholder.
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setContentsMargins(20, 20, 20, 20)
        
        pnl = QFrame()
        pnl.setProperty("cssClass", "DashPanel")
        pnl_layout = QVBoxLayout(pnl)
        
        lbl_title = QLabel("SYSTEM SETTINGS")
        lbl_title.setProperty("cssClass", "PanelTitle")
        lbl_title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        
        lbl_sub = QLabel("CONFIGURATION MODULE — STANDBY")
        lbl_sub.setProperty("cssClass", "PanelSub")
        lbl_sub.setAlignment(Qt.AlignmentFlag.AlignCenter)
        
        pnl_layout.addStretch()
        pnl_layout.addWidget(lbl_title)
        pnl_layout.addWidget(lbl_sub)
        pnl_layout.addStretch()
        
        layout.addWidget(pnl)
        return page

    def _create_status_bar(self):
        # Operational footer.
        self.footer_frame = QFrame()
        self.footer_frame.setObjectName("FooterFrame")
        self.footer_frame.setFixedHeight(30)
        
        layout = QHBoxLayout(self.footer_frame)
        layout.setContentsMargins(20, 0, 20, 0)
        
        # Database status block.
        db_layout = QHBoxLayout()
        db_layout.setSpacing(5)
        self.lbl_ft_db_icon = QLabel()
        self.lbl_ft_db_icon.setPixmap(qta.icon('fa5s.server', color='#9AA4AA').pixmap(12, 12))
        self.lbl_ft_db_text = QLabel("DB: WAITING")
        self.lbl_ft_db_text.setProperty("cssClass", "FooterText")
        db_layout.addWidget(self.lbl_ft_db_icon)
        db_layout.addWidget(self.lbl_ft_db_text)

        # Buffer status block.
        buf_layout = QHBoxLayout()
        buf_layout.setSpacing(5)
        self.lbl_ft_buf_icon = QLabel()
        self.lbl_ft_buf_icon.setPixmap(qta.icon('fa5s.database', color='#9AA4AA').pixmap(12, 12))
        self.lbl_ft_buf_text = QLabel("BUFFER: 0")
        self.lbl_ft_buf_text.setProperty("cssClass", "FooterText")
        buf_layout.addWidget(self.lbl_ft_buf_icon)
        buf_layout.addWidget(self.lbl_ft_buf_text)

        # Flush status block.
        flush_layout = QHBoxLayout()
        flush_layout.setSpacing(5)
        self.lbl_ft_flush_icon = QLabel()
        self.lbl_ft_flush_icon.setPixmap(qta.icon('fa5s.clock', color='#9AA4AA').pixmap(12, 12))
        self.lbl_ft_flush_text = QLabel("LAST FLUSH: --:--:--")
        self.lbl_ft_flush_text.setProperty("cssClass", "FooterText")
        flush_layout.addWidget(self.lbl_ft_flush_icon)
        flush_layout.addWidget(self.lbl_ft_flush_text)
        
        # Assemble footer.
        layout.addLayout(db_layout)
        layout.addStretch()
        layout.addLayout(buf_layout)
        layout.addStretch()
        layout.addLayout(flush_layout)
        
        self.content_layout.addWidget(self.footer_frame)

    def _change_page(self, index: int):
        # Route stacked widget to selected nav item.
        self.stack.setCurrentIndex(index)
        
        # Update active icons visually. 
        for btn in self.nav_group.buttons():
            if btn.isChecked():
                # Extract original icon name from setup logic, hardcoded here for active state coloring.
                icon_map = {0: 'fa5s.tachometer-alt', 1: 'fa5s.database', 2: 'fa5s.exclamation-triangle'}
                btn.setIcon(qta.icon(icon_map[self.nav_group.id(btn)], color='#4A90E2'))
            else:
                icon_map = {0: 'fa5s.tachometer-alt', 1: 'fa5s.database', 2: 'fa5s.exclamation-triangle'}
                btn.setIcon(qta.icon(icon_map[self.nav_group.id(btn)], color='#9AA4AA'))

    def _update_clock(self):
        # Refresh clock label. Avoid thread locking.
        current_time = QTime.currentTime().toString("HH:mm:ss")
        self.lbl_clock_text.setText(current_time)

    def update_hud(self, connected: bool, host_port: str, gps_fix: bool, battery: int):
        # Update UDP network state safely.
        if connected:
            self.lbl_udp_icon.setPixmap(qta.icon('fa5s.satellite-dish', color='#2ECC71').pixmap(18, 18))
            self.lbl_udp_text.setText(f"UDP: CONNECTED ({host_port})")
            self.lbl_udp_text.setStyleSheet("color: #2ECC71;")
        else:
            self.lbl_udp_icon.setPixmap(qta.icon('fa5s.satellite-dish', color='#E74C3C').pixmap(18, 18))
            self.lbl_udp_text.setText(f"UDP: DISCONNECTED ({host_port})")
            self.lbl_udp_text.setStyleSheet("color: #E74C3C;")

        # Update GPS location state safely.
        if gps_fix:
            self.lbl_gps_icon.setPixmap(qta.icon('fa5s.map-marker-alt', color='#2ECC71').pixmap(18, 18))
            self.lbl_gps_text.setText("GPS: FIX")
            self.lbl_gps_text.setStyleSheet("color: #2ECC71;")
        else:
            self.lbl_gps_icon.setPixmap(qta.icon('fa5s.map-marker-alt', color='#F1C40F').pixmap(18, 18))
            self.lbl_gps_text.setText("GPS: NO FIX")
            self.lbl_gps_text.setStyleSheet("color: #F1C40F;")

        # Clamp battery percentage and assign.
        clamped_batt = max(0, min(100, battery))
        self.bar_battery.setValue(clamped_batt)

    def update_attitude_and_map(self, pitch: float, roll: float, lat: float, lon: float, yaw: float):
        if hasattr(self, 'horizon_widget'):
            self.horizon_widget.update_attitude(pitch, roll)
        if hasattr(self, 'tactical_map_widget'):
            self.tactical_map_widget.update_position(lat, lon, yaw)

    def add_event_log(self, timestamp: str, severity: str, message: str):
        # Insert event at table bottom.
        row = self.tbl_events.rowCount()
        self.tbl_events.insertRow(row)

        it_time = QTableWidgetItem(timestamp)
        it_sev = QTableWidgetItem(severity)
        it_msg = QTableWidgetItem(message)

        # Apply tactical severity colors overriding zebra.
        if severity in ("CRITICAL", "EMERGENCY"):
            bg_color = QColor("#5C2222") # Dark red tint
            fg_color = QColor("#FFFFFF")
        elif severity == "WARNING":
            bg_color = QColor("#F1C40F") # Yellow tint
            fg_color = QColor("#000000")
        elif severity == "NORMAL":
            bg_color = None # Fallback to zebra
            fg_color = QColor("#2ECC71")
        else:
            bg_color = None # Fallback to zebra
            fg_color = QColor("#E8E8E8")

        for item in (it_time, it_sev, it_msg):
            if bg_color:
                item.setBackground(bg_color)
            item.setForeground(fg_color)

        self.tbl_events.setItem(row, 0, it_time)
        self.tbl_events.setItem(row, 1, it_sev)
        self.tbl_events.setItem(row, 2, it_msg)

        # Ensure operator sees the newest critical data.
        self.tbl_events.scrollToBottom()

    def update_footer(self, db_status: str, buffer_size: int, last_flush: str):
        # Update I/O status fields.
        self.lbl_ft_db_text.setText(f"DB: {db_status}")
        self.lbl_ft_buf_text.setText(f"BUFFER: {buffer_size}")
        self.lbl_ft_flush_text.setText(f"LAST FLUSH: {last_flush}")


if __name__ == "__main__":
    # Standalone module test runner
    app = QApplication(sys.argv)
    window = MainMenuWindow(username="OPERATOR-1", role="SYSTEM ADMIN")
    window.show()
    sys.exit(app.exec())