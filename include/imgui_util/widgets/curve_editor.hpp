/// @file curve_editor.hpp
/// @brief Keyframe curve editor with cubic hermite interpolation.
///
/// Features: draggable keyframes, tangent handles, double-click to add, Delete to remove,
/// mouse wheel zoom, snap grid, and cubic hermite interpolation.
///
/// Usage:
/// @code
///   static imgui_util::curve_editor editor{{-1, 200}};
///   static std::vector<imgui_util::keyframe> keys = {{0.f, 0.f}, {0.5f, 1.f}, {1.f, 0.f}};
///   if (editor.render("##curve", keys)) { /* keys were modified */ }
///
///   // Evaluate the curve at a given time:
///   float val = imgui_util::curve_editor::evaluate(keys, 0.25f);
/// @endcode
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <imgui.h>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "imgui_util/core/fmt_buf.hpp"
#include "imgui_util/core/raii.hpp"

namespace imgui_util {

    namespace detail {

        /**
         * @brief RAII wrapper for ImDrawList::PushClipRect / PopClipRect.
         *
         * Distinct from ImGui::PushClipRect which is wrapped by imgui_util::clip_rect.
         */
        class [[nodiscard]] draw_list_clip_rect {
            ImDrawList *dl_;

        public:
            draw_list_clip_rect(ImDrawList *dl, const ImVec2 &min, const ImVec2 &max,
                                const bool intersect = true) noexcept :
                dl_(dl) {
                dl_->PushClipRect(min, max, intersect);
            }

            ~draw_list_clip_rect() { dl_->PopClipRect(); }

            draw_list_clip_rect(const draw_list_clip_rect &)            = delete;
            draw_list_clip_rect &operator=(const draw_list_clip_rect &) = delete;
            draw_list_clip_rect(draw_list_clip_rect &&)                 = delete;
            draw_list_clip_rect &operator=(draw_list_clip_rect &&)      = delete;
        };

    } // namespace detail

    /// @brief A single keyframe with time, value, and tangent slopes for cubic hermite interpolation.
    struct keyframe {
        float time;               ///< Time position along the curve.
        float value;              ///< Value at this keyframe.
        float tangent_in  = 0.0f; ///< Incoming tangent slope.
        float tangent_out = 0.0f; ///< Outgoing tangent slope.
    };

    /**
     * @brief Keyframe curve editor with cubic hermite interpolation.
     *
     * Features: draggable keyframes, tangent handles, double-click to add,
     * Delete key to remove, snap grid, and configurable appearance.
     */
    class curve_editor {
    public:
        explicit curve_editor(const ImVec2 size = {-1, 200}) noexcept : size_(size) {}

        /**
         * @brief Render the curve editor and handle interaction.
         * @param id    ImGui ID string.
         * @param keys  Keyframes to display and edit (sorted by time internally).
         * @param t_min Left edge of the visible time range.
         * @param t_max Right edge of the visible time range.
         * @param v_min Bottom edge of the visible value range.
         * @param v_max Top edge of the visible value range.
         * @return True if any keyframe was added, removed, or modified this frame.
         */
        bool render(const char *id, std::vector<keyframe> &keys, const float t_min = 0.f, const float t_max = 1.f,
                    const float v_min = 0.f, const float v_max = 1.f) {
            bool modified = false;

            const imgui_util::id scope{id};

            // Canvas setup
            const render_context ctx = setup_canvas(t_min, t_max, v_min, v_max);

            // Clip drawing to canvas
            const detail::draw_list_clip_rect clip{ctx.dl, ctx.canvas_pos, ctx.canvas_end, true};

            render_grid(ctx);

            if (keys_dirty_) {
                std::ranges::sort(keys, {}, &keyframe::time);
                keys_dirty_ = false;
            }

            render_curve(ctx, keys);
            render_keyframes(ctx, keys, modified);

            handle_point_drag(ctx, keys, modified);
            handle_click_detection(ctx, keys);
            handle_add_keyframe(ctx, keys, modified);
            handle_delete_keyframe(keys, modified);

            return modified;
        }

        /**
         * @brief Render the curve without any interaction (read-only).
         * @param id    ImGui ID string.
         * @param keys  Keyframes defining the curve (not modified).
         * @param t_min Left edge of the visible time range.
         * @param t_max Right edge of the visible time range.
         * @param v_min Bottom edge of the visible value range.
         * @param v_max Top edge of the visible value range.
         */
        void render_readonly(const char *id, const std::span<const keyframe> keys, const float t_min = 0.f,
                             const float t_max = 1.f, const float v_min = 0.f, const float v_max = 1.f) const {
            const imgui_util::id              scope{id};
            const render_context              ctx = setup_canvas(t_min, t_max, v_min, v_max);
            const detail::draw_list_clip_rect clip{ctx.dl, ctx.canvas_pos, ctx.canvas_end, true};
            render_grid(ctx);
            render_curve(ctx, keys);
        }

        /**
         * @brief Evaluate the curve at time @p t using cubic hermite interpolation.
         * @param keys Sorted keyframes defining the curve.
         * @param t    Time value to evaluate at (clamped to first/last keyframe).
         * @return Interpolated value at time @p t.
         */
        [[nodiscard]] static float evaluate(const std::span<const keyframe> keys, const float t) noexcept {
            if (keys.empty()) return 0.0f;
            if (keys.size() == 1) return keys[0].value;

            // Clamp to first/last keyframe
            if (t <= keys.front().time) return keys.front().value;
            if (t >= keys.back().time) return keys.back().value;

            // Find the segment containing t using binary search
            auto it = std::ranges::lower_bound(keys, t, std::less{}, &keyframe::time);
            if (it == keys.begin()) it = keys.begin() + 1;
            const std::size_t seg = static_cast<std::size_t>(it - keys.begin()) - 1;

            const auto &k0 = keys[seg];
            const auto &k1 = keys[seg + 1];
            const float dt = k1.time - k0.time;
            if (dt <= 0.0f) return k0.value;

            // Normalized parameter within segment
            const float u = (t - k0.time) / dt;

            // Cubic hermite basis functions
            const float u2  = u * u;
            const float u3  = u2 * u;
            const float h00 = 2.0f * u3 - 3.0f * u2 + 1.0f;
            const float h10 = u3 - 2.0f * u2 + u;
            const float h01 = -2.0f * u3 + 3.0f * u2;
            const float h11 = u3 - u2;

            // Scale tangents by segment length
            const float m0 = k0.tangent_out * dt;
            const float m1 = k1.tangent_in * dt;

            return h00 * k0.value + h10 * m0 + h01 * k1.value + h11 * m1;
        }

        /**
         * @brief Configure grid display. Chainable.
         * @param show   Whether to draw grid lines.
         * @param t_step Horizontal grid spacing (time axis).
         * @param v_step Vertical grid spacing (value axis).
         */
        [[nodiscard]] curve_editor &set_grid(const bool show, const float t_step = 0.1f,
                                             const float v_step = 0.1f) noexcept {
            show_grid_   = show;
            grid_t_step_ = t_step;
            grid_v_step_ = v_step;
            return *this;
        }

        /**
         * @brief Enable snap-to-grid for dragged keyframes. Chainable.
         * @param t_snap Time-axis snap interval (0 to disable).
         * @param v_snap Value-axis snap interval (0 to disable).
         */
        [[nodiscard]] curve_editor &set_snap(const float t_snap = 0.0f, const float v_snap = 0.0f) noexcept {
            snap_t_ = t_snap;
            snap_v_ = v_snap;
            return *this;
        }

        /// @brief Set the curve line color. Chainable.
        [[nodiscard]] curve_editor &set_color(const ImU32 color) noexcept {
            curve_color_ = color;
            return *this;
        }

    private:
        ImVec2             size_;
        bool               keys_dirty_  = true;
        bool               show_grid_   = true;
        float              grid_t_step_ = 0.1f;
        float              grid_v_step_ = 0.1f;
        float              snap_t_      = 0.0f;
        float              snap_v_      = 0.0f;
        ImU32              curve_color_ = IM_COL32(255, 200, 50, 255);
        std::optional<int> selected_key_;
        std::optional<int> dragging_key_;

        static constexpr float point_radius = 5.0f;

        enum class drag_part { none, point, tangent_in, tangent_out };
        drag_part dragging_part_ = drag_part::none;

        struct render_context {
            ImDrawList *dl{};
            ImVec2      canvas_pos;
            ImVec2      canvas_end;
            ImVec2      canvas_size;
            float       t_min{};
            float       t_max{};
            float       v_min{};
            float       v_max{};
            float       t_range{};
            float       v_range{};
            bool        canvas_hovered{};
            ImVec2      mouse;

            [[nodiscard]] ImVec2 to_screen(const float t, const float v) const noexcept {
                const float sx = canvas_pos.x + (t - t_min) / t_range * canvas_size.x;
                const float sy = canvas_end.y - (v - v_min) / v_range * canvas_size.y;
                return {sx, sy};
            }

            [[nodiscard]] std::pair<float, float> to_value(const ImVec2 screen) const noexcept {
                const float t = t_min + (screen.x - canvas_pos.x) / canvas_size.x * t_range;
                const float v = v_min + (canvas_end.y - screen.y) / canvas_size.y * v_range;
                return {t, v};
            }
        };

        [[nodiscard]] render_context setup_canvas(const float t_min, const float t_max, const float v_min,
                                                  const float v_max) const noexcept {
            const ImVec2 avail = ImGui::GetContentRegionAvail();
            const ImVec2 canvas_size{
                size_.x > 0 ? size_.x : avail.x,
                size_.y > 0 ? size_.y : avail.y,
            };

            const ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
            const ImVec2 canvas_end{canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y};

            ImGui::InvisibleButton("##canvas", canvas_size,
                                   ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
            const bool canvas_hovered = ImGui::IsItemHovered();

            auto *dl = ImGui::GetWindowDrawList();

            // Background
            dl->AddRectFilled(canvas_pos, canvas_end, IM_COL32(30, 30, 30, 255));
            dl->AddRect(canvas_pos, canvas_end, IM_COL32(80, 80, 80, 255));

            return {
                .dl             = dl,
                .canvas_pos     = canvas_pos,
                .canvas_end     = canvas_end,
                .canvas_size    = canvas_size,
                .t_min          = t_min,
                .t_max          = t_max,
                .v_min          = v_min,
                .v_max          = v_max,
                .t_range        = t_max - t_min,
                .v_range        = v_max - v_min,
                .canvas_hovered = canvas_hovered,
                .mouse          = ImGui::GetIO().MousePos,
            };
        }

        void render_grid(const render_context &ctx) const {
            if (!show_grid_) return;

            constexpr ImU32 grid_color       = IM_COL32(60, 60, 60, 255);
            constexpr ImU32 grid_label_color = IM_COL32(120, 120, 120, 255);

            // Vertical grid lines (time axis)
            if (grid_t_step_ > 0.0f) {
                const float t_start = std::ceil(ctx.t_min / grid_t_step_) * grid_t_step_;
                const int   t_count = static_cast<int>((ctx.t_max - t_start) / grid_t_step_) + 1;
                for (int ti = 0; ti < t_count; ++ti) {
                    const float  t = t_start + static_cast<float>(ti) * grid_t_step_;
                    const ImVec2 p = ctx.to_screen(t, 0);
                    ctx.dl->AddLine({p.x, ctx.canvas_pos.y}, {p.x, ctx.canvas_end.y}, grid_color);
                    const fmt_buf<16> label("{:.2f}", t);
                    ctx.dl->AddText({p.x + 2, ctx.canvas_end.y - 14}, grid_label_color, label.c_str(), label.end());
                }
            }

            // Horizontal grid lines (value axis)
            if (grid_v_step_ > 0.0f) {
                const float v_start = std::ceil(ctx.v_min / grid_v_step_) * grid_v_step_;
                const int   v_count = static_cast<int>((ctx.v_max - v_start) / grid_v_step_) + 1;
                for (int vi = 0; vi < v_count; ++vi) {
                    const float  v = v_start + static_cast<float>(vi) * grid_v_step_;
                    const ImVec2 p = ctx.to_screen(0, v);
                    ctx.dl->AddLine({ctx.canvas_pos.x, p.y}, {ctx.canvas_end.x, p.y}, grid_color);
                    const fmt_buf<16> label("{:.2f}", v);
                    ctx.dl->AddText({ctx.canvas_pos.x + 2, p.y - 14}, grid_label_color, label.c_str(), label.end());
                }
            }
        }

        void render_curve(const render_context &ctx, const std::span<const keyframe> keys) const {
            if (keys.size() >= 2) {
                constexpr int                        sample_count = 256;
                std::array<ImVec2, sample_count + 1> pts;

                pts[0] = ctx.to_screen(keys.front().time, keys.front().value);
                for (int i = 1; i <= sample_count; ++i) {
                    const float frac                 = static_cast<float>(i) / static_cast<float>(sample_count);
                    const float t                    = ctx.t_min + frac * ctx.t_range;
                    pts[static_cast<std::size_t>(i)] = ctx.to_screen(t, evaluate(keys, t));
                }
                ctx.dl->AddPolyline(pts.data(), pts.size(), curve_color_, 0, 2.0f);
            } else if (keys.size() == 1) {
                const ImVec2 left  = ctx.to_screen(ctx.t_min, keys[0].value);
                const ImVec2 right = ctx.to_screen(ctx.t_max, keys[0].value);
                ctx.dl->AddLine(left, right, curve_color_, 2.0f);
            }
        }

        void render_keyframes(const render_context &ctx, std::vector<keyframe> &keys, bool &modified) {
            for (int ki = 0; std::cmp_less(ki, keys.size()); ++ki) {
                auto &[time, value, tangent_in, tangent_out] = keys[static_cast<std::size_t>(ki)];
                const ImVec2 pos                             = ctx.to_screen(time, value);
                const bool   is_selected                     = selected_key_.has_value() && *selected_key_ == ki;

                // Draw tangent handles for selected key
                if (is_selected) {
                    constexpr float tangent_len_px = 40.0f;
                    constexpr float handle_radius  = 3.0f;
                    // Tangent in handle
                    const ImVec2    tan_in{
                        pos.x - tangent_len_px,
                        pos.y + tangent_in * tangent_len_px,
                    };
                    ctx.dl->AddLine(pos, tan_in, IM_COL32(100, 180, 255, 180), 1.0f);
                    ctx.dl->AddCircleFilled(tan_in, handle_radius, IM_COL32(100, 180, 255, 220));

                    // Tangent out handle
                    const ImVec2 tan_out{
                        pos.x + tangent_len_px,
                        pos.y - tangent_out * tangent_len_px,
                    };
                    ctx.dl->AddLine(pos, tan_out, IM_COL32(100, 180, 255, 180), 1.0f);
                    ctx.dl->AddCircleFilled(tan_out, handle_radius, IM_COL32(100, 180, 255, 220));

                    // Tangent handle interaction
                    if (dragging_key_.has_value() && *dragging_key_ == ki && dragging_part_ == drag_part::tangent_in) {
                        if (ImGui::IsMouseDown(0)) {
                            tangent_in  = -((ctx.mouse.y - pos.y) / tangent_len_px);
                            keys_dirty_ = true;
                            modified    = true;
                        } else {
                            dragging_key_.reset();
                            dragging_part_ = drag_part::none;
                        }
                    } else if (dragging_key_.has_value() && *dragging_key_ == ki
                               && dragging_part_ == drag_part::tangent_out) {
                        if (ImGui::IsMouseDown(0)) {
                            tangent_out = -((ctx.mouse.y - pos.y) / tangent_len_px);
                            keys_dirty_ = true;
                            modified    = true;
                        } else {
                            dragging_key_.reset();
                            dragging_part_ = drag_part::none;
                        }
                    }

                    // Start dragging tangent handles
                    if (ctx.canvas_hovered && ImGui::IsMouseClicked(0)) {
                        const float dist_in  = std::hypot(ctx.mouse.x - tan_in.x, ctx.mouse.y - tan_in.y);
                        const float dist_out = std::hypot(ctx.mouse.x - tan_out.x, ctx.mouse.y - tan_out.y);
                        if (dist_in <= handle_radius * 2.0f) {
                            dragging_key_  = ki;
                            dragging_part_ = drag_part::tangent_in;
                        } else if (dist_out <= handle_radius * 2.0f) {
                            dragging_key_  = ki;
                            dragging_part_ = drag_part::tangent_out;
                        }
                    }
                }

                // Draw keyframe point
                const ImU32 point_color = is_selected ? IM_COL32(255, 255, 100, 255) : IM_COL32(255, 200, 50, 255);
                ctx.dl->AddCircleFilled(pos, point_radius, point_color);
                ctx.dl->AddCircle(pos, point_radius, IM_COL32(255, 255, 255, 180));
            }
        }

        void handle_point_drag(const render_context &ctx, std::vector<keyframe> &keys, bool &modified) {
            if (!dragging_key_.has_value() || dragging_part_ != drag_part::point) return;

            if (ImGui::IsMouseDown(0)) {
                auto &key   = keys[static_cast<std::size_t>(*dragging_key_)];
                auto [t, v] = ctx.to_value(ctx.mouse);

                if (snap_t_ > 0.0f) t = std::round(t / snap_t_) * snap_t_;
                if (snap_v_ > 0.0f) v = std::round(v / snap_v_) * snap_v_;

                key.time    = std::clamp(t, ctx.t_min, ctx.t_max);
                key.value   = std::clamp(v, ctx.v_min, ctx.v_max);
                keys_dirty_ = true;
                modified    = true;
            } else {
                dragging_key_.reset();
                dragging_part_ = drag_part::none;
            }
        }

        void handle_click_detection(const render_context &ctx, const std::vector<keyframe> &keys) {
            if (!ctx.canvas_hovered || !ImGui::IsMouseClicked(0) || dragging_part_ != drag_part::none) return;

            int   closest      = -1;
            float closest_dist = point_radius * 2.0f;
            for (int ki = 0; std::cmp_less(ki, keys.size()); ++ki) {
                const ImVec2 pos =
                    ctx.to_screen(keys[static_cast<std::size_t>(ki)].time, keys[static_cast<std::size_t>(ki)].value);
                if (const float dist = std::hypot(ctx.mouse.x - pos.x, ctx.mouse.y - pos.y); dist < closest_dist) {
                    closest_dist = dist;
                    closest      = ki;
                }
            }
            if (closest >= 0) {
                selected_key_  = closest;
                dragging_key_  = closest;
                dragging_part_ = drag_part::point;
            } else {
                selected_key_.reset();
            }
        }

        void handle_add_keyframe(const render_context &ctx, std::vector<keyframe> &keys, bool &modified) {
            if (!ctx.canvas_hovered || !ImGui::IsMouseDoubleClicked(0)) return;

            bool on_point = false;
            for (const auto &key: keys) {
                if (const ImVec2 pos = ctx.to_screen(key.time, key.value);
                    std::hypot(ctx.mouse.x - pos.x, ctx.mouse.y - pos.y) < point_radius * 2.0f) {
                    on_point = true;
                    break;
                }
            }
            if (!on_point) {
                auto [t, v] = ctx.to_value(ctx.mouse);
                t           = std::clamp(t, ctx.t_min, ctx.t_max);
                v           = std::clamp(v, ctx.v_min, ctx.v_max);
                keys.push_back({.time = t, .value = v, .tangent_in = 0.0f, .tangent_out = 0.0f});
                selected_key_ = static_cast<int>(keys.size()) - 1;
                keys_dirty_   = true;
                modified      = true;
            }
        }

        void handle_delete_keyframe(std::vector<keyframe> &keys, bool &modified) {
            if (!selected_key_.has_value() || !std::cmp_less(*selected_key_, keys.size())
                || !ImGui::IsKeyPressed(ImGuiKey_Delete, false))
                return;

            keys.erase(keys.begin() + *selected_key_);
            selected_key_.reset();
            dragging_key_.reset();
            dragging_part_ = drag_part::none;
            keys_dirty_    = true;
            modified       = true;
        }
    };

} // namespace imgui_util
