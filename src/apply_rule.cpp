#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "tile.hpp"

#include "common.hpp"

static void refresh(screenT& screen, const legacy::tileT& tile) {
    screen.refresh(tile.width(), tile.height(), [&tile](int y) { return tile.line(y); });
}

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
    legacy::tileT tile(range.size());
    legacy::copy(tile, {0, 0}, source, range);

    legacy::tileT temp(range.size());

    legacy::moldT::lockT lock{};

    // (wontfix) It's possible that the loop fails to catch all invocations in very rare cases,
    // due to that `limit` is not large enough.

    // Loop until there has been `limit` generations without newly invoked mappings.
    const int limit = 30;
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

// TODO: how to support open-capture in the gui?
#if 0
// However, if the patterns is not self-contained, what happens in the copied subrange (treated as torus space)
// is not exactly what we see in the source space.
// Especially, for the rules with interesting "texture"s (but no typical oscillators etc), `capture_closed` tends to
// make a full lock, which is not useful.
// ~ We may want to record what actually happened in the range (instead of the the captured pattern) during the run of
// the source space:
static void capture_open(const legacy::tileT& source, legacy::tileT::rangeT range, legacy::moldT::lockT& lock) {
    if (range.width() <= 2 || range.height() <= 2) {
        return;
    }
    range.begin.x += 1;
    range.begin.y += 1;
    range.end.x -= 1;
    range.end.y -= 1;
    source.record(lock, range);
}
#endif

class torusT {
    legacy::tileT m_tile, m_temp;
    int m_gen;

public:
    // Using float (instead of double) as there is only ImGui::SliderFloat.
    struct initT {
        legacy::tileT::sizeT size;
        uint32_t seed;
        float density; // ∈ [0.0f, 1.0f]
    };

    explicit torusT(const initT& init) : m_tile(init.size), m_temp(init.size), m_gen(0) { restart(init); }

    legacy::tileT& tile() { return m_tile; }
    const legacy::tileT& tile() const { return m_tile; }
    int gen() const { return m_gen; }

    void restart(const initT& init) {
        m_tile.resize(init.size);
        m_temp.resize(init.size);

        std::mt19937 rand(init.seed);
        legacy::random_fill(m_tile, rand, init.density);

        m_gen = 0;
    }

    void run(const legacy::ruleT& rule, int count = 1) {
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

static bool strobing(const legacy::ruleT& rule) {
    constexpr legacy::codeT all_0{0}, all_1{511};
    return rule[all_0] == 1 && rule[all_1] == 0;
}

// TODO: reduce to local lambdas...
struct ctrlT {
    legacy::ruleT rule{};

    static constexpr int pace_min = 1, pace_max = 20;
    int pace = 1;
    bool anti_strobing = true;
    int actual_pace() const {
        if (anti_strobing && strobing(rule) && pace % 2) {
            return pace + 1;
        }
        return pace;
    }

    // TODO: redesign?
    static constexpr int gap_min = 0, gap_max = 20;
    int gap_frame = 0;

    bool pause = false;
    bool pause2 = false; // TODO: explain...

    void run(torusT& runner, int extra) const {
        if (extra != 0) {
            runner.run(rule, extra);
        }
        if (!pause && !pause2) {
            if (ImGui::GetFrameCount() % (gap_frame + 1) == 0) {
                runner.run(rule, actual_pace());
            }
        }
    }
};

// TODO (temp) Surprisingly nasty to deal with...
// TODO: explain...
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

// TODO: the canvas part is horribly written, needs heavy refactorings in the future...
std::optional<legacy::moldT::lockT> apply_rule(const legacy::ruleT& rule, screenT& screen) {
    std::optional<legacy::moldT::lockT> out = std::nullopt;

    // TODO: the constraint is arbitrary; are there more sensible ways to decide size constraint?
    // TODO: (temp) being related to min canvas size...
    const auto size_clamped = [](int width, int height) {
        return legacy::tileT::sizeT{.width = std::clamp(width, 20, 1200), .height = std::clamp(height, 10, 1200)};
    };

    static torusT::initT init{.size{.width = 500, .height = 400}, .seed = 0, .density = 0.5};
    static torusT runner(init);
    assert(init.size == runner.tile().size());
    assert(init.size == size_clamped(init.size.width, init.size.height));

    static ctrlT ctrl{.rule = rule, .pace = 1, .anti_strobing = true, .gap_frame = 0, .pause = false};

    static ImVec2 screen_off = {0, 0};
    static int screen_zoom = 1;
    assert(screen_off.x == int(screen_off.x) && screen_off.y == int(screen_off.y));

    static std::optional<legacy::tileT> paste = std::nullopt;
    static legacy::tileT::posT paste_beg{0, 0}; // dbegin for copy... (TODO: this is confusing...)

    static std::optional<selectT> sel = std::nullopt;

    // TODO: not robust
    // ~ runner.restart(...) shall not happen before rendering.
    bool should_restart = false;
    int extra = 0;

    if (ctrl.rule != rule) {
        ctrl.rule = rule;
        should_restart = true;
        ctrl.anti_strobing = true;
        ctrl.pause = false;
    }

    static ImVec2 last_known_canvas_size{160, 80}; // TODO: make std::optional?

    // TODO: better be controlled by frame()?
    ImGui::PushItemWidth(item_width);

    // TODO: redesign keyboard ctrl...
    if (imgui_KeyPressed(ImGuiKey_1, true)) {
        ctrl.gap_frame = std::max(ctrl.gap_min, ctrl.gap_frame - 1);
    }
    if (imgui_KeyPressed(ImGuiKey_2, true)) {
        ctrl.gap_frame = std::min(ctrl.gap_max, ctrl.gap_frame + 1);
    }
    if (imgui_KeyPressed(ImGuiKey_3, true)) {
        ctrl.pace = std::max(ctrl.pace_min, ctrl.pace - 1);
    }
    if (imgui_KeyPressed(ImGuiKey_4, true)) {
        ctrl.pace = std::min(ctrl.pace_max, ctrl.pace + 1);
    }
    // TODO: explain... apply to other ctrls?
    if ((ctrl.pause2 || !ImGui::GetIO().WantCaptureKeyboard) && ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
        ctrl.pause = !ctrl.pause;
    }
    // Run by keystroke turns out to be necessary. (TODO: For example ...)
    if (imgui_KeyPressed(ImGuiKey_M, true)) {
        if (ctrl.pause) {
            extra = ctrl.actual_pace();
        }
        ctrl.pause = true;
    }

    {
        // TODO: (temp) keeping in-line with edit-rule's
        const float extra_w_sameline = ImGui::GetStyle().ItemSpacing.x * 1; // One SameLine...
        const float extra_w_padding = ImGui::GetStyle().FramePadding.x * 2; // One Button * two sides...
        const float extra_w = ImGui::CalcTextSize("Restart").x + extra_w_sameline + extra_w_padding;
        const std::string str = std::format("Generation:{}, density:{:.4f}{}", runner.gen(),
                                            float(legacy::count(runner.tile())) / runner.tile().area(),
                                            runner.gen() < 10     ? "   "
                                            : runner.gen() < 100  ? "  "
                                            : runner.gen() < 1000 ? " "
                                                                  : "");
        ImGui::SeparatorTextEx(0, str.c_str(), nullptr, extra_w);
        ImGui::SameLine();
        if (ImGui::Button("Restart") || imgui_KeyPressed(ImGuiKey_R, false)) {
            should_restart = true;
        }
    }

    ImGui::BeginGroup();
    {
        ImGui::Checkbox("Pause", &ctrl.pause);
        ImGui::SameLine();
        ImGui::BeginDisabled();
        ImGui::Checkbox("Pause2", &ctrl.pause2);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::PushButtonRepeat(true);
        if (ImGui::Button("+1")) {
            extra = 1;
        }
        // TODO: finish.
        helper::show_help("Advance generation by 1 (instead of pace). This is useful for ...");
        if (ctrl.pause) {
            ImGui::SameLine();
            if (ImGui::Button(std::format("+p({})###+p", ctrl.actual_pace()).c_str())) {
                extra = ctrl.actual_pace();
            }
        }
        ImGui::PopButtonRepeat();

        // TODO: Gap-frame shall be really timer-based...
        imgui_StepSliderInt("Gap Frame (0~20)", &ctrl.gap_frame, ctrl.gap_min, ctrl.gap_max);

        imgui_StepSliderInt("Pace (1~20)", &ctrl.pace, ctrl.pace_min, ctrl.pace_max);

        // TODO: should be interactive.
        // the rule has only all_0 and all_1 NOT flipped, and has interesting effect in extremely low/high densities.
        // (e.g. 0.01, 0.99)
        ImGui::Checkbox("Anti-strobing", &ctrl.anti_strobing);
        helper::show_help(
            "Actually this is not enough to guarantee smooth visual effect. For example, the following "
            "\"non-strobing\" rule has really terrible visual effect:\n\n"
            "MAPf/8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAQ\n\n"
            "In cases like this, it generally helps to set the pace to 2*n manually to remove bad visual effects.");
        if (ctrl.anti_strobing) {
            ImGui::SameLine();
            ImGui::Text(" (Actual pace: %d)", ctrl.actual_pace());
        }
    }
    ImGui::EndGroup();
    ImGui::SameLine();
    ImGui::BeginGroup();
    {
        if (int seed = init.seed; imgui_StepSliderInt("Init seed (0~99)", &seed, 0, 99)) {
            init.seed = seed;
            should_restart = true;
        }

        if (ImGui::SliderFloat("Init density (0~1)", &init.density, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_NoInput)) {
            should_restart = true;
        }

        {
            static char input_width[20]{}, input_height[20]{};
            const auto filter = [](ImGuiInputTextCallbackData* data) -> int {
                return (data->EventChar >= '0' && data->EventChar <= '9') ? 0 : 1;
            };
            const float s = imgui_ItemInnerSpacingX();
            const float w = (ImGui::CalcItemWidth() - s * 2 - ImGui::CalcTextSize("Resize").x -
                             ImGui::GetStyle().FramePadding.x * 2) /
                            2; // TODO: is floor/ceil needed?
            ImGui::SetNextItemWidth(w);
            ImGui::InputTextWithHint("##Width", "width", input_width, std::size(input_width),
                                     ImGuiInputTextFlags_CallbackCharFilter, filter);
            ImGui::SameLine(0, s);
            ImGui::SetNextItemWidth(w);
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
                    const legacy::tileT::sizeT size = size_clamped(width, height);
                    if (init.size != size) {
                        init.size = size;
                        sel.reset();
                        paste.reset();        // TODO: whether to show some message for these invalidation?
                        runner.restart(init); // TODO: about vs setting should_restart...
                    }
                }
                input_width[0] = '\0';
                input_height[0] = '\0';
            }

            ImGui::SameLine(0, s);
            ImGui::Text("Width:%d, Height:%d", runner.tile().width(), runner.tile().height());
        }

        ImGui::AlignTextToFramePadding();
        imgui_Str("Fit with zoom =");
        ImGui::SameLine();
        for (const ImVec2 size = square_size(); int z : {1, 2, 4, 8}) {
            if (z != 1) {
                ImGui::SameLine(0, imgui_ItemInnerSpacingX());
            }
            if (ImGui::Button(std::to_string(z).c_str(), size)) {
                screen_zoom = z;
                screen_off = {0, 0};

                // TODO: explain that `last_known_canvas_size` works well...
                const legacy::tileT::sizeT size = size_clamped((int)last_known_canvas_size.x / screen_zoom,
                                                               (int)last_known_canvas_size.y / screen_zoom);
                if (init.size != size) {
                    init.size = size;
                    sel.reset();
                    paste.reset(); // TODO: whether to show some message for these invalidation?
                    runner.restart(init);
                }
            }
        }
    }
    ImGui::EndGroup();

    ImGui::Separator();

    // TODO: set pattern as init state? what if size is already changed?
    // TODO: specify mouse-dragging behavior (especially, no-op must be an option)
    // TODO: range-selected randomization don't need fixed seed. However, there should be a way to specify density.
    // TODO: support drawing as a dragging behavior if easy.
    // TODO: copy vs copy to clipboard; paste vs paste from clipboard? (don't want to pollute clipboard with small
    // rle strings...
    // TODO: should be able to recognize "rule = " part in the rle string.
    // TODO: about the plan for boundless space and space period...

    const legacy::tileT::sizeT tile_size = runner.tile().size(); // TODO: which (vs init.size) is better?
    const ImVec2 screen_size(tile_size.width * screen_zoom, tile_size.height * screen_zoom);

    if (ImGui::Button("Corner")) {
        screen_off = {0, 0};
    }
    ImGui::SameLine();
    if (ImGui::Button("Center")) {
        screen_off = last_known_canvas_size / 2 - screen_size / 2;
        // TODO (temp) /screen_zoom->floor->*screen_zoom, so that screen_off will be {0, 0} after fitting.
        screen_off.x = floor(screen_off.x / screen_zoom) * screen_zoom;
        screen_off.y = floor(screen_off.y / screen_zoom) * screen_zoom;
    }
    // TODO: select all, unselect button...
    ImGui::SameLine();
    if (ImGui::Button("...")) {
        ImGui::OpenPopup("Tile_Menu"); // TODO: temp; when should the menu appear?
    }
    // TODO: whether to allow sel and paste to co-exist?
    if (sel) {
        ImGui::SameLine(), imgui_Str("|"), ImGui::SameLine();
        if (ImGui::Button(std::format("Drop selection (w:{} h:{})###drop_sel", sel->width(), sel->height()).c_str())) {
            sel.reset();
        }
    }
    if (paste) {
        ImGui::SameLine(), imgui_Str("|"), ImGui::SameLine();
        if (ImGui::Button(
                std::format("Drop paste (w:{} h:{})###drop_paste", paste->width(), paste->height()).c_str())) {
            paste.reset();
        }
    }

    ctrl.pause2 = false; // TODO: recheck when to set pause2...

    {
        ImGui::InvisibleButton("Canvas", [] {
            // Values of GetContentRegionAvail() can be negative...
            ImVec2 size = ImGui::GetContentRegionAvail();
            // TODO (temp) ~ min tile size * max zoom...
            return ImVec2(std::max(size.x, 160.0f), std::max(size.y, 80.0f));
        }());
        const ImVec2 canvas_min = ImGui::GetItemRectMin();
        const ImVec2 canvas_max = ImGui::GetItemRectMax();
        const ImVec2 canvas_size = ImGui::GetItemRectSize();
        last_known_canvas_size = canvas_size;

        if (ImGui::IsItemActive()) {
            ctrl.pause2 = true;
        }
        if (ImGui::IsItemHovered() && ImGui::IsItemActive()) {
            // Some logics rely on this to be done before rendering to work well.
            const ImGuiIO& io = ImGui::GetIO();
            if (io.KeyCtrl && screen_zoom == 1) {
                runner.rotate(io.MouseDelta.x, io.MouseDelta.y);
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
            refresh(screen, runner.tile());

            drawlist->AddImage(screen.texture(), screen_min, screen_max);
        } else {
            // TODO: displays poorly with miniwindow...

            assert(paste->width() <= tile_size.width && paste->height() <= tile_size.height);
            paste_beg.x = std::clamp(paste_beg.x, 0, tile_size.width - paste->width());
            paste_beg.y = std::clamp(paste_beg.y, 0, tile_size.height - paste->height());
            // TODO: rename...
            const legacy::tileT::rangeT range = {paste_beg, paste_beg + paste->size()};

            legacy::tileT temp(paste->size()); // TODO: wasteful...
            legacy::copy(temp, {0, 0}, runner.tile(), range);
            legacy::copy<legacy::copyE::Or>(runner.tile(), paste_beg, *paste);
            refresh(screen, runner.tile());
            legacy::copy(runner.tile(), paste_beg, temp);

            drawlist->AddImage(screen.texture(), screen_min, screen_max);
            const ImVec2 paste_min = screen_min + ImVec2(range.begin.x, range.begin.y) * screen_zoom;
            const ImVec2 paste_max = screen_min + ImVec2(range.end.x, range.end.y) * screen_zoom;
            drawlist->AddRectFilled(paste_min, paste_max, IM_COL32(255, 0, 0, 60));
        }

        if (sel) {
            const auto range = sel->to_range();
            const ImVec2 sel_min = screen_min + ImVec2(range.begin.x, range.begin.y) * screen_zoom;
            const ImVec2 sel_max = screen_min + ImVec2(range.end.x, range.end.y) * screen_zoom;
            drawlist->AddRectFilled(sel_min, sel_max, IM_COL32(0, 255, 0, 40));
            drawlist->AddRect(sel_min, sel_max, IM_COL32(0, 255, 0, 160));
        }
        drawlist->PopClipRect();

        // TODO: temporarily put here...
        if (imgui_KeyPressed(ImGuiKey_V, false)) {
            if (const char* text = ImGui::GetClipboardText()) {
                try {
                    // TODO: or ask whether to resize runner.tile?
                    paste.emplace(legacy::from_RLE_str(text, tile_size));
                } catch (const std::exception& err) {
                    messenger::add_msg("{}", err.what());
                }
            }
        }

        if (sel && sel->active && !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
            sel->active = false;
            // TODO: shrinking (bounding_box) has no size check like this. This is intentional.
            // (to allow a single r-click to unselect the area.)
            if (sel->width() <= 1 && sel->height() <= 1) {
                sel.reset();
            }
            // if (sel && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
            //     ImGui::OpenPopup("Tile_Menu");
            // }
        }

        if (ImGui::IsItemHovered()) {
            assert(ImGui::IsMousePosValid());
            // This will work well even if outside of the image.
            const ImVec2 mouse_pos = ImGui::GetIO().MousePos;
            const ImVec2 cell_pos_raw = (mouse_pos - screen_min) / screen_zoom;
            const int celx = floor(cell_pos_raw.x);
            const int cely = floor(cell_pos_raw.y);

            if (screen_zoom == 1 && !paste && !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                if (celx >= -10 && celx < tile_size.width + 10 && cely >= -10 && cely < tile_size.height + 10) {
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
                    if (ImGui::BeginItemTooltip()) {
                        const int w = std::min(tile_size.width, 40);
                        const int h = std::min(tile_size.height, 40);

                        int minx = celx - w / 2, miny = cely - h / 2;
                        int maxx = celx + (w - w / 2), maxy = cely + (h - h / 2);
                        if (minx < 0) {
                            minx = 0, maxx = w;
                        }
                        if (miny < 0) {
                            miny = 0, maxy = h;
                        }
                        if (maxx > tile_size.width) {
                            minx = tile_size.width - w, maxx = tile_size.width;
                        }
                        if (maxy > tile_size.height) {
                            miny = tile_size.height - h, maxy = tile_size.height;
                        }

                        assert(w == maxx - minx && h == maxy - miny);
                        ImGui::Image(screen.texture(), ImVec2(w * 4, h * 4),
                                     {(float)minx / tile_size.width, (float)miny / tile_size.height},
                                     {(float)maxx / tile_size.width, (float)maxy / tile_size.height});
                        ImGui::EndTooltip();
                    }
                    ImGui::PopStyleVar();
                }
            }

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                const legacy::tileT::posT pos{.x = std::clamp(celx, 0, tile_size.width - 1),
                                              .y = std::clamp(cely, 0, tile_size.height - 1)};
                sel = {.active = true, .beg = pos, .end = pos};
            } else if (sel && sel->active && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                sel->end.x = std::clamp(celx, 0, tile_size.width - 1);
                sel->end.y = std::clamp(cely, 0, tile_size.height - 1);
            }

            // TODO: refactor away this block...
            // TODO: the logic will be simplified if invisible button goes before the image...
            if (paste) {
                assert(paste->width() <= tile_size.width && paste->height() <= tile_size.height);
                paste_beg.x = std::clamp(celx, 0, tile_size.width - paste->width());
                paste_beg.y = std::clamp(cely, 0, tile_size.height - paste->height());

                if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    legacy::copy<legacy::copyE::Or>(runner.tile(), paste_beg, *paste);
                    paste.reset();
                }
            }

            if (imgui_MouseScrolling()) {
                if (imgui_MouseScrollingDown() && screen_zoom != 1) {
                    screen_zoom /= 2;
                }
                if (imgui_MouseScrollingUp() && screen_zoom != 8) {
                    screen_zoom *= 2;
                }
                screen_off = (mouse_pos - cell_pos_raw * screen_zoom) - canvas_min;
                screen_off.x = round(screen_off.x);
                screen_off.y = round(screen_off.y);
            }
        }

        if (imgui_KeyPressed(ImGuiKey_A, false)) {
            if (!sel || sel->width() != tile_size.width || sel->height() != tile_size.height) {
                sel = {.active = false, .beg = {0, 0}, .end = {.x = tile_size.width - 1, .y = tile_size.height - 1}};
            } else {
                sel.reset();
            }
        }

        // TODO: redesign keyboard ctrl...
        // TODO: (Must) finish...
        static float fill_den = 0.5;
        enum opE { None, Random_fill, Clear_inside, Clear_outside, Shrink, Copy, Cut, Capture };
        opE op = None;
        // TODO: make this actually work...
        if (ImGui::BeginPopup("Tile_Menu")) {
            ctrl.pause2 = true;
            ImGui::SliderFloat("Fill density", &fill_den, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_NoInput);
            ImGui::Separator();
            // These can be chained by `else if` without visual effect, as the popup will disappear after clicking.
            if (ImGui::MenuItem("Random fill", "=")) {
                op = Random_fill;
            } else if (ImGui::MenuItem("Clear", "backspace")) { // TODO: 0/1
                op = Clear_inside;
            } else if (ImGui::MenuItem("Clear outside", "0")) { // TODO: 0/1
                op = Clear_outside;
            } else if (ImGui::MenuItem("Shrink", "s")) { // TODO: 0/1
                op = Shrink;
            } else if (ImGui::MenuItem("Copy", "c")) {
                op = Copy;
            } else if (ImGui::MenuItem("Cut", "x")) {
                op = Cut;
            } else if (ImGui::MenuItem("Capture", "p")) {
                op = Capture;
            }
            // TODO: document other keyboard-only operations...
            ImGui::EndPopup();
        }
        if (sel) {
            const auto range = sel->to_range();

            // TODO: what if these keys are pressed together?
            if (imgui_KeyPressed(ImGuiKey_S, false) || op == Shrink) {
                const auto [begin, end] = legacy::bounding_box(runner.tile(), range);
                if (begin != end) {
                    sel = {.active = false, .beg = begin, .end = {.x = end.x - 1, .y = end.y - 1}};
                } else {
                    sel.reset();
                }
            }
            if (imgui_KeyPressed(ImGuiKey_C, false) || imgui_KeyPressed(ImGuiKey_X, false) || op == Copy || op == Cut) {
                ImGui::SetClipboardText(legacy::to_RLE_str(ctrl.rule, runner.tile(), range).c_str());
                // messenger::add_msg("{}", legacy::to_RLE_str(ctrl.rule, runner.tile(), range));
            }
            if (imgui_KeyPressed(ImGuiKey_Backspace, false) || imgui_KeyPressed(ImGuiKey_X, false) ||
                op == Clear_inside || op == Cut) {
                legacy::clear_inside(runner.tile(), range);
            }
            if (imgui_KeyPressed(ImGuiKey_Equal, false) || op == Random_fill) {
                legacy::random_fill(runner.tile(), global_mt19937(), fill_den, range);
            }
            if (imgui_KeyPressed(ImGuiKey_0, false) || op == Clear_outside) {
                legacy::clear_outside(runner.tile(), range);
            }
            if (imgui_KeyPressed(ImGuiKey_P, false) || op == Capture) {
                out = capture_closed(runner.tile(), range, ctrl.rule);
            }
        }
    }

    if (paste || (sel && sel->active)) {
        ctrl.pause2 = true;
    }

    ImGui::PopItemWidth();

    if (should_restart) {
        runner.restart(init);
    }
    ctrl.run(runner, extra); // TODO: able to result in low fps...

    return out;
}
