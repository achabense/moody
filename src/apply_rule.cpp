#include <unordered_map>

#include "tile.hpp"

#include "common.hpp"

// TODO: support boundless space...

using clockT = std::chrono::steady_clock;

[[nodiscard]] static ImTextureID make_screen(const aniso::tileT& tile, scaleE scale) {
    return make_screen(tile.width(), tile.height(), scale, [&tile](int y) { return tile.line(y); });
}

[[nodiscard]] static ImTextureID make_screen(const aniso::tileT& tile, const aniso::tileT::rangeT& range,
                                             scaleE scale) {
    assert(tile.has_range(range));
    return make_screen(range.width(), range.height(), scale,
                       [&tile, &range](int y) { return tile.line(y + range.begin.y) + range.begin.x; });
}

static void zoom_image(ImTextureID texture /* Using `scaleE::Nearest` */, ImVec2 texture_size, ImVec2 region_center,
                       ImVec2 region_size, int zoom) {
    region_size = ImMin(texture_size, region_size);
    const ImVec2 min_pos = ImClamp(region_center - ImFloor(region_size / 2), ImVec2(0, 0), texture_size - region_size);
    const ImVec2 max_pos = min_pos + region_size;

    ImGui::Image(texture, region_size * zoom, min_pos / texture_size, max_pos / texture_size);
}

// Always use `scaleE::Nearest`.
static void zoom_image(const aniso::tileT& tile, aniso::tileT::posT region_center, aniso::tileT::sizeT region_size,
                       int zoom) {
    region_size.width = std::min(tile.width(), region_size.width);
    region_size.height = std::min(tile.height(), region_size.height);
    const aniso::tileT::posT begin{
        .x = std::clamp(region_center.x - region_size.width / 2, 0, tile.width() - region_size.width),
        .y = std::clamp(region_center.y - region_size.height / 2, 0, tile.height() - region_size.height)};

    ImGui::Image(make_screen(tile, {begin, begin + region_size}, scaleE::Nearest),
                 ImVec2(region_size.width * zoom, region_size.height * zoom));
}

static bool strobing(const aniso::ruleT& rule) {
    constexpr aniso::codeT all_0{0}, all_1{511};
    return rule[all_0] == 1 && rule[all_1] == 0;
}

// `tileT` is able to serve as the building block for boundless space (`tileT::gather` was
// initially designed to enable this extension) but that's complicated, and torus space is enough to
// show the effect of the rules.
static void run_torus(aniso::tileT& tile, aniso::tileT& temp, const aniso::rule_like auto& rule) {
    assert(&tile != &temp);

    tile.gather_torus();
    tile.apply_v2(rule, temp);
    tile.swap(temp);
}

// Copy the subrange and run as a torus space, recording all invoked mappings.
// This is good at capturing "self-contained" patterns (oscillators/spaceships).
static aniso::moldT::lockT capture_closed(const aniso::tileT& source, const aniso::tileT::rangeT& range,
                                          const aniso::ruleT& rule) {
    aniso::tileT tile = aniso::copy(source, range);
    aniso::tileT temp(tile.size());

    aniso::moldT::lockT lock{};

    // (wontfix) It's possible that the loop fails to catch all invocations in very rare cases,
    // due to that `limit` is not large enough.

    // Loop until there has been `limit` generations without newly invoked mappings.
    const int limit = 120;
    for (int g = limit; g > 0; --g) {
        run_torus(tile, temp, [&](aniso::codeT code) {
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
static void capture_open(const aniso::tileT& source, aniso::tileT::rangeT range, aniso::moldT::lockT& lock) {
    if (range.width() <= 2 || range.height() <= 2) {
        return;
    }
    range.begin.x += 1;
    range.begin.y += 1;
    range.end.x -= 1;
    range.end.y -= 1;
    source.collect(range, lock);
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

class torusT {
    aniso::tileT m_tile, m_temp;
    int m_gen;

public:
    struct initT {
        aniso::tileT::sizeT size;
        uint32_t seed;
        densityT density;

        friend bool operator==(const initT&, const initT&) = default;
    };

    explicit torusT(const initT& init) : m_tile(init.size), m_temp(init.size), m_gen(0) { restart(init); }

    aniso::tileT& tile() { return m_tile; }
    const aniso::tileT& tile() const { return m_tile; }
    int gen() const { return m_gen; }

    void restart(const initT& init) {
        m_tile.resize(init.size);
        m_temp.resize(init.size);

        std::mt19937 rand(init.seed);
        aniso::random_fill(m_tile, rand, init.density.get());

        m_gen = 0;
    }

    void run(const aniso::ruleT& rule, int count = 1) {
        for (int c = 0; c < count; ++c) {
            run_torus(m_tile, m_temp, rule);
            ++m_gen;
        }
    }

    // (0, 0) is mapped to (wrap(dx), wrap(dy)).
    void rotate(int dx, int dy) {
        const int width = m_tile.width(), height = m_tile.height();

        const auto wrap = [](int v, int r) { return ((v % r) + r) % r; };
        dx = wrap(dx, width);
        dy = wrap(dy, height);
        assert(dx >= 0 && dx < width && dy >= 0 && dy < height);
        if (dx == 0 && dy == 0) {
            return;
        }

        [[maybe_unused]] const bool test = m_tile.line(0)[0];
        m_temp.resize(m_tile.size());
        for (int y = 0; y < height; ++y) {
            const bool* source = m_tile.line(y);
            bool* dest = m_temp.line((y + dy) % height);
            std::copy_n(source, width - dx, dest + dx);
            std::copy_n(source + width - dx, dx, dest);
        }
        m_tile.swap(m_temp);
        assert(test == m_tile.line(dy)[dx]);
    }
};

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
    static constexpr aniso::tileT::sizeT min_size{.width = 20, .height = 15};
    static constexpr aniso::tileT::sizeT max_size{.width = 1600, .height = 1200};
    static aniso::tileT::sizeT size_clamped(aniso::tileT::sizeT size) {
        return {.width = std::clamp(size.width, min_size.width, max_size.width),
                .height = std::clamp(size.height, min_size.height, max_size.height)};
    }

    static constexpr ImVec2 min_canvas_size{min_size.width * zoomT::max(), min_size.height* zoomT::max()};

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

        // TODO: pause for some fixed time for user-editions?
        clockT::time_point last_written = {};
        void mark_written() { last_written = clockT::now(); }

        void run(torusT& torus, int extra_step, bool extra_pause) {
            if (extra_step == 0) {
                if (!pause && !extra_pause &&
                    (last_written + std::chrono::milliseconds{gap * gap_unit} <= clockT::now())) {
                    extra_step = actual_pace();
                }
            }

            if (extra_step != 0) {
                mark_written();
                torus.run(rule, extra_step);
            }
        }
    };

    // TODO: support setting patterns as init state?
    torusT::initT m_init{.size{.width = 500, .height = 400}, .seed = 0, .density = 0.5};
    torusT m_torus{m_init};
    ctrlT m_ctrl{.rule{}, .pace = 1, .anti_strobing = true, .gap = 0, .pause = false};

    ImVec2 screen_off = {0, 0};
    zoomT screen_zoom{};

    // (Workaround: it's hard to get canvas-size in this frame when it's needed; this looks horrible but
    // will work well in all cases)
    ImVec2 last_known_canvas_size = min_canvas_size;

    std::optional<aniso::tileT> paste = std::nullopt;
    aniso::tileT::posT paste_beg{0, 0}; // Valid if paste.has_value().

    struct selectT {
        bool active = true;
        aniso::tileT::posT beg{0, 0}, end{0, 0}; // [] instead of [).

        aniso::tileT::rangeT to_range() const {
            const auto [xmin, xmax] = std::minmax(beg.x, end.x);
            const auto [ymin, ymax] = std::minmax(beg.y, end.y);

            return {.begin{xmin, ymin}, .end{xmax + 1, ymax + 1}};
        }

        int width() const { return std::abs(beg.x - end.x) + 1; }
        int height() const { return std::abs(beg.y - end.y) + 1; }
    };
    std::optional<selectT> m_sel = std::nullopt;

public:
    // TODO: redesign pause logics...
    // TODO: better control logics... (`display` is horribly written due to unorganized control logics...)
    // TODO: (wontfix?) there cannot actually be multiple instances in the program.
    // For example, there are a lot of static variables in `display`, and the keyboard controls are not designed
    // for per-object use.

    void display(sync_point& sync) {
        bool temp_pause = false;
        if (m_ctrl.rule != sync.current.rule) {
            m_ctrl.rule = sync.current.rule;
            m_ctrl.anti_strobing = true;
            m_ctrl.pause = false;
            m_torus.restart(m_init);
            m_ctrl.mark_written();

            temp_pause = true;
        }

        const char* const canvas_name = "Canvas";
        const bool enable_shortcuts =
            !ImGui::GetIO().WantCaptureKeyboard || (GImGui->ActiveId == ImGui::GetID(canvas_name));
        // Shadowing `::test_key`.
        auto test_key = [enable_shortcuts](ImGuiKey key, bool repeat) {
            return enable_shortcuts && ImGui::IsKeyPressed(key, repeat);
        };
        static bool background = 0; // TODO: move elsewhere...

        assert(m_init.size == m_torus.tile().size());
        assert(m_init.size == size_clamped(m_init.size));
        assert(screen_off.x == int(screen_off.x) && screen_off.y == int(screen_off.y));

        int extra_step = 0;
        auto restart = [&] {
            m_torus.restart(m_init);
            m_ctrl.mark_written();
            temp_pause = true;
        };

        {
            // (Keeping in line with edit-rule's.)
            if (ImGui::Button("Restart")) {
                restart();
            }
            ImGui::SameLine();
            ImGui::Text("Generation:%d, density:%.4f", m_torus.gen(),
                        float(aniso::count(m_torus.tile())) / m_torus.tile().area());
            ImGui::Separator();
        }

        ImGui::PushItemWidth(item_width);
        ImGui::BeginGroup();
        {
            ImGui::AlignTextToFramePadding();
            imgui_StrTooltip("(...)", "Keyboard shortcuts:\n"
                                      "R: Restart    Space: Pause\nN/M (repeatable): +p/+1\n"
                                      "1/2 (repeatable): -/+ Pace\n3/4 (repeatable): -/+ Gap time\n");
            quick_info("< Keyboard shortcuts.");
            ImGui::SameLine();
            ImGui::Checkbox("Pause", &m_ctrl.pause);
            ImGui::PushButtonRepeat(true);
            ImGui::SameLine();
            if (ImGui::Button(std::format("+p({})###+p", m_ctrl.actual_pace()).c_str())) {
                extra_step = m_ctrl.pause ? m_ctrl.actual_pace() : 0;
                m_ctrl.pause = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("+1")) {
                extra_step = 1;
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

            imgui_StepSliderInt("Pace", &m_ctrl.pace, m_ctrl.pace_min, m_ctrl.pace_max);

            imgui_StepSliderInt("Gap time", &m_ctrl.gap, m_ctrl.gap_min, m_ctrl.gap_max,
                                std::format("{} ms", m_ctrl.gap * m_ctrl.gap_unit).c_str());

            // TODO: this would better be explained with examples.
            // How to make interactive tooltips?
            assert(m_ctrl.anti_strobing);
            ImGui::BeginDisabled();
            ImGui::Checkbox("Anti-strobing", &m_ctrl.anti_strobing);
            ImGui::SameLine();
            ImGui::Text("(Actual pace: %d)", m_ctrl.actual_pace());
            ImGui::EndDisabled();
            ImGui::SameLine();
            imgui_StrTooltip("(?)",
                             "When there are '000...->1' and '111...->0', the pace will be adjusted to 2*n to avoid "
                             "bad visual effect.\n"
                             "In these cases, you can try the '+1' button to change the parity of generation.");

            if (test_key(ImGuiKey_R, false)) {
                restart();
            } else if (test_key(ImGuiKey_1, true)) {
                m_ctrl.pace = std::max(m_ctrl.pace_min, m_ctrl.pace - 1);
            } else if (test_key(ImGuiKey_2, true)) {
                m_ctrl.pace = std::min(m_ctrl.pace_max, m_ctrl.pace + 1);
            } else if (test_key(ImGuiKey_3, true)) {
                m_ctrl.gap = std::max(m_ctrl.gap_min, m_ctrl.gap - 1);
            } else if (test_key(ImGuiKey_4, true)) {
                m_ctrl.gap = std::min(m_ctrl.gap_max, m_ctrl.gap + 1);
            } else if (test_key(ImGuiKey_Space, false)) {
                m_ctrl.pause = !m_ctrl.pause;
            } else if (test_key(ImGuiKey_N, true)) {
                extra_step = m_ctrl.pause ? m_ctrl.actual_pace() : 0;
                m_ctrl.pause = true;
            } else if (test_key(ImGuiKey_M, true)) {
                extra_step = 1;
            }
        }
        ImGui::EndGroup();
        ImGui::SameLine(0, imgui_ItemSpacingX() + ImGui::CalcTextSize("  ").x);
        ImGui::BeginGroup();
        {
            torusT::initT init = m_init;
            int seed = init.seed;
            imgui_StepSliderInt("Init seed", &seed, 0, 29);
            if (ImGui::IsItemActive()) {
                temp_pause = true;
            }
            init.seed = seed;
            init.density.step_slide("Init density");
            if (ImGui::IsItemActive()) {
                temp_pause = true;
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
                    int width = init.size.width, height = init.size.height;
                    const bool has_w = std::from_chars(input_width, std::end(input_width), width).ec == std::errc{};
                    const bool has_h = std::from_chars(input_height, std::end(input_height), height).ec == std::errc{};
                    // ~ the value is unmodified if `from_chars` fails.
                    if (has_w || has_h) {
                        screen_off = {0, 0};
                        screen_zoom.set_1();
                        init.size = size_clamped({.width = width, .height = height});
                    }
                    input_width[0] = '\0';
                    input_height[0] = '\0';
                }

                ImGui::SameLine(0, s);
                ImGui::Text("Size = %d*%d", m_torus.tile().width(), m_torus.tile().height());
            }

            ImGui::AlignTextToFramePadding();
            imgui_Str("Zoom =");
            screen_zoom.select([&](bool is_cur, float z, const char* s) {
                ImGui::SameLine(0, imgui_ItemInnerSpacingX());
                if (z < 1) {
                    ImGui::BeginDisabled();
                }
                const bool sel = ImGui::RadioButton(s, is_cur);
                if (z < 1) {
                    ImGui::EndDisabled();
                }

                if (sel) {
                    screen_off = {0, 0};

                    init.size = size_clamped(
                        {.width = (int)(last_known_canvas_size.x / z), .height = (int)(last_known_canvas_size.y / z)});
                }
                return sel;
            });
            ImGui::SameLine();
            imgui_StrTooltip("(?)", "Click to resize to full-screen.");

            if (init != m_init) {
                if (init.size != m_init.size) {
                    m_sel.reset();
                    paste.reset();
                }
                m_init = init;
                restart();
            }
        }
        ImGui::EndGroup();
        ImGui::PopItemWidth();

        ImGui::Separator();

        const aniso::tileT::sizeT tile_size = m_torus.tile().size();

        static bool lock_mouse = false; // Lock scrolling control and window moving.
        ImGui::AlignTextToFramePadding();
        imgui_StrTooltip(lock_mouse ? "[...]" : "(...)", [] {
            // TODO: is "window" ambiguous here?
            imgui_Str(
                "Mouse operations:\n"
                "1. Scroll in the window to change the zoom.\n"
                "2. When there is nothing to paste, you can drag with left button to move the window, or drag with "
                "right button to select area (for range operations). When zoom = 1, you can also 'Ctrl + left-drag' "
                "to \"rotate\" the space.\n"
                "3. Otherwise, left-click to decide where to paste. In this case, to move the window you can drag with "
                "right button. (Range-selection and rotation is disabled when there are patterns to paste.)");
            ImGui::Separator();
            ImGui::Text("(You can right-click this '%s' to enable/disable scrolling control and window moving.)",
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
            screen_off = {0, 0};
        }
        ImGui::SameLine();
        if (ImGui::Button("Center")) {
            // "Center" will have the same effect as "Corner" (screen_off == {0, 0}) if fullscreen-resized.
            const ImVec2 screen_size(tile_size.width * screen_zoom, tile_size.height * screen_zoom);
            screen_off = last_known_canvas_size / 2 - screen_size / 2;
            screen_off.x = floor(floor(screen_off.x / screen_zoom) * screen_zoom);
            screen_off.y = floor(floor(screen_off.y / screen_zoom) * screen_zoom);
        }
        ImGui::SameLine();
        static bool show_range_window = false;
        ImGui::Checkbox("Range operations", &show_range_window);
        quick_info("^ Copy, paste, etc.\nv Drag with right button to select area.");

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

        if (paste) {
            ImGui::SameLine();
            ImGui::Text("  Paste:%d*%d", paste->width(), paste->height());
            ImGui::SameLine();
            if (ImGui::SmallButton("Drop##P")) {
                paste.reset();
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

            const bool active = ImGui::IsItemActive();
            const bool hovered = ImGui::IsItemHovered();
            const bool l_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
            const bool r_down = ImGui::IsMouseDown(ImGuiMouseButton_Right);
            if (active) {
                temp_pause = true;
            }
            if (m_sel && m_sel->active && (!r_down || paste || ImGui::IsItemDeactivated())) {
                m_sel->active = false;
                // Allow a single right-click to unselect the area.
                // (`bounding_box` has no size check like this. This is intentional.)
                if (m_sel->width() * m_sel->height() <= 2) {
                    m_sel.reset();
                }
            }

            std::optional<aniso::tileT::posT> zoom_center = std::nullopt; // Not clamped.
            if (hovered) {
                const ImGuiIO& io = ImGui::GetIO();
                assert(ImGui::IsMousePosValid(&io.MousePos));
                const ImVec2 mouse_pos = io.MousePos;

                // TODO: this looks messy...
                if (active && (l_down || (paste && r_down))) {
                    if (!r_down && io.KeyCtrl) {
                        if (screen_zoom == 1) {
                            // (This does not need `mark_written`.)
                            m_torus.rotate(io.MouseDelta.x, io.MouseDelta.y);
                        }
                    } else if (!lock_mouse) {
                        screen_off += io.MouseDelta;
                    }
                }

                if (!lock_mouse && imgui_MouseScrolling()) {
                    const ImVec2 screen_min = canvas_min + screen_off;
                    const ImVec2 cell_pos_raw = (mouse_pos - screen_min) / screen_zoom;
                    if (imgui_MouseScrollingDown()) {
                        screen_zoom.slide(-1);
                    } else if (imgui_MouseScrollingUp()) {
                        screen_zoom.slide(1);
                    }
                    screen_off = (mouse_pos - cell_pos_raw * screen_zoom) - canvas_min;
                    screen_off.x = round(screen_off.x);
                    screen_off.y = round(screen_off.y);
                }

                const ImVec2 screen_min = canvas_min + screen_off;
                const ImVec2 cell_pos_raw = (mouse_pos - screen_min) / screen_zoom;
                const int celx = floor(cell_pos_raw.x);
                const int cely = floor(cell_pos_raw.y);

                if (screen_zoom <= 1 && !paste && !active) {
                    if (celx >= -10 && celx < tile_size.width + 10 && cely >= -10 && cely < tile_size.height + 10) {
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
                            zoom_center = {{.x = celx, .y = cely}};
                        }
                    }
                }

                if (paste) {
                    assert(paste->width() <= tile_size.width && paste->height() <= tile_size.height);
                    paste_beg.x = std::clamp(celx - paste->width() / 2, 0, tile_size.width - paste->width());
                    paste_beg.y = std::clamp(cely - paste->height() / 2, 0, tile_size.height - paste->height());
                } else {
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                        const aniso::tileT::posT pos{.x = std::clamp(celx, 0, tile_size.width - 1),
                                                     .y = std::clamp(cely, 0, tile_size.height - 1)};
                        m_sel = {.active = true, .beg = pos, .end = pos};
                    } else if (m_sel && m_sel->active && r_down) {
                        m_sel->end.x = std::clamp(celx, 0, tile_size.width - 1);
                        m_sel->end.y = std::clamp(cely, 0, tile_size.height - 1);
                    }
                }
            }

            // Render.
            {
                const ImVec2 screen_size(tile_size.width * screen_zoom, tile_size.height * screen_zoom);
                const ImVec2 screen_min = canvas_min + screen_off;
                const ImVec2 screen_max = screen_min + screen_size;

                ImDrawList* const drawlist = ImGui::GetWindowDrawList();
                drawlist->PushClipRect(canvas_min, canvas_max);
                drawlist->AddRectFilled(canvas_min, canvas_max, IM_COL32(32, 32, 32, 255));

                const scaleE scale_mode = screen_zoom < 1 ? scaleE::Linear : scaleE::Nearest;
                if (!paste) {
                    const ImTextureID texture = make_screen(m_torus.tile(), scale_mode);

                    drawlist->AddImage(texture, screen_min, screen_max);
                    if (zoom_center.has_value()) {
                        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
                        if (ImGui::BeginTooltip()) {
                            if (scale_mode == scaleE::Nearest) {
                                zoom_image(texture, ImVec2(tile_size.width, tile_size.height),
                                           ImVec2(zoom_center->x, zoom_center->y), ImVec2(60, 60), 3);
                            } else {
                                // TODO: is it possible to reuse the texture in a different scale mode?
                                // (Related: https://github.com/ocornut/imgui/issues/7616)
                                zoom_image(m_torus.tile(), *zoom_center, {60, 60}, 3);
                            }
                            ImGui::EndTooltip();
                        }
                        ImGui::PopStyleVar();
                    }
                } else {
                    assert(!zoom_center);
                    assert(paste->width() <= tile_size.width && paste->height() <= tile_size.height);
                    paste_beg.x = std::clamp(paste_beg.x, 0, tile_size.width - paste->width());
                    paste_beg.y = std::clamp(paste_beg.y, 0, tile_size.height - paste->height());
                    const aniso::tileT::posT paste_end = paste_beg + paste->size();

                    // (wontfix) Wasteful, but after all this works...
                    aniso::tileT temp = aniso::copy(m_torus.tile(), {{paste_beg, paste_end}});
                    (background == 0 ? aniso::blit<aniso::blitE::Or>
                                     : aniso::blit<aniso::blitE::And>)(m_torus.tile(), paste_beg, *paste, std::nullopt);
                    const ImTextureID texture = make_screen(m_torus.tile(), scale_mode);
                    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                        m_ctrl.mark_written();
                        temp_pause = true;
                        paste.reset();
                    } else { // Restore.
                        aniso::blit<aniso::blitE::Copy>(m_torus.tile(), paste_beg, temp);
                    }

                    // (`paste_beg` and `paste_end` remain valid even if `paste` has been consumed.)
                    drawlist->AddImage(texture, screen_min, screen_max);
                    const ImVec2 paste_min = screen_min + ImVec2(paste_beg.x, paste_beg.y) * screen_zoom;
                    const ImVec2 paste_max = screen_min + ImVec2(paste_end.x, paste_end.y) * screen_zoom;
                    drawlist->AddRectFilled(paste_min, paste_max, IM_COL32(255, 0, 0, 60));
                }

                if (m_sel) {
                    const auto [sel_beg, sel_end] = m_sel->to_range();
                    const ImVec2 sel_min = screen_min + ImVec2(sel_beg.x, sel_beg.y) * screen_zoom;
                    const ImVec2 sel_max = screen_min + ImVec2(sel_end.x, sel_end.y) * screen_zoom;
                    drawlist->AddRectFilled(sel_min, sel_max, IM_COL32(0, 255, 0, 40));
                    drawlist->AddRect(sel_min, sel_max, IM_COL32(0, 255, 0, 160));
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
                        std::string rle_str =
                            aniso::to_RLE_str(add_rule ? &m_ctrl.rule : nullptr, m_torus.tile(), m_sel->to_range());
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
                    term("Capture (open)", "O (repeatable)", ImGuiKey_None, true, _capture_open);
                    // (`ImGui::IsKeyPressed(..., repeat = true)` does not return true in every frame.)
                    if (m_sel && enable_shortcuts && ImGui::IsKeyDown(ImGuiKey_O)) {
                        op = _capture_open;
                    }
                    term("Capture (closed)", "P", ImGuiKey_P, true, _capture_closed);
                });

                auto range_window = [&](const bool shortcut_only) {
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
                    auto window =
                        imgui_Window("Range operations", &show_range_window, ImGuiWindowFlags_AlwaysAutoResize);
                    range_window(false /* !shortcut_only */);
                } else {
                    range_window(true /* shortcut_only */);
                }

                if (op == _capture_closed && m_sel) {
                    auto lock = capture_closed(m_torus.tile(), m_sel->to_range(), m_ctrl.rule);
                    if (!replace) {
                        aniso::for_each_code([&](aniso::codeT c) { lock[c] = lock[c] || sync.current.lock[c]; });
                    }
                    sync.set_lock(lock);
                } else if (op == _capture_open && m_sel) {
                    auto lock = sync.current.lock;
                    capture_open(m_torus.tile(), m_sel->to_range(), lock);
                    sync.set_lock(lock);
                } else if (op == _random_fill && m_sel) {
                    aniso::random_fill(m_torus.tile(), global_mt19937(), fill_den.get(), m_sel->to_range());
                    m_ctrl.mark_written();
                    temp_pause = true;
                } else if (op == _clear_inside && m_sel) {
                    aniso::clear_inside(m_torus.tile(), m_sel->to_range(), background);
                    m_ctrl.mark_written();
                    temp_pause = true;
                } else if (op == _clear_outside && m_sel) {
                    aniso::clear_outside(m_torus.tile(), m_sel->to_range(), background);
                    m_ctrl.mark_written();
                    temp_pause = true;
                } else if (op == _select_all) {
                    if (!m_sel || m_sel->width() != tile_size.width || m_sel->height() != tile_size.height) {
                        m_sel = {.active = false,
                                 .beg = {0, 0},
                                 .end = {.x = tile_size.width - 1, .y = tile_size.height - 1}};
                    } else {
                        m_sel.reset();
                    }
                } else if (op == _bounding_box && m_sel) {
                    const auto [begin, end] = aniso::bounding_box(m_torus.tile(), m_sel->to_range(), background);
                    if (begin != end) {
                        m_sel = {.active = false, .beg = begin, .end = {.x = end.x - 1, .y = end.y - 1}};
                    } else {
                        m_sel.reset();
                    }
                } else if (op == _copy && m_sel) {
                    copy_sel();
                } else if (op == _cut && m_sel) {
                    copy_sel();
                    aniso::clear_inside(m_torus.tile(), m_sel->to_range());
                    m_ctrl.mark_written();
                    temp_pause = true;
                } else if (op == _paste) {
                    if (const char* text = ImGui::GetClipboardText()) {
                        paste.reset();
                        if (m_sel) {
                            m_sel->active = false;
                        }

                        auto result = aniso::from_RLE_str(text, tile_size);
                        if (result.succ) {
                            paste.emplace(std::move(result.tile));
                        } else if (result.width == 0 || result.height == 0) {
                            messenger::set_msg("Found no pattern.\n\n"
                                               "('V' is for pasting patterns. If you want to read rules from the "
                                               "clipboard, use the 'Clipboard' window instead.)");
                        } else {
                            messenger::set_msg("The space is not large enough for the pattern.\n"
                                               "Space size: x = {}, y = {}\n"
                                               "Pattern size: x = {}, y = {}",
                                               tile_size.width, tile_size.height, result.width, result.height);
                        }
                    }
                }
            }
        }

        if (paste) {
            temp_pause = true;
        }

        m_ctrl.run(m_torus, extra_step, temp_pause);
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
        if (ImGui::RadioButton(size_labels[i], size == i)) {
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
    static aniso::tileT temps[configT::Count];

    static unsigned latest = ImGui::GetFrameCount();
    if (const unsigned frame = ImGui::GetFrameCount(); frame != latest) {
        if (latest + 1 != frame) {
            terms.clear();
        } else {
            auto pos = terms.begin();
            const auto end = terms.end();
            while (pos != end) {
                if (pos->second.active) {
                    pos->second.active = false;
                    ++pos;
                } else {
                    // ~ won't invalidate `end`.
                    pos = terms.erase(pos);
                }
            }
        }
        latest = frame;
    }

    const int width = config.width(), height = config.height();
    assert(ImGui::GetItemRectSize() == ImVec2(width, height));
    assert(ImGui::IsItemVisible());

    termT& term = terms[id];
    if (term.active) [[unlikely]] {
        imgui_ItemRect(IM_COL32(255, 255, 255, 255));
        return;
    }
    term.active = true;

    if (test_key(ImGuiKey_T, false) || (interactive && ImGui::IsItemClicked(ImGuiMouseButton_Right)) ||
        term.tile.width() != width || term.tile.height() != height || term.seed != config.seed || term.rule != rule) {
        term.tile.resize({.width = width, .height = height});
        term.seed = config.seed;
        term.rule = rule;
        std::mt19937 rand(config.seed);
        aniso::random_fill(term.tile, rand, 0.5);
    }

    // (`IsItemActive` does not work as preview-window is based on `Dummy`.)
    const bool pause = interactive && ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left);
    if (!pause) {
        const int p = config.pace + ((config.pace % 2 == 1) && strobing(rule));
        for (int i = 0; i < p; ++i) {
            run_torus(term.tile, temps[config.size], rule);
        }
    }

    // Intentionally display after running for better visual effect (the initial state of the tile won't be displayed).
    const ImTextureID texture = make_screen(term.tile, scaleE::Nearest);
    ImGui::GetWindowDrawList()->AddImage(texture, ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    if (interactive && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
        assert(ImGui::IsMousePosValid());
        const ImVec2 pos = ImGui::GetIO().MousePos - ImGui::GetItemRectMin();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
        if (ImGui::BeginTooltip()) {
            zoom_image(texture, ImVec2(width, height), pos, ImVec2(60, 60), 3);
            ImGui::EndTooltip();
        }
        ImGui::PopStyleVar();
    }
}
