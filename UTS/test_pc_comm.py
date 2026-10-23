#!/usr/bin/env python3
"""
Script Pengujian Komunikasi PC <-> STM32F407 Discovery
Menguji:
1. Penerimaan data streaming ADC 3-Channel (7-byte packet)
2. Penerimaan event Push Button PA0 (7-byte packet)
3. Pengiriman paket data ke STM32 dengan validasi Checksum
"""

import sys
import time
import serial

SERIAL_PORT = "/dev/ttyUSB0"  # Sesuaikan dengan port serial Anda (misal: COM3 di Windows)
BAUDRATE = 115200
PACKET_SIZE = 7

def calculate_checksum(data_bytes):
    """Menghitung penjumlahan modulo 256 dari array byte"""
    return sum(data_bytes) & 0xFF

def parse_packet(packet):
    if len(packet) != PACKET_SIZE:
        return

    computed_csum = calculate_checksum(packet[:6])
    packet_csum = packet[6]

    if computed_csum != packet_csum:
        print(f"[ERROR] Checksum Mismatch! Got: 0x{packet_csum:02X}, Expected: 0x{computed_csum:02X}")
        return

    # Cek tipe paket berdasarkan Header / Pola
    if packet[0] == 0xAA:
        # Paket Push Button
        btn_state = packet[1]
        btn_count = int.from_bytes(packet[2:6], byteorder='big')
        print(f"[BUTTON EVENT] Status: {'PRESSED' if btn_state == 1 else 'RELEASED'} | Counter: {btn_count}")
    else:
        # Paket ADC 3-Channel
        ch1 = (packet[0] << 8) | packet[1]
        ch2 = (packet[2] << 8) | packet[3]
        ch3 = (packet[4] << 8) | packet[5]
        v1 = (ch1 / 4095.0) * 3.0
        v2 = (ch2 / 4095.0) * 3.0
        v3 = (ch3 / 4095.0) * 3.0
        print(f"[ADC STREAM] CH1: {ch1:4d} ({v1:.2f}V) | CH2: {ch2:4d} ({v2:.2f}V) | CH3: {ch3:4d} ({v3:.2f}V)")

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else SERIAL_PORT
    print(f"Membuka port serial {port} pada {BAUDRATE} baud...")

    try:
        ser = serial.Serial(port, BAUDRATE, timeout=1.0)
    except Exception as e:
        print(f"Gagal membuka port {port}: {e}")
        print("Tip: Jalankan dengan argumen port, misal: python3 test_comm.py /dev/ttyUSB0 atau python test_comm.py COM3")
        return

    print("Terhubung! Mendengarkan transmisi paket STM32...")
    last_tx_time = time.time()
    tx_counter = 0

    try:
        while True:
            # Baca data jika ada paket 7 byte yang masuk dari STM32
            if ser.in_waiting >= PACKET_SIZE:
                raw_pkt = ser.read(PACKET_SIZE)
                parse_packet(raw_pkt)

            # Kirim paket periodik dari PC ke STM32 setiap 2 detik (Tugas 3)
            current_time = time.time()
            if current_time - last_tx_time >= 2.0:
                last_tx_time = current_time
                tx_counter += 1

                # Siapkan 6 byte data
                tx_payload = [0x55, 0x01, (tx_counter >> 8) & 0xFF, tx_counter & 0xFF, 0x12, 0x34]
                # Hitung checksum byte ke-6
                csum = calculate_checksum(tx_payload)
                tx_packet = bytes(tx_payload + [csum])

                ser.write(tx_packet)
                print(f"[PC -> STM32] Mengirim paket valid #{tx_counter}: {[f'0x{b:02X}' for b in tx_packet]}")

    except KeyboardInterrupt:
        print("\nProgram dihentikan oleh pengguna.")
    finally:
        ser.close()

if __name__ == "__main__":
    main()
