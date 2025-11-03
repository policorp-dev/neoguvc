#pragma once

#include <functional>
#include <vector>

#include "ControlsBase.hpp"

extern "C" {
#include "audio.h"
}

class MainWindow;

class AudioControls : public ControlsBase {
public:
  explicit AudioControls(MainWindow &window);
  ~AudioControls() override = default;

private:
  MainWindow &main_window_;
  audio_context_t *audio_ctx_ = nullptr;

  Gtk::ComboBoxText *api_combo_ = nullptr;
  Gtk::ComboBoxText *device_combo_ = nullptr;
  Gtk::ComboBoxText *samplerate_combo_ = nullptr;
  Gtk::ComboBoxText *channels_combo_ = nullptr;
  Gtk::Scale *latency_scale_ = nullptr;

  std::vector<int> samplerate_values_;
  std::vector<int> channel_values_;
  std::vector<std::pair<Gtk::CheckButton *, uint32_t>> audio_filter_buttons_;

  bool updating_ui_ = false;

  void initialise_ui();
  void populate_api();
  void populate_devices();
  void populate_samplerates();
  void populate_channels();
  void update_latency();
  void update_filters();
  void refresh_state();

  void on_api_changed();
  void on_device_changed();
  void on_samplerate_changed();
  void on_channels_changed();
  void on_latency_changed();
  void on_filter_toggled(Gtk::CheckButton *button, uint32_t mask);
  void on_reset_clicked();

  void with_update_guard(const std::function<void()> &fn);
};
