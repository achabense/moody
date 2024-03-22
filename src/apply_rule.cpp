#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "tile.hpp"

#include "common.hpp"

using clockT = std::chrono::steady_clock;

static void refresh(screenT& screen, const legacy::tileT& tile) {
    screen.refresh(tile.width(), tile.height(), [&tile](int y) { return tile.line(y); });
}

// TODO: whether to support boundless space?
// `tileT` is able to serve as the building block for boundless space (`tileT::gather` was
// initially designed to enable this extension) but that's complicated, and torus space is enough to
// show the effect of the rules.
static void run_torus(legacy::tileT& tile, legacy::tileT& temp, const legacy::rule_like auto& rule) {
    assert(&tile != &temp);

    tile.gather(tile, tile, tile, tile, tile, tile, tile, tile);
    tile.apply(rule, temp);
    tile.swap(temp);
}

// Copy the subrange and run as a torus space, recording all invoked mappings.
// This is good at capturing "self-contained" patterns (oscillators/spaceships).
static legacy::moldT::lockT capture_closed(const legacy::tileT& source, const legacy::tileT::rangeT& range,
                                           const legacy::ruleT& rule) {
    legacy::tileT tile = legacy::copy(source, range);
    legacy::tileT temp(tile.size());

    legacy::moldT::lockT lock{};

    // (wontfix) It's possible that the loop fails to catch all invocations in very rare cases,
    // due to that `limit` is not large enough.

    // Loop until there has been `limit` generations without newly invoked mappings.
    const int limit = 100;
    for (int g = limit; g > 0; --g) {
        run_torus(tile, temp, [&](legacy::codeT code) {
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
static void capture_open(const legacy::tileT& source, legacy::tileT::rangeT range, legacy::moldT::lockT& lock) {
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
    legacy::tileT m_tile, m_temp;
    int m_gen;

public:
    struct initT {
        legacy::tileT::sizeT size;
        uint32_t seed;
        densityT density;

        friend bool operator==(const initT&, const initT&) = default;
    };

    explicit torusT(const initT& init) : m_tile(init.size), m_temp(init.size), m_gen(0) { restart(init); }

    legacy::tileT& tile() { return m_tile; }
    const legacy::tileT& tile() const { return m_tile; }
    int gen() const { return m_gen; }

    void restart(const initT& init) {
        m_tile.resize(init.size);
        m_temp.resize(init.size);

        std::mt19937 rand(init.seed);
        legacy::random_fill(m_tile, rand, init.density.get());

        m_gen = 0;
    }

    void run(const legacy::ruleT& rule, int count = 1) {
        for (int c = 0; c < count; ++c) {
            run_torus(m_tile, m_temp, rule);
#if 0
            // Add salt; the effect is amazing sometimes...
            // This is mostly an "off-topic" feature; whether to add a mode to enable this?
            m_tile.for_each_line(m_tile.entire_range(), [](std::span<bool> line) {
                static std::mt19937 rand{1};
                for (bool& b : line) {
                    b ^= ((rand() & 0x1fff) == 0);
                }
            });
#endif
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

class runnerT {
    static constexpr legacy::tileT::sizeT min_size{.width = 20, .height = 10};
    static constexpr legacy::tileT::sizeT max_size{.width = 1600, .height = 1200};
    static legacy::tileT::sizeT size_clamped(legacy::tileT::sizeT size) {
        return {.width = std::clamp(size.width, min_size.width, max_size.width),
                .height = std::clamp(size.height, min_size.height, max_size.height)};
    }

    static constexpr int max_zoom = 8;
    static constexpr ImVec2 min_canvas_size{min_size.width * max_zoom, min_size.height* max_zoom};

    struct ctrlT {
        legacy::ruleT rule{};

        static constexpr int pace_min = 1, pace_max = 20;
        int pace = 1;
        bool anti_strobing = true;
        int actual_pace() const {
            constexpr legacy::codeT all_0{0}, all_1{511};
            const bool strobing = rule[all_0] == 1 && rule[all_1] == 0;
            if (anti_strobing && strobing && pace % 2) {
                return pace + 1;
            }
            return pace;
        }

        static constexpr int gap_unit = 25; // ms.
        static constexpr int gap_min = 0, gap_max = 20;
        int gap = 0;

        bool pause = false;

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
    int screen_zoom = 1;

    // (Workaround: it's hard to get canvas-size in this frame when it's needed; this looks horrible but
    // will work well in all cases)
    ImVec2 last_known_canvas_size = min_canvas_size;

    std::optional<legacy::tileT> paste = std::nullopt;
    legacy::tileT::posT paste_beg{0, 0}; // Valid if paste.has_value().

    struct selectT {
        bool active = true;
        legacy::tileT::posT beg{0, 0}, end{0, 0}; // [] instead of [).

        legacy::tileT::rangeT to_range() const {
            const auto [xmin, xmax] = std::minmax(beg.x, end.x);
            const auto [ymin, ymax] = std::minmax(beg.y, end.y);

            return {.begin{xmin, ymin}, .end{xmax + 1, ymax + 1}};
        }

        int width() const { return std::abs(beg.x - end.x) + 1; }
        int height() const { return std::abs(beg.y - end.y) + 1; }
    };
    std::optional<selectT> m_sel = std::nullopt;

    legacy::moldT::lockT m_lock{}; // For open-capture.

public:
    // TODO: redesign pause logics...
    // TODO: better control logics... (`display` is horribly written due to unorganized control logics...)
    // TODO: more sensible keyboard controls...
    // TODO: (wontfix?) there cannot actually be multiple instances in the program.
    // For example, there are a lot of static variables in `display`, and the keyboard controls are not designed
    // for per-object use.

    void apply_rule(const legacy::ruleT& rule, bool& temp_pause /* Workaround */) {
        if (m_ctrl.rule != rule) {
            m_ctrl.rule = rule;
            m_ctrl.anti_strobing = true;
            m_ctrl.pause = false;
            m_torus.restart(m_init);
            m_ctrl.mark_written();
            m_lock = {};

            temp_pause = true;
        }
    }

    std::optional<legacy::moldT::lockT> display(screenT& screen, bool& temp_pause /* Workaround */) {
        std::optional<legacy::moldT::lockT> out = std::nullopt;

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
            const float extra_w_sameline = ImGui::GetStyle().ItemSpacing.x * 1; // One SameLine...
            const float extra_w_padding = ImGui::GetStyle().FramePadding.x * 2; // One Button * two sides...
            const float extra_w = ImGui::CalcTextSize("Restart").x + extra_w_sameline + extra_w_padding;
            const std::string str = std::format("Generation:{}, density:{:.4f}{}", m_torus.gen(),
                                                float(legacy::count(m_torus.tile())) / m_torus.tile().area(),
                                                m_torus.gen() < 10     ? "   "
                                                : m_torus.gen() < 100  ? "  "
                                                : m_torus.gen() < 1000 ? " "
                                                                       : "");
            ImGui::SeparatorTextEx(0, str.c_str(), nullptr, extra_w);
            ImGui::SameLine();
            if (ImGui::Button("Restart")) {
                restart();
            }
        }

        ImGui::PushItemWidth(item_width);
        ImGui::BeginGroup();
        {
            ImGui::Checkbox("Pause", &m_ctrl.pause);
            ImGui::SameLine();
            ImGui::PushButtonRepeat(true);
            if (ImGui::Button("+1")) {
                extra_step = 1;
            }
            ImGui::SameLine(0, imgui_ItemInnerSpacingX());
            imgui_StrTooltip("(?)", "Advance generation by 1 (instead of pace). This is useful for changing the parity "
                                    "of generation when pace != 1.");
            if (m_ctrl.pause) {
                ImGui::SameLine();
                if (ImGui::Button(std::format("+p({})###+p", m_ctrl.actual_pace()).c_str())) {
                    extra_step = m_ctrl.actual_pace();
                }
            }
            ImGui::PopButtonRepeat();

            imgui_StepSliderInt("Gap time (0~500ms)", &m_ctrl.gap, m_ctrl.gap_min, m_ctrl.gap_max,
                                std::format("{} ms", m_ctrl.gap * m_ctrl.gap_unit).c_str());

            imgui_StepSliderInt("Pace (1~20)", &m_ctrl.pace, m_ctrl.pace_min, m_ctrl.pace_max);

            // !!TODO: redesign this part.
            // TODO: should be interactive.
            // the rule has only all_0 and all_1 NOT flipped, and has interesting effect in extremely low/high
            // densities. (e.g. 0.01, 0.99)
            ImGui::BeginDisabled();
            ImGui::Checkbox("Anti-strobing", &m_ctrl.anti_strobing);
            if (m_ctrl.anti_strobing) {
                ImGui::SameLine();
                ImGui::Text("(Actual pace: %d)", m_ctrl.actual_pace());
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            imgui_StrTooltip(
                "(?)",
                "Actually this is not enough to guarantee smooth visual effect. For example, the following "
                "\"non-strobing\" rule has really terrible visual effect:\n\n"
                "MAPf/8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAQ\n\n"
                "In cases like this, it generally helps to set the pace to 2*n manually to remove bad visual effects.");
        }
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BeginGroup();
        {
            torusT::initT init = m_init;
            int seed = init.seed;
            imgui_StepSliderInt("Init seed (0~49)", &seed, 0, 49);
            if (ImGui::IsItemActive()) {
                temp_pause = true;
            }
            init.seed = seed;
            init.density.step_slide("Init density (0~1)");
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
                ImGui::InputTextWithHint("##Width", "width", input_width, std::size(input_width),
                                         ImGuiInputTextFlags_CallbackCharFilter, filter);
                ImGui::SameLine(0, s);
                ImGui::SetNextItemWidth(ceil(w));
                ImGui::InputTextWithHint("##Height", "height", input_height, std::size(input_height),
                                         ImGuiInputTextFlags_CallbackCharFilter, filter);
                ImGui::SameLine(0, s);
                if (ImGui::Button("Resize")) {
                    int width = init.size.width, height = init.size.height;
                    const bool has_w = std::from_chars(input_width, std::end(input_width), width).ec == std::errc{};
                    const bool has_h = std::from_chars(input_height, std::end(input_height), height).ec == std::errc{};
                    // ~ the value is unmodified if `from_chars` fails.
                    if (has_w || has_h) {
                        screen_off = {0, 0};
                        screen_zoom = 1;
                        init.size = size_clamped({.width = width, .height = height});
                    }
                    input_width[0] = '\0';
                    input_height[0] = '\0';
                }

                ImGui::SameLine(0, s);
                ImGui::Text("Width:%d, height:%d", m_torus.tile().width(), m_torus.tile().height());
            }

            ImGui::AlignTextToFramePadding();
            imgui_Str("Fullscreen with zoom =");
            ImGui::SameLine();
            assert(max_zoom == 8);
            for (const ImVec2 size = square_size(); const int z : {1, 2, 4, 8}) {
                if (z != 1) {
                    ImGui::SameLine(0, imgui_ItemInnerSpacingX());
                }
                if (ImGui::Button(std::to_string(z).c_str(), size)) {
                    screen_zoom = z;
                    screen_off = {0, 0};

                    init.size = size_clamped({.width = (int)last_known_canvas_size.x / screen_zoom,
                                              .height = (int)last_known_canvas_size.y / screen_zoom});
                }
            }
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

        const legacy::tileT::sizeT tile_size = m_torus.tile().size();
        const ImVec2 screen_size(tile_size.width * screen_zoom, tile_size.height * screen_zoom);

        if (ImGui::Button("Corner")) {
            screen_off = {0, 0};
        }
        ImGui::SameLine();
        if (ImGui::Button("Center")) {
            // "Center" will have the same effect as "Corner" (screen_off == {0, 0}) if fullscreen-resized.
            screen_off = last_known_canvas_size / 2 - screen_size / 2;
            screen_off.x = floor(screen_off.x / screen_zoom) * screen_zoom;
            screen_off.y = floor(screen_off.y / screen_zoom) * screen_zoom;
        }
        ImGui::SameLine();
        static bool other_op = true;
        ImGui::Checkbox("Range operations", &other_op);
        ImGui::SameLine(0, imgui_ItemInnerSpacingX());
        imgui_StrTooltip("(!)", "The keyboard shortcuts are available only when the window is open.");

        if (m_sel) {
            ImGui::SameLine(), imgui_Str("|"), ImGui::SameLine();
            if (ImGui::Button(
                    std::format("Drop selection (w:{} h:{})###drop_sel", m_sel->width(), m_sel->height()).c_str())) {
                m_sel.reset();
            }
        }
        if (paste) {
            ImGui::SameLine(), imgui_Str("|"), ImGui::SameLine();
            if (ImGui::Button(
                    std::format("Drop paste (w:{} h:{})###drop_paste", paste->width(), paste->height()).c_str())) {
                paste.reset();
            }
        }

        {
            // (Values of GetContentRegionAvail() can be negative...)
            ImGui::InvisibleButton("Canvas", ImMax(min_canvas_size, ImGui::GetContentRegionAvail()),
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
            if (active && hovered && l_down || (paste && r_down)) {
                // Some logics rely on this to be done before rendering.
                const ImGuiIO& io = ImGui::GetIO();
                if (!r_down && io.KeyCtrl && screen_zoom == 1) {
                    // (This does not need `mark_written`.)
                    m_torus.rotate(io.MouseDelta.x, io.MouseDelta.y);
                } else {
                    screen_off += io.MouseDelta;
                }
            }

            const ImVec2 screen_min = canvas_min + screen_off;
            const ImVec2 screen_max = screen_min + screen_size;

            ImDrawList* const drawlist = ImGui::GetWindowDrawList();
            drawlist->PushClipRect(canvas_min, canvas_max);
            drawlist->AddRectFilled(canvas_min, canvas_max, IM_COL32(20, 20, 20, 255));

            if (!paste) {
                refresh(screen, m_torus.tile());

                drawlist->AddImage(screen.texture(), screen_min, screen_max);
            } else {
                assert(paste->width() <= tile_size.width && paste->height() <= tile_size.height);
                paste_beg.x = std::clamp(paste_beg.x, 0, tile_size.width - paste->width());
                paste_beg.y = std::clamp(paste_beg.y, 0, tile_size.height - paste->height());
                const legacy::tileT::posT paste_end = paste_beg + paste->size();

                // !!TODO: the Or-mode cannot paste patterns into white background...
                // (wontfix) Wasteful, but after all this works...
                legacy::tileT temp = legacy::copy(m_torus.tile(), {{paste_beg, paste_end}});
                legacy::blit<legacy::blitE::Or>(m_torus.tile(), paste_beg, *paste);
                refresh(screen, m_torus.tile());
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                    m_ctrl.mark_written();
                    temp_pause = true;
                    paste.reset();
                } else { // Restore.
                    legacy::blit<legacy::blitE::Copy>(m_torus.tile(), paste_beg, temp);
                }

                drawlist->AddImage(screen.texture(), screen_min, screen_max);
                const ImVec2 paste_min = screen_min + ImVec2(paste_beg.x, paste_beg.y) * screen_zoom;
                const ImVec2 paste_max = screen_min + ImVec2(paste_end.x, paste_end.y) * screen_zoom;
                drawlist->AddRectFilled(paste_min, paste_max, IM_COL32(255, 0, 0, 60));
            }

            if (m_sel) {
                const auto range = m_sel->to_range();
                const ImVec2 sel_min = screen_min + ImVec2(range.begin.x, range.begin.y) * screen_zoom;
                const ImVec2 sel_max = screen_min + ImVec2(range.end.x, range.end.y) * screen_zoom;
                drawlist->AddRectFilled(sel_min, sel_max, IM_COL32(0, 255, 0, 40));
                drawlist->AddRect(sel_min, sel_max, IM_COL32(0, 255, 0, 160));
            }
            drawlist->PopClipRect();

            if (m_sel && m_sel->active && !r_down) {
                m_sel->active = false;
                // Allow a single right-click to unselect the area.
                // (Shrinking (`bounding_box`) has no size check like this. This is intentional.)
                if (m_sel->width() * m_sel->height() <= 2) {
                    m_sel.reset();
                }
            }

            if (hovered) {
                assert(ImGui::IsMousePosValid());
                // This will work well even if outside of the image.
                const ImVec2 mouse_pos = ImGui::GetIO().MousePos;
                const ImVec2 cell_pos_raw = (mouse_pos - screen_min) / screen_zoom;
                const int celx = floor(cell_pos_raw.x);
                const int cely = floor(cell_pos_raw.y);

                if (screen_zoom == 1 && !paste && !active) {
                    if (celx >= -10 && celx < tile_size.width + 10 && cely >= -10 && cely < tile_size.height + 10) {
                        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
                        if (ImGui::BeginItemTooltip()) {
                            const int w = std::min(tile_size.width, 40);
                            const int h = std::min(tile_size.height, 40);
                            const legacy::tileT::posT min = {.x = std::clamp(celx - w / 2, 0, tile_size.width - w),
                                                             .y = std::clamp(cely - h / 2, 0, tile_size.height - h)};
                            const legacy::tileT::posT max = {.x = min.x + w, .y = min.y + h};

                            ImGui::Image(screen.texture(), ImVec2(w * 4, h * 4),
                                         {(float)min.x / tile_size.width, (float)min.y / tile_size.height},
                                         {(float)max.x / tile_size.width, (float)max.y / tile_size.height});
                            ImGui::EndTooltip();
                        }
                        ImGui::PopStyleVar();
                    }
                }

                if (paste) {
                    assert(paste->width() <= tile_size.width && paste->height() <= tile_size.height);
                    paste_beg.x = std::clamp(celx - paste->width() / 2, 0, tile_size.width - paste->width());
                    paste_beg.y = std::clamp(cely - paste->height() / 2, 0, tile_size.height - paste->height());
                } else {
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                        const legacy::tileT::posT pos{.x = std::clamp(celx, 0, tile_size.width - 1),
                                                      .y = std::clamp(cely, 0, tile_size.height - 1)};
                        m_sel = {.active = true, .beg = pos, .end = pos};
                    } else if (m_sel && m_sel->active && r_down) {
                        m_sel->end.x = std::clamp(celx, 0, tile_size.width - 1);
                        m_sel->end.y = std::clamp(cely, 0, tile_size.height - 1);
                    }
                }

                if (imgui_MouseScrolling()) {
                    if (imgui_MouseScrollingDown() && screen_zoom != 1) {
                        screen_zoom /= 2;
                    }
                    if (imgui_MouseScrollingUp() && screen_zoom != max_zoom) {
                        screen_zoom *= 2;
                    }
                    screen_off = (mouse_pos - cell_pos_raw * screen_zoom) - canvas_min;
                    screen_off.x = round(screen_off.x);
                    screen_off.y = round(screen_off.y);
                }
            }

            if (other_op) {
                if (auto window = imgui_Window("Range operations", &other_op,
                                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
                    // (Shadowing `::imgui_KeyPressed`...)
                    auto imgui_KeyPressed = [active](ImGuiKey key, bool repeat) {
                        return (active || !ImGui::GetIO().WantCaptureKeyboard) && ImGui::IsKeyPressed(key, repeat);
                    };

                    if (imgui_KeyPressed(ImGuiKey_1, true)) {
                        m_ctrl.gap = std::max(m_ctrl.gap_min, m_ctrl.gap - 1);
                    } else if (imgui_KeyPressed(ImGuiKey_2, true)) {
                        m_ctrl.gap = std::min(m_ctrl.gap_max, m_ctrl.gap + 1);
                    } else if (imgui_KeyPressed(ImGuiKey_3, true)) {
                        m_ctrl.pace = std::max(m_ctrl.pace_min, m_ctrl.pace - 1);
                    } else if (imgui_KeyPressed(ImGuiKey_4, true)) {
                        m_ctrl.pace = std::min(m_ctrl.pace_max, m_ctrl.pace + 1);
                    } else if (imgui_KeyPressed(ImGuiKey_Space, false)) {
                        m_ctrl.pause = !m_ctrl.pause;
                    } else if (imgui_KeyPressed(ImGuiKey_N, true)) {
                        extra_step = 1;
                    } else if (imgui_KeyPressed(ImGuiKey_M, true)) {
                        if (m_ctrl.pause) {
                            extra_step = m_ctrl.actual_pace();
                        }
                        m_ctrl.pause = true;
                    } else if (imgui_KeyPressed(ImGuiKey_R, false)) {
                        restart();
                    }

                    // !!TODO: whether to explain the effects?
                    imgui_Str("Other shortcuts: 1, 2, 3, 4, Space, N, M, R");
                    ImGui::Separator();

                    auto term = [&, any_called = false](const char* label, const char* shortcut, ImGuiKey key,
                                                        bool use_sel, const auto& op) mutable {
                        const bool enabled = !use_sel || m_sel.has_value();
                        if (ImGui::MenuItem(label, shortcut, nullptr, enabled) ||
                            (enabled && imgui_KeyPressed(key, false))) {
                            if (!any_called) {
                                any_called = true;
                                op();
                            }
                        }
                        if (!enabled && ImGui::BeginItemTooltip()) {
                            imgui_Str("This operation is meaningful only when there are selected areas.");
                            ImGui::EndTooltip();
                        }
                    };

                    auto set_tag = [](bool& tag, const char* label, const char* message) {
                        ImGui::Checkbox(label, &tag);
                        ImGui::SameLine(0, imgui_ItemInnerSpacingX());
                        imgui_StrTooltip("(?)", message);
                    };

                    // Filling.
                    // TODO: for "Clear inside/outside" & "Shrink", add a tag for `bool v`?
                    static densityT fill_den = 0.5;
                    fill_den.step_slide("Fill density");
                    term("Random fill", "+/=", ImGuiKey_Equal, true, [&] {
                        assert(m_sel);
                        legacy::random_fill(m_torus.tile(), global_mt19937(), fill_den.get(), m_sel->to_range());
                        m_ctrl.mark_written();
                        temp_pause = true;
                    });
                    term("Clear inside", "Backspace", ImGuiKey_Backspace, true, [&] {
                        assert(m_sel);
                        legacy::clear_inside(m_torus.tile(), m_sel->to_range());
                        m_ctrl.mark_written();
                        temp_pause = true;
                    });
                    term("Clear outside", "0 (zero)", ImGuiKey_0, true, [&] {
                        assert(m_sel);
                        legacy::clear_outside(m_torus.tile(), m_sel->to_range());
                        m_ctrl.mark_written();
                        temp_pause = true;
                    });

                    ImGui::Separator();
                    term("Select all", "A", ImGuiKey_A, false, [&] {
                        if (!m_sel || m_sel->width() != tile_size.width || m_sel->height() != tile_size.height) {
                            m_sel = {.active = false,
                                     .beg = {0, 0},
                                     .end = {.x = tile_size.width - 1, .y = tile_size.height - 1}};
                        } else {
                            m_sel.reset();
                        }
                    });
                    term("Shrink", "S", ImGuiKey_S, true, [&] {
                        assert(m_sel);
                        const auto [begin, end] = legacy::bounding_box(m_torus.tile(), m_sel->to_range());
                        if (begin != end) {
                            m_sel = {.active = false, .beg = begin, .end = {.x = end.x - 1, .y = end.y - 1}};
                        } else {
                            m_sel.reset();
                        }
                    });

                    // Copy/Cut/Paste.
                    static bool add_rule = false;
                    static bool copy_silently = false;
                    auto copy_sel = [&] {
                        assert(m_sel);
                        std::string rle_str =
                            legacy::to_RLE_str(add_rule ? &m_ctrl.rule : nullptr, m_torus.tile(), m_sel->to_range());
                        if (copy_silently) {
                            ImGui::SetClipboardText(rle_str.c_str());
                        } else {
                            messenger::add_msg(std::move(rle_str));
                        }
                    };

                    ImGui::Separator();
                    set_tag(add_rule, "Add rule", "Whether to append the current rule when copying patterns.");
                    ImGui::SameLine();
                    set_tag(copy_silently, "Copy silently", "Whether to directly copy to the clipboard.");
                    term("Copy", "C", ImGuiKey_C, true, [&] { copy_sel(); });
                    term("Cut", "X", ImGuiKey_X, true, [&] {
                        copy_sel();
                        legacy::clear_inside(m_torus.tile(), m_sel->to_range());
                        m_ctrl.mark_written();
                        temp_pause = true;
                    });
                    term("Paste", "V", ImGuiKey_V, false, [&] {
                        if (const char* text = ImGui::GetClipboardText()) {
                            try {
                                paste.emplace(legacy::from_RLE_str(text, tile_size));
                            } catch (const std::exception& err) {
                                // !!TODO: better message...
                                messenger::add_msg(err.what());
                            }
                        }
                    });

                    // Pattern capturing.
                    // TODO: enable getting current.lock?
                    static bool adopt_eagerly = true;
                    ImGui::Separator();
                    set_tag(adopt_eagerly, "Adopt eagerly",
                            "Whether to automatically adopt the lock after doing closed-capture. (This does not affect "
                            "open-capture.)");
                    term("Capture (closed)", "P", ImGuiKey_P, true, [&] {
                        assert(m_sel);
                        const auto lock = capture_closed(m_torus.tile(), m_sel->to_range(), m_ctrl.rule);
                        legacy::for_each_code([&](legacy::codeT c) { m_lock[c] = m_lock[c] || lock[c]; });
                        if (adopt_eagerly) {
                            out = m_lock;
                        }
                    });
                    term("Capture (open)", "L (repeatable)", ImGuiKey_None, true, [&] {
                        assert(m_sel);
                        capture_open(m_torus.tile(), m_sel->to_range(), m_lock);
                    });
                    if (m_sel && imgui_KeyPressed(ImGuiKey_L, true)) {
                        capture_open(m_torus.tile(), m_sel->to_range(), m_lock);
                    }
                    if (ImGui::Button("Clear")) {
                        m_lock = {};
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Adopt")) {
                        out = m_lock;
                    }
                    ImGui::SameLine();
                    int count = 0;
                    legacy::for_each_code([&](legacy::codeT code) { count += m_lock[code]; });
                    ImGui::Text("Count:%d/512", count);
                }
            }
        }

        if (paste) {
            temp_pause = true;
        }

        m_ctrl.run(m_torus, extra_step, temp_pause);

        return out;
    }
};

std::optional<legacy::moldT::lockT> apply_rule(const legacy::ruleT& rule, screenT& screen) {
    static runnerT runner;
    bool temp_pause = false;
    runner.apply_rule(rule, temp_pause);
    return runner.display(screen, temp_pause);
}
