import sys
from PyQt6.QtWidgets import QApplication

# Yazdığımız modülleri içeri aktarıyoruz
from database.db_manager import DatabaseManager
from auth.authenticator import Authenticator
from ui.login_window import LoginWindow

def main():
    # 1. PyQt Uygulamasını Başlat
    app = QApplication(sys.argv)
    
    # 2. Veritabanı Köprüsünü (Backend) Kur ve Bağlan
    db = DatabaseManager()
    db.connect()
    
    try:
        # 3. Güvenlik Beynini (Middle Tier) Çağır ve İçine Veritabanını Koy
        auth = Authenticator(db)
        
        # 4. Arayüzü (Frontend) Çiz ve İçine Güvenlik Beynini Koy
        window = LoginWindow(auth)
        window.show()
        
        # 5. Sistemi Döngüye Sok (Kapanana kadar bekle)
        sys.exit(app.exec())
        
    finally:
        # Uygulama çarpılsa bile çıkışta veritabanı bağlantısını güvenlice kes!
        print("Sistem kapanıyor, veritabanı bağlantısı kesiliyor...")
        db.disconnect()

if __name__ == '__main__':
    main()