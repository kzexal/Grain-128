#include "grain128.h"
#include <string.h> // Dùng cho memset

// === Các hàm tiện ích nội bộ (static) ===

// --- Bit utils: LSB-first within each byte ---
static inline uint8_t get_bit(const uint8_t *buf, size_t bit_idx){
    size_t byte = bit_idx >> 3; uint8_t off = bit_idx & 7;
    return (buf[byte] >> off) & 1u;
}
static inline void set_bit(uint8_t *buf, size_t bit_idx, uint8_t v){
    size_t byte = bit_idx >> 3; uint8_t off = bit_idx & 7;
    if (v&1u) buf[byte] |= (uint8_t)(1u<<off); else buf[byte] &= (uint8_t)~(1u<<off);
}
static void bytes_to_bits_lsb(const uint8_t*in,size_t inlen,uint8_t*out_bits){
    for (size_t i=0;i<8*inlen;i++) out_bits[i]=get_bit(in,i);
}
static void shift128(uint8_t x[128], uint8_t inbit){
    for (int i=0;i<127;i++) x[i]=x[i+1];
    x[127]=inbit&1u;
}

// --- Feedbacks (Grain-128) ---
static inline uint8_t L_feedback(const grain128_state* st){
    // 1 + x^32 + x^47 + x^58 + x^90 + x^121 + x^128
    // s[0] = x^128, s[7]=x^121, s[38]=x^90, s[70]=x^58, s[81]=x^47, s[96]=x^32
    return st->s[0]^st->s[7]^st->s[38]^st->s[70]^st->s[81]^st->s[96];
}
static inline uint8_t F_feedback(const grain128_state* st){
    // b_{i+128} terms for Grain-128 (not 128a)
    const uint8_t* b=st->b; const uint8_t* s=st->s;
    uint8_t v = s[0] ^ b[0] ^ b[26] ^ b[56] ^ b[91] ^ b[96];
    v ^= (b[3]&b[67]) ^ (b[11]&b[13]) ^ (b[17]&b[18]) ^ (b[27]&b[59]);
    v ^= (b[40]&b[48]) ^ (b[61]&b[65]) ^ (b[68]&b[84]);
    return v & 1u;
}

// --- Clocks (Hàm nội bộ) ---
static inline void clk_init_with_y(grain128_state* st, uint8_t y){
    uint8_t lfb = L_feedback(st) ^ y;
    uint8_t nfb = F_feedback(st) ^ y;
    shift128(st->s, lfb);
    shift128(st->b, nfb);
}

/*
 * === CÁC HÀM "NỘI BỘ" ĐƯỢC CÔNG KHAI CHO TEST 6 ===
 * Đã xóa 'static' để linker có thể thấy chúng.
 */

// Keystream bit z_i for Grain-128
uint8_t z_preout(const grain128_state* st){
    const uint8_t* b=st->b; const uint8_t* s=st->s;
    uint8_t v = b[2]^b[15]^b[36]^b[45]^b[64]^b[73]^b[89]^s[93];
    v ^= (b[12]&b[95]&s[95]) ^ (b[12]&s[8]) ^ (s[13]&s[20]) ^ (b[95]&s[42]) ^ (s[60]&s[79]);
    return v & 1u;
}

// Clock thông thường (dùng khi tạo keystream)
void clk_gen(grain128_state* st){
    uint8_t lfb = L_feedback(st);
    uint8_t nfb = F_feedback(st);
    shift128(st->s, lfb);
    shift128(st->b, nfb);
}


// === Định nghĩa các hàm API (public) ===

void grain128_init(grain128_state* st, const uint8_t key[16], const uint8_t iv[12]){
    memset(st,0,sizeof(*st));
    uint8_t kbits[128], ivbits[96]; bytes_to_bits_lsb(key,16,kbits); bytes_to_bits_lsb(iv,12,ivbits);
    for (int i=0;i<128;i++) st->b[i]=kbits[i]&1u;     // NFSR = key
    for (int i=0;i<96;i++)  st->s[i]=ivbits[i]&1u;    // LFSR = IV || 32*1
    for (int i=96;i<128;i++) st->s[i]=1u;            // padding 0xFFFFFFFF (32 bits of 1)
    
    // 256 init clocks with y feedback
    for (int t=0;t<256;t++){ uint8_t y=z_preout(st); clk_init_with_y(st,y); }
}

void grain128_keystream_bytes(grain128_state* st, uint8_t *out, size_t nbytes){
    memset(out,0,nbytes);
    for (size_t i=0;i<nbytes*8;i++){
        uint8_t z=z_preout(st); set_bit(out,i,z); clk_gen(st);
    }
}

void grain128_encrypt(const uint8_t key[16], const uint8_t iv[12],
                      const uint8_t *in, uint8_t *out, size_t len){
    grain128_state st; grain128_init(&st,key,iv);
    
    // Tối ưu: Tạo keystream theo chunk và XOR
    for (size_t off=0; off<len; ){
        uint8_t ks[64]; // Kích thước chunk có thể điều chỉnh
        size_t chunk = (len-off>64)?64:(len-off);
        
        grain128_keystream_bytes(&st, ks, chunk);
        
        for (size_t i=0;i<chunk;i++) out[off+i]=in[off+i]^ks[i];
        off+=chunk;
    }
}

void grain128_decrypt(const uint8_t key[16], const uint8_t iv[12],
                      const uint8_t *in, uint8_t *out, size_t len){
    // Giải mã của stream cipher giống hệt mã hóa
    grain128_encrypt(key,iv,in,out,len);
}

