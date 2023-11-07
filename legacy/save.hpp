#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>

#include <imgui.h>

#include "rule.hpp"
#include "serialize2.hpp"

// TODO: should redesign...

namespace legacy {
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

    // TODO: record system...

    // TODO: configurable...
    inline std::filesystem::path m_folder = R"(C:\*redacted*\Desktop\rulelists_app)";
    inline std::error_code m_errc{};
    inline void prepare_folder() {
        std::filesystem::create_directories(m_folder, m_errc);
    }
    inline string filename_from(const timeT& time) {
        char str[100];
        snprintf(str, 100, "%d_%d_%d.txt", time.year, time.month, time.day); // TODO: can fail?
        return str;
    }

    // TODO: should be redesigned...
    inline void record_rule(const ruleT& rule) {
        static std::optional<ruleT> last{std::nullopt};
        if (last == rule) {
            return;
        }

        last = rule;
        prepare_folder();

        auto time = timeT::now();

        std::ofstream append(m_folder / filename_from(time), std::ios::app);
        if (append) {
            char str[100];
            snprintf(str, 100, "%d-%d-%d %02d:%02d:%02d", time.year, time.month, time.day, time.hour, time.min,
                     time.sec);

            // TODO: bad; should provide a way to show consecutive diff though...
            append << "# " << str << " rule-hash: " << std::hash<string>{}(to_MAP_str(rule)) << "\n";
            append << to_MAP_str(rule) << "\n"; // and what?
        } else {
            // TODO: what to do?
        }
    }
} // namespace legacy
