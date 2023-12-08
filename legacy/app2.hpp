#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>

#include "app.hpp"
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

struct [[nodiscard]] imgui_window {
    imgui_window(const imgui_window&) = delete;
    imgui_window& operator=(const imgui_window&) = delete;

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
    imgui_childwindow(const imgui_childwindow&) = delete;
    imgui_childwindow& operator=(const imgui_childwindow&) = delete;

    const bool visible;
    explicit imgui_childwindow(const char* name, const ImVec2& size = {}, ImGuiChildFlags child_flags = {},
                               ImGuiWindowFlags window_flags = {})
        : visible(ImGui::BeginChild(name, size, child_flags, window_flags)) {}
    ~imgui_childwindow() {
        ImGui::EndChild(); // Unconditional.
    }
    explicit operator bool() const { return visible; }
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
        if (auto window = imgui_window(id_str, p_open)) {
            if (ImGui::Button("Clear")) {
                m_strs.clear();
            }
            ImGui::SameLine();
            bool to_bottom = ImGui::Button("Bottom");
            ImGui::Separator();

            if (auto child = imgui_childwindow("Child")) {
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
