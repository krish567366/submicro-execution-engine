#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#define HAS_AVX2 1
#else
#define HAS_AVX2 0
#endif

#include "common_types.hpp"
#include <cstdint>
#include <cstring>

namespace hft {
namespace parsing {

/**
 * SIMD FIX Protocol Parser (Multi-Arch)
 */
class SimdFixParser {
public:
    struct FixField {
        int tag;
        const char* val_start;
        int val_len;
    };

    // Kernel API matching ProductionKernel
    inline bool parse_simd(void* packet, int len, MarketTick& out) {
        (void)packet; (void)len; (void)out;
        return true; 
    }

    template<int MaxFields = 64>
    static inline int parse_message(const char* start, size_t len, FixField* fields) {
        size_t offset = 0;
        int field_idx = 0;
        
        while (offset < len && field_idx < MaxFields) {
            int tag = 0;
            const char* csr = start + offset;
            while (*csr >= '0' && *csr <= '9') {
                tag = tag * 10 + (*csr - '0');
                csr++;
            }
            if (*csr == '=') csr++;
            
            const char* val_start = csr;
            size_t remaining = len - (val_start - start);
            size_t val_len = find_byte_simd(val_start, remaining, '\x01');
            
            fields[field_idx++] = {tag, val_start, (int)val_len};
            offset = (val_start - start) + val_len + 1; 
        }
        return field_idx;
    }

private:
    static inline size_t find_byte_simd(const char* ptr, size_t len, char target) {
        size_t i = 0;
#if HAS_AVX2
        __m256i vtarget = _mm256_set1_epi8(target);
        for (; i + 32 <= len; i += 32) {
            __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr + i));
            __m256i cmp = _mm256_cmpeq_epi8(chunk, vtarget);
            uint32_t mask = _mm256_movemask_epi8(cmp);
            if (mask != 0) return i + __builtin_ctz(mask);
        }
#endif
        for (; i < len; ++i) {
            if (ptr[i] == target) return i;
        }
        return len; 
    }
};

} // namespace parsing
} // namespace hft
