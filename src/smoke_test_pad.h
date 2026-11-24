#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <utility>

#include <rex/input/input_system.h>
#include <rex/logging.h>

namespace rex {

// Test-only pad. Each command contains an increasing sequence number and A, Left or Forward.
class SmokeTestPad final : public rex::input::InputDriver {
 public:
  explicit SmokeTestPad(std::filesystem::path command_file)
      : InputDriver(nullptr, 0), command_file_(std::move(command_file)) {}

  X_STATUS Setup() override { return X_STATUS_SUCCESS; }

  void EnumerateDevices(std::vector<rex::input::DeviceInfo>& out) override {
    out.push_back({.id = kId, .name = "Pablo smoke-test pad", .synthetic = true});
  }

  X_RESULT GetDeviceState(rex::input::DeviceId, rex::input::X_INPUT_STATE* out) override {
    using namespace std::chrono_literals;
    std::lock_guard lock(mutex_);
    const auto now = Clock::now();
    if (HasInput() && now >= release_at_) {
      buttons_ = 0;
      stick_y_ = 0;
      ++packet_;
      next_command_ = now + 150ms;
    }
    if (!HasInput() && now >= next_command_) {
      next_command_ = now + 30ms;
      PollCommand(now);
    }
    *out = {};
    out->packet_number = packet_;
    out->gamepad.buttons = buttons_;
    out->gamepad.thumb_ly = stick_y_;
    return X_ERROR_SUCCESS;
  }

  X_RESULT GetDeviceCapabilities(rex::input::DeviceId, uint32_t,
                                rex::input::X_INPUT_CAPABILITIES* out) override {
    *out = {};
    out->type = 1;
    out->sub_type = 1;
    out->gamepad.buttons = kA | kLeft;
    out->gamepad.thumb_ly = 32767;
    return X_ERROR_SUCCESS;
  }

  X_RESULT SetDeviceVibration(rex::input::DeviceId,
                              rex::input::X_INPUT_VIBRATION*) override {
    return X_ERROR_SUCCESS;
  }

  X_RESULT GetDeviceKeystroke(rex::input::DeviceId, uint32_t,
                              rex::input::X_INPUT_KEYSTROKE*) override {
    return X_ERROR_EMPTY;
  }

 private:
  using Clock = std::chrono::steady_clock;
  static constexpr auto kId = static_cast<rex::input::DeviceId>(1);
  static constexpr auto kA = rex::input::X_INPUT_GAMEPAD_A;
  static constexpr auto kLeft = rex::input::X_INPUT_GAMEPAD_DPAD_LEFT;

  bool HasInput() const { return buttons_ != 0 || stick_y_ != 0; }

  void PollCommand(Clock::time_point now) {
    std::ifstream input(command_file_);
    uint64_t sequence = 0;
    std::string command;
    if (!(input >> sequence >> command) || sequence <= sequence_) {
      return;
    }
    if (command == "A") {
      buttons_ = kA;
    } else if (command == "Left") {
      buttons_ = kLeft;
    } else if (command == "Forward") {
      stick_y_ = 32767;
    } else {
      return;
    }
    sequence_ = sequence;
    ++packet_;
    release_at_ = now + std::chrono::milliseconds(stick_y_ ? 1600 : 200);
    REXLOG_INFO("Smoke test input {}: {}", sequence, command);
  }

  const std::filesystem::path command_file_;
  std::mutex mutex_;
  Clock::time_point release_at_{};
  Clock::time_point next_command_{};
  uint64_t sequence_ = 0;
  uint32_t packet_ = 1;
  uint16_t buttons_ = 0;
  int16_t stick_y_ = 0;
};

}  // namespace rex
