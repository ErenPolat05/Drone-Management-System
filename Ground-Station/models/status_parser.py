class StatusCodeParser:
    @staticmethod
    def parse(status_code: int) -> dict:
        # Check normal state. Fast exit.
        if status_code == 0:
            return {
                "code": 0,
                "severity": "NORMAL",
                "alerts": ["ALL SYSTEMS GO"]
            }

        alerts = []
        severity_level = 0  # 1=WARN, 2=CRIT, 3=EMERG

        # Bit 0: BATT_LOW (WARNING)
        if status_code & 1:
            alerts.append("BATT_LOW")
            if severity_level < 1: severity_level = 1

        # Bit 1: BATT_CRIT (CRITICAL)
        if status_code & 2:
            alerts.append("BATT_CRIT")
            if severity_level < 2: severity_level = 2

        # Bit 2: MOTOR_HOT (WARNING)
        if status_code & 4:
            alerts.append("MOTOR_HOT")
            if severity_level < 1: severity_level = 1

        # Bit 3: MOTOR_CRIT (CRITICAL)
        if status_code & 8:
            alerts.append("MOTOR_CRIT")
            if severity_level < 2: severity_level = 2

        # Bit 4: BATT_COLD (CRITICAL)
        if status_code & 16:
            alerts.append("BATT_COLD")
            if severity_level < 2: severity_level = 2

        # Bit 5: BATT_HOT (CRITICAL)
        if status_code & 32:
            alerts.append("BATT_HOT")
            if severity_level < 2: severity_level = 2

        # Bit 6: AERODYNAMICS (CRITICAL)
        if status_code & 64:
            alerts.append("AERODYNAMICS")
            if severity_level < 2: severity_level = 2

        # Bit 7: ALTITUDE_MAX (WARNING)
        if status_code & 128:
            alerts.append("ALTITUDE_MAX")
            if severity_level < 1: severity_level = 1

        # Bit 8: TERRAIN_WARN (CRITICAL)
        if status_code & 256:
            alerts.append("TERRAIN_WARN")
            if severity_level < 2: severity_level = 2

        # Bit 9: FREEFALL (EMERGENCY)
        if status_code & 512:
            alerts.append("FREEFALL")
            severity_level = 3

        # Bit 10: GPS_SPOOFING (EMERGENCY)
        if status_code & 1024:
            alerts.append("GPS_SPOOFING")
            severity_level = 3

        # Map level to string. Define overall state.
        severity_map = {0: "NORMAL", 1: "WARNING", 2: "CRITICAL", 3: "EMERGENCY"}
        overall_severity = severity_map.get(severity_level, "UNKNOWN")

        return {
            "code": status_code,
            "severity": overall_severity,
            "alerts": alerts
        }