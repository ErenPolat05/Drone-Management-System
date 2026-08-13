import struct
import logging
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.exceptions import InvalidTag

class DataProcessor:
    def __init__(self):
        # Init logger. Track security events.
        self.logger = logging.getLogger(__name__)
        
        # Init AES-128-GCM. Fixed key per requirements.
        key = bytes([0x4F, 0x1A, 0x7B, 0x92, 0xC3, 0x8D, 0xE5, 0x22, 
                     0xA1, 0x0F, 0x66, 0x39, 0xBB, 0x44, 0xDF, 0x19])
        self.aesgcm = AESGCM(key)

    def process(self, packet_type: int, payload: bytes):
        # Route by type. Prevent invalid processing.
        if packet_type == 2:
            # Check length. Heartbeat strictly 8 bytes.
            if len(payload) != 8:
                return None
                
            # Unpack heartbeat. Little-endian layout.
            try:
                unpacked = struct.unpack('<IHBB', payload)
            except struct.error:
                return None
                
            # Map struct fields. Ignore reserved byte.
            return {
                "type": "heartbeat",
                "timestamp": unpacked[0],
                "drone_id": unpacked[1],
                "state": unpacked[2]
            }

        elif packet_type == 1:
            # Check length. 12 Nonce + 40 CT + 16 Tag = 68 bytes.
            if len(payload) != 68:
                return None
                
            # Extract cryptographic components.
            nonce = payload[:12]
            ciphertext_with_tag = payload[12:68]
            
            # Decrypt payload. Validate GCM authentication tag.
            try:
                decrypted = self.aesgcm.decrypt(nonce, ciphertext_with_tag, None)
            except InvalidTag:
                # Log failure. Reject spoofed or corrupted payload.
                self.logger.warning("SECURITY ALERT: Payload authentication failed (InvalidTag). Possible spoofing or corruption.")
                return None
                
            # Unpack telemetry. Little-endian layout.
            # TODO: 32-bit float coordinates have limited GPS precision; migration to 64-bit double requires a C protocol/struct change and larger payload.
            try:
                unpacked = struct.unpack('<IffffffHhhHB3s', decrypted)
            except struct.error:
                return None
                
            # Map struct fields. Ignore reserved bytes.
            return {
                "type": "telemetry",
                "timestamp": unpacked[0],
                "latitude": unpacked[1],
                "longitude": unpacked[2],
                "altitude": unpacked[3],
                "pitch": unpacked[4],
                "roll": unpacked[5],
                "yaw": unpacked[6],
                "drone_id": unpacked[7],
                "motor_temp": unpacked[8],
                "battery_temp": unpacked[9],
                "status_code": unpacked[10],
                "battery_percent": unpacked[11]
            }

        # Drop unknown packet types.
        return None