from PySide6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel, 
                             QLineEdit, QPushButton)
from PySide6.QtCore import Qt, Signal

class LoginWindow(QWidget):

    login_success= Signal(str)

    def __init__(self, authenticator):
        super().__init__()
        
        # Store injected auth. Decouple logic.
        self.authenticator = authenticator
        
        # Setup window parameters.
        self.setWindowTitle("UAV Ground Station - Login")
        self.setFixedSize(400, 400)
        
        # Position window.
        self.center_window()
        
        # Build interface.
        self.setup_ui()

    def center_window(self):
        # Center on active screen.
        qr = self.frameGeometry()
        cp = self.screen().availableGeometry().center()
        qr.moveCenter(cp)
        self.move(qr.topLeft())

    def setup_ui(self):
        # Main vertical layout.
        layout = QVBoxLayout(self)
        layout.setContentsMargins(40, 40, 40, 30)
        layout.setSpacing(10)

        # App title.
        self.lbl_title = QLabel("UAV GROUND STATION")
        self.lbl_title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.lbl_title.setObjectName("TitleLabel")

        # Subtitle.
        self.lbl_subtitle = QLabel("SECURE LOGIN")
        self.lbl_subtitle.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.lbl_subtitle.setObjectName("SubtitleLabel")

        # Username fields.
        self.lbl_user = QLabel("USERNAME")
        self.inp_user = QLineEdit()
        
        # Password fields. Hide text.
        self.lbl_pass = QLabel("PASSWORD")
        self.inp_pass = QLineEdit()
        self.inp_pass.setEchoMode(QLineEdit.EchoMode.Password)

        # Login action.
        self.btn_login = QPushButton("LOGIN")
        self.btn_login.setCursor(Qt.CursorShape.PointingHandCursor)
        
        # Button layout. Align right.
        btn_layout = QHBoxLayout()
        btn_layout.addStretch()
        btn_layout.addWidget(self.btn_login)

        # Error display. Init empty.
        self.lbl_error = QLabel("")
        self.lbl_error.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.lbl_error.setObjectName("ErrorLabel")

        # System state display.
        self.lbl_status = QLabel("SYSTEM READY")
        self.lbl_status.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.lbl_status.setObjectName("StatusLabel")

        # Assemble layout.
        layout.addWidget(self.lbl_title)
        layout.addWidget(self.lbl_subtitle)
        layout.addSpacing(20)
        layout.addWidget(self.lbl_user)
        layout.addWidget(self.inp_user)
        layout.addSpacing(5)
        layout.addWidget(self.lbl_pass)
        layout.addWidget(self.inp_pass)
        layout.addSpacing(15)
        layout.addLayout(btn_layout)
        layout.addSpacing(10)
        layout.addWidget(self.lbl_error)
        layout.addStretch()
        layout.addWidget(self.lbl_status)

        # Bind events.
        self.inp_user.returnPressed.connect(self.inp_pass.setFocus)
        self.inp_pass.returnPressed.connect(self.handle_login)
        self.btn_login.clicked.connect(self.handle_login)

        # Enforce tab order.
        self.setTabOrder(self.inp_user, self.inp_pass)
        self.setTabOrder(self.inp_pass, self.btn_login)

        # Apply QSS.
        self.apply_styles()

    def apply_styles(self):
        # Dark defense theme. Restrained colors.
        self.setStyleSheet("""
            QWidget {
                background-color: #121212;
                color: #CCCCCC;
                font-family: 'Segoe UI', Arial, sans-serif;
            }
            QLabel {
                font-size: 10px;
                font-weight: bold;
                letter-spacing: 1px;
                color: #888888;
            }
            #TitleLabel {
                font-size: 18px;
                color: #FFFFFF;
                font-weight: bold;
                letter-spacing: 2px;
            }
            #SubtitleLabel {
                font-size: 11px;
                color: #4A90E2;
                margin-bottom: 10px;
            }
            QLineEdit {
                background-color: #1A1A1A;
                border: 1px solid #333333;
                border-radius: 4px;
                padding: 6px 10px;
                color: white;
                font-size: 13px;
                min-height: 30px;
            }
            QLineEdit:focus {
                border: 1px solid #4A90E2;
                background-color: #1E1E1E;
            }
            QPushButton {
                background-color: #2C3E50;
                border: 1px solid #34495E;
                border-radius: 4px;
                padding: 10px 24px;
                color: white;
                font-size: 12px;
                font-weight: bold;
                letter-spacing: 1px;
                min-width: 100px;
            }
            QPushButton:hover {
                background-color: #34495E;
                border: 1px solid #4A90E2;
            }
            QPushButton:pressed {
                background-color: #1A252F;
            }
            QPushButton:disabled {
                background-color: #111111;
                color: #555555;
                border: 1px solid #222222;
            }
            #ErrorLabel {
                color: #E74C3C;
                font-size: 11px;
                font-weight: normal;
            }
            #StatusLabel {
                color: #2ECC71;
                font-size: 9px;
                font-weight: normal;
                letter-spacing: 2px;
            }
        """)

    def handle_login(self):
        # Reset error state.
        self.lbl_error.setText("")

        # Extract inputs.
        username = self.inp_user.text().strip()
        password = self.inp_pass.text()

        # Block empty credentials.
        if not username or not password:
            self.lbl_error.setText("Invalid username or password")
            return

        # Lock UI. Prevent double submit.
        self.btn_login.setEnabled(False)
        self.btn_login.repaint()

        # Delegate auth logic.
        success, role, message = self.authenticator.login(username, password)

        if success:
            # Output role. Keep generic.
            print(f"Login OK! Role: {role}")
            self.login_success.emit(role)
        else:
            # Show generic error.
            self.lbl_error.setText(message)

        # Clear sensitive data. Unlock UI.
        self.inp_pass.clear()
        self.btn_login.setEnabled(True)