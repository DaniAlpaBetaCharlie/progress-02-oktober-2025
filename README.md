# Cara Publish Website (Static Frontend)
Website ini hanya melakukan `fetch` ke endpoint `http://<IP-ESP>/status` yang disediakan oleh program ESP8266 kamu.

## Langkah Cepat
1. Buka file `index.html` dan **tidak perlu** mengubah apa pun selain mengisi IP ESP di kolom pada halaman saat dijalankan.
2. Tes lokal: klik dua kali `index.html` untuk membukanya di browser (Chrome/Edge/Firefox).
3. Publish:
   - **GitHub Pages** (tanpa server):
     - Buat repository baru → upload `index.html`.
     - Settings → Pages → Deploy from **main** / **root**.
     - Tunggu URL aktif, lalu buka.
   - **Netlify**:
     - Drag-and-drop `index.html` ke dashboard Netlify.
     - Dapatkan URL publik otomatis.

> Catatan: Karena ini adalah halaman statis, pastikan laptop/HP yang membuka website **berada pada jaringan yang sama** dengan ESP8266 (atau buat port forwarding/VPN bila perlu).

## Bukti yang Disarankan
- **Screenshot kode** di VS Code.
- **Screenshot tampilan website** di browser ketika berhasil membaca data.
- **URL** halaman (GitHub Pages/Netlify) setelah publish.

