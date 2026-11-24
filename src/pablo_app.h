// pablo - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <cstdlib>

#if defined(_WIN32)
#include <array>
#include <fstream>
#include <iterator>
#include <windows.h>
#include <commdlg.h>
#endif

#include <rex/cvar.h>
#include <rex/rex_app.h>

#include <rex/input/sdl/sdl_input_driver.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/windowed_app_context.h>

#include "input_trace.h"
#include "smoke_test_pad.h"

#if defined(_WIN32)
REXCVAR_DEFINE_BOOL(pablo_select_iso, false, "Runtime", "Choose a different Xbox 360 disc image");
#endif

class PabloApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<PabloApp>(new PabloApp(ctx, "pablo",
        PPCImageConfig));
  }

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}
  void OnPreSetup(rex::RuntimeConfig& config) override {
    // Two guest vblanks per rendered frame in the measured gameplay scene.
    constexpr std::string_view refresh_rate = "video_mode_refresh_rate";
    if (rex::cvar::GetFlagSource(refresh_rate) == rex::cvar::Source::kDefault &&
        !rex::cvar::SetFlagByName(refresh_rate, "120")) {
      REXLOG_WARN("Could not apply the default 120 Hz guest refresh rate");
    }


    const auto path_from_env = [](const char* name) {
      const char* value = std::getenv(name);
      return value ? std::filesystem::path(value) : std::filesystem::path();
    };
    const auto smoke = path_from_env("PABLO_SMOKE_TEST_INPUT");
    const auto record = path_from_env("PABLO_INPUT_RECORD");
    const auto replay = path_from_env("PABLO_INPUT_REPLAY");
    if (smoke.empty() && record.empty() && replay.empty()) {
      return;
    }
    if (!replay.empty() && (!record.empty() || !smoke.empty())) {
      throw std::runtime_error("Input replay cannot be combined with recording or smoke inputs");
    }

    config.input_factory = [smoke, record, replay](bool) {
      std::unique_ptr<rex::input::InputDriver> driver;
      if (!replay.empty()) {
        driver = std::make_unique<pablo::ReplayPad>(replay);
      } else {
        if (!smoke.empty()) {
          driver = std::make_unique<rex::SmokeTestPad>(smoke);
        } else {
          driver = std::make_unique<rex::input::sdl::SDLInputDriver>(nullptr, 0);
        }
        if (!record.empty()) {
          driver = std::make_unique<pablo::RecordingPad>(std::move(driver), record);
        }
      }
      if (driver->Setup() != rex::X_STATUS(0)) {
        throw std::runtime_error("Could not initialize the input test driver");
      }
      auto input = std::make_unique<rex::input::InputSystem>(nullptr);
      input->AddDriver(std::move(driver));
      input->SetDeviceAssignment(std::make_unique<rex::input::SlotAssignment>());
      return input;
    };
  }
  // void OnLoadXexImage(std::string& xex_image) override {}
#if defined(_WIN32)
  std::optional<rex::PathConfig> OnFinalizePaths(
      const rex::PathConfig& defaults, std::function<void(rex::PathConfig)>) override {
    if (!defaults.game_data_root.empty()) {
      return defaults;
    }
    auto paths = defaults;
    const auto selection_file = paths.user_data_root / "disc-image-path.txt";
    if (!REXCVAR_GET(pablo_select_iso)) {
      std::ifstream saved(selection_file, std::ios::binary);
      if (saved) {
        std::string utf8{std::istreambuf_iterator<char>(saved), {}};
        paths.game_data_root = std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
        std::error_code error;
        if (std::filesystem::is_regular_file(paths.game_data_root, error)) {
          return paths;
        }
      }
    }

    std::array<wchar_t, 32768> selected{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = static_cast<HWND>(window()->GetNativeWindowHandle());
    dialog.lpstrTitle = L"Pablo - Locate your ISO";
    dialog.lpstrFilter = L"Xbox 360 disc image (*.iso)\0*.iso\0";
    dialog.lpstrFile = selected.data();
    dialog.nMaxFile = static_cast<DWORD>(selected.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    if (!GetOpenFileNameW(&dialog)) {
      if (const auto error = CommDlgExtendedError()) {
        REXLOG_ERROR("Could not open the ISO picker: {}", error);
      }
      app_context().CallInUIThreadDeferred([this] { app_context().QuitFromUIThread(); });
      return std::nullopt;
    }
    paths.game_data_root = selected.data();
    selected_iso_ = paths.game_data_root;
    iso_selection_file_ = selection_file;
    return paths;
  }

  void OnPostLoadXexImage() override {
    if (selected_iso_.empty()) {
      return;
    }
    std::error_code error;
    std::filesystem::create_directories(iso_selection_file_.parent_path(), error);
    if (error) {
      REXLOG_WARN("Could not create the user data directory: {}", error.message());
      return;
    }
    std::ofstream saved(iso_selection_file_, std::ios::binary);
    const auto path = selected_iso_.u8string();
    saved.write(reinterpret_cast<const char*>(path.data()), path.size());
    if (!saved) {
      REXLOG_WARN("Could not remember the selected ISO path");
    }
  }
#endif
  // void OnPostSetup() override {}
  void OnCreateDialogs(rex::ui::ImGuiDrawer*) override {
    rex::ui::RegisterBind("bind_fullscreen", "F11", "Toggle fullscreen", [] {
      constexpr std::string_view flag = "fullscreen";
      rex::cvar::SetFlagByName(flag, rex::cvar::Query<bool>(flag) ? "false" : "true");
    });
  }
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  void OnShutdown() override {
    rex::ui::UnregisterBind("bind_fullscreen");
  }
#if defined(_WIN32)
 private:
  std::filesystem::path selected_iso_;
  std::filesystem::path iso_selection_file_;
#endif
};
