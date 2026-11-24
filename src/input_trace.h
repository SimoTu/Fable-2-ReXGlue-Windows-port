#pragma once

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <rex/input/input_driver.h>
#include <rex/logging.h>

namespace pablo {
namespace ri = rex::input;
using rex::X_RESULT;
using rex::X_STATUS;

inline constexpr char kTraceHeader[] = "pablo-input-v1";
using InputValues = std::array<int, 7>;

inline InputValues ToValues(const ri::X_INPUT_GAMEPAD& pad) {
  return {pad.buttons, pad.left_trigger, pad.right_trigger, pad.thumb_lx,
          pad.thumb_ly, pad.thumb_rx, pad.thumb_ry};
}

class RecordingPad final : public ri::InputDriver {
 public:
  RecordingPad(std::unique_ptr<ri::InputDriver> source, const std::filesystem::path& path)
      : InputDriver(nullptr, 0), source_(std::move(source)),
        trace_(path, std::ios::out | std::ios::noreplace) {
    if (!trace_) {
      throw std::runtime_error("Cannot create new input trace: " + path.string());
    }
    trace_ << kTraceHeader << std::endl;
    source_->set_is_active_callback([this] { return is_active(); });
  }

  X_STATUS Setup() override { return source_->Setup(); }

  void OnWindowAvailable(rex::ui::Window* window) override {
    source_->OnWindowAvailable(window);
  }

  void EnumerateDevices(std::vector<ri::DeviceInfo>& out) override {
    std::vector<ri::DeviceInfo> devices;
    source_->EnumerateDevices(devices);
    if (!devices.empty()) {
      out.push_back(std::move(devices.front()));
    }
  }

  X_RESULT GetDeviceState(ri::DeviceId id, ri::X_INPUT_STATE* out) override {
    auto result = source_->GetDeviceState(id, out);
    if (result != X_ERROR_SUCCESS) {
      return result;
    }

    std::lock_guard lock(mutex_);
    const auto now = Clock::now();
    if (started_ == Clock::time_point{}) {
      started_ = now;
    }
    const auto values = ToValues(out->gamepad);
    if (!has_sample_ || values != last_) {
      const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - started_);
      trace_ << elapsed.count();
      for (auto value : values) {
        trace_ << ' ' << value;
      }
      trace_ << std::endl;
      if (!trace_) {
        REXLOG_ERROR("Input trace write failed");
      }
      last_ = values;
      has_sample_ = true;
    }
    return result;
  }

  X_RESULT GetDeviceCapabilities(ri::DeviceId id, uint32_t flags,
                                ri::X_INPUT_CAPABILITIES* out) override {
    return source_->GetDeviceCapabilities(id, flags, out);
  }

  X_RESULT SetDeviceVibration(ri::DeviceId id, ri::X_INPUT_VIBRATION* vibration) override {
    return source_->SetDeviceVibration(id, vibration);
  }

  X_RESULT GetDeviceKeystroke(ri::DeviceId id, uint32_t flags,
                             ri::X_INPUT_KEYSTROKE* out) override {
    return source_->GetDeviceKeystroke(id, flags, out);
  }

 private:
  using Clock = std::chrono::steady_clock;
  std::unique_ptr<ri::InputDriver> source_;
  std::ofstream trace_;
  std::mutex mutex_;
  Clock::time_point started_{};
  InputValues last_{};
  bool has_sample_ = false;
};

class ReplayPad final : public ri::InputDriver {
 public:
  explicit ReplayPad(const std::filesystem::path& path) : InputDriver(nullptr, 0) {
    std::ifstream trace(path);
    std::string line;
    if (!std::getline(trace, line) || line != kTraceHeader) {
      throw std::runtime_error("Invalid input trace header: " + path.string());
    }

    while (std::getline(trace, line)) {
      std::istringstream row(line);
      int64_t milliseconds = 0;
      InputValues values{};
      std::string extra;
      if (!(row >> milliseconds >> values[0] >> values[1] >> values[2] >>
            values[3] >> values[4] >> values[5] >> values[6]) ||
          (row >> extra) || milliseconds < 0 ||
          (!samples_.empty() && milliseconds < samples_.back().time.count()) ||
          values[0] < 0 || values[0] > 65535 ||
          values[1] < 0 || values[1] > 255 || values[2] < 0 || values[2] > 255) {
        throw std::runtime_error("Invalid input trace row: " + path.string());
      }
      for (size_t i = 3; i < values.size(); ++i) {
        if (values[i] < -32768 || values[i] > 32767) {
          throw std::runtime_error("Invalid input trace axis: " + path.string());
        }
      }
      samples_.push_back({std::chrono::milliseconds(milliseconds), values});
    }
    if (trace.bad() || samples_.empty()) {
      throw std::runtime_error("Incomplete or empty input trace: " + path.string());
    }
  }

  X_STATUS Setup() override { return X_STATUS_SUCCESS; }

  void EnumerateDevices(std::vector<ri::DeviceInfo>& out) override {
    out.push_back({.id = static_cast<ri::DeviceId>(1), .name = "Pablo replay pad",
                   .synthetic = true});
  }

  X_RESULT GetDeviceState(ri::DeviceId, ri::X_INPUT_STATE* out) override {
    std::lock_guard lock(mutex_);
    const auto now = Clock::now();
    if (started_ == Clock::time_point{}) {
      started_ = now;
    }
    const auto elapsed = now - started_;
    while (next_ < samples_.size() && samples_[next_].time <= elapsed) {
      const auto& sample = samples_[next_++];
      values_ = sample.values;
      REXLOG_INFO("Replay sample {} at {} ms: buttons={}, left Y={}", next_,
                  sample.time.count(), values_[0], values_[4]);
    }
    // A trace cut short by a crash may end with a held button.
    if (!released_ && next_ == samples_.size() &&
        elapsed >= samples_.back().time + std::chrono::milliseconds(200)) {
      values_ = {};
      released_ = true;
      REXLOG_INFO("Input replay complete");
    }
    *out = {};
    out->packet_number = static_cast<uint32_t>(next_) + 1 + released_;
    out->gamepad.buttons = static_cast<uint16_t>(values_[0]);
    out->gamepad.left_trigger = static_cast<uint8_t>(values_[1]);
    out->gamepad.right_trigger = static_cast<uint8_t>(values_[2]);
    out->gamepad.thumb_lx = static_cast<int16_t>(values_[3]);
    out->gamepad.thumb_ly = static_cast<int16_t>(values_[4]);
    out->gamepad.thumb_rx = static_cast<int16_t>(values_[5]);
    out->gamepad.thumb_ry = static_cast<int16_t>(values_[6]);
    return X_ERROR_SUCCESS;
  }

  X_RESULT GetDeviceCapabilities(ri::DeviceId, uint32_t,
                                ri::X_INPUT_CAPABILITIES* out) override {
    *out = {};
    out->type = ri::XINPUT_DEVTYPE_GAMEPAD;
    out->sub_type = 1;
    out->gamepad.buttons = 0xF3FF;
    out->gamepad.left_trigger = 255;
    out->gamepad.right_trigger = 255;
    out->gamepad.thumb_lx = -1;
    out->gamepad.thumb_ly = -1;
    out->gamepad.thumb_rx = -1;
    out->gamepad.thumb_ry = -1;
    return X_ERROR_SUCCESS;
  }

  X_RESULT SetDeviceVibration(ri::DeviceId, ri::X_INPUT_VIBRATION*) override {
    return X_ERROR_SUCCESS;
  }

  X_RESULT GetDeviceKeystroke(ri::DeviceId, uint32_t, ri::X_INPUT_KEYSTROKE*) override {
    return X_ERROR_EMPTY;
  }

 private:
  using Clock = std::chrono::steady_clock;
  struct Sample {
    std::chrono::milliseconds time;
    InputValues values;
  };
  std::vector<Sample> samples_;
  std::mutex mutex_;
  Clock::time_point started_{};
  size_t next_ = 0;
  bool released_ = false;
  InputValues values_{};
};

}  // namespace pablo
