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

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <imgui.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "imgui_util/core/fmt_buf.hpp"
#include "imgui_util/core/raii.hpp"
#include "imgui_util/widgets/search_bar.hpp"
#include "imgui_util/widgets/text.hpp"

namespace imgui_util {

    // Reusable log viewer widget. Each instance holds its own entry buffer, filter state, and
    // search buffer, so multiple panels can coexist independently.
    // Text is stored in a contiguous buffer with offset-based indexing for cache locality.
    // Entry metadata lives in a separate ring buffer.
    class log_viewer {
    public:
        enum class level { info, warning, error }; // severity for filtering and coloring

        // Construct with pre-allocated ring buffer at given capacity.
        explicit log_viewer(const std::size_t max_entries = 100'000) :
            max_entries_(max_entries), entries_(max_entries) {
            text_buf_.reserve(initial_text_capacity);
        }

        // Call each frame to drain pending entries and render the log panel.
        // drain: void(auto cb) where cb is void(level, string_view). Returns true if errors were drained.
        template<typename DrainFn>
        bool render(DrainFn &&drain, const char *search_id, const char *child_id) {
            const bool had_error = drain_entries(std::forward<DrainFn>(drain));

            // --- Toolbar ---
            {
                const fmt_buf<32> info_label("Info ({})", info_count_);
                ImGui::Checkbox(info_label.c_str(), &show_info_);
            }
            ImGui::SameLine();
            {
                const fmt_buf<32> warn_label("Warn ({})", warn_count_);
                ImGui::Checkbox(warn_label.c_str(), &show_warn_);
            }
            ImGui::SameLine();
            {
                const fmt_buf<32> err_label("Error ({})", error_count_);
                ImGui::Checkbox(err_label.c_str(), &show_error_);
            }
            ImGui::SameLine();
            search_.render("Search...", 200.0f, search_id);
            ImGui::SameLine();
            ImGui::Checkbox("Auto-scroll", &auto_scroll_);
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                clear();
            }
            ImGui::SameLine();
            const bool has_filter = !search_.empty();
            if (ImGui::Button(has_filter ? "Export Filtered" : "Export")) {
                ImGui::SetClipboardText(export_text().c_str());
            }
            ImGui::SameLine();
            if (ImGui::Button("Next Error")) {
                // Find next error in filtered list after current position
                const std::size_t start = scroll_to_error_last_ + 1;
                bool found = false;
                for (std::size_t fi = start; fi < filtered_.size(); ++fi) {
                    if (entry_at(filtered_[fi]).lvl == level::error) {
                        scroll_to_filtered_ = static_cast<int>(fi);
                        scroll_to_error_last_ = fi;
                        found = true;
                        break;
                    }
                }
                // Wrap around if not found
                if (!found) {
                    for (std::size_t fi = 0; fi < start && fi < filtered_.size(); ++fi) {
                        if (entry_at(filtered_[fi]).lvl == level::error) {
                            scroll_to_filtered_ = static_cast<int>(fi);
                            scroll_to_error_last_ = fi;
                            break;
                        }
                    }
                }
            }

            // --- Detect filter changes ---
            const std::uint8_t current_level_mask =
                (show_info_ ? 1u : 0u) | (show_warn_ ? 2u : 0u) | (show_error_ ? 4u : 0u);
            const auto current_query = search_.query();
            const bool criteria_changed =
                current_level_mask != last_level_mask_ ||
                current_query != std::string_view{last_query_.data(), last_query_len_};
            if (criteria_changed) {
                last_level_mask_ = current_level_mask;
                last_query_len_  = static_cast<std::uint8_t>(
                    current_query.size() < last_query_.size() ? current_query.size() : last_query_.size());
                std::memcpy(last_query_.data(), current_query.data(), last_query_len_);
            }

            // --- Build filtered index vector (incremental when only new entries arrived) ---
            if (criteria_changed) {
                // Full rebuild when filter criteria changed
                filtered_.clear();
                for (std::size_t i = 0; i < count_; ++i) {
                    if (passes_filter(i, current_query)) {
                        filtered_.push_back(i);
                    }
                }
                last_filtered_count_ = count_;
            } else if (count_ > last_filtered_count_) {
                // Incremental: only scan new entries
                for (std::size_t i = last_filtered_count_; i < count_; ++i) {
                    if (passes_filter(i, current_query)) {
                        filtered_.push_back(i);
                    }
                }
                last_filtered_count_ = count_;
            } else if (count_ < last_filtered_count_) {
                // Ring buffer wrapped (evictions happened while at capacity) -- full rebuild
                filtered_.clear();
                for (std::size_t i = 0; i < count_; ++i) {
                    if (passes_filter(i, current_query)) {
                        filtered_.push_back(i);
                    }
                }
                last_filtered_count_ = count_;
            }

            // --- Scrolling child region ---
            if (const imgui_util::child child_scope{child_id, ImVec2(0, 0), ImGuiChildFlags_Borders}) {
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(filtered_.size()));
                while (clipper.Step()) {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                        const auto       &entry  = entry_at(filtered_[static_cast<std::size_t>(row)]);
                        const auto        text   = entry_text(entry);
                        const char *const prefix = level_prefix(entry.lvl);

                        const imgui_util::id id_scope{row};
                        const bool colored = entry.lvl != level::info;
                        std::optional<imgui_util::style_color> color_scope;
                        if (colored) {
                            color_scope.emplace(ImGuiCol_Text,
                                                entry.lvl == level::warning ? colors::warning : colors::error);
                        }
                        if (show_timestamps_) {
                            const auto secs =
                                std::chrono::duration_cast<std::chrono::seconds>(entry.timestamp.time_since_epoch());
                            const fmt_buf<24> ts_buf("[{:>6}s] ", secs.count());
                            ImGui::TextUnformatted(ts_buf.c_str(), ts_buf.end());
                            ImGui::SameLine(0.0f, 0.0f);
                        }
                        ImGui::TextUnformatted(prefix);
                        ImGui::SameLine(0.0f, 0.0f);
                        ImGui::TextUnformatted(text.data(), text.data() + text.size());

                        // Right-click context menu: copy line
                        if (const imgui_util::popup_context_item ctx{"##log_ctx"}) {
                            if (ImGui::Selectable("Copy line")) {
                                std::string full_line  = prefix;
                                full_line             += text;
                                ImGui::SetClipboardText(full_line.c_str());
                            }
                        }
                    }
                }

                if (scroll_to_filtered_ >= 0) {
                    const float target_y = static_cast<float>(scroll_to_filtered_) *
                                           ImGui::GetTextLineHeightWithSpacing();
                    ImGui::SetScrollY(target_y);
                    scroll_to_filtered_ = -1;
                } else if (auto_scroll_ && drained_this_frame_) {
                    ImGui::SetScrollHereY(1.0f);
                }
            }

            return had_error;
        }

        // Drain entries without rendering. Call when panel is hidden so source doesn't back up.
        template<typename DrainFn>
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
                const auto       &entry      = entry_at(idx);
                const auto        text       = entry_text(entry);
                const char *const prefix     = level_prefix(entry.lvl);
                const std::size_t prefix_len = level_prefix_len();
                result.append(prefix, prefix_len);
                result.append(text.data(), text.size());
                result += '\n';
            }
            return result;
        }

        void set_show_timestamps(const bool v) noexcept { show_timestamps_ = v; }

        // Clear all entries and reset text buffer
        void clear() {
            count_               = 0;
            head_                = 0;
            text_head_           = 0;
            text_tail_           = 0;
            text_base_offset_    = 0;
            info_count_          = 0;
            warn_count_          = 0;
            error_count_         = 0;
            last_filtered_count_ = 0;
            last_level_mask_     = 0xFF;
            last_query_len_      = 0;
            scroll_to_filtered_  = -1;
            scroll_to_error_last_ = 0;
            filtered_.clear();
            text_buf_.clear();
        }

    private:
        struct log_entry {
            level                                 lvl{};
            std::uint32_t                         text_offset{}; // byte offset into text_buf_
            std::uint32_t                         text_length{}; // byte length (not null-terminated)
            std::chrono::steady_clock::time_point timestamp{};
        };

        static constexpr std::size_t initial_text_capacity = 1u << 20; // 1 MiB

        // Ring buffer access for entry metadata.
        [[nodiscard]]
        const log_entry &entry_at(const std::size_t logical_index) const {
            return entries_[(head_ + logical_index) %
                            max_entries_]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        }

        // Resolve an entry's text as a string_view into the contiguous text buffer.
        [[nodiscard]]
        std::string_view entry_text(const log_entry &entry) const {
            return {text_buf_.data() + entry.text_offset - text_base_offset_, entry.text_length};
        }

        // Check whether a logical entry index passes the current filter
        [[nodiscard]]
        bool passes_filter(const std::size_t logical_index, const std::string_view query) const {
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
            if (!query.empty() && !search::contains_ignore_case(entry_text(entry), query)) {
                return false;
            }
            return true;
        }

        [[nodiscard]]
        static constexpr const char *level_prefix(const level lvl) {
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
        static constexpr std::size_t level_prefix_len() {
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
        void compact_text_buffer() {
            if (text_head_ <= text_base_offset_) return;

            const std::size_t dead = text_head_ - text_base_offset_;
            const std::size_t live_size = text_tail_ - text_head_;
            std::memmove(text_buf_.data(), text_buf_.data() + dead, live_size);

            text_base_offset_ = text_head_;
            text_buf_.resize(live_size);
        }

        // Get mutable reference to the count for a given level
        std::size_t &count_for_level(const level lvl) {
            switch (lvl) {
                case level::info:
                    return info_count_;
                case level::warning:
                    return warn_count_;
                case level::error:
                    return error_count_;
            }
            std::unreachable();
        }

        void decrement_level_count(const level lvl) { --count_for_level(lvl); }
        void increment_level_count(const level lvl) { ++count_for_level(lvl); }

        // Simplified single-path ring buffer: always write at (head_ + count_) % capacity.
        // If full, advance head and reclaim dead text; otherwise increment count.
        static constexpr std::size_t max_message_length = 4096;

        template<typename DrainFn>
        [[nodiscard]]
        bool drain_entries(DrainFn &&drain_fn) {
            drained_this_frame_  = false;
            bool       had_error = false;
            const auto now       = std::chrono::steady_clock::now();
            std::forward<DrainFn>(drain_fn)([&](const level lvl, const std::string_view msg) {
                drained_this_frame_ = true;

                // Cap individual messages
                const auto capped =
                    (msg.size() <= max_message_length)
                        ? msg
                        : std::string_view{msg.data(),
                                           max_message_length -
                                               3}; // NOLINT(bugprone-suspicious-stringview-data-usage) -- size-bounded
                const bool was_truncated = msg.size() > max_message_length;

                const std::size_t write_pos = (head_ + count_) % max_entries_;

                // If the ring is full, the oldest entry is being evicted -- advance text_head_.
                if (count_ == max_entries_) {
                    const auto &evicted = entries_[head_]; // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                    decrement_level_count(evicted.lvl);
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
                    // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
                    .lvl         = lvl,
                    .text_offset = text_offset,
                    .text_length = text_len,
                    .timestamp   = now,
                };

                increment_level_count(lvl);

                if (lvl == level::error) {
                    had_error = true;
                }
            });

            // Compact when dead space exceeds live space.
            if (text_head_ > text_base_offset_ &&
                (text_head_ - text_base_offset_) >= (text_tail_ - text_head_)) {
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

        // Per-level counts (for toolbar badge text)
        std::size_t info_count_  = 0;
        std::size_t warn_count_  = 0;
        std::size_t error_count_ = 0;

        // Filter cache: rebuilt only when level mask or search query changes
        std::size_t              last_filtered_count_ = 0;
        std::array<char, 256>    last_query_{};
        std::uint8_t             last_query_len_  = 0;
        std::uint8_t             last_level_mask_ = 0xFF;

        // Scroll-to-error state
        int                      scroll_to_filtered_   = -1; // filtered row to scroll to, or -1
        std::size_t              scroll_to_error_last_  = 0;  // last error position for wrap-around
    };

} // namespace imgui_util
