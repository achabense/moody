#pragma once

#include "rule.hpp"

namespace legacy {
    // TODO: regex...
    class compress {
        array<uint8_t, 64> bits{}; // as bitset.
    public:
        explicit compress(const ruleT& rule) : bits() {
            for (int i = 0; i < 512; ++i) {
                set(i, rule[i]);
            }
        }

        explicit operator ruleT() const {
            ruleT rule{};
            for (int i = 0; i < 512; ++i) {
                rule[i] = get(i);
            }
            return rule;
        }

        explicit operator string() const {
            string str;
            for (int wpos = 0; wpos < 64; ++wpos) {
                uint8_t word = bits[wpos];
                str.push_back(to_hex(word & 0b1111)); // low.
                str.push_back(to_hex(word >> 4));     // high.
            }
            assert(str.size() == 128);
            return str;
        }

        explicit compress(const string& str) : bits() {
            if (str.size() < 128) {
                throw std::invalid_argument(str);
            }
            int s = 0;
            for (int wpos = 0; wpos < 64; ++wpos) {
                uint8_t low = from_hex(str[s++], str);
                uint8_t high = from_hex(str[s++], str);
                bits[wpos] = low | (high << 4);
            }
        }

        friend bool operator==(const compress& l, const compress& r) {
            return l.bits == r.bits;
        }

        friend bool operator<(const compress& l, const compress& r) {
            return l.bits < r.bits;
        }

        size_t hash() const {
            // it's ridiculous there is no std::hash<byte-array>...
            string as_bytes(bits.begin(), bits.end());
            return std::hash<string>{}(as_bytes);
        }

    private:
        bool get(int i) const {
            assert(i >= 0 && i < 512);
            int wpos = i / 8, bpos = i % 8;
            return (bits[wpos] >> bpos) & 0b1;
        }

        void set(int i, bool b) {
            assert(i >= 0 && i < 512);
            int wpos = i / 8, bpos = i % 8;
            bits[wpos] |= uint8_t(b) << bpos;
        }

        static char to_hex(uint8_t half) {
            assert(half < 16);
            return "0123456789abcdef"[half];
        }

        static uint8_t from_hex(char ch, const string& source_str) {
            if (ch >= '0' && ch <= '9') {
                return ch - '0';
            } else if (ch >= 'a' && ch <= 'f') {
                return 10 + ch - 'a';
            } else if (ch >= 'A' && ch <= 'F') /* legacy */ {
                return 10 + ch - 'A';
            } else {
                throw std::invalid_argument(source_str);
            }
        }
    };

    inline string to_string(const compress& cmpr) {
        return string(cmpr);
    }

    inline string to_string(const ruleT& rule) {
        return string(compress(rule));
    }

    // TODO: unreliable...
    template <class T> inline T from_string(const string&) = delete;

    template <> inline compress from_string<compress>(const string& str) {
        return compress(str);
    }

    template <> inline ruleT from_string<ruleT>(const string& str) {
        return ruleT(compress(str));
    }
} // namespace legacy

template <> struct std::hash<legacy::compress> {
    size_t operator()(const legacy::compress& cmpr) const {
        return cmpr.hash();
    }
};
