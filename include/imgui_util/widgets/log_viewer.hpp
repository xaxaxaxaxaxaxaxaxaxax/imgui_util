// log_viewer.hpp - scrolling log panel with level filtering, search, and clipboard export
//
// Usage:
//   static imgui_util::log_viewer log;
//   bool had_err = log.render([](auto cb) {
//       for (auto& msg : pending) cb(log_viewer::level::info, msg);
//   }, "##LogSearch", "LogEntries");
//
// Each instance owns its entry ring buffer and text storage.
// Call render() every frame (or drain() when panel is hidden) to consume entries.
#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <imgui.h>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "imgui_util/core/fmt_buf.hpp"
#include "imgui_util/core/raii.hpp"
#include "imgui_util/widgets/search_bar.hpp"
#include "imgui_util/widgets/text.hpp"

namespace imgui_util {

    // Forward-declare level enum for the drain_fn concept
    enum class log_level { info, warning, error };

    // Concept for the drain callback: invocable with a callback that accepts (log_level, string_view)
    template<typename F>
    concept drain_fn = std::invocable<F, std::move_only_function<void(log_level, std::string_view)>>;

    // Reusable log viewer widget. Each instance holds its own entry buffer, filter state, and
    // search buffer, so multiple panels can coexist independently.
    // Text is stored in a contiguous buffer with offset-based indexing for cache locality.
    // Entry metadata lives in a separate ring buffer.
    class log_viewer {
    public:
        using level = log_level; // severity for filtering and coloring

        // Construct with pre-allocated ring buffer at given capacity.
        explicit log_viewer(const std::size_t max_entries = 100'000) :
            max_entries_(max_entries), entries_(max_entries) {
            text_buf_.reserve(initial_text_capacity);
        }

        // Call each frame to drain pending entries and render the log panel.
        // drain: void(auto cb) where cb is void(level, string_view). Returns true if errors were drained.
        template<drain_fn DrainFn>
        [[nodiscard]]
        bool render(DrainFn &&drain, const char *search_id, const char *child_id) {
            const bool had_error = drain_entries(std::forward<DrainFn>(drain));

            render_toolbar(search_id);
            update_filter();

            // --- Scrolling child region ---
            if (const child child_scope{child_id, ImVec2(0, 0), ImGuiChildFlags_Borders}) {
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(filtered_.size()));
                while (clipper.Step()) {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                        render_row(row);
                    }
                }

                if (scroll_to_filtered_.has_value()) {
                    const float target_y =
                        static_cast<float>(*scroll_to_filtered_) * ImGui::GetTextLineHeightWithSpacing();
                    ImGui::SetScrollY(target_y);
                    scroll_to_filtered_.reset();
                } else if (auto_scroll_ && drained_this_frame_) {
                    ImGui::SetScrollHereY(1.0f);
                }
            }

            return had_error;
        }

        // Drain entries without rendering. Call when panel is hidden so source doesn't back up.
        template<drain_fn DrainFn>
        [[nodiscard]]
        bool drain(DrainFn &&drain_fn) {
            return drain_entries(std::forward<DrainFn>(drain_fn));
        }

        // Export all visible (filtered) entries as a newline-delimited string
        [[nodiscard]]
        std::string export_text() const {
            // Pre-calculate total size to avoid repeated reallocations
            std::size_t total_size = 0;
            for (const std::size_t idx: filtered_) {
                const auto &entry  = entry_at(idx);
                total_size        += level_prefix_len() + entry.text_length + 1; // +1 for '\n'
            }

            std::string result;
            result.reserve(total_size);
            for (const std::size_t idx: filtered_) {
                const auto       &entry  = entry_at(idx);
                const auto        text   = entry_text(entry);
                const char *const prefix = level_prefix(entry.lvl);
                result.append(prefix, level_prefix_len());
                result.append(text.data(), text.size());
                result += '\n';
            }
            return result;
        }

        // Thread-safe-ish push: append a log entry to the pending queue.
        // Entries are drained into the ring buffer at the start of the next render() call.
        // Safe for single-writer single-reader; does not use a mutex.
        void push(const level lvl, const std::string_view message) { pending_.push_back({lvl, std::string(message)}); }

        void set_show_timestamps(const bool v) noexcept { show_timestamps_ = v; }

        // Clear all entries and reset text buffer
        void clear() noexcept {
            count_            = 0;
            head_             = 0;
            text_head_        = 0;
            text_tail_        = 0;
            text_base_offset_ = 0;
            level_counts_.fill(0);
            last_filtered_count_ = 0;
            last_level_mask_     = 0xFF;
            last_query_.fill('\0');
            scroll_to_filtered_.reset();
            scroll_to_error_last_ = 0;
            filtered_.clear();
            text_buf_.clear();
        }

    private:
        struct pending_entry {
            level       lvl;
            std::string text;
        };

        struct log_entry {
            level                                 lvl{};
            std::uint32_t                         text_offset{}; // byte offset into text_buf_
            std::uint32_t                         text_length{}; // byte length (not null-terminated)
            std::chrono::steady_clock::time_point timestamp{};
        };

        static constexpr std::size_t initial_text_capacity = 1u << 20; // 1 MiB

        // Ring buffer access for entry metadata.
        [[nodiscard]]
        const log_entry &entry_at(const std::size_t logical_index) const noexcept {
            return entries_[(head_ + logical_index) % max_entries_];
        }

        // Resolve an entry's text as a string_view into the contiguous text buffer.
        [[nodiscard]]
        std::string_view entry_text(const log_entry &entry) const noexcept {
            return {text_buf_.data() + entry.text_offset - text_base_offset_, entry.text_length};
        }

        // Render the toolbar: level filter checkboxes, search bar, auto-scroll, clear/export/next-error buttons
        void render_toolbar(const char *search_id) {
            constexpr std::array level_indices = {
                static_cast<int>(level::info),
                static_cast<int>(level::warning),
                static_cast<int>(level::error),
            };

            const std::array     level_flags  = {&show_info_, &show_warn_, &show_error_};
            constexpr std::array level_labels = {"Info", "Warn", "Error"};

            for (int li = 0; li < 3; ++li) {
                if (li > 0) ImGui::SameLine();
                const fmt_buf<32> lbl("{} ({})", level_labels[li], level_counts_[level_indices[li]]);
                ImGui::Checkbox(lbl.c_str(), level_flags[li]);
            }
            ImGui::SameLine();
            (void) search_.render("Search...", 200.0f, search_id);
            ImGui::SameLine();
            ImGui::Checkbox("Auto-scroll", &auto_scroll_);
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                clear();
            }
            ImGui::SameLine();
            if (const bool has_filter = !search_.empty(); ImGui::Button(has_filter ? "Export Filtered" : "Export")) {
                ImGui::SetClipboardText(export_text().c_str());
            }
            ImGui::SameLine();
            if (ImGui::Button("Next Error")) {
                scroll_to_next_error();
            }
        }

        // Detect filter criteria changes and rebuild/extend the filtered index vector
        void update_filter() {
            const std::uint8_t current_level_mask =
                (show_info_ ? 1u : 0u) | (show_warn_ ? 2u : 0u) | (show_error_ ? 4u : 0u);
            const std::string_view current_query = search_.query();
            const bool             criteria_changed =
                current_level_mask != last_level_mask_ || current_query != std::string_view(last_query_.data());
            if (criteria_changed) {
                last_level_mask_ = current_level_mask;
                std::ranges::fill(last_query_, '\0');
                const auto n = std::min(current_query.size(), last_query_.size() - 1);
                std::ranges::copy_n(current_query.data(), static_cast<std::ptrdiff_t>(n), last_query_.data());
            }

            // Build filtered index vector (incremental when only new entries arrived)
            if (criteria_changed || count_ < last_filtered_count_) {
                // Full rebuild when filter criteria changed or ring buffer wrapped
                rebuild_filter(current_query);
            } else if (count_ > last_filtered_count_) {
                // Incremental: only scan new entries
                for (std::size_t i = last_filtered_count_; i < count_; ++i) {
                    if (passes_filter(i, current_query)) {
                        filtered_.push_back(i);
                    }
                }
                last_filtered_count_ = count_;
            }
        }

        // Render a single row in the clipped list
        void render_row(const int row) const {
            const auto       &entry  = entry_at(filtered_[static_cast<std::size_t>(row)]);
            const auto        text   = entry_text(entry);
            const char *const prefix = level_prefix(entry.lvl);

            const id                   id_scope{row};
            std::optional<style_color> color_scope;
            if (entry.lvl != level::info) {
                color_scope.emplace(ImGuiCol_Text, entry.lvl == level::warning ? colors::warning : colors::error);
            }
            if (show_timestamps_) {
                const auto secs = std::chrono::duration_cast<std::chrono::seconds>(entry.timestamp.time_since_epoch());
                const fmt_buf<24> ts_buf("[{:>6}s] ", secs.count());
                ImGui::TextUnformatted(ts_buf.c_str(), ts_buf.end());
                ImGui::SameLine(0.0f, 0.0f);
            }
            ImGui::TextUnformatted(prefix);
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextUnformatted(text.data(), text.data() + text.size());

            // Right-click context menu: copy line
            if (const popup_context_item ctx{"##log_ctx"}) {
                if (ImGui::Selectable("Copy line")) {
                    std::string full_line  = prefix;
                    full_line             += text;
                    ImGui::SetClipboardText(full_line.c_str());
                }
            }
        }

        // Find and scroll to the next error entry, wrapping around
        void scroll_to_next_error() {
            const std::size_t start = scroll_to_error_last_ + 1;
            for (std::size_t fi = start; fi < filtered_.size(); ++fi) {
                if (entry_at(filtered_[fi]).lvl == level::error) {
                    scroll_to_filtered_   = fi;
                    scroll_to_error_last_ = fi;
                    return;
                }
            }
            // Wrap around if not found
            for (std::size_t fi = 0; fi < start && fi < filtered_.size(); ++fi) {
                if (entry_at(filtered_[fi]).lvl == level::error) {
                    scroll_to_filtered_   = fi;
                    scroll_to_error_last_ = fi;
                    return;
                }
            }
        }

        // Full rebuild of filtered index vector
        void rebuild_filter(const std::string_view query) {
            filtered_.clear();
            for (std::size_t i = 0; i < count_; ++i) {
                if (passes_filter(i, query)) {
                    filtered_.push_back(i);
                }
            }
            last_filtered_count_ = count_;
        }

        // Check whether a logical entry index passes the current filter
        [[nodiscard]]
        bool passes_filter(const std::size_t logical_index, const std::string_view query) const noexcept {
            const auto &entry = entry_at(logical_index);
            switch (entry.lvl) {
                case level::info:
                    if (!show_info_) return false;
                    break;
                case level::warning:
                    if (!show_warn_) return false;
                    break;
                case level::error:
                    if (!show_error_) return false;
                    break;
            }
            return query.empty() || search::contains_ignore_case(entry_text(entry), query);
        }

        [[nodiscard]]
        static constexpr const char *level_prefix(const level lvl) noexcept {
            switch (lvl) {
                case level::warning:
                    return "[WARN] ";
                case level::error:
                    return "[ERR]  ";
                default:
                    return "[INFO] ";
            }
        }

        [[nodiscard]]
        static constexpr std::size_t level_prefix_len() noexcept {
            constexpr auto len = std::string_view("[INFO] ").size();
            static_assert(std::string_view("[WARN] ").size() == len);
            static_assert(std::string_view("[ERR]  ").size() == len);
            return len;
        }

        // Append message text to the contiguous buffer and return the offset.
        [[nodiscard]]
        std::uint32_t append_text(const std::string_view msg) {
            const auto offset = static_cast<std::uint32_t>(text_tail_);
            text_buf_.resize(text_tail_ - text_base_offset_ + msg.size());
            std::memcpy(text_buf_.data() + text_tail_ - text_base_offset_, msg.data(), msg.size());
            text_tail_ += msg.size();
            return offset;
        }

        // Compact the text buffer by removing dead text before text_head_.
        // O(1): memmove + update base offset, no per-entry adjustment needed.
        void compact_text_buffer() noexcept {
            if (text_head_ <= text_base_offset_) return;

            const std::size_t dead      = text_head_ - text_base_offset_;
            const std::size_t live_size = text_tail_ - text_head_;
            std::memmove(text_buf_.data(), text_buf_.data() + dead, live_size);

            text_base_offset_ = text_head_;
            text_buf_.resize(live_size);
        }

        // Simplified single-path ring buffer: always write at (head_ + count_) % capacity.
        // If full, advance head and reclaim dead text; otherwise increment count.
        static constexpr std::size_t max_message_length = 4096;

        // Write a single message into the ring buffer. Shared by drain_entries and push() drain path.
        bool append_entry(const level lvl, const std::string_view msg,
                          const std::chrono::steady_clock::time_point now) {
            const auto capped =
                msg.size() <= max_message_length ? msg : std::string_view{msg.data(), max_message_length - 3};
            const bool was_truncated = msg.size() > max_message_length;

            const std::size_t write_pos = (head_ + count_) % max_entries_;

            if (count_ == max_entries_) {
                const auto &evicted = entries_[head_];
                --level_counts_[static_cast<int>(evicted.lvl)];
                text_head_ = static_cast<std::size_t>(evicted.text_offset) + evicted.text_length;
                head_      = (head_ + 1) % max_entries_;
            } else {
                ++count_;
            }

            const auto text_offset = append_text(capped);
            if (was_truncated) {
                (void) append_text("...");
            }
            const auto text_len = was_truncated ? static_cast<std::uint32_t>(capped.size() + 3)
                                                : static_cast<std::uint32_t>(capped.size());
            entries_[write_pos] = {
                .lvl         = lvl,
                .text_offset = text_offset,
                .text_length = text_len,
                .timestamp   = now,
            };

            ++level_counts_[static_cast<int>(lvl)];
            return lvl == level::error;
        }

        template<typename DrainFn>
        [[nodiscard]]
        bool drain_entries(DrainFn &&drain_fn) {
            drained_this_frame_  = false;
            bool       had_error = false;
            const auto now       = std::chrono::steady_clock::now();

            // Drain entries queued via push() before the callback path
            if (!pending_.empty()) {
                for (auto &[lvl, text]: pending_) {
                    if (append_entry(lvl, text, now)) had_error = true;
                }
                pending_.clear();
                drained_this_frame_ = true;
            }

            std::forward<DrainFn>(drain_fn)([&](const level lvl, const std::string_view msg) {
                drained_this_frame_ = true;
                if (append_entry(lvl, msg, now)) had_error = true;
            });

            // Compact when dead space exceeds live space.
            if (text_head_ > text_base_offset_ && text_head_ - text_base_offset_ >= text_tail_ - text_head_) {
                compact_text_buffer();
            }

            return had_error;
        }

        // Ring buffer for entry metadata
        const std::size_t      max_entries_;
        std::vector<log_entry> entries_;   // pre-allocated ring of max_entries_
        std::size_t            head_  = 0; // oldest entry index in ring
        std::size_t            count_ = 0; // number of live entries

        // Contiguous text buffer -- entries reference byte ranges in here
        std::vector<char> text_buf_;
        std::size_t       text_head_        = 0; // start of live text (dead text before this)
        std::size_t       text_tail_        = 0; // write cursor (end of live text)
        std::size_t       text_base_offset_ = 0; // subtracted from entry offsets for O(1) compaction

        std::vector<std::size_t> filtered_; // indices of entries passing current filter
        search::search_bar<256>  search_;
        bool                     show_info_          = true;
        bool                     show_warn_          = true;
        bool                     show_error_         = true;
        bool                     auto_scroll_        = true;
        bool                     drained_this_frame_ = false;
        bool                     show_timestamps_    = false;

        // Per-level counts (for toolbar badge text), indexed by static_cast<int>(level)
        std::array<std::size_t, 3> level_counts_{};

        // Filter cache: rebuilt only when level mask or search query changes
        std::size_t           last_filtered_count_ = 0;
        std::array<char, 256> last_query_{};
        std::uint8_t          last_level_mask_ = 0xFF;

        // Scroll-to-error state
        std::optional<std::size_t> scroll_to_filtered_;       // filtered row to scroll to
        std::size_t                scroll_to_error_last_ = 0; // last error position for wrap-around

        // Pending queue for push() -- drained at start of drain_entries()
        std::deque<pending_entry> pending_;
    };

} // namespace imgui_util
