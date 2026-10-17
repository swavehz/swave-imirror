# swave-imirror

AirPlay Screen Mirroring receiver for Windows 10 and 11, built with C++ and Qt6.

## 🚀 Download & Jalankan Langsung (Tanpa Perlu Build)

Untuk pengguna yang ingin langsung memakai aplikasi tanpa compile / build sendiri:

1. Buka halaman **[Releases](https://github.com/swavehz/swave-imirror/releases/latest)**.
2. Download paket portable: **`swave-imirror-v1.0.0-windows-x64-portable.zip`**.
3. Extract file `.zip` tersebut.
4. Dobel klik **`swave-imirror.exe`** untuk menjalankan.
5. Pada iPhone / iPad Anda:
   - Buka **Control Center**.
   - Tap ikon **Screen Mirroring / Pencerminan Layar**.
   - Pilih **`swave-imirror`**.

---

## Fitur Utama

- **Zero-Config Portable**: Tidak perlu install dependensi tambahan, seluruh runtime Qt6, GStreamer, dan mDNS sudah terintegrasi.
- **60 FPS Support**: Opsi target frame rate halus (30 FPS / 60 FPS).
- **Latency Mode Selector**: Pilihan profil latensi (Stable, Balanced, Low Latency).
- **Modern Dark UI**: Tampilan antarmuka native bergaya dark mode dengan kontrol konfigurasi langsung.
- **System Tray Integration**: Berjalan ringan di latar belakang dengan tray icon dan notifikasi status.
- **D3D11 & D3D12 Renderer**: Akselerasi render hardware Windows.
- **Bluetooth LE Beacon**: Membantu deteksi instan oleh iOS di jaringan lokal.

---

## Build Sendiri (Developer / Advanced)

Jika Anda ingin melakukan kompilasi dari source code:

### Prasyarat
- Windows 10 atau Windows 11 (x64 / ARM64)
- [MSYS2](https://www.msys2.org/) (lingkungan UCRT64)
- Qt 6 (Core, Gui, Widgets)
- GStreamer 1.0 runtime & development package
- Python 3.11+ (untuk WinRT Bluetooth beacon)

### Langkah Build
```powershell
# Build dan buat bundle portable x64
.\build.ps1 package -Architecture x64 -SkipInstaller
```
File executable dan bundle portable akan dihasilkan di folder `out/x64/artifacts/`.

---

## Konfigurasi

File konfigurasi runtime disimpan otomatis di:
```text
%APPDATA%\Swave\swave-imirror\arguments.txt
```

---

## License

Lihat [LICENSE](./docs/LICENSE.rtf) untuk informasi lisensi.
