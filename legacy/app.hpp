#pragma once

// TODO: the utilities provided by "app.hpp" appears unsystematic, and generally too weak.

#include <deque>
#include <format>
#include <random>

#include "partition.hpp"
#include "rule.hpp"
#include "tile.hpp"

// TODO: partial rule (e.g. collect from tile; the rest is generated from another generator...)
// TODO: allow resizing the grid.

// TODO: notify on quit... (? there seems no such need.)
// TODO: support more run-mode... (? related: when will the current run-mode cause problems?)

void random_fill(bool* begin, bool* end, int count, auto&& rand) {
    std::fill(begin, end, false);
    std::fill(begin, begin + std::clamp(count, 0, int(end - begin)), true);
    std::shuffle(begin, end, rand);
}

// TODO: rename...
struct tile_filler {
    uint64_t seed; // arbitrary value
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

        const auto seed = filler.seed;
        const auto density = filler.density;
        random_fill(m_tile.begin(), m_tile.end(), m_tile.area() * density, std::mt19937_64{seed});
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

std::vector<legacy::compressT> extract_rules(const char* begin, const char* end) {
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

std::optional<std::vector<char>> load_binary(const char* filename, int max_size) {
    const std::unique_ptr<FILE, decltype(+fclose)> file(fopen(filename, "rb"), fclose);
    if (file) {
        FILE* fp = file.get();
        fseek(fp, 0, SEEK_END); // <-- what if fails?
        int size = ftell(fp);
        if (size < max_size) {
            fseek(fp, 0, SEEK_SET); // <-- what if fails?
            std::vector<char> data(size);
            fread(data.data(), 1, size, fp); // what if fails?
            return data;
        }
    }
    return {};
}

// TODO: redesign return types... also optional?
// TODO: add error log.
std::vector<legacy::compressT> read_rule_from_file(const char* filename) {
    auto result = load_binary(filename, 1'000'000);
    if (result) {
        return extract_rules(result->data(), result->data() + result->size());
    }
    return {};
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// TODO: whether to accept <imgui.h> as base header?

#include <chrono>
#include <filesystem>

#include <imgui.h>

// Unlike ImGui::TextWrapped, doesn't take fmt str...
// TODO: the name is awful...
inline void imgui_strwrapped(std::string_view str) {
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(str.data(), str.data() + str.size());
    ImGui::PopTextWrapPos();
}

inline bool imgui_keypressed(ImGuiKey key, bool repeat) {
    return !ImGui::GetIO().WantCaptureKeyboard && ImGui::IsKeyPressed(key, repeat);
};

// TODO: forbid copying...
struct [[nodiscard]] imgui_window {
    const bool visible;
    explicit imgui_window(const char* name, bool* p_open = {}, ImGuiWindowFlags flags = {})
        : visible(ImGui::Begin(name, p_open, flags)) {}
    ~imgui_window() {
        ImGui::End(); // Unconditional.
    }
    explicit operator bool() const { return visible; }
};

struct [[nodiscard]] imgui_childwindow {
    const bool visible;
    explicit imgui_childwindow(const char* name, const ImVec2& size = {}, ImGuiWindowFlags flags = {})
        : visible(ImGui::BeginChild(name, size, false, flags)) {}
    ~imgui_childwindow() {
        ImGui::EndChild(); // Unconditional.
    }
    explicit operator bool() const { return visible; }
};

// TODO: should imgui_childwindow/... support "enabled feature"?
// TODO: forbid copying...
struct [[nodiscard]] imgui_itemtooltip {
    const bool opened; // TODO: proper name?
    imgui_itemtooltip(bool enabled = true) : opened(enabled && ImGui::BeginItemTooltip()) {}
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

// Likely to be the only singleton...
class logger {
    // TODO: maybe better if data(inheritance)-based?
    static inline std::deque<std::string> m_strs{};
    static constexpr int m_max = 40;
    // ~ "ith" must not be a static in log(), as it's a template...
    static inline int ith = 0;

public:
    logger() = delete;

    // Also serve as error handler; must succeed.
    template <class... T>
    static void log(std::format_string<const T&...> fmt, const T&... args) noexcept {
        auto now = timeT::now();
        // TODO: the format should be refined...
        char str[100];
        snprintf(str, 100, "%d-%d %02d:%02d:%02d [%d] ", now.month, now.day, now.hour, now.min, now.sec, ith++);

        m_strs.push_back(str + std::format(fmt, args...));
        if (m_strs.size() > m_max) {
            m_strs.pop_front();
        }
    }

    // TODO: better name...
    template <class... T>
    static void append(std::format_string<const T&...> fmt, const T&... args) noexcept {
        assert(!m_strs.empty());
        m_strs.back() += std::format(fmt, args...);
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
