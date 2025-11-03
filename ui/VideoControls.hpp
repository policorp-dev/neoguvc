#pragma once

#include <functional>
#include <vector>

#include "ControlsBase.hpp"

extern "C" {
#include "gviewv4l2core.h"
}

class MainWindow;
namespace Gtk {
class Grid;
}

class VideoControls : public ControlsBase {
public:
  explicit VideoControls(MainWindow &window);
  ~VideoControls() override = default;

private:
  struct DeviceEntry {
    std::string label;
    std::string device_path;
  };

  struct FormatEntry {
    Glib::ustring label;
    uint32_t fourcc = 0;
    int index = -1;
    bool supported = false;
  };

  struct ResolutionEntry {
    int width = 0;
    int height = 0;
    int index = -1;
  };

  struct FrameRateEntry {
    int numerator = 0;
    int denominator = 0;
    int index = -1;
  };

  struct FilterBinding {
    Gtk::CheckButton *button = nullptr;
    uint32_t mask = 0;
  };

  MainWindow &main_window_;
  v4l2_dev_t *device_ = nullptr;

  Gtk::ComboBoxText *device_combo_ = nullptr;
  Gtk::ComboBoxText *format_combo_ = nullptr;
  Gtk::ComboBoxText *resolution_combo_ = nullptr;
  Gtk::ComboBoxText *frame_rate_combo_ = nullptr;

  std::vector<DeviceEntry> devices_;
  std::vector<FormatEntry> formats_;
  std::vector<ResolutionEntry> resolutions_;
  std::vector<FrameRateEntry> frame_rates_;
  std::vector<FilterBinding> filter_bindings_;

  bool updating_ui_ = false;

  void initialise_ui();
  void populate_devices();
  void populate_formats();
  void populate_resolutions();
  void populate_frame_rates();
  void refresh_state();

  void on_device_changed();
  void on_format_changed();
  void on_resolution_changed();
  void on_frame_rate_changed();
  void on_filter_toggled(Gtk::CheckButton *button, uint32_t mask);
  void on_reset_clicked();

  void bind_filter_buttons(const std::vector<std::pair<uint32_t, Glib::ustring>> &filters,
                           Gtk::Grid &grid);
  void update_filter_buttons(uint32_t mask);

  void with_update_guard(const std::function<void()> &fn);
};
