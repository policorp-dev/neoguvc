#pragma once

#include <unordered_map>
#include <vector>

#include <sigc++/connection.h>

#include "ControlsBase.hpp"

extern "C" {
#include "gviewv4l2core.h"
}

class ImageControls : public ControlsBase {
public:
  explicit ImageControls(v4l2_dev_t *device);
  ~ImageControls() override = default;

private:
  struct SliderBinding {
    int control_id;
    Gtk::Scale *scale = nullptr;
    Gtk::Box *row = nullptr;
    Glib::RefPtr<Gtk::Adjustment> adjustment;
    sigc::connection handler;
  };

  struct CheckBinding {
    int control_id;
    Gtk::CheckButton *button = nullptr;
    Gtk::Box *row = nullptr;
    sigc::connection handler;
  };

  struct ComboBinding {
    int control_id;
    Gtk::ComboBoxText *combo = nullptr;
    std::vector<int> values;
    Gtk::Box *row = nullptr;
    sigc::connection handler;
  };

  v4l2_dev_t *device_;
  std::vector<SliderBinding> slider_bindings_;
  std::vector<CheckBinding> check_bindings_;
  std::vector<ComboBinding> combo_bindings_;

  v4l2_ctrl_t *refresh_control(int control_id) const;
  bool control_is_active(const v4l2_ctrl_t &control) const;

  void add_slider_control(
      int control_id, const Glib::ustring &label,
      const std::function<void(ControlsBase::SliderRowConfig &)>
          &configurator = std::function<void(
              ControlsBase::SliderRowConfig &)>());
  void add_check_control(int control_id, const Glib::ustring &label);
  void add_combo_control(
      int control_id, const Glib::ustring &label,
      const std::unordered_map<int, Glib::ustring> &label_overrides = {});

  void on_slider_value_changed(int control_id, Gtk::Scale *scale);
  void on_check_toggled(int control_id, Gtk::CheckButton *button);
  void on_combo_changed(int control_id, Gtk::ComboBoxText *combo);
  void on_reset_clicked();

  ComboBinding *find_combo_binding(int control_id, Gtk::ComboBoxText *combo);

  void refresh_controls_state();
  bool should_force_reset(int control_id) const;
};
