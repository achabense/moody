#include <unordered_map>

#include "tile.hpp"

#include "common.hpp"

// TODO: support boundless space...

using clockT = std::chrono::steady_clock;

static ImVec2 to_imvec(const aniso::vecT& vec) { return ImVec2(vec.x, vec.y); }

static aniso::vecT from_imvec_floor(const ImVec2& vec) { return {.x = int(floor(vec.x)), .y = int(floor(vec.y))}; }

static aniso::rangeT clamp_window(aniso::vecT size, aniso::vecT region_center, aniso::vecT region_size) {
    region_size.x = std::min(size.x, region_size.x);
    region_size.y = std::min(size.y, region_size.y);
    aniso::vecT begin = aniso::clamp(region_center - region_size / 2, {0, 0}, size - region_size);
    return {.begin = begin, .end = begin + region_size};
}

static bool strobing(const aniso::ruleT& rule) {
    constexpr aniso::codeT all_0{0}, all_1{511};
    return rule[all_0] == 1 && rule[all_1] == 0;
}

// TODO: error-prone. Should work in a way similar to `identify`.
// Copy the subrange and run as a torus space, recording all invoked mappings.
// This is only good at capturing simple, "self-contained" patterns (oscillators/spaceships).
// For more complex situations, the program has "open-capture" (`fake_apply`) to record
// areas frame-by-frame.
static aniso::moldT::lockT capture_closed(const aniso::tile_const_ref tile, const aniso::ruleT& rule) {
    aniso::moldT::lockT lock{};
    aniso::tileT torus(tile);

    // (wontfix) It's possible that the loop fails to catch all invocations in very rare cases,
    // due to that `limit` is not large enough.

    // Loop until there has been `limit` generations without newly invoked mappings.
    const int limit = 120;
    for (int g = limit; g > 0; --g) {
        torus.run_torus([&](const aniso::codeT code) {
            if (!lock[code]) {
                g = limit;
                lock[code] = true;
            }
            return rule[code];
        });
    }

    return lock;
}

// Identify spaceships or oscillators in 2*2 periodic (including pure) background. (Cannot deal with non-trivial
// objects like guns, puffers etc.)
// The area should be fully surrounded by 2*2 periodic border, and contain a full phase of the object (one or
// several oscillators, or a single spaceship).
static void identify(const aniso::tile_const_ref tile, const aniso::ruleT& rule,
                     const bool require_matching_background = true) {
    static constexpr aniso::vecT period_size{2, 2};
    struct periodT {
        std::array<bool, period_size.x * period_size.y> m_data{};

        bool operator==(const periodT&) const = default;

        static aniso::vecT size() { return period_size; }
        aniso::tile_ref data() { return {period_size, period_size.x, m_data.data()}; }
        aniso::tile_const_ref data() const { return {period_size, period_size.x, m_data.data()}; }

        bool is_periodic(const aniso::ruleT& rule) const {
            periodT torus = *this;
            for (int g = 0; g < (1 << (period_size.x * period_size.y)); ++g) {
                aniso::apply_rule_torus(rule, torus.data());
                if (torus == *this) {
                    return true;
                }
            }
            return false;
        }
        bool rotate_equal(const periodT& b) const {
            for (int dy = 0; dy < period_size.y; ++dy) {
                for (int dx = 0; dx < period_size.x; ++dx) {
                    periodT ro{};
                    aniso::rotate_copy_00_to(ro.data(), data(), {.x = dx, .y = dy});
                    if (ro == b) {
                        return true;
                    }
                }
            }
            return false;
        }
    };

    static const auto take_corner = [](const aniso::tile_const_ref tile) {
        assert(tile.size.both_gteq(period_size));
        periodT p{};
        aniso::copy(p.data(), tile.clip({{0, 0}, period_size}));
        return p;
    };
    static const auto locate_pattern = [](const aniso::tile_const_ref tile,
                                          const bool for_input = false) -> std::optional<aniso::rangeT> {
        assert(tile.size.both_gt(period_size * 2));
        const aniso::rangeT range = aniso::bounding_box(tile, take_corner(tile).data());
        if (range.empty()) {
            messenger::set_msg(for_input ? "The area contains nothing." : "The pattern dies out.");
            return {};
        } else if (!(range.begin.both_gteq(period_size) && range.end.both_lteq(tile.size - period_size))) {
            if (for_input) {
                // !!TODO: better message.
                messenger::set_msg("The border is invalid.");
            } else {
                assert(false); // Guaranteed by `regionT::run`.
            }
            return {};
        } else if (const auto size = range.size(); size.x > 3000 || size.y > 3000 || size.xy() > 400 * 400) {
            // For example, this can happen when the initial area contains a still life and a spaceship.
            messenger::set_msg(for_input ? "The area is too large." : "The pattern grows too large.");
            return {};
        }
        return aniso::rangeT{.begin = range.begin - period_size, .end = range.end + period_size};
    };
    struct regionT {
        aniso::tileT tile;
        aniso::rangeT range; // Range of pattern (including the background border), relative to the tile.
        aniso::vecT off;     // Pattern's begin pos, relative to the initial pattern.
        bool run(const aniso::ruleT& rule) {
            const aniso::tile_const_ref pattern = tile.data().clip(range);
            const aniso::tile_const_ref background = pattern.clip({{0, 0}, period_size});
            const aniso::vecT padding = {1, 1};
            // (Ceiled for torus run. This can be avoided if `border_ref` is calculated manually, but that
            // will be a lot of code.)
            aniso::tileT next(aniso::divmul_ceil(range.size() + padding * 2, period_size));

            periodT aligned{}; // Aligned to next.data().at(0, 0).
            aniso::rotate_copy_00_to(aligned.data(), background, padding);
            const aniso::rangeT relocate{.begin = padding, .end = padding + pattern.size};
            aniso::fill_outside(next.data(), relocate, aligned.data());
            aniso::copy(next.data().clip(relocate), pattern);
            next.run_torus(rule);

            tile.swap(next);
            if (const auto next_range = locate_pattern(tile.data())) {
                off = off - padding + next_range->begin;
                range = *next_range;
                return true;
            }
            return false;
        }
    };

    if (!tile.size.both_gt(period_size * 2)) {
        // !!TODO: better message.
        messenger::set_msg("The area is too small.");
        return;
    }

    const periodT init_background = take_corner(tile);
    const std::optional<aniso::rangeT> init_range = locate_pattern(tile, true);
    if (!init_range) {
        return;
    } else if (!init_background.is_periodic(rule)) {
        // !!TODO: better message.
        messenger::set_msg("The background is not periodic.");
        return;
    }

    const aniso::tile_const_ref init_pattern = tile.clip(*init_range);
    regionT region{.tile = aniso::tileT(init_pattern), .range = {{0, 0}, init_pattern.size}, .off = {0, 0}};
    aniso::tileT smallest = region.tile;

    const int limit = 4000; // Max period to deal with.
    for (int g = 1; g <= limit; ++g) {
        if (!region.run(rule)) {
            return;
        }

        const aniso::tile_const_ref pattern = region.tile.data().clip(region.range);
        if ((!require_matching_background || init_background.rotate_equal(take_corner(pattern))) &&
            pattern.size.xy() < smallest.size().xy()) {
            smallest = aniso::tileT(pattern);
        }
        if (aniso::equal(init_pattern, pattern)) {
            std::string str;
            const bool is_oscillator = region.off == aniso::vecT{0, 0};
            if (is_oscillator && g == 1) {
                str = "#C Still life.\n";
            } else if (is_oscillator) {
                str = std::format("#C Oscillator. Period:{}.\n", g);
            } else {
                str = std::format("#C Spaceship. Period:{}. Offset(x,y):({},{}).\n", g, region.off.x, region.off.y);
            }
            assert(str.ends_with('\n'));
            str += aniso::to_RLE_str(smallest.data(), &rule);
            ImGui::SetClipboardText(str.c_str());
            messenger::set_msg(std::move(str));
            return;
        }
    }
    // For example, this can happen the object really has a huge period, or the initial area doesn't
    // contain a full phase (fragments that evolves to full objects are not recognized), or whatever else.
    messenger::set_msg("Cannot identify.");
}

class percentT {
    int m_val; // ∈ [0, 100].
public:
    percentT(double v) { m_val = std::clamp(v, 0.0, 1.0) * 100; }

    double get() const { return m_val / 100.0; }
    void step_slide(const char* label) {
        imgui_StepSliderInt(label, &m_val, 0, 100, std::format("{:.2f}", m_val / 100.0).c_str());
    }

    friend bool operator==(const percentT&, const percentT&) = default;
};

struct initT {
    int seed;
    percentT density;
    percentT area; // TODO: support more strategies (separate x/y, and fixed size)?

    // TODO: avoid dynamic allocation?
    aniso::tileT background; // Periodic background.

    friend bool operator==(const initT&, const initT&) = default;

    void initialize(aniso::tileT& tile) const {
        assert(!tile.empty() && !background.empty());
        const aniso::vecT tile_size = tile.size();
        const auto range = clamp_window(tile_size, tile_size / 2, from_imvec_floor(to_imvec(tile_size) * area.get()));

        if (!range.empty()) {
            // It does not matter much how the area is aligned with the background, as
            // it's randomized.
            aniso::fill(tile.data(), background.data());
            std::mt19937 rand{(uint32_t)seed};
            aniso::random_flip(tile.data().clip(range), rand, density.get());
            // TODO: support random_fill mode (fill_outside + random_fill) as well?
        } else {
            aniso::fill(tile.data(), background.data());
        }
    }
};

class zoomT {
    struct termT {
        float val;
        const char* str;
    };
    static constexpr termT terms[]{{0.5, "0.5"}, {1, "1"}, {2, "2"}, {3, "3"}, {4, "4"}, {5, "5"}};

    static constexpr int index_1 = 1;
    static constexpr int index_max = std::size(terms) - 1; // ]
    int m_index = index_1;

public:
    void set_1() {
        static_assert(terms[index_1].val == 1);
        m_index = index_1;
    }
    void slide(int di) { m_index = std::clamp(m_index + di, 0, index_max); }
    void select(const std::invocable<bool, float, const char*> auto& fn) {
        int n_index = m_index;
        for (int i = 0; const auto [z, s] : terms) {
            const int this_i = i++;
            if (fn(m_index == this_i, z, s)) {
                n_index = this_i;
            }
        }
        m_index = n_index;
    }

    static constexpr float min() { return terms[0].val; }
    static constexpr float max() { return terms[index_max].val; }
    operator float() const {
        assert(m_index >= 0 && m_index <= index_max);
        return terms[m_index].val;
    }
};

class runnerT {
    static constexpr aniso::vecT size_min{.x = 20, .y = 15};
    static constexpr aniso::vecT size_max{.x = 1600, .y = 1200};
    static constexpr ImVec2 min_canvas_size{size_min.x * zoomT::max(), size_min.y* zoomT::max()};

    struct ctrlT {
        aniso::ruleT rule{};

        static constexpr int step_min = 1, step_max = 100;
        int step = 1;
        bool anti_strobing = true;
        int actual_step() const {
            if (anti_strobing && strobing(rule) && step % 2) {
                return step + 1;
            }
            return step;
        }

        // gap * gap_unit ~ interval.
        static constexpr int gap_unit = 25; // ms.
        static constexpr int gap_min = 0, gap_max = 20;
        int gap = 0;

        bool pause = false;

        int extra_step = 0;

        clockT::time_point last_time = {};
        int calc_step_this_frame() {
            int step = std::exchange(extra_step, 0);
            if (step == 0) {
                if (!pause && (last_time + std::chrono::milliseconds{gap * gap_unit} <= clockT::now())) {
                    step = actual_step();
                }
            }

            if (step != 0) {
                last_time = clockT::now();
            }
            return step;
        }
    };

    class torusT {
        initT m_init{.seed = 0, .density = 0.5, .area = 0.5, .background = aniso::tileT{{.x = 1, .y = 1}}};
        aniso::tileT m_torus{{.x = 600, .y = 400}};
        ctrlT m_ctrl{.rule{}, .step = 1, .anti_strobing = true, .gap = 0, .pause = false};
        int m_gen = 0;

        bool extra_pause = false;
        void mark_written() {
            extra_pause = true;
            m_ctrl.last_time = clockT::now();
        }

    public:
        torusT() {
            assert(m_torus.size() == calc_size(m_torus.size()));
            restart();
        }

        void begin_frame(const aniso::ruleT& rule) {
            extra_pause = false;
            m_ctrl.extra_step = 0;

            if (m_ctrl.rule != rule) {
                m_ctrl.rule = rule;
                m_ctrl.anti_strobing = true;
                m_ctrl.pause = false;

                restart();
            }
        }
        void restart() {
            m_gen = 0;
            m_init.initialize(m_torus);
            mark_written();
        }
        void pause_for_this_frame() { extra_pause = true; }

        aniso::tile_const_ref read_only() const { return m_torus.data(); }
        aniso::tile_const_ref read_only(const aniso::rangeT& range) const { return m_torus.data().clip(range); }

        aniso::tile_ref write_only() {
            mark_written();
            return m_torus.data();
        }
        aniso::tile_ref write_only(const aniso::rangeT& range) {
            mark_written();
            return m_torus.data().clip(range);
        }

        void read_and_maybe_write(const std::invocable<aniso::tile_ref> auto& fn) {
            if (fn(m_torus.data())) {
                mark_written();
            }
        }

        void rotate_00_to(int dx, int dy) {
            if (dx != 0 || dy != 0) {
                aniso::tileT temp(m_torus.size());
                aniso::rotate_copy_00_to(temp.data(), m_torus.data(), {.x = dx, .y = dy});
                m_torus.swap(temp);
            }
        }

        // int area() const { return m_torus.size().xy(); }
        int gen() const { return m_gen; }
        aniso::vecT size() const {
            assert(m_torus.size() == calc_size(m_torus.size()));
            return m_torus.size();
        }

        aniso::vecT calc_size(const aniso::vecT size) const {
            const aniso::vecT n_size =
                aniso::divmul_floor(aniso::clamp(size, size_min, size_max), m_init.background.size());
            if (!n_size.both_gteq(size_min)) [[unlikely]] {
                return aniso::divmul_ceil(size_min, m_init.background.size());
            }
            return n_size;
        }

        bool resize(const aniso::vecT size) {
            const aniso::vecT n_size = calc_size(size);
            if (m_torus.size() != n_size) {
                m_torus.resize(n_size);
                restart();
                return true;
            }
            return false;
        }
        void set_ctrl(const auto& fn) { fn(m_ctrl); }
        void set_init(const auto& fn) {
            const initT old_init = m_init;
            if (fn(m_init) || old_init != m_init) {
                const bool restarted = resize(m_torus.size()); // In case the background is resized.
                if (!restarted) {
                    restart();
                }
            }
        }

        void end_frame() {
            if (!extra_pause) {
                const int count = m_ctrl.calc_step_this_frame();
                for (int c = 0; c < count; ++c) {
                    m_torus.run_torus(m_ctrl.rule);
                    ++m_gen;
                }
            }
        }
    };

    torusT m_torus{}; // Space.

    // space-pos == corner-pos + canvas-pos / zoom
    struct coordT {
        zoomT zoom{};
        ImVec2 corner_pos = {0, 0}; // Space.
        ImVec2 to_space(ImVec2 canvas_pos) const { return corner_pos + canvas_pos / zoom; }
        ImVec2 to_canvas(ImVec2 space_pos) const { return (space_pos - corner_pos) * zoom; }
        void bind(const ImVec2 space_pos, const ImVec2 canvas_pos) { corner_pos = space_pos - canvas_pos / zoom; }
    };

    coordT m_coord{};
    ImVec2 to_rotate = {0, 0};

    std::optional<aniso::tileT> m_paste = std::nullopt;
    aniso::vecT paste_beg{0, 0}; // Valid if paste.has_value().

    struct selectT {
        bool active = true;
        aniso::vecT beg{0, 0}, end{0, 0}; // [] instead of [).

        aniso::rangeT to_range() const {
            const auto [xmin, xmax] = std::minmax(beg.x, end.x);
            const auto [ymin, ymax] = std::minmax(beg.y, end.y);

            return {.begin{xmin, ymin}, .end{xmax + 1, ymax + 1}};
        }

        int width() const { return std::abs(beg.x - end.x) + 1; }
        int height() const { return std::abs(beg.y - end.y) + 1; }
    };
    std::optional<selectT> m_sel = std::nullopt;

public:
    // TODO: (wontfix?) there cannot actually be multiple instances in the program.
    // For example, there are a lot of static variables in `display`, and the keyboard controls are not designed
    // for per-object use.
    void display(sync_point& sync) {
        m_torus.begin_frame(sync.current.rule);

        static bool background = 0;
        static bool auto_fit = false;
        bool locate_center = false;
        bool find_suitable_zoom = false;
        {
            // Could be `ImGui::GetFrameCount() == 1`, but that looks unstable.
            static bool first = true;
            if (std::exchange(first, false)) {
                locate_center = true;
                find_suitable_zoom = true;
            }
        }

        const char* const canvas_name = "Canvas";
        const bool enable_shortcuts =
            (!ImGui::GetIO().WantCaptureKeyboard && !ImGui::IsPopupOpen(ImGuiID(0), ImGuiPopupFlags_AnyPopupId)) ||
            (GImGui->ActiveId == ImGui::GetID(canvas_name));
        // Shadowing `::test_key`.
        auto test_key = [enable_shortcuts](ImGuiKey key, bool repeat) {
            return enable_shortcuts && ImGui::IsKeyPressed(key, repeat);
        };

        auto set_init_state = [&] {
            // TODO: redesign this part. This has became clumsy due to the introduction of periodic background.
            // !!TODO: recheck restarting/blocking logic...
            m_torus.set_init([&](initT& init) {
                ImGui::BeginGroup();
                ImGui::PushItemWidth(item_width);
                imgui_StepSliderInt("Seed", &init.seed, 0, 29);
                init.density.step_slide("Density");
                init.area.step_slide("Area");
                ImGui::PopItemWidth();
                ImGui::EndGroup();
                if (ImGui::IsItemActive()) {
                    m_torus.pause_for_this_frame();
                }
                bool force_restart = ImGui::IsItemActivated();

                ImGui::Separator();

                ImGui::AlignTextToFramePadding();
                imgui_StrTooltip("(...)", "Left-click a cell to set it to 1.\n"
                                          "Right-click a cell to set it to 0.\n"
                                          "Ctrl + left-click to resize.");
                ImGui::SameLine();
                imgui_Str("Background ~");
                ImGui::SameLine();
                if (ImGui::Button("Clear##Bg")) {
                    aniso::fill(init.background.data(), 0);
                }

                std::optional<aniso::vecT> resize{};
                const aniso::tile_ref data = init.background.data();
                ImGui::BeginGroup();
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, 0});
                for (int y = 0; y < 4 /*max-height*/; ++y) {
                    for (int x = 0; x < 4 /*max-width*/; ++x) {
                        if (x != 0) {
                            ImGui::SameLine();
                        }
                        const bool in_range = x < data.size.x && y < data.size.y;

                        // (No need for unique ID here.)
                        ImGui::InvisibleButton(
                            "##Invisible", square_size(),
                            ImGuiButtonFlags_MouseButtonLeft |
                                ImGuiButtonFlags_MouseButtonRight); // So right-click can activate the button.
                        imgui_ItemRectFilled(in_range ? (data.at(x, y) ? IM_COL32_WHITE : IM_COL32_BLACK)
                                                      : IM_COL32_GREY(40, 255));
                        imgui_ItemRect(IM_COL32_GREY(100, 255));
                        // TODO: unify colors with `static_constraints`.

                        if (ImGui::IsItemHovered()) {
                            if (ImGui::GetIO().KeyCtrl) {
                                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                                    resize = {.x = x + 1, .y = y + 1};
                                }
                            } else if (in_range) {
                                if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                                    data.at(x, y) = 0;
                                } else if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                                    data.at(x, y) = 1;
                                }
                            }
                        }
                    }
                }
                ImGui::PopStyleVar();
                ImGui::EndGroup();
                if (ImGui::IsItemActive()) {
                    m_torus.pause_for_this_frame();
                }
                if (ImGui::IsItemActivated()) {
                    force_restart = true;
                }

                ImGui::SameLine(0, 0);
                imgui_Str(" ~ ");
                ImGui::SameLine(0, 0);
                aniso::tileT result({.x = data.size.x * 5, .y = data.size.y * 5});
                aniso::fill(result.data(), data);
                ImGui::Image(make_screen(result.data(), scaleE::Nearest), to_imvec(result.size() * 3), ImVec2(0, 0),
                             ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImGui::ColorConvertU32ToFloat4(IM_COL32_GREY(100, 255)));
                // The background does not make much sense if it is not stable for the current rule, but
                // I have no idea how it should behave.

                if (resize) {
                    init.background.resize(*resize);
                    aniso::fill(init.background.data(), 0);
                    // The space may not be resized; reset anyway.
                    m_sel.reset();
                    m_paste.reset();
                    // !!TODO: recheck resetting logic. `m_sel` and `m_paste` may need to reset
                    // in more situations.
                }

                return force_restart;
            });
        };

        auto input_size = [&] {
            static char input_w[6]{}, input_h[6]{};

            const float inner_spacing = imgui_ItemInnerSpacingX();
            const auto input_flags = ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_EnterReturnsTrue;
            const auto input_filter = [](ImGuiInputTextCallbackData* data) -> int {
                return (data->EventChar >= '0' && data->EventChar <= '9') ? 0 : 1;
            };

            aniso::vecT size = m_torus.size();

            // TODO: whether / how to add hint ('enter' to resize)?
            bool resize = false;
            ImGui::SetNextItemWidth(floor((item_width - inner_spacing) / 2));
            if (ImGui::InputTextWithHint("##Width", std::format("Width:{}", size.x).c_str(), input_w,
                                         std::size(input_w), input_flags, input_filter)) {
                resize = true;
            }
            ImGui::SameLine(0, inner_spacing);
            ImGui::SetNextItemWidth(ceil((item_width - inner_spacing) / 2));
            if (ImGui::InputTextWithHint("##Height", std::format("Height:{}", size.y).c_str(), input_h,
                                         std::size(input_h), input_flags, input_filter)) {
                resize = true;
            }
            ImGui::SameLine(0, inner_spacing);
            imgui_Str("Space size");

            if (resize) {
                const bool has_w = std::from_chars(input_w, std::end(input_w), size.x).ec == std::errc{};
                const bool has_h = std::from_chars(input_h, std::end(input_h), size.y).ec == std::errc{};
                // ~ the value is unmodified if `from_chars` fails.
                if (has_w || has_h) {
                    auto_fit = false;
                    locate_center = true;
                    find_suitable_zoom = true;

                    if (m_torus.resize(size)) {
                        m_sel.reset();
                        m_paste.reset();
                    }
                }
                input_w[0] = '\0';
                input_h[0] = '\0';
            }
        };

        auto select_zoom = [&] {
            ImGui::AlignTextToFramePadding();
            imgui_Str("Zoom ~");
            bool auto_fit_next = auto_fit;
            m_coord.zoom.select([&](const bool is_cur, const float z, const char* const s) {
                ImGui::SameLine(0, imgui_ItemInnerSpacingX());
                if (ImGui::RadioButton((auto_fit && is_cur) ? std::format("[{0}]###{0}", s).c_str() : s, is_cur)) {
                    auto_fit_next = is_cur ? !auto_fit : true;
                    return true;
                }
                if (z == 1) {
                    quick_info("^ Auto full-screen.");
                }
                return false;
            });
            auto_fit = auto_fit_next;
            ImGui::SameLine(); // TODO: clarify that rotating is still allowed?
            imgui_StrTooltip("(?)",
                             "Click to automatically resize to full-screen.\n\n"
                             "(Click again to quit this mode; dragging and scrolling are not available in this mode.)");
        };

        ImGui::PushItemWidth(item_width);
        ImGui::BeginGroup();
        m_torus.set_ctrl([&](ctrlT& ctrl) {
            ImGui::AlignTextToFramePadding();
            imgui_StrTooltip("(...)", "Keyboard shortcuts:\n"
                                      "R: Restart    Space: Pause\nN/M (repeatable): +p/+1\n"
                                      "1/2 (repeatable): -/+ Step\n3/4 (repeatable): -/+ Interval\n");
            quick_info("< Keyboard shortcuts.");
            ImGui::SameLine();
            if (ImGui::Button("Restart")) {
                m_torus.restart();
            }
            ImGui::SameLine();
            ImGui::Checkbox("Pause", &ctrl.pause);
            ImGui::PushButtonRepeat(true);
            ImGui::SameLine();
            if (ImGui::Button("+p")) {
                ctrl.extra_step = ctrl.pause ? ctrl.actual_step() : 0;
                ctrl.pause = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("+1")) {
                ctrl.extra_step = 1;
            }
            ImGui::PopButtonRepeat();
            ImGui::SameLine();
            imgui_StrTooltip("(?)", [] {
                imgui_Str("+p: ");
                ImGui::SameLine(0, 0);
                imgui_Str("Run manually (advance generation by step, controlled by the button/'N').");
                imgui_Str("+1: ");
                ImGui::SameLine(0, 0);
                imgui_Str("Advance generation by 1 instead of step. This is useful for changing the parity "
                          "of generation when (actual) step != 1.");
            });

            ImGui::Separator(); // To align with the left panel.

            assert(ctrl.anti_strobing);
            const bool is_strobing = strobing(ctrl.rule);
            std::string step_str = std::to_string(ctrl.step);
            if (is_strobing) {
                step_str += std::format(" -> {}", ctrl.actual_step());
            }

            imgui_StepSliderInt("Step", &ctrl.step, ctrl.step_min, ctrl.step_max, step_str.c_str());
            if (is_strobing) {
                ImGui::SameLine();
                imgui_StrTooltip("(?)",
                                 "As the current rule has '000...->1' and '111...->0', the step will be adjusted "
                                 "to 2*n to avoid bad visual effect.\n\n"
                                 "(You can change the parity of generation with the '+1' button.)");
            }

            imgui_StepSliderInt("Interval", &ctrl.gap, ctrl.gap_min, ctrl.gap_max,
                                std::format("{} ms", ctrl.gap * ctrl.gap_unit).c_str());

            if (test_key(ImGuiKey_R, false)) {
                m_torus.restart();
            } else if (test_key(ImGuiKey_1, true)) {
                ctrl.step = std::max(ctrl.step_min, ctrl.step - 1);
            } else if (test_key(ImGuiKey_2, true)) {
                ctrl.step = std::min(ctrl.step_max, ctrl.step + 1);
            } else if (test_key(ImGuiKey_3, true)) {
                ctrl.gap = std::max(ctrl.gap_min, ctrl.gap - 1);
            } else if (test_key(ImGuiKey_4, true)) {
                ctrl.gap = std::min(ctrl.gap_max, ctrl.gap + 1);
            } else if (test_key(ImGuiKey_Space, false)) {
                ctrl.pause = !ctrl.pause;
            } else if (test_key(ImGuiKey_N, true)) {
                ctrl.extra_step = ctrl.pause ? ctrl.actual_step() : 0;
                ctrl.pause = true;
            } else if (test_key(ImGuiKey_M, true)) {
                ctrl.extra_step = 1;
            }
        });
        ImGui::EndGroup();
        ImGui::SameLine(floor(1.5 * item_width));
        ImGui::BeginGroup();
        if (begin_popup_for_item(ImGui::Button("Init state"))) {
            set_init_state();
            ImGui::EndPopup();
        }
        quick_info("< Seed, density and area.");
        ImGui::SameLine();
        imgui_StrTooltip("(?)", set_init_state);

        ImGui::Spacing(); // To align with the separator.

        input_size();
        select_zoom();

        ImGui::EndGroup();
        ImGui::PopItemWidth();

        ImGui::Separator();

        ImGui::AlignTextToFramePadding();
        imgui_StrTooltip("(...)", [] {
            imgui_Str("Mouse operations:\n"
                      "1. Scroll in the window to zoom in/out.\n"
                      "2. When there is nothing to paste, you can drag with left button to move the window, or 'Ctrl + "
                      "left-drag' to \"rotate\" the space, or drag with right button to select area.\n"
                      "3. Otherwise, left-click to decide where to paste. To move the window you can drag with "
                      "right button. Rotating and selecting are not available in this mode.");
        });
        quick_info("v Mouse operations.");

        ImGui::SameLine();
        if (ImGui::Button("Center")) {
            locate_center = true;
            find_suitable_zoom = true;
        }

        ImGui::SameLine();
        static bool show_range_window = false;
        ImGui::Checkbox("Range ops", &show_range_window);
        quick_info("^ Copy, paste, clear, etc.\nv Drag with right button to select area.");
        ImGui::SameLine();
        imgui_StrDisabled("(?)");
        const bool show_range_window_in_tooltip = ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip);

        ImGui::SameLine(0, 0);
        ImGui::Text("  Generation:%d", m_torus.gen());

        ImGui::SameLine(0, 0);
        if (m_sel) {
            ImGui::Text("  Selected:%d*%d", m_sel->width(), m_sel->height());
            if (!m_sel->active) {
                ImGui::SameLine();
                if (ImGui::Button("Drop##S")) {
                    m_sel.reset();
                }
            }
        } else {
            imgui_Str("  Selected:N/A");
        }

        if (m_paste) {
            ImGui::SameLine(0, 0);
            const aniso::vecT size = m_paste->size();
            ImGui::Text("  Paste:%d*%d", size.x, size.y);
            ImGui::SameLine();
            if (ImGui::Button("Drop##P")) {
                m_paste.reset();
            }
        }

        {
            // (Values of GetContentRegionAvail() can be negative...)
            ImGui::InvisibleButton(canvas_name, ImMax(min_canvas_size, ImGui::GetContentRegionAvail()),
                                   ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
            const ImVec2 canvas_min = ImGui::GetItemRectMin();
            const ImVec2 canvas_max = ImGui::GetItemRectMax();
            const ImVec2 canvas_size = ImGui::GetItemRectSize();

            const bool active = ImGui::IsItemActive();
            const bool hovered = ImGui::IsItemHovered();
            const bool l_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
            const bool r_down = ImGui::IsMouseDown(ImGuiMouseButton_Right);
            if (active) {
                m_torus.pause_for_this_frame();
            }

            if (auto_fit) {
                locate_center = true;
                if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    const aniso::vecT size = from_imvec_floor((canvas_size - ImVec2(20, 20)) / m_coord.zoom);
                    if (m_torus.resize(size)) {
                        m_sel.reset();
                        m_paste.reset();
                    }
                }
            }

            // `m_torus` won't resize now.
            const aniso::vecT tile_size = m_torus.size();

            if (find_suitable_zoom) {
                // (wontfix) Poorly written, but works...
                // Select the largest zoom that can hold the entire tile.
                m_coord.zoom.slide(-100); // -> smallest.
                m_coord.zoom.select([&](bool, float z, const char*) {
                    const aniso::vecT size = m_torus.calc_size(from_imvec_floor((canvas_size - ImVec2(20, 20)) / z));
                    return size.both_gteq(tile_size);
                });
            }

            if (locate_center) {
                m_coord.bind(to_imvec(tile_size) / 2, canvas_size / 2);
                to_rotate = {0, 0};
            }

            if (m_sel && m_sel->active && (!r_down || m_paste || ImGui::IsItemDeactivated())) {
                m_sel->active = false;
                // Allow a single right-click to unselect the area.
                // (`bounding_box` has no size check like this. This is intentional.)
                if (m_sel->width() * m_sel->height() <= 2) {
                    m_sel.reset();
                }
            }

            std::optional<aniso::vecT> zoom_center = std::nullopt; // Not clamped.
            if (hovered) {
                const ImGuiIO& io = ImGui::GetIO();
                assert(ImGui::IsMousePosValid(&io.MousePos));
                const ImVec2 mouse_pos = io.MousePos - canvas_min;

                // TODO: this looks messy...
                if (active && (l_down || (m_paste && r_down))) {
                    if (!r_down && io.KeyCtrl) {
                        to_rotate += io.MouseDelta / m_coord.zoom;
                        const int dx = to_rotate.x, dy = to_rotate.y; // Truncate.
                        if (dx || dy) {
                            to_rotate -= ImVec2(dx, dy);
                            m_torus.rotate_00_to(dx, dy);
                        }
                    } else if (!auto_fit) {
                        m_coord.corner_pos -= io.MouseDelta / m_coord.zoom;
                    }
                }

                if (!auto_fit && imgui_MouseScrolling()) {
                    to_rotate = {0, 0};

                    const ImVec2 space_pos = m_coord.to_space(mouse_pos);
                    const ImVec2 space_pos_clamped = ImClamp(space_pos, {0, 0}, to_imvec(tile_size));
                    const ImVec2 mouse_pos_clamped = m_coord.to_canvas(space_pos_clamped); // Nearest point.
                    if (imgui_MouseScrollingDown()) {
                        m_coord.zoom.slide(-1);
                    } else if (imgui_MouseScrollingUp()) {
                        m_coord.zoom.slide(1);
                    }
                    m_coord.bind(space_pos_clamped, mouse_pos_clamped);
                }

                const aniso::vecT cel_pos = from_imvec_floor(m_coord.to_space(mouse_pos));

                if (m_coord.zoom <= 1 && !m_paste && (!active || false /* for testing */)) {
                    if (cel_pos.both_gteq({-10, -10}) && cel_pos.both_lt(tile_size.plus(10, 10))) {
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
                            zoom_center = cel_pos;
                        }
                    }
                }

                if (m_paste) {
                    assert(m_paste->size().both_lteq(tile_size));
                    paste_beg = aniso::clamp(cel_pos - m_paste->size() / 2, {0, 0}, tile_size - m_paste->size());
                } else {
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                        const aniso::vecT pos = aniso::clamp(cel_pos, {0, 0}, tile_size.plus(-1, -1));
                        m_sel = {.active = true, .beg = pos, .end = pos};
                    } else if (m_sel && m_sel->active && r_down) {
                        m_sel->end = aniso::clamp(cel_pos, {0, 0}, tile_size.plus(-1, -1));
                    }
                }
            }

            // Render.
            {
                const ImVec2 screen_min = ImFloor(canvas_min + m_coord.to_canvas({0, 0}));
                const ImVec2 screen_max = screen_min + to_imvec(tile_size) * m_coord.zoom;

                ImDrawList* const drawlist = ImGui::GetWindowDrawList();
                drawlist->PushClipRect(canvas_min, canvas_max);
                drawlist->AddRectFilled(canvas_min, canvas_max, IM_COL32_GREY(24, 255));

                const scaleE scale_mode = m_coord.zoom < 1 ? scaleE::Linear : scaleE::Nearest;
                if (!m_paste) {
                    const ImTextureID texture = make_screen(m_torus.read_only(), scale_mode);

                    drawlist->AddImage(texture, screen_min, screen_max);
                    if (zoom_center.has_value()) {
                        const auto clamped = clamp_window(tile_size, *zoom_center, {80, 60});
                        assert(!clamped.empty());

                        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
                        if (ImGui::BeginTooltip()) {
                            if (scale_mode == scaleE::Nearest) {
                                ImGui::Image(texture, to_imvec(clamped.size() * 3),
                                             to_imvec(clamped.begin) / to_imvec(tile_size),
                                             to_imvec(clamped.end) / to_imvec(tile_size));
                            } else {
                                // TODO: is it possible to reuse the texture in a different scale mode?
                                // (Related: https://github.com/ocornut/imgui/issues/7616)
                                ImGui::Image(make_screen(m_torus.read_only(clamped), scaleE::Nearest),
                                             to_imvec(clamped.size() * 3));
                            }

                            if (m_sel) {
                                const aniso::rangeT sel = aniso::common(clamped, m_sel->to_range());
                                if (!sel.empty()) {
                                    const ImVec2 zoom_min = ImGui::GetItemRectMin();
                                    ImGui::GetWindowDrawList()->AddRectFilled(
                                        zoom_min + to_imvec((sel.begin - clamped.begin) * 3),
                                        zoom_min + to_imvec((sel.end - clamped.begin) * 3), IM_COL32(0, 255, 0, 40));
                                }
                            }
                            ImGui::EndTooltip();
                        }
                        ImGui::PopStyleVar();
                    }
                } else {
                    assert(!zoom_center);
                    assert(m_paste->size().both_lteq(tile_size));
                    paste_beg = aniso::clamp(paste_beg, {0, 0}, tile_size - m_paste->size());
                    const aniso::vecT paste_end = paste_beg + m_paste->size();

                    ImTextureID texture = nullptr;
                    // (wontfix) Wasteful, but after all this works...
                    m_torus.read_and_maybe_write([&](const aniso::tile_ref tile) {
                        const aniso::tile_ref paste_area = tile.clip({paste_beg, paste_end});
                        aniso::tileT temp(paste_area);
                        // TODO: redesign pasting behavior. `blit` cannot work for periodic background.
                        // aniso::copy(paste_area, m_paste->data());
                        (background == 0 ? aniso::blit<aniso::blitE::Or>
                                         : aniso::blit<aniso::blitE::And>)(paste_area, m_paste->data());
                        texture = make_screen(tile, scale_mode);
                        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                            m_paste.reset();
                            return true;
                        } else { // Restore.
                            aniso::copy(paste_area, temp.data());
                            return false;
                        }
                    });

                    // (`paste_beg` and `paste_end` remain valid even if `paste` has been consumed.)
                    drawlist->AddImage(texture, screen_min, screen_max);
                    const ImVec2 paste_min = screen_min + to_imvec(paste_beg) * m_coord.zoom;
                    const ImVec2 paste_max = screen_min + to_imvec(paste_end) * m_coord.zoom;
                    drawlist->AddRectFilled(paste_min, paste_max, IM_COL32(255, 0, 0, 60));
                }

                if (m_sel) {
                    const auto [sel_beg, sel_end] = m_sel->to_range();
                    const ImVec2 sel_min = screen_min + to_imvec(sel_beg) * m_coord.zoom;
                    const ImVec2 sel_max = screen_min + to_imvec(sel_end) * m_coord.zoom;
                    drawlist->AddRectFilled(sel_min, sel_max, IM_COL32(0, 255, 0, 40));
                    // drawlist->AddRect(sel_min, sel_max, IM_COL32(0, 255, 0, 160));
                }
                drawlist->AddRect(screen_min, screen_max, ImGui::GetColorU32(ImGuiCol_TableBorderStrong));
                drawlist->PopClipRect();
            }

            // Range operations.
            {
                enum operationE {
                    _none,
                    _capture_closed,
                    _capture_open,
                    _random_fill,
                    _clear_inside,
                    _clear_outside,
                    _select_all,
                    _bounding_box,
                    _copy,
                    _cut,
                    _paste,
                    _identify
                };

                static bool replace = true;     // Closed-capture.
                static percentT fill_den = 0.5; // Random-fill.
                static bool add_rule = true;
                static bool show_result = true; // Copy / cut.
                auto copy_sel = [&] {
                    if (m_sel) {
                        std::string rle_str = aniso::to_RLE_str(m_torus.read_only(m_sel->to_range()),
                                                                add_rule ? &sync.current.rule : nullptr);
                        ImGui::SetClipboardText(rle_str.c_str());

                        if (show_result) {
                            messenger::set_msg(std::move(rle_str));
                        }
                    }
                };

                operationE op = _none;

                auto term = [&](const char* label, const char* shortcut, ImGuiKey key, bool use_sel, operationE op2) {
                    const bool enabled = !use_sel || m_sel.has_value();
                    if (ImGui::MenuItem(label, shortcut, nullptr, enabled) || (enabled && test_key(key, false))) {
                        op = op2;
                    }
                    if (!enabled) {
                        imgui_ItemTooltip("There is no selected area.");
                    }
                };

                // Pattern capturing.
                sync.display_if_enable_lock([&](bool /* visible */) {
                    ImGui::Separator();

                    ImGui::AlignTextToFramePadding();
                    imgui_StrTooltip(
                        "(...)",
                        "Open-capture: Record what there exists in the selected area (in the current frame, and "
                        "not including the border). The result will always be appended to the current lock.\n\n"
                        "Closed-capture: Run the selected area as torus space (with the current rule) and "
                        "record all mappings. Depending on the setting, the result will replace the current lock "
                        "directly, or will be appended to the current lock like open-capture.");
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Replace", replace)) {
                        replace = true;
                    }
                    ImGui::SameLine(0, imgui_ItemInnerSpacingX());
                    if (ImGui::RadioButton("Append", !replace)) {
                        replace = false;
                    }
                    ImGui::SameLine();
                    imgui_StrTooltip("(?)", "This affects only closed-capture. See '(...)' for explanation.");
                    term("Capture (open)", "O (repeatable)", ImGuiKey_O, true, _capture_open);
                    // (`ImGui::IsKeyPressed(..., repeat = true)` does not return true in every frame.)
                    if (m_sel && enable_shortcuts && ImGui::IsKeyDown(ImGuiKey_O)) {
                        op = _capture_open;
                    }
                    term("Capture (closed)", "P", ImGuiKey_P, true, _capture_closed);
                });

                auto range_operations = [&](const bool display) {
                    if (display) {
                        const auto set_tag = [](bool& tag, const char* label, const char* message) {
                            ImGui::Checkbox(label, &tag);
                            ImGui::SameLine();
                            imgui_StrTooltip("(?)", message);
                        };

                        ImGui::AlignTextToFramePadding();
                        imgui_Str("Background ~");
                        ImGui::SameLine(0, imgui_ItemInnerSpacingX());
                        if (ImGui::RadioButton("0", background == 0)) {
                            background = 0;
                        }
                        ImGui::SameLine(0, imgui_ItemInnerSpacingX());
                        if (ImGui::RadioButton("1", background == 1)) {
                            background = 1;
                        }
                        ImGui::SameLine();
                        imgui_StrTooltip(
                            "(?)", "'Clear inside/outside' and 'Cut' will clear the range with the value.\n"
                                   "'Bound' will get the bounding-box for the pattern that consists of !(value).\n"
                                   "'Paste' will use different pasting modes based on the value. "
                                   "When pasting into white background you need to set this to 1.");

                        // Filling.
                        ImGui::Separator();
                        fill_den.step_slide("Fill density");
                        term("Random fill", "+/=", ImGuiKey_Equal, true, _random_fill);
                        term("Clear inside", "Backspace", ImGuiKey_Backspace, true, _clear_inside);
                        term("Clear outside", "0 (zero)", ImGuiKey_0, true, _clear_outside);

                        ImGui::Separator();
                        term("Select all", "A", ImGuiKey_A, false, _select_all);
                        term("Bound", "B", ImGuiKey_B, true, _bounding_box);

                        // Copy/Cut/Paste.
                        // !!TODO: add tooltip...
                        ImGui::Separator();
                        set_tag(add_rule, "Rule info",
                                "Whether to include rule info ('rule = ...') in the header when copying patterns.");
                        ImGui::SameLine();
                        set_tag(show_result, "Show result", "Whether to display the result when copying patterns.");
                        term("Copy", "C", ImGuiKey_C, true, _copy);
                        term("Cut", "X", ImGuiKey_X, true, _cut);
                        term("Paste", "V", ImGuiKey_V, false, _paste);
                        term("Identify", "I (i)", ImGuiKey_I, true, _identify);
                    } else { // Shortcut only.
                        auto term2 = [&](ImGuiKey key, bool use_sel, operationE op2) {
                            const bool enabled = !use_sel || m_sel.has_value();
                            if (enabled && test_key(key, false)) {
                                op = op2;
                            }
                        };

                        term2(ImGuiKey_Equal, true, _random_fill);
                        term2(ImGuiKey_Backspace, true, _clear_inside);
                        term2(ImGuiKey_0, true, _clear_outside);
                        term2(ImGuiKey_A, false, _select_all);
                        term2(ImGuiKey_B, true, _bounding_box);
                        term2(ImGuiKey_C, true, _copy);
                        term2(ImGuiKey_X, true, _cut);
                        term2(ImGuiKey_V, false, _paste);
                        term2(ImGuiKey_I, true, _identify);
                    }
                };

                [&] {
                    if (show_range_window) {
                        ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
                        if (ImGui::IsMousePosValid()) {
                            ImGui::SetNextWindowPos(ImGui::GetMousePos() + ImVec2(2, 2), ImGuiCond_Appearing);
                        }
                        auto window =
                            imgui_Window("Range operations", &show_range_window,
                                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
                        if (window.visible) {
                            range_operations(true /* display */);
                            return; // Using lambda for this return.
                        }
                    }
                    if (show_range_window_in_tooltip && ImGui::BeginTooltip()) {
                        range_operations(true /* display */);
                        ImGui::EndTooltip();
                    } else {
                        range_operations(false /* shortcut only */);
                    }
                }();

                if (op == _capture_closed && m_sel) {
                    auto lock = capture_closed(m_torus.read_only(m_sel->to_range()), sync.current.rule);
                    if (!replace) {
                        aniso::for_each_code([&](aniso::codeT c) { lock[c] = lock[c] || sync.current.lock[c]; });
                    }
                    sync.set_lock(lock);
                } else if (op == _capture_open && m_sel) {
                    auto lock = sync.current.lock;
                    aniso::fake_apply(m_torus.read_only(m_sel->to_range()), lock);
                    sync.set_lock(lock);
                } else if (op == _random_fill && m_sel) {
                    // TODO: or `random_flip`?
                    aniso::random_fill(m_torus.write_only(m_sel->to_range()), global_mt19937(), fill_den.get());
                } else if (op == _clear_inside && m_sel) {
                    aniso::fill(m_torus.write_only(m_sel->to_range()), background);
                } else if (op == _clear_outside && m_sel) {
                    aniso::fill_outside(m_torus.write_only(), m_sel->to_range(), background);
                } else if (op == _select_all) {
                    if (!m_sel || m_sel->width() != tile_size.x || m_sel->height() != tile_size.y) {
                        m_sel = {.active = false, .beg = {0, 0}, .end = tile_size.plus(-1, -1)};
                    } else {
                        m_sel.reset();
                    }
                } else if (op == _bounding_box && m_sel) {
                    const aniso::rangeT sel_range = m_sel->to_range();
                    const auto [begin, end] = aniso::bounding_box(m_torus.read_only(sel_range), background);
                    if (begin != end) {
                        // (Should not be m_sel->beg here.)
                        m_sel = {
                            .active = false, .beg = sel_range.begin + begin, .end = sel_range.begin + end.plus(-1, -1)};
                    } else {
                        m_sel.reset();
                    }
                } else if (op == _copy && m_sel) {
                    copy_sel();
                } else if (op == _cut && m_sel) {
                    copy_sel();
                    aniso::fill(m_torus.write_only(m_sel->to_range()), background);
                } else if (op == _paste) {
                    // TODO: support pasting rule as well?
                    if (const char* text = ImGui::GetClipboardText()) {
                        m_paste.reset();
                        if (m_sel) {
                            m_sel->active = false;
                        }

                        aniso::from_RLE_str(text, [&](long long w, long long h) -> std::optional<aniso::tile_ref> {
                            if (w == 0 || h == 0) {
                                messenger::set_msg("Found no pattern.\n\n"
                                                   "('V' is for pasting patterns. If you want to read rules from the "
                                                   "clipboard, use the 'Clipboard' window instead.)");
                                return std::nullopt;
                            } else if (w > tile_size.x || h > tile_size.y) {
                                messenger::set_msg("The space is not large enough for the pattern.\n"
                                                   "Space size: x = {}, y = {}\n"
                                                   "Pattern size: x = {}, y = {}",
                                                   tile_size.x, tile_size.y, w, h);
                                return std::nullopt;
                            } else {
                                m_paste.emplace(aniso::vecT{.x = (int)w, .y = (int)h});
                                return m_paste->data();
                            }
                        });
                    }
                } else if (op == _identify) {
                    identify(m_torus.read_only(m_sel->to_range()), sync.current.rule);
                }
            }

            assert(tile_size == m_torus.size());
        }

        if (m_paste) {
            m_torus.pause_for_this_frame();
        }

        m_torus.end_frame();
    }
};

void apply_rule(sync_point& sync) {
    static runnerT runner;
    return runner.display(sync);
}

// TODO: support zoom = 0.5, 1, 2?
// TODO: apply initT?
void previewer::configT::_set() {
    ImGui::AlignTextToFramePadding();
    imgui_Str("Size ~");
    for (int i = 0; i < Count; ++i) {
        ImGui::SameLine(0, imgui_ItemInnerSpacingX());
        if (ImGui::RadioButton(size_terms[i].str, size == i)) {
            size = sizeE{i};
        }
    }
    ImGui::SetNextItemWidth(item_width);
    imgui_StepSliderInt("Seed", &seed, 0, 9);
    imgui_Str("Density ~ 0.5, area ~ 1.0");
    // TODO: what about background?

    ImGui::Separator();

    ImGui::SetNextItemWidth(item_width);
    imgui_StepSliderInt("Step", &step, 1, 16);
    imgui_Str("Interval ~ 0ms, zoom ~ 1");
}

// TODO: support ctrl + drag to rotate?
void previewer::_preview(uint64_t id, const configT& config, const aniso::ruleT& rule, bool interactive) {
    struct termT {
        bool active = false;
        int seed = {};
        aniso::ruleT rule = {};
        aniso::tileT tile = {};
    };
    static std::unordered_map<uint64_t, termT> terms;

    static unsigned latest = ImGui::GetFrameCount();
    if (const unsigned frame = ImGui::GetFrameCount(); frame != latest) {
        if (latest + 1 != frame) {
            terms.clear();
        } else {
            // According to https://en.cppreference.com/w/cpp/container/unordered_map/erase_if
            // There is no requirement on the predicate.
            std::erase_if(terms, [](std::pair<const uint64_t, termT>& pair) {
                return !std::exchange(pair.second.active, false);
            });
        }
        latest = frame;
    }

    const int width = config.width(), height = config.height();
    const aniso::vecT size{.x = config.width(), .y = config.height()};
    assert(ImGui::GetItemRectSize() == ImVec2(width, height));
    assert(ImGui::IsItemVisible());

    termT& term = terms[id];
    if (term.active) [[unlikely]] {
        imgui_ItemRect(IM_COL32_WHITE);
        return;
    }
    term.active = true;

    if (test_key(ImGuiKey_T, false) || (interactive && ImGui::IsItemClicked(ImGuiMouseButton_Right)) ||
        term.tile.size() != size || term.seed != config.seed || term.rule != rule) {
        term.tile.resize(size);
        term.seed = config.seed;
        term.rule = rule;
        std::mt19937 rand(config.seed);
        aniso::random_fill(term.tile.data(), rand, 0.5);
    }

    // (`IsItemActive` does not work as preview-window is based on `Dummy`.)
    const bool pause = interactive && ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left);
    if (!pause) {
        const int p = config.step + ((config.step % 2 == 1) && strobing(rule));
        for (int i = 0; i < p; ++i) {
            term.tile.run_torus(rule);
        }
    }

    // Intentionally display after running for better visual effect (the initial state of the tile won't be displayed).
    const ImTextureID texture = make_screen(term.tile.data(), scaleE::Nearest);
    ImGui::GetWindowDrawList()->AddImage(texture, ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    if (interactive && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
        assert(ImGui::IsMousePosValid());
        const ImVec2 pos = ImGui::GetMousePos() - ImGui::GetItemRectMin();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
        if (ImGui::BeginTooltip()) {
            const aniso::rangeT clamped = clamp_window(size, from_imvec_floor(pos), {64, 48});
            assert(!clamped.empty());
            ImGui::Image(texture, to_imvec(clamped.size() * 3), to_imvec(clamped.begin) / to_imvec(size),
                         to_imvec(clamped.end) / to_imvec(size));
            ImGui::EndTooltip();
        }
        ImGui::PopStyleVar();
    }
}
