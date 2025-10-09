#include <stdio.h>
#include <stdint.h>
#include <string.h>

// =================== CÁC THAM SỐ CƠ BẢN ===================
#define KEY_BYTES 16       
#define IV_BYTES 12       
#define STATE_BYTES 16      
#define WARMUP 256          
// =================== CẤU TRÚC LƯU TRẠNG THÁI ===================
typedef struct {
    uint8_t lfsr[STATE_BYTES];  // Linear Feedback Shift Register
    uint8_t nfsr[STATE_BYTES];  // Nonlinear Feedback Shift Register
} grain128_ctx;


// Lấy bit thứ i (LSB-first)
static inline uint8_t getbit(const uint8_t *arr, int i) {
    return (arr[i/8] >> (i%8)) & 1;
}

// Gán giá trị bit thứ i bằng b (0 hoặc 1)
static inline void setbit(uint8_t *arr, int i, uint8_t b) {
    if (b) arr[i/8] |= (1u << (i%8));
    else   arr[i/8] &= ~(1u << (i%8));
}

// Dịch trái toàn bộ mảng arr (128 bit) và chèn newbit vào cuối
static void shift_left_insert(uint8_t *arr, int nbits, uint8_t newbit) {
    for (int i = 0; i < nbits - 1; ++i) {
        uint8_t v = getbit(arr, i + 1);
        setbit(arr, i, v);
    }
    setbit(arr, nbits - 1, newbit & 1);
}

// =================== CÁC HÀM HỒI TIẾP (FEEDBACK FUNCTIONS) ===================

// LFSR feedback (tuyến tính)
static uint8_t lfsr_feedback(const grain128_ctx *ctx, uint8_t z, int init) {
    // f = XOR của các vị trí cố định trong LFSR
    uint8_t f = getbit(ctx->lfsr,0) ^ getbit(ctx->lfsr,7) ^ getbit(ctx->lfsr,38) ^
                getbit(ctx->lfsr,70) ^ getbit(ctx->lfsr,81) ^ getbit(ctx->lfsr,96);
    // Trong giai đoạn khởi tạo (init=1), f thêm cả z để trộn mạnh hơn
    if (init) f ^= z;
    return f & 1;
}

// NFSR feedback (phi tuyến)
static uint8_t nfsr_feedback(const grain128_ctx *ctx, uint8_t z, int init) {
    // Bắt đầu với XOR các vị trí bit cố định
    uint8_t t = getbit(ctx->nfsr,0) ^ getbit(ctx->nfsr,26) ^ getbit(ctx->nfsr,56) ^
                getbit(ctx->nfsr,91) ^ getbit(ctx->nfsr,96) ^ getbit(ctx->lfsr,0);

    // Thêm các tổ hợp AND phi tuyến
    t ^= getbit(ctx->nfsr,3) & getbit(ctx->nfsr,17);
    t ^= getbit(ctx->nfsr,67) & getbit(ctx->nfsr,80);
    t ^= getbit(ctx->nfsr,11) & getbit(ctx->nfsr,13) & getbit(ctx->nfsr,15);
    t ^= getbit(ctx->nfsr,17) & getbit(ctx->nfsr,18) & getbit(ctx->nfsr,27) & getbit(ctx->nfsr,59);

    // Trong giai đoạn khởi tạo, XOR thêm với z
    if (init) t ^= z;

    return t & 1;
}

// =================== HÀM LỌC PHI TUYẾN (FILTER FUNCTION h(x)) ===================
static uint8_t filter_h(const grain128_ctx *ctx) {
    // Hàm h kết hợp một vài bit của LFSR và NFSR để tạo ra bit đầu ra
    uint8_t h = (getbit(ctx->lfsr,3) & getbit(ctx->lfsr,25)) ^ getbit(ctx->lfsr,46) ^ getbit(ctx->lfsr,64);
    h ^= (getbit(ctx->lfsr,89) & getbit(ctx->nfsr,90));
    h ^= (getbit(ctx->lfsr,93) & getbit(ctx->nfsr,2) & getbit(ctx->nfsr,95));
    return h & 1;
}

// =================== HÀM SINH 1 BIT KEYSSTREAM (Z) ===================
static uint8_t preoutput_z(const grain128_ctx *ctx) {
    uint8_t h = filter_h(ctx);
    // z = h XOR nfsr[0] XOR nfsr[63]
    return h ^ getbit(ctx->nfsr,0) ^ getbit(ctx->nfsr,63);
}

// =================== HÀM CHẠY 1 VÒNG CLOCK ===================
static uint8_t grain_clock(grain128_ctx *ctx, int init) {
    // 1. Sinh ra bit z (bit keystream)
    uint8_t z = preoutput_z(ctx);

    // 2. Tính feedback mới cho LFSR và NFSR
    uint8_t lfb = lfsr_feedback(ctx, z, init);
    uint8_t nfb = nfsr_feedback(ctx, z, init);

    // 3. Dịch trái cả 2 thanh ghi và chèn bit mới vào cuối
    shift_left_insert(ctx->lfsr, 128, lfb);
    shift_left_insert(ctx->nfsr, 128, nfb);

    // 4. Trả về z (bit keystream)
    return z;
}

// =================== KHỞI TẠO GRAIN-128 ===================
void grain_init(grain128_ctx *ctx, const uint8_t key[KEY_BYTES], const uint8_t iv[IV_BYTES]) {
    memset(ctx, 0, sizeof(*ctx));  // reset tất cả về 0

    // Nạp key (128 bit) vào NFSR
    for (int i = 0; i < 128; ++i)
        setbit(ctx->nfsr, i, getbit(key, i));

    // Nạp IV (96 bit) vào LFSR
    for (int i = 0; i < 96; ++i)
        setbit(ctx->lfsr, i, getbit(iv, i));

    // Theo chuẩn: bit 96..126 = 1, bit 127 = 0
    for (int i = 96; i <= 126; ++i)
        setbit(ctx->lfsr, i, 1);
    setbit(ctx->lfsr, 127, 0);

    // Chạy warmup 256 vòng để trộn key + iv
    for (int i = 0; i < WARMUP; ++i)
        (void)grain_clock(ctx, 1);
}

// =================== SINH KEYSSTREAM DẠNG BYTE ===================
void grain_keystream_bytes(grain128_ctx *ctx, uint8_t *out, size_t nbytes) {
    memset(out, 0, nbytes);  // reset output
    size_t nbits = nbytes * 8;

    // Lặp lại để sinh ra nbits bit keystream
    for (size_t bit = 0; bit < nbits; ++bit) {
        // Mỗi vòng gọi grain_clock(ctx,0) => sinh 1 bit keystream thực
        uint8_t z = grain_clock(ctx, 0);
        // Ghép 8 bit thành 1 byte (LSB trước)
        out[bit/8] |= (z & 1) << (bit % 8);
    }
}

// =================== HÀM IN HEX ===================
static void print_hex(const char *label, const uint8_t *buf, size_t n) {
    printf("%-12s", label);
    for (size_t i = 0; i < n; ++i)
        printf("%02X ", buf[i]);
    printf("\n");
}

// =================== HÀM MAIN ===================
int main(void) {
    grain128_ctx ctx;

    // Khóa 128-bit
    uint8_t key[KEY_BYTES] = {
        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
        0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F
    };

    // IV 96-bit
    uint8_t iv[IV_BYTES] = {
        0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
        0x08,0x09,0x0A,0x0B
    };

    // Chuỗi plaintext: "lythuyetmatma"
    const char *pt_str = "lythuyetmatma";
    size_t pt_len = strlen(pt_str);

    uint8_t plaintext[64] = {0};
    memcpy(plaintext, pt_str, pt_len);  // copy chuỗi ASCII vào mảng byte

    uint8_t ks[64] = {0};  // Keystream
    uint8_t ct[64] = {0};  // Ciphertext

    // --- BẮT ĐẦU QUÁ TRÌNH MÃ HÓA ---
    grain_init(&ctx, key, iv);               // Khởi tạo hệ thống với key + iv
    grain_keystream_bytes(&ctx, ks, pt_len); // Sinh keystream dài bằng plaintext

    // Mã hóa: XOR từng byte plaintext với keystream
    for (size_t i = 0; i < pt_len; ++i)
        ct[i] = plaintext[i] ^ ks[i];

    // --- IN KẾT QUẢ ---
    putchar('\n');
    print_hex("Key:", key, KEY_BYTES);
    print_hex("IV:", iv, IV_BYTES);
    print_hex("Plaintext:", plaintext, pt_len);
    print_hex("Keystream:", ks, pt_len);
    print_hex("Ciphertext:", ct, pt_len);
    putchar('\n');

    return 0;
}
