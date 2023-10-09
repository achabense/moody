#include <algorithm>
#include <cassert>
#include <numeric>
// #include <span>

#include "rule.hpp"

// SET: if up at a button...

namespace legacy {
    class rule_setter : private array<bool, sym.k> /* NOT sruleT*/ {
        using array_base = array<bool, sym.k>;

        // partition of [0, ..., sym.k)
        vector<vector<int>> m_groups;
        vector<int> m_inactive;

    public:
        rule_setter() : array_base{}, m_inactive(sym.k) {
            for (int scode = 0; scode < sym.k; ++scode) {
                m_inactive[scode] = scode;
            }
        }

        bool* data() {
            return array_base::data();
        }

        int size() {
            return m_groups.size();
        }

        bool* begin() {
            return array_base::data();
        }

        bool* end() {
            return data() + size();
        }

        bool& at(int i) {
            assert(i >= 0 && i < size());
            return array_base::at(i);
        }

        void set_n(int n) {
            std::fill_n(data(), size(), false);
            std::fill_n(data(), std::clamp(n, 0, size()), true);
        }

        void inc() {
            bool* pos = begin();
            while (pos != end() && *pos == true) {
                *pos++ = false;
            }
            if (pos != end()) {
                *pos = true;
            }
        }

        void dec() {
            bool* pos = begin();
            while (pos != end() && *pos == false) {
                *pos++ = true;
            }
            if (pos != end()) {
                *pos = true;
            }
        }

        void next_perm() {
            std::next_permutation(begin(), end());
        }

        void prev_perm() {
            std::prev_permutation(begin(), end());
        }

        void shuffle(auto&& rand) {
            std::shuffle(begin(), end(), rand);
        }

        sruleT set(const sruleT& srule) {
            sruleT ret{srule};
            for (int i = 0; i < size(); ++i) {
                bool v = at(i);
                for (auto s : m_groups[i]) {
                    ret[s] = v;
                }
            }
            return ret;
        }

        // TODO: transfer code between groups...
    };

} // namespace legacy
