#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

#include <cstdint>
#include <array>
#include <cstring>

namespace hft {
namespace networking {

/**
 * Pre-Serialized Order Template with Incremental Checksumming
 */
template<size_t PayloadSize = 64>
class OrderTemplate {
public:
    struct alignas(64) PacketBuffer {
        uint8_t eth_dst[6];
        uint8_t eth_src[6];
        uint16_t eth_type;
        uint8_t ip_ver_ihl;
        uint8_t ip_tos;
        uint16_t ip_len;
        uint16_t ip_id;
        uint16_t ip_frag;
        uint8_t ip_ttl;
        uint8_t ip_proto;
        uint16_t ip_csum;
        uint32_t ip_src;
        uint32_t ip_dst;
        uint16_t udp_src;
        uint16_t udp_dst;
        uint16_t udp_len;
        uint16_t udp_csum;
        uint8_t payload[PayloadSize];
    };

    OrderTemplate() : price_offset_(0), qty_offset_(0) {
        std::memset(&buffer_, 0, sizeof(buffer_));
    }

    void init(uint32_t src_ip, uint32_t dst_ip, uint16_t src_port, uint16_t dst_port) {
        buffer_.ip_ver_ihl = 0x45;
        buffer_.ip_proto = 17;
        buffer_.ip_src = src_ip;
        buffer_.ip_dst = dst_ip;
        buffer_.udp_src = src_port;
        buffer_.udp_dst = dst_port;
        price_offset_ = 42 + 0;
        qty_offset_ = 42 + 8;
    }

    inline const uint8_t* fast_prepare(double new_price, uint32_t new_qty, size_t& len) {
        uint64_t old_price_bits = *reinterpret_cast<uint64_t*>(&buffer_.payload[0]);
        uint64_t new_price_bits;
        std::memcpy(&new_price_bits, &new_price, 8);
        
        if (old_price_bits != new_price_bits) {
            *reinterpret_cast<double*>(&buffer_.payload[0]) = new_price;
            update_csum_word(0, (uint16_t)old_price_bits, (uint16_t)new_price_bits);
            update_csum_word(2, (uint16_t)(old_price_bits>>16), (uint16_t)(new_price_bits>>16));
            update_csum_word(4, (uint16_t)(old_price_bits>>32), (uint16_t)(new_price_bits>>32));
            update_csum_word(6, (uint16_t)(old_price_bits>>48), (uint16_t)(new_price_bits>>48));
        }

        len = sizeof(PacketBuffer);
        return reinterpret_cast<uint8_t*>(&buffer_);
    }

private:
    PacketBuffer buffer_;
    size_t price_offset_;
    size_t qty_offset_;

    inline void update_csum_word(size_t byte_offset_relative, uint16_t old_val, uint16_t new_val) {
        uint32_t sum = (~buffer_.udp_csum & 0xFFFF);
        sum += (~old_val & 0xFFFF);
        sum += new_val;
        while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
        buffer_.udp_csum = ~sum;
    }
};

/**
 * Kernel Convenience Wrapper
 */
class PreserializedOrderTemplates {
public:
    inline void prepare_buy(double price, uint64_t qty, char** out_pkt, size_t* out_len) {
        size_t len = 0;
        const uint8_t* pkt = buy_tpl_.fast_prepare(price, (uint32_t)qty, len);
        *out_pkt = (char*)pkt;
        *out_len = len;
    }

private:
    OrderTemplate<64> buy_tpl_;
};

} // namespace networking
} // namespace hft