import socket

class UDPReceiver:
    def __init__(self, host='0.0.0.0', port=8080):
        # Store connection config.
        self.host = host
        self.port = port
        
        # Init socket state.
        self.sock = None
        self.is_running = False

    def start(self):
        # Prevent multiple starts.
        if self.is_running:
            return

        try:
            # Create UDP socket. IPv4.
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            
            # Allow port reuse. Prevent bind conflicts.
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            
            # Bind to interface. Listen all.
            self.sock.bind((self.host, self.port))
            
            # Set timeout. Prevent blocking forever.
            self.sock.settimeout(1.0)
            
            # Update state.
            self.is_running = True
            print(f"UDP Receiver started on {self.host}:{self.port}")
            
        except OSError as e:
            # Handle creation or bind fail. Clean up partially created socket.
            print(f"Socket bind failed: {e}")
            if self.sock:
                try:
                    self.sock.close()
                except OSError:
                    pass
            self.sock = None
            self.is_running = False

    def stop(self):
        # Flag shutdown. Safe for multiple calls.
        self.is_running = False
        
        # Close socket safely. Clear reference.
        if self.sock:
            try:
                self.sock.close()
            except OSError:
                pass
            self.sock = None

    def listen(self):
        # Verify valid init state before loop.
        if not self.is_running or self.sock is None:
            return

        # Main receive loop.
        while self.is_running:
            # Guard against mid-loop socket destruction.
            if self.sock is None:
                break
                
            try:
                # Read max 1024 bytes.
                data, addr = self.sock.recvfrom(1024)
                
                # Check packet size.
                data_len = len(data)
                
                if data_len == 22:
                    # Unencrypted status.
                    print("Heartbeat received (22 bytes)")
                elif data_len == 82:
                    # Encrypted payload.
                    print("Encrypted Telemetry received (82 bytes). Ready for decryption.")
                else:
                    # Malformed or unexpected.
                    print(f"Invalid packet size: {len(data)} bytes. Dropped. Raw: {data}")
                    
            except socket.timeout:
                # Normal timeout. Continue loop.
                continue
            except OSError as e:
                # Handle network error or closed socket during read.
                if self.is_running:
                    print(f"Socket error: {e}")
                break