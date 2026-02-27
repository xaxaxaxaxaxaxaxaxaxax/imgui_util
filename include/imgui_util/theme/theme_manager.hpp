// theme_manager.hpp - theme persistence, preset registry, and built-in editor window
//
// Usage:
//   theme_manager mgr;
//   mgr.set_theme(theme_config::from_preset(my_preset));
//   mgr.save_to_file("theme.json");
//   mgr.load_from_file("theme.json");
//   mgr.render_theme_editor(&show_editor);      // call each frame to show the editor

#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "imgui_util/theme/theme.hpp"

namespace imgui_util::theme {

    // Generate a valid C++ theme_preset{...} initializer string from a theme_config.
    [[nodiscard]] std::string generate_preset_code(const theme_config &cfg);

    // Validate a theme_config, returning a list of human-readable error strings.
    // Checks for: alpha values out of [0,1], fully transparent window background,
    // nonsensical rounding values, etc.
    [[nodiscard]] std::vector<std::string> validate(const theme_config &cfg);

    class theme_manager {
    public:
        static constexpr int file_version = 1; // bumped on breaking serialization changes

        theme_manager();

        // Theme management
        void                              set_theme(theme_config theme); // apply and store as current
        [[nodiscard]] theme_config       &get_current_theme() { return current_theme_; }
        [[nodiscard]] const theme_config &get_current_theme() const { return current_theme_; }

        // Preset access
        [[nodiscard]] static std::span<const theme_preset> get_presets();    // all built-in presets
        [[nodiscard]] static theme_config get_preset(std::string_view name); // build theme_config from named preset
        [[nodiscard]] static const theme_preset *find_preset(std::string_view name); // nullptr if not found

        // Persistence (JSON)
        [[nodiscard]] bool save_to_file(const std::filesystem::path &path);
        [[nodiscard]] bool load_from_file(const std::filesystem::path &path);

        // Editor UI -- call each frame when open
        void render_theme_editor(bool *open);

    private:
        theme_config    current_theme_;
        theme_config    editing_theme_; // scratch copy while editor is open
        bool            live_preview_ = true;
        ImGuiTextFilter color_filter_;

        void        render_color_category(const char *name, std::span<const int> indices);
        void        render_node_colors();
        static void render_sizes_tab();
        static void render_fonts_tab();
        static void render_rendering_tab();
    };

} // namespace imgui_util::theme
