#ifndef GRAIN128_H_
#define GRAIN128_H_

#include <stdint.h>
#include <stddef.h> // Dùng cho size_t

/**
 * @brief Cấu trúc state của Grain-128.
 */
typedef struct {
    uint8_t s[128]; // LFSR bits
    uint8_t b[128]; // NFSR bits
} grain128_state;

/**
 * @brief Khởi tạo state của Grain-128 với Key và IV.
 */
void grain128_init(grain128_state* st, const uint8_t key[16], const uint8_t iv[12]);

/**
 * @brief Tạo ra một số byte keystream từ state hiện tại.
 */
void grain128_keystream_bytes(grain128_state* st, uint8_t *out, size_t nbytes);

/**
 * @brief Mã hóa một buffer dữ liệu.
 */
void grain128_encrypt(const uint8_t key[16], const uint8_t iv[12],
                      const uint8_t *in, uint8_t *out, size_t len);

/**
 * @brief Giải mã một buffer dữ liệu.
 */
void grain128_decrypt(const uint8_t key[16], const uint8_t iv[12],
                      const uint8_t *in, uint8_t *out, size_t len);

/*
 * === CÁC HÀM NỘI BỘ (ĐƯỢC TEST 6 SỬ DỤNG) ===
 * Đây là các hàm nội bộ được công khai để main.c có thể chạy Test 6.
 * Thông thường, người dùng thư viện sẽ không gọi trực tiếp các hàm này.
 */

/**
 * @brief (Nội bộ) Lấy bit keystream z_i (chưa clock).
 */
uint8_t z_preout(const grain128_state* st);

/**
 * @brief (Nội bộ) Clock state 1 lần (không có feedback 'y').
 */
void clk_gen(grain128_state* st);


#endif // GRAIN128_H_

