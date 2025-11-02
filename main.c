#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include "grain128.h"

// === Các hàm tiện ích cho việc test ===

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

// Hàm trợ giúp test: lấy N byte keystream
static void ks_bytes(const uint8_t K[16], const uint8_t IV[12], uint8_t *out, size_t n){
    grain128_state st; grain128_init(&st,K,IV);
    grain128_keystream_bytes(&st,out,n);
}

// Hàm trợ giúp test: so sánh và in kết quả
static int expect_eq(const char* name, const uint8_t* got, const uint8_t* exp, size_t n){
    int ok = (memcmp(got,exp,n)==0);
    printf("%s: %s", name, ok?"OK":"FAIL");
    if(!ok){ printf(" (got="); print_hex(got,n); printf(", exp="); print_hex(exp,n); printf(")"); }
    printf("\n"); return ok;
}

// --- KAT fixtures ---
static const uint8_t KAT1_K[16] = {0};               // K = 16B zero
static const uint8_t KAT1_IV[12] = {0};              // IV = 12B zero
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
            assert(len <= 256); // Đảm bảo không tràn buffer
            
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

    // ===== Test 2: KAT2 — Khóa/IV tăng dần (chuỗi hex trong mã nguồn) =====
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
        uint8_t pt[16];  
        uint8_t ct[16];
        uint8_t ks[16]; // buffer cho keystream
        memcpy(pt, message_str, message_len);

        // Tạo keystream để hiển thị 
        ks_bytes(K, IV, ks, message_len);

        // Mã hóa message bằng hàm encrypt 
        grain128_encrypt(K, IV, pt, ct, message_len);

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

    // ===== Test 5: Giải mã ciphertext từ Test 4 và kiểm tra =====
    {
        const char* custom_K_hex = "0123456789abcdef123456789abcdef0";
        const char* custom_IV_hex = "0123456789abcdef12345678";
        uint8_t K[16], IV[12];
        hex_to_bytes(custom_K_hex, K, 16);
        hex_to_bytes(custom_IV_hex, IV, 12);

    
        const uint8_t test4_ct[4] = {0xDB, 0xD0, 0xC9, 0xCB};
        const uint8_t expected_pt[4] = {'t','e','s','t'};
        uint8_t rec[4];

        
        uint8_t ks[4]; ks_bytes(K, IV, ks, 4);
        // Giải mã
        grain128_decrypt(K, IV, test4_ct, rec, 4);

        printf("\n--- TEST5: Full decrypt output for Test4 ---\n");
        printf("Key: %s\n", custom_K_hex);
        printf("IV : %s\n", custom_IV_hex);
        printf("Ciphertext (HEX): "); print_hex(test4_ct, 4); printf("\n");
        printf("Keystream (HEX):  "); print_hex(ks, 4); printf("\n");
        printf("Recovered PT (HEX): "); print_hex(rec, 4); printf("\n");
        printf("Recovered PT (ASCII): "); for (int i=0;i<4;i++) putchar((rec[i]>=32&&rec[i]<127)?rec[i]:'.'); printf("\n");

        pass_all &= expect_eq("TEST5 DECRYPT_TEST4", rec, expected_pt, 4);
        printf("-----------------------------------------\n");
    }

    // ===== Test 6: Xuất 1,000,000 bit keystream ra file (dạng '0' '1') =====
    {
        printf("\n--- TEST6: Xuat 1,000,000 keystream bits vao file ---\n");
        
    
        const char* custom_K_hex = "0123456789abcdef123456789abcdef0";
        const char* custom_IV_hex = "0123456789abcdef12345678";
        uint8_t K[16];
        uint8_t IV[12];
        hex_to_bytes(custom_K_hex, K, 16);
        hex_to_bytes(custom_IV_hex, IV, 12);
        
        printf("Using Key: %s\n", custom_K_hex);
        printf("Using IV : %s\n", custom_IV_hex);
        
        const char* filename = "keystream_1M_bits.txt";
        const size_t num_bits = 1048576;
        

        grain128_state st;
        grain128_init(&st, K, IV); // Khởi tạo state

        FILE *f = fopen(filename, "w"); // Mở file để ghi (write)
        if (f == NULL) {
            perror("ERROR: Không thể mở file để ghi");
            pass_all = 0; // Đánh dấu test thất bại
        } else {
            printf("Generating %zu bits to %s \n", num_bits, filename);
            
            // Tối ưu: Ghi vào buffer trước khi flush ra file
            char buffer[4096];
            size_t buf_idx = 0;

            // Vòng lặp chính: chạy 1,000,000 lần
            for (size_t i = 0; i < num_bits; i++) {
                // 1. Lấy bit keystream (z_i)
                uint8_t z = z_preout(&st); 
                
                // 2. Thêm ký tự '1' hoặc '0' vào buffer
                buffer[buf_idx++] = (z & 1u) ? '1' : '0';
                
                // 3. Clock state để chuẩn bị cho bit tiếp theo
                clk_gen(&st); 

                // 4. Nếu buffer đầy, ghi ra file và reset
                if (buf_idx == sizeof(buffer)) {
                    fwrite(buffer, 1, buf_idx, f);
                    buf_idx = 0;
                }
            }
            
            // Ghi nốt phần còn lại trong buffer
            if (buf_idx > 0) {
                fwrite(buffer, 1, buf_idx, f);
            }
            
            fclose(f); // Đóng file
            printf("Xuat file thanh cong: %s\n", filename);
        }
        printf("----------------------------------------------------------\n");
    }

    printf("\n==== SUMMARY: %s ====\n", pass_all?"ALL PASSED":"HAS FAILURES");
    return pass_all ? 0 : 1;
}