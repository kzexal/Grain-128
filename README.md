# Grain-128 

Đây là một triển khai tham khảo của bộ mã hóa dòng Grain-128 bằng C, tách biệt giữa thư viện và phần kiểm thử.

Bao gồm 3 file:

grain128.h: Định nghĩa cấu trúc grain128_state và các hàm API.

grain128.c: Triển khai lõi của thuật toán Grain-128 (khởi tạo, clock, sinh keystream).

main.c: Chứa các bộ test (KAT fixtures), các hàm test roundtrip, và hàm main() để chạy toàn bộ kiểm thử.

## Hướng dẫn build 
Mở terminal  và chạy:

```
gcc  main.c grain128.c -o run_grain            
./run_grain
```

## Test vectors 
### Test vectors 1
 KAT1 — Key = 16B zero, IV = 12B zero

- Key : 00000000000000000000000000000000
- IV  : 000000000000000000000000
- Keystream 16 bytes:

```
F0 9B 7B F7 D7 F6 B5 C2 DE 2F FC 73 AC 21 39 7F
```
### Test vectors 2
KAT2 — Khóa/IV tăng dần (chuỗi hex trong mã nguồn)

- Key : 000102030405060708090A0B0C0D0E0F
- IV  : 000102030405060708090A0B
- Plaintext for KAT2: bytes msg_i = (i*3+1) mod 256, i=0..23
- Ciphertext (24 bytes) from source:

```
8B 3D BE A4 17 57 B5 FC 05 B0 57 FB 54 4A B0 73 EC C0 65 D9 C3 7D 85 10
```
### Test vectors 3
Kiểm tra keystream đầy đủ

- Key:
```
0123456789abcdef123456789abcdef0
```
- IV:
 ```
0123456789abcdef12345678
```
- Keystream:
```
afb5babfa8de896b4b9c6acaf7c4fbfd
```
### Test vectors 4 
Mã hoá message "test"
- Key:
```
0123456789abcdef123456789abcdef0
```
- IV:
```
0123456789abcdef12345678
```
- Keystream:
```
AF B5 BA BF A8 DE 89 6B 4B 9C 6A CA F7 C4 FB FD
```
- Plaintext:
```
test
```
- Ciphertext:
```
DB D0 C9 CB
```

### Test vectors 5
Giải mã test vectors 4

- Key:
```
0123456789abcdef123456789abcdef0
```
- IV:
```
0123456789abcdef12345678
```
- Ciphertext (HEX):
```
DB D0 C9 CB
```
- Plaintext: "test"

Test vectors 6

Xuất 1,000,000 bit keystream ra file.

Test này sử dụng Key và IV từ KAT1 (toàn số 0) để đảm bảo tính nhất quán.

Key: 00000000000000000000000000000000

IV: 000000000000000000000000

Hành động: Tạo ra 1,000,000 bit keystream (dưới dạng ký tự '0' và '1').

Đầu ra: Ghi kết quả vào file có tên keystream_1M_bits.txt.

