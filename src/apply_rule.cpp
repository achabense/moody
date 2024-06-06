#include <unordered_map>

#include "tile.hpp"

#include "common.hpp"

// TODO: support boundless space...

using clockT = std::chrono::steady_clock;

static ImVec2 to_imvec(const aniso::vecT& vec) { return ImVec2(vec.x, vec.y); }

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

// Copy the subrange and run as a torus space, recording all invoked mappings.
// This is good at capturing "self-contained" patterns (oscillators/spaceships).
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

// `capture_closed` is not suitable for capturing patterns that are not self-contained (what happens in the
// copied subrange (treated as torus space) is not exactly what we see in the source space)
// In these cases we need a way to record what actually happened in the range.
static void capture_open(const aniso::tile_const_ref tile, aniso::moldT::lockT& lock) { //
    aniso::fake_apply(tile, lock);
}

class densityT {
    int m_dens; // ∈ [0, 100], /= 100 to serve as density.
public:
    densityT(double density) { m_dens = std::clamp(density, 0.0, 1.0) * 100; }

    double get() const { return m_dens / 100.0; }
    void step_slide(const char* label) {
        imgui_StepSliderInt(label, &m_dens, 0, 100, std::format("{:.2f}", m_dens / 100.0).c_str());
    }

    friend bool operator==(const densityT&, const densityT&) = default;
};

// TODO: support setting patterns as init state?
struct initT {
    static constexpr aniso::vecT size_min{.x = 20, .y = 15};
    static constexpr aniso::vecT size_max{.x = 1600, .y = 1200};
    static aniso::vecT size_clamped(aniso::vecT size) { return aniso::clamp(size, size_min, size_max); }

    aniso::vecT size;
    int seed;
    densityT density;

    friend bool operator==(const initT&, const initT&) = default;
};

static void restart(aniso::tileT& tile, const initT& init) {
    tile.resize(init.size);

    std::mt19937 rand(uint32_t(init.seed));
    aniso::random_fill(tile.data(), rand, init.density.get());
}

class zoomT {
    struct termT {
        float val;
        const char* str;
    };
    static constexpr termT terms[]{{0.5, "0.5"}, {1, "1"}, {2, "2"}, {3, "3"}, {4, "4"}, {6, "6"}};

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
        for (int i = 0; const auto [z, s] : terms) {
            const int this_i = i++;
            if (fn(m_index == this_i, z, s)) {
                m_index = this_i;
            }
        }
    }

    static constexpr float min() { return terms[0].val; }
    static constexpr float max() { return terms[index_max].val; }
    operator float() const {
        assert(m_index >= 0 && m_index <= index_max);
        return terms[m_index].val;
    }
};

class runnerT {
    struct ctrlT {
        aniso::ruleT rule{};

        static constexpr int pace_min = 1, pace_max = 80;
        int pace = 1;
        bool anti_strobing = true;
        int actual_pace() const {
            if (anti_strobing && strobing(rule) && pace % 2) {
                return pace + 1;
            }
            return pace;
        }

        static constexpr int gap_unit = 25; // ms.
        static constexpr int gap_min = 0, gap_max = 20;
        int gap = 0;

        bool pause = false;

        int extra_step = 0;

        clockT::time_point last_time = {};
        int calc_step() {
            int step = std::exchange(extra_step, 0);
            if (step == 0) {
                if (!pause && (last_time + std::chrono::milliseconds{gap * gap_unit} <= clockT::now())) {
                    step = actual_pace();
                }
            }

            if (step != 0) {
                last_time = clockT::now();
            }
            return step;
        }
    };

    class torusT_ex {
        initT m_init{.size{.x = 600, .y = 400}, .seed = 0, .density = 0.5};
        aniso::tileT m_torus{};
        ctrlT m_ctrl{.rule{}, .pace = 1, .anti_strobing = true, .gap = 0, .pause = false};
        int m_gen = 0;

        bool extra_pause = false;
        void mark_written() {
            extra_pause = true;
            m_ctrl.last_time = clockT::now();
        }

    public:
        torusT_ex() { restart(); }

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
            ::restart(m_torus, m_init);
            m_gen = 0;
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

        void rotate(int dx, int dy) {
            if (dx != 0 || dy != 0) {
                aniso::tileT temp(m_torus.size());
                aniso::rotate_copy(temp.data(), m_torus.data(), {.x = dx, .y = dy});
                m_torus.swap(temp);
            }
        }

        int area() const { return m_torus.size().xy(); }
        int gen() const { return m_gen; }
        aniso::vecT size() const {
            assert(m_init.size == m_torus.size());
            assert(m_init.size == m_init.size_clamped(m_init.size));
            return m_init.size;
        }

        void set_ctrl(const auto& fn) { fn(m_ctrl); }
        void set_init(const auto& fn) {
            initT init = m_init;
            fn(init);
            if (init != m_init) {
                m_init = init;
                restart();
            }
        }

        void end_frame() {
            if (!extra_pause) {
                const int count = m_ctrl.calc_step();
                for (int c = 0; c < count; ++c) {
                    m_torus.run_torus(m_ctrl.rule);
                    ++m_gen;
                }
            }
        }
    };

    torusT_ex m_torus{}; // Space.

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

    // (Workaround: it's hard to get canvas-size in this frame when it's needed; this looks horrible but
    // will work well in all cases)
    static constexpr ImVec2 min_canvas_size{initT::size_min.x * zoomT::max(), initT::size_min.y* zoomT::max()};
    ImVec2 last_known_canvas_size = min_canvas_size;

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

        const char* const canvas_name = "Canvas";
        const bool enable_shortcuts =
            (!ImGui::GetIO().WantCaptureKeyboard && !ImGui::IsPopupOpen(ImGuiID(0), ImGuiPopupFlags_AnyPopupId)) ||
            (GImGui->ActiveId == ImGui::GetID(canvas_name));
        // Shadowing `::test_key`.
        auto test_key = [enable_shortcuts](ImGuiKey key, bool repeat) {
            return enable_shortcuts && ImGui::IsKeyPressed(key, repeat);
        };
        static bool background = 0; // TODO: move elsewhere...
        static bool locate_center = true;

        {
            // (Keeping in line with edit-rule's.)
            if (ImGui::Button("Restart")) {
                m_torus.restart();
            }
            ImGui::SameLine();
            ImGui::Text("Generation:%d, density:%.4f", m_torus.gen(),
                        double(aniso::count(m_torus.read_only())) / m_torus.area());
            ImGui::Separator();
        }

        ImGui::PushItemWidth(item_width);
        ImGui::BeginGroup();
        m_torus.set_ctrl([&](ctrlT& ctrl) {
            ImGui::AlignTextToFramePadding();
            imgui_StrTooltip("(...)", "Keyboard shortcuts:\n"
                                      "R: Restart    Space: Pause\nN/M (repeatable): +p/+1\n"
                                      "1/2 (repeatable): -/+ Pace\n3/4 (repeatable): -/+ Gap time\n");
            quick_info("< Keyboard shortcuts.");
            ImGui::SameLine();
            ImGui::Checkbox("Pause", &ctrl.pause);
            ImGui::PushButtonRepeat(true);
            ImGui::SameLine();
            if (ImGui::Button(std::format("+p({})###+p", ctrl.actual_pace()).c_str())) {
                ctrl.extra_step = ctrl.pause ? ctrl.actual_pace() : 0;
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
                imgui_Str("Run manually (advance generation by actual-pace, controlled by the button/'N').");
                imgui_Str("+1: ");
                ImGui::SameLine(0, 0);
                imgui_Str("Advance generation by 1 instead of actual-pace. This is useful for changing the parity "
                          "of generation when actual-pace != 1.");
            });

            imgui_StepSliderInt("Pace", &ctrl.pace, ctrl.pace_min, ctrl.pace_max);

            imgui_StepSliderInt("Gap time", &ctrl.gap, ctrl.gap_min, ctrl.gap_max,
                                std::format("{} ms", ctrl.gap * ctrl.gap_unit).c_str());

            // TODO: this would better be explained with examples.
            // How to make interactive tooltips?
            assert(ctrl.anti_strobing);
            ImGui::BeginDisabled();
            ImGui::Checkbox("Anti-strobing", &ctrl.anti_strobing);
            ImGui::SameLine();
            ImGui::Text("(Actual pace: %d)", ctrl.actual_pace());
            ImGui::EndDisabled();
            ImGui::SameLine();
            imgui_StrTooltip("(?)",
                             "When there are '000...->1' and '111...->0', the pace will be adjusted to 2*n to avoid "
                             "bad visual effect.\n"
                             "In these cases, you can try the '+1' button to change the parity of generation.");

            if (test_key(ImGuiKey_R, false)) {
                m_torus.restart();
            } else if (test_key(ImGuiKey_1, true)) {
                ctrl.pace = std::max(ctrl.pace_min, ctrl.pace - 1);
            } else if (test_key(ImGuiKey_2, true)) {
                ctrl.pace = std::min(ctrl.pace_max, ctrl.pace + 1);
            } else if (test_key(ImGuiKey_3, true)) {
                ctrl.gap = std::max(ctrl.gap_min, ctrl.gap - 1);
            } else if (test_key(ImGuiKey_4, true)) {
                ctrl.gap = std::min(ctrl.gap_max, ctrl.gap + 1);
            } else if (test_key(ImGuiKey_Space, false)) {
                ctrl.pause = !ctrl.pause;
            } else if (test_key(ImGuiKey_N, true)) {
                ctrl.extra_step = ctrl.pause ? ctrl.actual_pace() : 0;
                ctrl.pause = true;
            } else if (test_key(ImGuiKey_M, true)) {
                ctrl.extra_step = 1;
            }
        });
        ImGui::EndGroup();
        ImGui::SameLine(0, imgui_ItemSpacingX() + ImGui::CalcTextSize("  ").x);
        ImGui::BeginGroup();
        m_torus.set_init([&](initT& init) {
            const auto old_size = init.size;

            imgui_StepSliderInt("Init seed", &init.seed, 0, 29);
            if (ImGui::IsItemActive()) {
                m_torus.pause_for_this_frame();
            }
            init.density.step_slide("Init density");
            if (ImGui::IsItemActive()) {
                m_torus.pause_for_this_frame();
            }

            {
                static char input_width[20]{}, input_height[20]{};
                const auto filter = [](ImGuiInputTextCallbackData* data) -> int {
                    return (data->EventChar >= '0' && data->EventChar <= '9') ? 0 : 1;
                };
                const float s = imgui_ItemInnerSpacingX();
                const float w = (ImGui::CalcItemWidth() - s * 2 - ImGui::CalcTextSize("Resize").x -
                                 ImGui::GetStyle().FramePadding.x * 2) /
                                2;
                ImGui::SetNextItemWidth(floor(w));
                ImGui::InputTextWithHint("##Width", "Width", input_width, std::size(input_width),
                                         ImGuiInputTextFlags_CallbackCharFilter, filter);
                ImGui::SameLine(0, s);
                ImGui::SetNextItemWidth(ceil(w));
                ImGui::InputTextWithHint("##Height", "Height", input_height, std::size(input_height),
                                         ImGuiInputTextFlags_CallbackCharFilter, filter);
                ImGui::SameLine(0, s);
                if (ImGui::Button("Resize")) {
                    int width = init.size.x, height = init.size.y;
                    const bool has_w = std::from_chars(input_width, std::end(input_width), width).ec == std::errc{};
                    const bool has_h = std::from_chars(input_height, std::end(input_height), height).ec == std::errc{};
                    // ~ the value is unmodified if `from_chars` fails.
                    if (has_w || has_h) {
                        m_coord.zoom.set_1();
                        init.size = init.size_clamped({.x = width, .y = height});
                        locate_center = true;
                    }
                    input_width[0] = '\0';
                    input_height[0] = '\0';
                }

                ImGui::SameLine(0, s);
                ImGui::Text("Size = %d*%d", init.size.x, init.size.y);
            }

            ImGui::AlignTextToFramePadding();
            imgui_Str("Zoom =");
            m_coord.zoom.select([&](const bool is_cur, const float z, const char* const s) {
                ImGui::SameLine(0, imgui_ItemInnerSpacingX());
                if (z < 1) {
                    ImGui::BeginDisabled();
                }
                const bool sel = ImGui::RadioButton(s, is_cur);
                if (z < 1) {
                    ImGui::EndDisabled();
                }

                if (sel) {
                    init.size = init.size_clamped(
                        {.x = (int)(last_known_canvas_size.x / z), .y = (int)(last_known_canvas_size.y / z)});
                    locate_center = true;
                }
                return sel;
            });
            ImGui::SameLine();
            imgui_StrTooltip("(?)", "Click to resize to full-screen.");

            if (old_size != init.size) {
                m_sel.reset();
                m_paste.reset();
            }
        });
        ImGui::EndGroup();
        ImGui::PopItemWidth();

        ImGui::Separator();

        const aniso::vecT tile_size = m_torus.size();

        static bool lock_mouse = false; // Lock scrolling control and window moving.
        ImGui::AlignTextToFramePadding();
        imgui_StrTooltip(lock_mouse ? "[...]" : "(...)", [] {
            // TODO: is "window" ambiguous here?
            imgui_Str(
                "Mouse operations:\n"
                "1. Scroll in the window to zoom in/out.\n"
                "2. When there is nothing to paste, you can drag with left button to move the window, or 'Ctrl + "
                "left-drag' to \"rotate\" the space, or drag with right button to select area (to copy, clear, etc.).\n"
                "3. Otherwise, left-click to decide where to paste. In this case, to move the window you can drag with "
                "right button. (Rotating and selecting are not available when there are patterns to paste.)");
            ImGui::Separator();
            ImGui::Text("(You can right-click this '%s' to enable/disable scrolling and window moving.)",
                        lock_mouse ? "[...]" : "(...)");
            bool enabled = !lock_mouse;
            ImGui::Checkbox("Enabling scrolling and window moving", &enabled);
        });
        quick_info("v Mouse operations.");
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            lock_mouse = !lock_mouse;
        }

        ImGui::SameLine();
        if (ImGui::Button("Corner")) {
            m_coord.bind({0, 0}, {0, 0});
            to_rotate = {0, 0};
        }
        ImGui::SameLine();
        if (ImGui::Button("Center")) {
            locate_center = true;
        }
        ImGui::SameLine();
        static bool show_range_window = false;
        ImGui::Checkbox("Range operations", &show_range_window);
        quick_info("^ Copy, paste, clear, etc.\nv Drag with right button to select area.");

        ImGui::SameLine();
        if (m_sel) {
            ImGui::Text("  Selected:%d*%d", m_sel->width(), m_sel->height());
            if (!m_sel->active) {
                ImGui::SameLine();
                if (ImGui::SmallButton("Drop##S")) {
                    m_sel.reset();
                }
            }
        } else {
            imgui_Str("  Selected:N/A");
        }

        if (m_paste) {
            ImGui::SameLine();
            const aniso::vecT size = m_paste->size();
            ImGui::Text("  Paste:%d*%d", size.x, size.y);
            ImGui::SameLine();
            if (ImGui::SmallButton("Drop##P")) {
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
            last_known_canvas_size = canvas_size;
            if (std::exchange(locate_center, false)) {
                m_coord.bind(to_imvec(tile_size) / 2, canvas_size / 2);
                to_rotate = {0, 0};
            }

            const bool active = ImGui::IsItemActive();
            const bool hovered = ImGui::IsItemHovered();
            const bool l_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
            const bool r_down = ImGui::IsMouseDown(ImGuiMouseButton_Right);
            if (active) {
                m_torus.pause_for_this_frame();
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
                            m_torus.rotate(dx, dy);
                        }
                    } else if (!lock_mouse) {
                        m_coord.corner_pos -= io.MouseDelta / m_coord.zoom;
                    }
                }

                if (!lock_mouse && imgui_MouseScrolling()) {
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

                const ImVec2 space_pos = m_coord.to_space(mouse_pos);
                const aniso::vecT cel_pos{.x = (int)floor(space_pos.x), .y = (int)floor(space_pos.y)};

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
                drawlist->AddRectFilled(canvas_min, canvas_max, IM_COL32(32, 32, 32, 255));

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
                    _paste
                };

                static bool replace = true;     // Closed-capture.
                static densityT fill_den = 0.5; // Random-fill.
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

                auto range_operations = [&](const bool shortcut_only) {
                    static auto set_tag = [](bool& tag, const char* label, const char* message) {
                        ImGui::Checkbox(label, &tag);
                        ImGui::SameLine();
                        imgui_StrTooltip("(?)", message);
                    };

                    if (!shortcut_only) {
                        ImGui::AlignTextToFramePadding();
                        imgui_Str("Background =");
                        ImGui::SameLine(0, imgui_ItemInnerSpacingX());
                        if (ImGui::RadioButton("0", background == 0)) {
                            background = 0;
                        }
                        ImGui::SameLine(0, imgui_ItemInnerSpacingX());
                        if (ImGui::RadioButton("1", background == 1)) {
                            background = 1;
                        }
                        ImGui::SameLine();
                        imgui_StrTooltip("(?)", "Treat 0 (black) or 1 (white) as background value. "
                                                "This affects the behavior of clearing, shrinking and pasting mode.\n\n"
                                                "'Clear inside/outside' will fill the range with (background).\n"
                                                "'Bound' will get the bounding-box for !(background).\n"
                                                "'Paste' will use different pasting modes based on (background). "
                                                "(When pasting into the white background you need to set this to 1.)");

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
                        ImGui::Separator();
                        set_tag(add_rule, "Add rule",
                                "Whether to add rule info ('rule = ...') to the header when copying patterns.");
                        ImGui::SameLine();
                        set_tag(show_result, "Show result", "Whether to display the result when copying patterns.");
                        term("Copy", "C", ImGuiKey_C, true, _copy);
                        term("Cut", "X", ImGuiKey_X, true, _cut);
                        term("Paste", "V", ImGuiKey_V, false, _paste);
                    } else {
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
                    }
                };

                if (show_range_window) {
                    ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
                    if (ImGui::IsMousePosValid()) {
                        ImGui::SetNextWindowPos(ImGui::GetIO().MousePos + ImVec2(2, 2), ImGuiCond_Appearing);
                    }
                    auto window = imgui_Window("Range operations", &show_range_window,
                                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
                    range_operations(!window.visible /* shortcut-only if !visible */);
                } else {
                    range_operations(true /* shortcut-only */);
                }

                if (op == _capture_closed && m_sel) {
                    auto lock = capture_closed(m_torus.read_only(m_sel->to_range()), sync.current.rule);
                    if (!replace) {
                        aniso::for_each_code([&](aniso::codeT c) { lock[c] = lock[c] || sync.current.lock[c]; });
                    }
                    sync.set_lock(lock);
                } else if (op == _capture_open && m_sel) {
                    auto lock = sync.current.lock;
                    capture_open(m_torus.read_only(m_sel->to_range()), lock);
                    sync.set_lock(lock);
                } else if (op == _random_fill && m_sel) {
                    aniso::random_fill(m_torus.write_only(m_sel->to_range()), global_mt19937(), fill_den.get());
                } else if (op == _clear_inside && m_sel) {
                    aniso::clear(m_torus.write_only(m_sel->to_range()), background);
                } else if (op == _clear_outside && m_sel) {
                    aniso::clear_outside(m_torus.write_only(), m_sel->to_range(), background);
                } else if (op == _select_all) {
                    if (!m_sel || m_sel->width() != tile_size.x || m_sel->height() != tile_size.y) {
                        m_sel = {.active = false, .beg = {0, 0}, .end = tile_size.plus(-1, -1)};
                    } else {
                        m_sel.reset();
                    }
                } else if (op == _bounding_box && m_sel) {
                    const auto [begin, end] = aniso::bounding_box(m_torus.read_only(), m_sel->to_range(), background);
                    if (begin != end) {
                        m_sel = {.active = false, .beg = begin, .end = end.plus(-1, -1)};
                    } else {
                        m_sel.reset();
                    }
                } else if (op == _copy && m_sel) {
                    copy_sel();
                } else if (op == _cut && m_sel) {
                    copy_sel();
                    aniso::clear(m_torus.write_only(m_sel->to_range()));
                } else if (op == _paste) {
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
                }
            }
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

void previewer::configT::_set() {
    ImGui::AlignTextToFramePadding();
    imgui_Str("Size =");
    for (int i = 0; i < Count; ++i) {
        ImGui::SameLine(0, imgui_ItemInnerSpacingX());
        if (ImGui::RadioButton(size_terms[i].str, size == i)) {
            size = sizeE{i};
        }
    }
    ImGui::SetNextItemWidth(item_width);
    imgui_StepSliderInt("Init seed", &seed, 0, 9);
    imgui_Str("Init density = 0.5");

    ImGui::Separator();

    ImGui::SetNextItemWidth(item_width);
    imgui_StepSliderInt("Pace (1~10)", &pace, 1, 10);
    imgui_Str("Gap time = 0ms, anti-strobing = true");
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
        imgui_ItemRect(IM_COL32(255, 255, 255, 255));
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
        const int p = config.pace + ((config.pace % 2 == 1) && strobing(rule));
        for (int i = 0; i < p; ++i) {
            term.tile.run_torus(rule);
        }
    }

    // Intentionally display after running for better visual effect (the initial state of the tile won't be displayed).
    const ImTextureID texture = make_screen(term.tile.data(), scaleE::Nearest);
    ImGui::GetWindowDrawList()->AddImage(texture, ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    if (interactive && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
        assert(ImGui::IsMousePosValid());
        const ImVec2 pos = ImGui::GetIO().MousePos - ImGui::GetItemRectMin();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
        if (ImGui::BeginTooltip()) {
            const aniso::rangeT clamped = clamp_window(size, {.x = int(pos.x), .y = int(pos.y)}, {64, 48});
            assert(!clamped.empty());
            ImGui::Image(texture, to_imvec(clamped.size() * 3), to_imvec(clamped.begin) / to_imvec(size),
                         to_imvec(clamped.end) / to_imvec(size));
            ImGui::EndTooltip();
        }
        ImGui::PopStyleVar();
    }
}
