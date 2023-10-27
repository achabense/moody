#pragma once

#include <regex>

#include "rule.hpp"

namespace legacy {
    namespace _impl_details {
        inline uint8_t get_half(const ruleT& rule, int i) {
            assert(i >= 0 && i < 128);
            i *= 4;
            // ~ bool is implicitly promoted to int.
            return (rule[i] << 0) | (rule[i + 1] << 1) | (rule[i + 2] << 2) | (rule[i + 3] << 3);
        }

        inline void put_half(ruleT& rule, int i, uint8_t half) {
            assert(i >= 0 && i < 128);
            i *= 4;
            rule[i] = (half >> 0) & 1;
            rule[i + 1] = (half >> 1) & 1;
            rule[i + 2] = (half >> 2) & 1;
            rule[i + 3] = (half >> 3) & 1;
        }

        // TODO: reconsider using A-F instead...
        inline char to_hex(uint8_t half) {
            assert(half < 16);
            return "0123456789abcdef"[half];
        }

        inline uint8_t from_hex(char ch) {
            if (ch >= '0' && ch <= '9') {
                return ch - '0';
            } else if (ch >= 'a' && ch <= 'f') {
                return 10 + ch - 'a';
            } else /* for backward compat */ {
                assert(ch >= 'A' && ch <= 'F');
                return 10 + ch - 'A';
            }
        }
    } // namespace _impl_details

    // TODO: support "to_str(rule,char*)" when needed...
    inline string to_string(const ruleT& rule) {
        string str(128, '\0');
        for (int i = 0; i < 128; ++i) {
            str[i] = _impl_details::to_hex(_impl_details::get_half(rule, i));
        }
        assert(str[128] == '\0'); // ~ should be guaranteed by str's ctor.
        return str;
    }

    // TODO: rename; inconvenient to use...
    inline const std::regex& rulestr_regex() {
        static std::regex reg("[0-9a-zA-Z]{128}", std::regex_constants::optimize);
        return reg;
    }

    // TODO: support string_view version when needed...
    inline ruleT to_rule(const string& str) {
        assert(std::regex_match(str, rulestr_regex()));
        ruleT rule{};
        for (int i = 0; i < 128; ++i) {
            _impl_details::put_half(rule, i, _impl_details::from_hex(str[i]));
        }
        return rule;
    }

    class compressT {
        array<uint8_t, 64> bits; // as bitset.
    public:
        explicit compressT(const ruleT& rule) : bits{} {
            for (int w = 0; w < 64; ++w) {
                uint8_t low = _impl_details::get_half(rule, w * 2);
                uint8_t high = _impl_details::get_half(rule, w * 2 + 1);
                bits[w] = low | high << 4;
            }
        }

        explicit operator ruleT() const {
            ruleT rule{};
            for (int w = 0; w < 64; ++w) {
                _impl_details::put_half(rule, w * 2, bits[w] & 0b1111); // low
                _impl_details::put_half(rule, w * 2 + 1, bits[w] >> 4); // high
            }
            return rule;
        }

        explicit compressT(const string& str) : bits{} {
            assert(std::regex_match(str, rulestr_regex()));
            for (int w = 0; w < 64; ++w) {
                uint8_t low = _impl_details::from_hex(str[w * 2]);
                uint8_t high = _impl_details::from_hex(str[w * 2 + 1]);
                bits[w] = low | high << 4;
            }
        }

        // TODO: support "operator string()" when needed...

        friend bool operator==(const compressT& l, const compressT& r) {
            return l.bits == r.bits;
        }

        friend bool operator<(const compressT& l, const compressT& r) {
            return l.bits < r.bits;
        }

        size_t hash() const {
            // it's ridiculous there is no std::hash<byte-array>...
            string as_bytes(bits.begin(), bits.end());
            return std::hash<string>{}(as_bytes);
        }
    };
} // namespace legacy

template <> struct std::hash<legacy::compressT> {
    size_t operator()(const legacy::compressT& cmpr) const {
        return cmpr.hash();
    }
};
