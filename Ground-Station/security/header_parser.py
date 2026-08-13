import struct
from collections import deque

# Protocol constants. Match C header.
HEADER_SIZE = 14
MAGIC = 0xABCD
VERSION = 1
PACKET_TYPE_TELEMETRY = 1
PACKET_TYPE_HEARTBEAT = 2
HEARTBEAT_PAYLOAD_SIZE = 8
TELEMETRY_PAYLOAD_SIZE = 68
MAX_SEQ_GAP = 1000

class HeaderParser:
    def __init__(self, window_size=50):
        # Init state. Track sessions and replays.
        self.window_size = window_size
        self.current_session = None
        self.highest_seq = -1
        # Bounded queue. Auto-drops old entries.
        self.seen_seqs = deque(maxlen=window_size)

    def parse(self, raw_data):
        # Check type. Reject bad input.
        if not isinstance(raw_data, bytes):
            return False, "Input not bytes", None, None

        # Check min length. Prevent index out of bounds.
        if len(raw_data) < HEADER_SIZE:
            return False, "Packet too short", None, None

        try:
            # Parse bytes. Big-Endian network order.
            unpacked = struct.unpack('!HBBIIH', raw_data[:HEADER_SIZE])
            magic, version, packet_type, session_id, sequence_id, payload_len = unpacked
        except struct.error:
            # Handle unpack fail.
            return False, "Header parse failed", None, None

        # Verify magic. Ensure protocol match.
        if magic != MAGIC:
            return False, "Invalid magic", None, None

        # Verify version. Drop unsupported.
        if version != VERSION:
            return False, "Invalid version", None, None

        # Verify type. Allow only known types.
        if packet_type not in (PACKET_TYPE_TELEMETRY, PACKET_TYPE_HEARTBEAT):
            return False, "Invalid packet type", None, None

        # Verify declared payload length. Prevent truncation.
        actual_payload_len = len(raw_data) - HEADER_SIZE
        if actual_payload_len != payload_len:
            return False, "Payload length mismatch", None, None

        # Verify heartbeat size.
        if packet_type == PACKET_TYPE_HEARTBEAT and payload_len != HEARTBEAT_PAYLOAD_SIZE:
            return False, "Invalid heartbeat payload size", None, None

        # Verify telemetry size.
        if packet_type == PACKET_TYPE_TELEMETRY and payload_len != TELEMETRY_PAYLOAD_SIZE:
            return False, "Invalid telemetry payload size", None, None

        # Check session. Handle UAV reboot.
        if session_id != self.current_session:
            self.current_session = session_id
            self.highest_seq = -1
            self.seen_seqs.clear()

        # Check spoofing gap. Reject massive sequence jumps.
        if self.highest_seq != -1 and sequence_id > self.highest_seq + MAX_SEQ_GAP:
            return False, "Sequence gap too large", None, None

        # Check replay. Reject old packets.
        if sequence_id < self.highest_seq - self.window_size:
            return False, "Packet too old", None, None

        # Check duplicate. Prevent replay attacks.
        if sequence_id in self.seen_seqs:
            return False, "Duplicate packet", None, None

        # Update state. Track highest sequence.
        if sequence_id > self.highest_seq:
            self.highest_seq = sequence_id

        # Mark seen. Auto-bounds memory via maxlen.
        self.seen_seqs.append(sequence_id)

        # Build header dict.
        header_dict = {
            "magic": magic,
            "version": version,
            "packet_type": packet_type,
            "session_id": session_id,
            "sequence_id": sequence_id,
            "payload_len": payload_len
        }

        # Extract payload. Leave raw for next stage.
        payload = raw_data[HEADER_SIZE:]

        return True, "Valid", header_dict, payload