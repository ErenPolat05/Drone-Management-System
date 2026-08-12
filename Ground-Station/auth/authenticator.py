import bcrypt
import logging

class Authenticator:
    # Pre-computed dummy hash. Timing attack mitigation.
    # Valid bcrypt hash. Cost factor 12.
    DUMMY_HASH = b'$2b$12$LQv3c1yqBWVHxkd0LHAkCOYz6TtxMQJqhN8/LewK.bVbV/f.H/sXm'

    def __init__(self, db_manager):
        # Store injected DB. Decouple auth from DB connection.
        self.db_manager = db_manager

    def login(self, username, plaintext_password):
        # Validate inputs. Prevent empty strings.
        if not username or not plaintext_password:
            return False, None, "Invalid username or password"

        pwd_bytes = plaintext_password.encode('utf-8')
        user_data = None

        try:
            # Fetch user. Catch DB errors.
            user_data = self.db_manager.get_user_by_username(username)
        except Exception:
            # Hide DB error. Proceed to dummy hash.
            logging.error("Auth DB query failed.")

        # Validate user existence and fields.
        if not user_data or not user_data.get('PasswordHash') or not user_data.get('Role'):
            # Missing user or corrupt data. Run dummy check.
            # Normalizes response time. Mitigates timing attacks.
            try:
                bcrypt.checkpw(pwd_bytes, self.DUMMY_HASH)
            except Exception:
                pass
            return False, None, "Invalid username or password"

        try:
            # Encode stored hash. Bcrypt requirement.
            stored_hash = user_data.get('PasswordHash')
            hash_bytes = stored_hash.encode('utf-8')

            # Verify real hash. Crypto check.
            if bcrypt.checkpw(pwd_bytes, hash_bytes):
                # Match. Return role.
                return True, user_data.get('Role'), "Login successful"
            
            # Mismatch. Generic fail.
            return False, None, "Invalid username or password"
                
        except Exception:
            # Handle malformed hashes. Hide details.
            logging.error("Bcrypt check failed.")
            return False, None, "Invalid username or password"