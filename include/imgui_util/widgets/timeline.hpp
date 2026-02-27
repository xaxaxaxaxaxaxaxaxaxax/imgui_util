// timeline.hpp - Horizontal timeline with tracks and draggable events
//
// Usage:
//   static imgui_util::timeline tl(200.0f);
//   std::vector<imgui_util::timeline_event> events = {
//       {0.0f, 5.0f, "Intro", IM_COL32(100, 150, 255, 255), 0},
//       {3.0f, 8.0f, "Audio", IM_COL32(255, 100, 100, 255), 1},
//   };
//   static float playhead = 0.0f;
//   if (tl.render("##timeline", events, playhead, 0.0f, 20.0f)) {
//       // events or playhead changed
//   }
//
// Supports dragging events (move, resize from edges), playhead dragging,
// and snap-to-grid.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <imgui.h>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "imgui_util/core/fmt_buf.hpp"
#include "imgui_util/core/raii.hpp"

namespace imgui_util {

    struct timeline_event {
        float            start = 0.0f;
        float            end   = 0.0f;
        std::string_view label;
        ImU32            color = IM_COL32(100, 150, 255, 255);
        int              track = 0;
    };

    class timeline {
    public:
        explicit timeline(const float height = 150.0f) noexcept : height_(height) {}

        // Returns true if any event or the playhead was modified.
        [[nodiscard]] bool render(const std::string_view str_id, const std::span<timeline_event> events,
                                  float &playhead, const float visible_start, const float visible_end) noexcept {
            if (visible_end <= visible_start) return false;

            const id     scope{str_id.data()};
            const ImVec2 canvas_pos  = ImGui::GetCursorScreenPos();
            const float  canvas_w    = ImGui::GetContentRegionAvail().x;
            const ImVec2 canvas_size = {canvas_w, height_};

            ImGui::InvisibleButton("##timeline_canvas", canvas_size);

            const render_context ctx{
                .dl             = ImGui::GetWindowDrawList(),
                .canvas_pos     = canvas_pos,
                .canvas_w       = canvas_w,
                .height         = height_,
                .visible_start  = visible_start,
                .visible_end    = visible_end,
                .time_range     = visible_end - visible_start,
                .canvas_hovered = ImGui::IsItemHovered(),
                .mouse          = ImGui::GetMousePos(),
                .snap           = snap_,
            };

            ctx.dl->AddRectFilled(canvas_pos, {canvas_pos.x + canvas_w, canvas_pos.y + height_},
                                  IM_COL32(30, 30, 30, 255));

            bool changed = false;
            render_ruler(ctx);
            const auto [tracks_top, actual_track_h] = render_tracks(ctx, events);
            render_events(ctx, events, tracks_top, actual_track_h, changed);
            handle_event_drag(ctx, events, changed);
            render_playhead(ctx, playhead, changed);

            return changed;
        }

        [[nodiscard]] timeline &set_snap(const float interval = 0.0f) noexcept {
            snap_ = interval;
            return *this;
        }

        [[nodiscard]] timeline &set_track_height(const float h) noexcept {
            track_height_ = h;
            return *this;
        }

        [[nodiscard]] timeline &set_track_labels(const std::span<const std::string_view> labels) {
            track_labels_.assign(labels.begin(), labels.end());
            return *this;
        }

    private:
        float                         height_;
        float                         snap_         = 0.0f;
        float                         track_height_ = 30.0f;
        std::vector<std::string_view> track_labels_;
        int                           dragging_event_               = -1;
        enum class drag_edge { none, body, left, right } drag_edge_ = drag_edge::none;
        float drag_offset_                                          = 0.0f;

        struct render_context {
            ImDrawList *dl;
            ImVec2      canvas_pos;
            float       canvas_w, height;
            float       visible_start, visible_end, time_range;
            bool        canvas_hovered;
            ImVec2      mouse;
            float       snap;

            [[nodiscard]] float time_to_x(const float t) const noexcept {
                return canvas_pos.x + (t - visible_start) / time_range * canvas_w;
            }

            [[nodiscard]] float x_to_time(const float x) const noexcept {
                return visible_start + (x - canvas_pos.x) / canvas_w * time_range;
            }

            [[nodiscard]] float snap_time(float t) const noexcept {
                if (snap > 0.0f) t = std::round(t / snap) * snap;
                return t;
            }
        };

        struct track_layout {
            float tracks_top;
            float actual_track_h;
        };

        static constexpr float ruler_h = 20.0f;

        void render_ruler(const render_context &ctx) const noexcept {
            ctx.dl->AddRectFilled(ctx.canvas_pos, {ctx.canvas_pos.x + ctx.canvas_w, ctx.canvas_pos.y + ruler_h},
                                  IM_COL32(45, 45, 45, 255));

            const float pixels_per_unit = ctx.canvas_w / ctx.time_range;
            const float tick_interval   = [&]() noexcept -> float {
                if (pixels_per_unit < 5.0f) return 10.0f;
                if (pixels_per_unit < 20.0f) return 5.0f;
                if (pixels_per_unit < 50.0f) return 2.0f;
                if (pixels_per_unit > 200.0f) return 0.5f;
                return 1.0f;
            }();

            const float first_tick = std::ceil(ctx.visible_start / tick_interval) * tick_interval;
            const int   tick_count = static_cast<int>((ctx.visible_end - first_tick) / tick_interval) + 1;
            for (int ti = 0; ti < tick_count; ++ti) {
                const float t = first_tick + static_cast<float>(ti) * tick_interval;
                const float x = ctx.time_to_x(t);
                ctx.dl->AddLine({x, ctx.canvas_pos.y}, {x, ctx.canvas_pos.y + ruler_h}, IM_COL32(120, 120, 120, 255),
                                1.0f);
                const fmt_buf<16> label{"{:.1f}", t};
                ctx.dl->AddText({x + 2.0f, ctx.canvas_pos.y + 2.0f}, IM_COL32(180, 180, 180, 255), label.c_str(),
                                label.end());
            }
        }

        [[nodiscard]] track_layout render_tracks(const render_context           &ctx,
                                                 const std::span<timeline_event> events) const noexcept {
            const int   max_track    = events.empty() ? 0 : std::ranges::max(events, {}, &timeline_event::track).track;
            const int   num_tracks   = max_track + 1;
            const float tracks_top   = ctx.canvas_pos.y + ruler_h;
            const float tracks_avail = ctx.height - ruler_h;
            const float actual_track_h =
                std::min(track_height_, num_tracks > 0 ? tracks_avail / static_cast<float>(num_tracks) : tracks_avail);

            for (int t = 0; t < num_tracks; ++t) {
                const float y0 = tracks_top + static_cast<float>(t) * actual_track_h;
                const float y1 = y0 + actual_track_h;

                const ImU32 track_bg = t % 2 == 0 ? IM_COL32(35, 35, 35, 255) : IM_COL32(40, 40, 40, 255);
                ctx.dl->AddRectFilled({ctx.canvas_pos.x, y0}, {ctx.canvas_pos.x + ctx.canvas_w, y1}, track_bg);

                if (std::cmp_less(t, track_labels_.size())) {
                    const auto &tl = track_labels_[static_cast<std::size_t>(t)];
                    ctx.dl->AddText({ctx.canvas_pos.x + 4.0f, y0 + 2.0f}, IM_COL32(140, 140, 140, 255), tl.data(),
                                    tl.data() + tl.size());
                }

                ctx.dl->AddLine({ctx.canvas_pos.x, y1}, {ctx.canvas_pos.x + ctx.canvas_w, y1},
                                IM_COL32(60, 60, 60, 255));
            }

            return {tracks_top, actual_track_h};
        }

        void render_events(const render_context &ctx, const std::span<timeline_event> events, const float tracks_top,
                           const float actual_track_h, bool &changed) noexcept {
            for (int i = 0; i < static_cast<int>(events.size()); ++i) {
                constexpr float event_padding           = 2.0f;
                auto &[start, end, label, color, track] = events[static_cast<std::size_t>(i)];

                const float x0 = ctx.time_to_x(start);
                const float x1 = ctx.time_to_x(end);
                const float y0 = tracks_top + static_cast<float>(track) * actual_track_h + event_padding;
                const float y1 = y0 + actual_track_h - event_padding * 2.0f;

                ctx.dl->AddRectFilled({x0, y0}, {x1, y1}, color, 3.0f);
                ctx.dl->AddRect({x0, y0}, {x1, y1}, IM_COL32(255, 255, 255, 60), 3.0f);

                if (!label.empty() && x1 - x0 > 20.0f) {
                    ctx.dl->PushClipRect({x0, y0}, {x1, y1}, true);
                    ctx.dl->AddText({x0 + 4.0f, y0 + 2.0f}, IM_COL32(255, 255, 255, 220), label.data(),
                                    label.data() + label.size());
                    ctx.dl->PopClipRect();
                }

                if (ctx.canvas_hovered && dragging_event_ == -1) {
                    if (ctx.mouse.x >= x0 && ctx.mouse.x <= x1 && ctx.mouse.y >= y0 && ctx.mouse.y <= y1) {
                        if (constexpr float edge_grab_w = 6.0f; ctx.mouse.x - x0 < edge_grab_w) {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                            if (ImGui::IsMouseClicked(0)) {
                                dragging_event_ = i;
                                drag_edge_      = drag_edge::left;
                                drag_offset_    = 0.0f;
                            }
                        } else if (x1 - ctx.mouse.x < edge_grab_w) {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                            if (ImGui::IsMouseClicked(0)) {
                                dragging_event_ = i;
                                drag_edge_      = drag_edge::right;
                                drag_offset_    = 0.0f;
                            }
                        } else {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                            if (ImGui::IsMouseClicked(0)) {
                                dragging_event_ = i;
                                drag_edge_      = drag_edge::body;
                                drag_offset_    = ctx.x_to_time(ctx.mouse.x) - start;
                            }
                        }
                    }
                }
            }
        }

        void handle_event_drag(const render_context &ctx, const std::span<timeline_event> events,
                               bool &changed) noexcept {
            if (dragging_event_ >= 0 && std::cmp_less(dragging_event_, events.size())) {
                if (ImGui::IsMouseDragging(0)) {
                    auto       &ev      = events[static_cast<std::size_t>(dragging_event_)];
                    const float mouse_t = ctx.x_to_time(ctx.mouse.x);

                    switch (drag_edge_) {
                        case drag_edge::body: {
                            const float duration  = ev.end - ev.start;
                            const float new_start = ctx.snap_time(mouse_t - drag_offset_);
                            ev.start              = new_start;
                            ev.end                = new_start + duration;
                            changed               = true;
                            break;
                        }
                        case drag_edge::left: {
                            if (const float new_start = ctx.snap_time(mouse_t); new_start < ev.end) {
                                ev.start = new_start;
                                changed  = true;
                            }
                            break;
                        }
                        case drag_edge::right: {
                            if (const float new_end = ctx.snap_time(mouse_t); new_end > ev.start) {
                                ev.end  = new_end;
                                changed = true;
                            }
                            break;
                        }
                        case drag_edge::none:
                            break;
                    }
                }
                if (ImGui::IsMouseReleased(0)) {
                    dragging_event_ = -1;
                    drag_edge_      = drag_edge::none;
                }
            }
        }

        void render_playhead(const render_context &ctx, float &playhead, bool &changed) const noexcept {
            const float ph_x = ctx.time_to_x(playhead);
            ctx.dl->AddLine({ph_x, ctx.canvas_pos.y}, {ph_x, ctx.canvas_pos.y + ctx.height}, IM_COL32(255, 80, 80, 255),
                            2.0f);
            ctx.dl->AddTriangleFilled({ph_x - 5.0f, ctx.canvas_pos.y}, {ph_x + 5.0f, ctx.canvas_pos.y},
                                      {ph_x, ctx.canvas_pos.y + 8.0f}, IM_COL32(255, 80, 80, 255));

            if (ctx.canvas_hovered && dragging_event_ == -1 && ctx.mouse.y >= ctx.canvas_pos.y
                && ctx.mouse.y <= ctx.canvas_pos.y + ruler_h) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                if (ImGui::IsMouseClicked(0) || ImGui::IsMouseDragging(0)) {
                    if (const float new_ph = ctx.snap_time(ctx.x_to_time(ctx.mouse.x)); new_ph != playhead) {
                        playhead = new_ph;
                        changed  = true;
                    }
                }
            }
        }
    };

} // namespace imgui_util
