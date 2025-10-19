# Grain-128 (C reference)

Đây là một triển khai tham khảo của bộ mã hóa dòng Grain-128 bằng C.

Nội dung file `Grain128.c` bao gồm:
- Hàm khởi tạo và sinh luồng khóa (keystream) theo spec Grain-128.
- Một bộ test nội bộ (KAT fixtures) và các kiểm tra mẫu in ra kết quả.

## Hướng dẫn build 
Mở terminal  và chạy:

```
gcc -O2 -std=c11 Grain128.c -o Grain128.exe
./Grain128.exe
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



