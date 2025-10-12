#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
    uint8_t s[128]; // LFSR bits
    uint8_t b[128]; // NFSR bits
} grain128_state;

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

// Keystream bit z_i for Grain-128
static inline uint8_t z_preout(const grain128_state* st){
    const uint8_t* b=st->b; const uint8_t* s=st->s;
    uint8_t v = b[2]^b[15]^b[36]^b[45]^b[64]^b[73]^b[89]^s[93];
    v ^= (b[12]&b[95]&s[95]) ^ (b[12]&s[8]) ^ (s[13]&s[20]) ^ (b[95]&s[42]) ^ (s[60]&s[79]);
    return v & 1u;
}

// --- Clocks ---
static inline void clk_init_with_y(grain128_state* st, uint8_t y){
    uint8_t lfb = L_feedback(st) ^ y;
    uint8_t nfb = F_feedback(st) ^ y;
    shift128(st->s, lfb);
    shift128(st->b, nfb);
}
static inline void clk_gen(grain128_state* st){
    uint8_t lfb = L_feedback(st);
    uint8_t nfb = F_feedback(st);
    shift128(st->s, lfb);
    shift128(st->b, nfb);
}

// --- API ---
static void grain128_init(grain128_state* st, const uint8_t key[16], const uint8_t iv[12]){
    memset(st,0,sizeof(*st));
    uint8_t kbits[128], ivbits[96]; bytes_to_bits_lsb(key,16,kbits); bytes_to_bits_lsb(iv,12,ivbits);
    for (int i=0;i<128;i++) st->b[i]=kbits[i]&1u;      // NFSR = key
    for (int i=0;i<96;i++)  st->s[i]=ivbits[i]&1u;     // LFSR = IV || 32*1
    for (int i=96;i<128;i++) st->s[i]=1u;              // padding 0xFFFFFFFF (32 bits of 1)
    // 256 init clocks with y feedback
    for (int t=0;t<256;t++){ uint8_t y=z_preout(st); clk_init_with_y(st,y); }
}
static void grain128_keystream_bytes(grain128_state* st, uint8_t *out, size_t nbytes){
    memset(out,0,nbytes);
    for (size_t i=0;i<nbytes*8;i++){
        uint8_t z=z_preout(st); set_bit(out,i,z); clk_gen(st);
    }
}
void grain128_encrypt(const uint8_t key[16], const uint8_t iv[12],
                      const uint8_t *in, uint8_t *out, size_t len){
    grain128_state st; grain128_init(&st,key,iv);
    for (size_t off=0; off<len; ){
        uint8_t ks[64]; size_t chunk = (len-off>64)?64:(len-off);
        grain128_keystream_bytes(&st, ks, chunk);
        for (size_t i=0;i<chunk;i++) out[off+i]=in[off+i]^ks[i];
        off+=chunk;
    }
}
// decrypt = encrypt
void grain128_decrypt(const uint8_t key[16], const uint8_t iv[12],
                      const uint8_t *in, uint8_t *out, size_t len){
    grain128_encrypt(key,iv,in,out,len);
}

// --- simple tests ---
static void hex_to_bytes(const char*hex,uint8_t*out,size_t n){
    for (size_t i=0;i<n;i++){ unsigned v; sscanf(hex+2*i,"%2x",&v); out[i]=(uint8_t)v; }
}
static void print_hex(const uint8_t*b,size_t n){ for(size_t i=0;i<n;i++) printf("%02X",b[i]); }
static void print_binary(const uint8_t* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        for (int j = 7; j >= 0; j--) {
            printf("%d", (b[i] >> j) & 1);
        }
        printf(" ");
    }
}


// --- helpers riêng cho test ---
static int hamming(const uint8_t *a, const uint8_t *b, size_t n){
    int d=0; for (size_t i=0;i<n;i++){ uint8_t x=a[i]^b[i]; d += __builtin_popcount((unsigned)x); } return d;
}
static void fill_seq(uint8_t *p, size_t n, uint8_t start, uint8_t step){
    for (size_t i=0;i<n;i++) p[i] = (uint8_t)(start + step*i);
}
static void ks_bytes(const uint8_t K[16], const uint8_t IV[12], uint8_t *out, size_t n){
    grain128_state st; grain128_init(&st,K,IV);
    grain128_keystream_bytes(&st,out,n);
}
static int expect_eq(const char* name, const uint8_t* got, const uint8_t* exp, size_t n){
    int ok = (memcmp(got,exp,n)==0);
    printf("%s: %s", name, ok?"OK":"FAIL");
    if(!ok){ printf(" (got="); print_hex(got,n); printf(", exp="); print_hex(exp,n); printf(")"); }
    printf("\n"); return ok;
}

// --- KAT fixtures ---
static const uint8_t KAT1_K[16] = {0};              // K = 16B zero
static const uint8_t KAT1_IV[12] = {0};             // IV = 12B zero
// Keystream 16B đầu tiên (LSB-first/byte)
static const uint8_t KAT1_KS16[16] =
    {0xF0,0x9B,0x7B,0xF7,0xD7,0xF6,0xB5,0xC2,0xDE,0x2F,0xFC,0x73,0xAC,0x21,0x39,0x7F};

static const char* KAT2_K_hex = "000102030405060708090A0B0C0D0E0F";
static const char* KAT2_IV_hex= "000102030405060708090A0B";
// msg_i = (i*3+1) mod 256, i=0..23
static const uint8_t KAT2_CT24[24] =
    {0x8B,0x3D,0xBE,0xA4,0x17,0x57,0xB5,0xFC,0x05,0xB0,0x57,0xFB,
     0x54,0x4A,0xB0,0x73,0xEC,0xC0,0x65,0xD9,0xC3,0x7D,0x85,0x10};

// --- main test suite ---
int main(void){
    int pass_all = 1;

    // ===== Test 0: Roundtrip nhiều độ dài & random =====
    {
        srand(12345);
        for (int t=0;t<8;t++){
            size_t len = (size_t)(t*31 + 7); // dải độ dài khác nhau
            uint8_t K[16], IV[12], pt[256], ct[256], rec[256];
            for (int i=0;i<16;i++) K[i]=(uint8_t)(rand());
            for (int i=0;i<12;i++) IV[i]=(uint8_t)(rand());
            for (size_t i=0;i<len;i++) pt[i]=(uint8_t)(rand());

            grain128_encrypt(K,IV,pt,ct,len);
            grain128_decrypt(K,IV,ct,rec,len);
            int ok = (memcmp(pt,rec,len)==0);
            printf("Roundtrip len=%zu: %s\n", len, ok?"OK":"FAIL");
            pass_all &= ok;
        }
    }

    // ===== Test 1: KAT1 — KS 16B đầu cho (K=0, IV=0) =====
    {
        uint8_t ks[16];
        ks_bytes(KAT1_K, KAT1_IV, ks, sizeof(ks));
        pass_all &= expect_eq("KAT1 KS16", ks, KAT1_KS16, sizeof(ks));
    }

    // ===== Test 2: KAT2 — Sample ciphertext 24B (K/IV incremental) =====
    {
        uint8_t K[16], IV[12];
        hex_to_bytes(KAT2_K_hex, K, 16);
        hex_to_bytes(KAT2_IV_hex, IV, 12);
        uint8_t msg[24]; for (int i=0;i<24;i++) msg[i]=(uint8_t)(i*3+1);
        uint8_t ct[24];
        grain128_encrypt(K,IV,msg,ct,24);
        pass_all &= expect_eq("KAT2 CT24", ct, KAT2_CT24, 24);
    }

    
    // ===== Test 3: Keystream cho key/IV theo yêu cầu =====
    {
        const char* custom_K_hex = "0123456789abcdef123456789abcdef0";
        const char* custom_IV_hex = "0123456789abcdef12345678";
        uint8_t K[16], IV[12];
        
        hex_to_bytes(custom_K_hex, K, 16);
        hex_to_bytes(custom_IV_hex, IV, 12);
        
        uint8_t keystream_result[16];
        ks_bytes(K, IV, keystream_result, sizeof(keystream_result));
        
        printf("\n--- Custom Keystream Calculation ---\n");
        printf("Key:                %s\n", custom_K_hex);
        printf("IV:                 %s\n", custom_IV_hex);
        printf("Keystream (16 bytes): ");
        print_hex(keystream_result, sizeof(keystream_result));
        printf("\n------------------------------------\n");
    }

    // ===== Test 4: Mã hóa message "test" =====
    {
        const char* custom_K_hex = "0123456789abcdef123456789abcdef0";
        const char* custom_IV_hex = "0123456789abcdef12345678";
        uint8_t K[16], IV[12];
        hex_to_bytes(custom_K_hex, K, 16);
        hex_to_bytes(custom_IV_hex, IV, 12);

        const char* message_str = "test";
        size_t message_len = strlen(message_str);
        uint8_t pt[16]; // buffer đủ lớn
        uint8_t ct[16];
        uint8_t ks[16]; // buffer cho keystream
        memcpy(pt, message_str, message_len);

        // Tạo keystream để hiển thị
        ks_bytes(K, IV, ks, message_len);
        
        // Mã hóa message bằng cách XOR với keystream
        for (size_t i = 0; i < message_len; i++) {
            ct[i] = pt[i] ^ ks[i];
        }

        printf("\n--- Encrypting 'test' message ---\n");
        printf("Key:                %s\n", custom_K_hex);
        printf("IV:                 %s\n", custom_IV_hex);
        printf("Message:            %s\n", message_str);
        printf("Keystream (HEX):    ");
        print_hex(ks, message_len);
          printf("\n");
          printf("Keystream (BIN):    ");
        print_binary(ks, message_len);
        printf("\n");
        printf("Message (BIN):      ");
        print_binary(pt, message_len);
        printf("\n");
        printf("Ciphertext (BIN):   ");
        print_binary(ct, message_len);
        printf("\n");
        printf("Ciphertext (HEX):   ");
        print_hex(ct, message_len);
        printf("\n-----------------------------------\n");
    }


    printf("\n==== SUMMARY: %s ====\n", pass_all?"ALL PASSED":"HAS FAILURES");
    return pass_all ? 0 : 1;
}

