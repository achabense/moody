#pragma once

// TODO: the utilities provided by "app.hpp" appears unsystematic, and generally too weak.

#include <deque>
#include <format>
#include <random>
#include <span>

#include "partition.hpp"
#include "rule.hpp"
#include "tile.hpp"

// TODO: allow resizing the grid.

// TODO: is the algo correct?
// TODO: explain count's impl-specific behavior...
inline void random_fill_density(bool* begin, bool* end, double density, std::mt19937& rand) {
    const uint32_t c = std::mt19937::max() * std::clamp(density, 0.0, 1.0);
    while (begin < end) {
        *begin++ = rand() < c;
    }
}

inline void random_fill_count(bool* begin, bool* end, int count, std::mt19937& rand) {
    std::fill(begin, end, false);
    std::fill(begin, begin + std::clamp(count, 0, int(end - begin)), true);
    std::shuffle(begin, end, rand);
}

// TODO: rename...
// TODO: explain why not double...
struct tile_filler {
    uint32_t seed; // arbitrary value
    float density; // ∈ [0.0f, 1.0f]
};

// TODO: better name...
inline int round_clip(int v, int r) {
    assert(r > 0);
    return ((v % r) + r) % r;
}

// TODO: better name...
class torus {
    legacy::tileT m_tile, m_side;

public:
    explicit torus(legacy::rectT size) : m_tile(size), m_side(size) {}

    const legacy::tileT& tile() const { return m_tile; }

    // TODO: is passing by optional acceptible?
    // by value or by cref? (also in tileT)
    void fill(tile_filler filler, std::optional<legacy::rectT> resize = {}) {
        if (resize) {
            m_tile.resize(*resize);
        }

        // TODO: count or density?
        std::mt19937 rand{filler.seed};
        random_fill_density(m_tile.begin(), m_tile.end(), filler.density, rand);
    }

    void run(const legacy::ruleT& rule) {
        m_tile.gather().apply(rule, m_side);
        m_tile.swap(m_side);
    }

    // TODO: recheck logic...
    void shift(int dx, int dy) {
        const int width = m_tile.width(), height = m_tile.height();

        dx = round_clip(-dx, width);
        dy = round_clip(-dy, height);
        if (dx == 0 && dy == 0) {
            return;
        }

        m_side.resize(m_tile.size());
        for (int y = 0; y < height; ++y) {
            bool* source = m_tile.line((y + dy) % height);
            bool* dest = m_side.line(y);
            std::copy_n(source, width, dest);
            std::rotate(dest, dest + dx, dest + width);
        }
        m_tile.swap(m_side);
    }
};

// TODO: merge with those in main...
class rule_runner {
    torus m_tile;
    int m_gen = 0;

    int init_shift_x = 0, init_shift_y = 0;

public:
    explicit rule_runner(legacy::rectT size) : m_tile(size) {}

    const legacy::tileT& tile() const { return m_tile.tile(); }
    int gen() const { return m_gen; }

    void restart(tile_filler filler, std::optional<legacy::rectT> resize = {}) {
        m_tile.fill(filler, resize);
        m_tile.shift(init_shift_x, init_shift_y);
        m_gen = 0;
    }

    void run(const legacy::ruleT& rule, int count) {
        for (int i = 0; i < count; ++i) {
            m_tile.run(rule);
            ++m_gen;
        }
    }

    void shift(int dx, int dy) {
        m_tile.shift(dx, dy);
        init_shift_x = round_clip(init_shift_x + dx, tile().width());
        init_shift_y = round_clip(init_shift_x + dy, tile().height());
    }
};

// Never empty.
// TODO: should be able to tell different sources...
class rule_recorder {
    std::vector<legacy::compressT> m_record;
    int m_pos;

public:
    rule_recorder() {
        m_record.emplace_back(legacy::game_of_life());
        m_pos = 0;
    }

    int size() const { return m_record.size(); }

    // [0, size() - 1]
    int pos() const { return m_pos; }

    // TODO: look for better names...
    // TODO: reconsider what should be done when already exists in the whole vec...
    // TODO: next/prev work still poorly with editor...
    void take(const legacy::ruleT& rule) {
        legacy::compressT cmpr(rule);
        if (cmpr != m_record[m_pos]) {
            m_record.push_back(cmpr);
            m_pos = m_record.size() - 1;
        }
    }

    legacy::ruleT current() const {
        assert(m_pos >= 0 && m_pos < size());
        return static_cast<legacy::ruleT>(m_record[m_pos]);
    }

    void set_pos(int pos) { //
        m_pos = std::clamp(pos, 0, size() - 1);
    }
    void next() { set_pos(m_pos + 1); }
    void prev() { set_pos(m_pos - 1); }

    // TODO: reconsider m_pos logic...
    void append(const std::vector<legacy::compressT>& vec) { //
        m_record.insert(m_record.end(), vec.begin(), vec.end());
    }

    void replace(std::vector<legacy::compressT>&& vec) {
        if (!vec.empty()) {
            m_record.swap(vec);
            m_pos = 0;
        }
        // else???
    }
};

// TODO: together with these, move to/from_MAP_str to a standalone header?
inline std::vector<legacy::compressT> extract_rules(const char* begin, const char* end) {
    std::vector<legacy::compressT> rules;

    const auto& regex = legacy::regex_MAP_str();
    std::cmatch match;
    while (std::regex_search(begin, end, match, regex)) {
        legacy::compressT rule(legacy::from_MAP_str(match[0]));
        if (rules.empty() || rules.back() != rule) {
            rules.push_back(rule);
        }
        begin = match.suffix().first;
    }
    return rules;
}

inline std::vector<legacy::compressT> extract_rules(std::span<const char> data) {
    return extract_rules(data.data(), data.data() + data.size());
}

inline std::vector<legacy::compressT> extract_rules(const char* str) {
    return extract_rules(str, str + strlen(str));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

#include <chrono>
#include <filesystem>
#include <fstream>

#include "imgui.h"

using namespace std::chrono_literals;

// - Assert that ordinary string literals are encoded with utf-8.
// - u8"..." is not used in this project, as it becomes `char8_t[]` after C++20 (which is not usable).
// - TODO: document ways to pass this check (/utf-8 etc; different compilers)...
inline void assert_utf8_encoding() {
    constexpr auto a = std::to_array("中文");
    constexpr auto b = std::to_array(u8"中文");

    static_assert(std::equal(a.begin(), a.end(), b.begin(), b.end(), [](auto l, auto r) {
        return static_cast<unsigned char>(l) == static_cast<unsigned char>(r);
    }));
}

// - Experience in MSVC
// - It turns out that there are still a lot of messy encoding problems even if "/utf-8" is specified.
//   (For example, how is `exception.what()` encoded? What does `path` expects from `string`? And what about
//   `filesystem.path.string()`?)
inline std::string cpp17_u8string(const std::filesystem::path& p) {
    return reinterpret_cast<const char*>(p.u8string().c_str());
}

// Unlike ImGui::TextWrapped, doesn't take fmt str...
// TODO: the name is awful...

inline void imgui_str(std::string_view str) {
    ImGui::TextUnformatted(str.data(), str.data() + str.size());
}

inline void imgui_strwrapped(std::string_view str) {
    ImGui::PushTextWrapPos(0.0f);
    imgui_str(str);
    ImGui::PopTextWrapPos();
}

inline void imgui_strdisabled(std::string_view str) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    imgui_str(str);
    ImGui::PopStyleColor();
}

inline bool imgui_keypressed(ImGuiKey key, bool repeat) {
    return !ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(key, repeat);
};

// TODO: mouse?

// I hate these "deprecated but still generated" copiers.
// TODO: clang-format works wrongly with emojis?
// #define 🤢(T)
#define NOCOPY(T)         \
    T(const T&) = delete; \
    T& operator=(const T&) = delete;

struct [[nodiscard]] imgui_window {
    NOCOPY(imgui_window);

    const bool visible;
    // TODO: refine interface and documentation
    explicit imgui_window(const char* name, ImGuiWindowFlags flags = {})
        : visible(ImGui::Begin(name, nullptr, flags)) {}
    explicit imgui_window(const char* name, bool* p_open, ImGuiWindowFlags flags = {})
        : visible(ImGui::Begin(name, p_open, flags)) {}
    ~imgui_window() {
        ImGui::End(); // Unconditional.
    }
    explicit operator bool() const { return visible; }
};

struct [[nodiscard]] imgui_childwindow {
    NOCOPY(imgui_childwindow);

    const bool visible;
    explicit imgui_childwindow(const char* name, const ImVec2& size = {}, ImGuiWindowFlags flags = {})
        : visible(ImGui::BeginChild(name, size, false, flags)) {}
    ~imgui_childwindow() {
        ImGui::EndChild(); // Unconditional.
    }
    explicit operator bool() const { return visible; }
};

// TODO: should imgui_childwindow/... support "enabled feature"?
struct [[nodiscard]] imgui_itemtooltip {
    NOCOPY(imgui_itemtooltip);

    const bool opened; // TODO: proper name?
    explicit imgui_itemtooltip(bool enabled = true) : opened(enabled && ImGui::BeginItemTooltip()) {}
    ~imgui_itemtooltip() {
        if (opened) {
            ImGui::EndTooltip();
        }
    }
    explicit operator bool() const { return opened; }
};

struct timeT {
    int year;
    int month; // [1,12]
    int day;   // [1,31]
    int hour;  // [0,23]
    int min;   // [0,59]
    int sec;   // [0,59]
    int ms;

    static timeT now() {
        using namespace std::chrono;
        auto now = current_zone()->to_local(system_clock::now());

        year_month_day ymd(floor<days>(now));
        int year = ymd.year().operator int();
        int month = ymd.month().operator unsigned int();
        int day = ymd.day().operator unsigned int();

        hh_mm_ss hms(floor<milliseconds>(now - floor<days>(now)));
        int hour = hms.hours().count();
        int min = hms.minutes().count();
        int sec = hms.seconds().count();
        int ms = hms.subseconds().count();
        return timeT{year, month, day, hour, min, sec, ms};
    }
};

class logger {
    // TODO: maybe better if data(inheritance)-based?
    static inline std::deque<std::string> m_strs{};
    static constexpr int m_max = 40;
    // ~ "ith" must not be a static in log(), as it's a template...
    static inline int ith = 0;

    // TODO: refine...
    using steady = std::chrono::steady_clock;
    struct temp_str {
        std::string str;
        steady::time_point deadline;

        temp_str(std::string&& str, std::chrono::milliseconds ms) : str(std::move(str)), deadline(steady::now() + ms) {}

        // TODO: better be expired(now=steady::now) return now>=deadline;
        bool expired() const { return steady::now() >= deadline; }
    };

    static inline std::vector<temp_str> m_tempstrs{};

public:
    logger() = delete;

    // TODO: logfmt and logplain...
    // Also serve as error handler; must succeed.
    template <class... T>
    static void log(std::format_string<const T&...> fmt, const T&... args) noexcept {
        auto now = timeT::now();
        // TODO: the format should be refined...
        char str[100];
        snprintf(str, 100, "%02d:%02d:%02d [%d] ", now.hour, now.min, now.sec, ith++);

        m_strs.push_back(str + std::format(fmt, args...));
        if (m_strs.size() > m_max) {
            m_strs.pop_front();
        }
    }

    template <class... U>
    static void log_temp(std::chrono::milliseconds ms, std::format_string<const U&...> fmt, const U&... args) noexcept {
        m_tempstrs.emplace_back(std::format(fmt, args...), ms);
    }

    // TODO: this might combine with itemtooltip...
    static void tempwindow() {
        if (!m_tempstrs.empty()) {
            ImGui::BeginTooltip();
            auto pos = m_tempstrs.begin();
            for (auto& temp : m_tempstrs) {
                imgui_str(temp.str);
                if (!temp.expired()) {
                    *pos++ = std::move(temp);
                }
            }
            m_tempstrs.erase(pos, m_tempstrs.end());
            ImGui::EndTooltip();
        }
    }

    // TODO: layout is terrible...
    static void window(const char* id_str, bool* p_open) {
        if (imgui_window window(id_str, p_open); window) {
            if (ImGui::Button("Clear")) {
                m_strs.clear();
            }
            ImGui::SameLine();
            bool to_bottom = ImGui::Button("Bottom");
            ImGui::Separator();

            if (imgui_childwindow child("Child"); child) {
                // TODO: optimize...
                std::string str;
                for (const auto& s : m_strs) {
                    str += s;
                    str += '\n';
                }
                imgui_strwrapped(str);
                if (to_bottom || ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }
            }
        }
    }
};

// - Why using C++ at all: there seems no standard way to `fopen` a unicode C-string path.
// (The only one being ignore(max)->gcount, which appears )
inline std::vector<char> load_binary(const std::filesystem::path& path, int max_size) {
    using namespace std;
    using namespace std::filesystem;

    error_code ec{};
    const auto size = file_size(path, ec);
    if (size != -1 && size < max_size) {
        ifstream file(path, ios::in | ios::binary);
        if (file) {
            vector<char> data(size);
            file.read(data.data(), size);
            if (file && file.gcount() == size) {
                return data;
            }
        }
    }
    // TODO: refine msg?
    logger::log_temp(300ms, "Cannot load");
    return {};
}

inline std::vector<legacy::compressT> read_rule_from_file(const std::filesystem::path& path) {
    return extract_rules(load_binary(path, 1'000'000));
}
