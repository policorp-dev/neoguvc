#include "ImageControls.hpp"

#include <algorithm>
#include <iostream>

#include <sigc++/functors/mem_fun.h>

namespace {
ControlsBase::ConstructionOptions make_window_options() {
  ControlsBase::ConstructionOptions options;
  options.title = "Controles de imagem";
  options.header_title = options.title;
  options.width = 520;
  options.height = 620;
  options.show_reset_button = true;
  options.reset_button_label = "Restaurar padrão";
  return options;
}

double resolve_step(const v4l2_ctrl_t &control) {
  return control.control.step > 0 ? static_cast<double>(control.control.step)
                                  : 1.0;
}
} // namespace

ImageControls::ImageControls(v4l2_dev_t *device)
    : ControlsBase(make_window_options()), device_(device) {
  if (!device_) {
    if (has_reset_button())
      reset_button().set_sensitive(false);
    auto message =
        Gtk::manage(new Gtk::Label("Nenhum dispositivo de vídeo ativo."));
    message->set_halign(Gtk::ALIGN_START);
    message->get_style_context()->add_class("controls-label");
    body_container().pack_start(*message, Gtk::PACK_SHRINK, 0);
    show_all_children();
    return;
  }

  if (has_reset_button()) {
    reset_button().signal_clicked().connect(
        sigc::mem_fun(*this, &ImageControls::on_reset_clicked));
  }

  add_slider_control(V4L2_CID_BRIGHTNESS, "Brilho:");
  add_slider_control(V4L2_CID_CONTRAST, "Contraste:");
  add_slider_control(V4L2_CID_SATURATION, "Saturação:");
  add_slider_control(V4L2_CID_HUE, "Matiz:");
  add_slider_control(
      V4L2_CID_GAMMA, "Gama:",
      [](ControlsBase::SliderRowConfig &config) { config.digits = 0; });
  add_slider_control(V4L2_CID_SHARPNESS, "Nitidez:");
  add_slider_control(V4L2_CID_BACKLIGHT_COMPENSATION,
                     "Compensação de Luz:");

  add_check_control(V4L2_CID_AUTO_WHITE_BALANCE,
                    "Balanço de brancos automático");
  add_slider_control(V4L2_CID_WHITE_BALANCE_TEMPERATURE,
                     "Balanço de branco:");

  add_combo_control(
      V4L2_CID_EXPOSURE_AUTO, "Exposição automática:",
      {
          {V4L2_EXPOSURE_AUTO, "Automático"},
          {V4L2_EXPOSURE_MANUAL, "Manual"},
          {V4L2_EXPOSURE_APERTURE_PRIORITY, "Modo prioridade da abertura"},
          {V4L2_EXPOSURE_SHUTTER_PRIORITY,
           "Modo prioridade do obturador"},
      });

  add_slider_control(V4L2_CID_EXPOSURE_ABSOLUTE, "Tempo de exposição:",
                     [](ControlsBase::SliderRowConfig &config) {
                       config.digits = 0;
                     });

  add_combo_control(
      V4L2_CID_POWER_LINE_FREQUENCY, "Frequência:",
      {
          {V4L2_CID_POWER_LINE_FREQUENCY_DISABLED, "Desligado"},
          {V4L2_CID_POWER_LINE_FREQUENCY_50HZ, "50 Hz"},
          {V4L2_CID_POWER_LINE_FREQUENCY_60HZ, "60 Hz"},
          {V4L2_CID_POWER_LINE_FREQUENCY_AUTO, "Automático"},
      });

  add_check_control(V4L2_CID_EXPOSURE_AUTO_PRIORITY,
                    "Prioridade de exposição automática");

  refresh_controls_state();
  show_all_children();
}

v4l2_ctrl_t *ImageControls::refresh_control(int control_id) const {
  if (!device_)
    return nullptr;

  auto *control = v4l2core_get_control_by_id(device_, control_id);
  if (!control)
    return nullptr;

  v4l2core_get_control_value_by_id(device_, control_id);
  return control;
}

bool ImageControls::control_is_active(const v4l2_ctrl_t &control) const {
  const uint32_t disabled_flags =
      V4L2_CTRL_FLAG_DISABLED | V4L2_CTRL_FLAG_GRABBED |
      V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_INACTIVE;
  return (control.control.flags & disabled_flags) == 0;
}

void ImageControls::add_slider_control(
    int control_id, const Glib::ustring &label,
    const std::function<void(ControlsBase::SliderRowConfig &)> &configurator) {
  auto *control = refresh_control(control_id);
  if (!control)
    return;

  ControlsBase::SliderRowConfig config;
  config.step = resolve_step(*control);
  config.page_increment = config.step * 5.0;
  config.page_size = 0.0;
  config.digits = 0;
  config.sensitive = control_is_active(*control);
  if (configurator)
    configurator(config);

  config.on_configure = [this, control_id](Gtk::Scale &scale,
                                           const Glib::RefPtr<Gtk::Adjustment>
                                               &adjustment) {
    SliderBinding binding;
    binding.control_id = control_id;
    binding.scale = &scale;
    binding.row = dynamic_cast<Gtk::Box *>(scale.get_parent());
    binding.adjustment = adjustment;
    binding.handler = scale.signal_value_changed().connect(
        sigc::bind(sigc::mem_fun(*this, &ImageControls::on_slider_value_changed),
                   control_id, &scale));
    slider_bindings_.push_back(std::move(binding));
  };

  auto row = create_slider_row(label, control->value,
                               control->control.minimum,
                               control->control.maximum, config);
  add_row(*row);
}

void ImageControls::add_check_control(int control_id,
                                      const Glib::ustring &label) {
  auto *control = refresh_control(control_id);
  if (!control)
    return;

  ControlsBase::CheckRowConfig config;
  config.hexpand = false;
  config.on_configure = [this, control_id](Gtk::CheckButton &button) {
    CheckBinding binding;
    binding.control_id = control_id;
    binding.button = &button;
    binding.row = dynamic_cast<Gtk::Box *>(button.get_parent());
    binding.handler = button.signal_toggled().connect(
        sigc::bind(sigc::mem_fun(*this, &ImageControls::on_check_toggled),
                   control_id, &button));
    check_bindings_.push_back(std::move(binding));
  };

  auto row =
      create_check_row(label, control->value != 0, config);
  add_row(*row);
}

void ImageControls::add_combo_control(
    int control_id, const Glib::ustring &label,
    const std::unordered_map<int, Glib::ustring> &label_overrides) {
  auto *control = refresh_control(control_id);
  if (!control || !control->menu)
    return;

  std::vector<Glib::ustring> options;
  std::vector<int> values;
  int active_index = 0;
  for (int j = 0; control->menu[j].index <= control->control.maximum; ++j) {
    const int value = static_cast<int>(control->menu[j].index);
    values.push_back(value);

    Glib::ustring option_label;
    auto override_it = label_overrides.find(value);
    if (override_it != label_overrides.end()) {
      option_label = override_it->second;
    } else if (control->menu_entry && j < control->menu_entries &&
               control->menu_entry[j]) {
      option_label = control->menu_entry[j];
    } else {
      option_label =
          Glib::ustring(reinterpret_cast<const char *>(control->menu[j].name));
    }

    if (option_label.empty())
      option_label = Glib::ustring::compose("%1", value);

    options.push_back(option_label);
    if (value == control->value)
      active_index = static_cast<int>(options.size()) - 1;
  }

  if (options.empty())
    return;

  ControlsBase::ComboRowConfig config;
  config.active_index = active_index;
  config.on_configure = [this, control_id,
                         values](Gtk::ComboBoxText &combo) mutable {
    ComboBinding binding;
    binding.control_id = control_id;
    binding.combo = &combo;
    binding.values = values;
    binding.row = dynamic_cast<Gtk::Box *>(combo.get_parent());
    binding.handler = combo.signal_changed().connect(
        sigc::bind(sigc::mem_fun(*this, &ImageControls::on_combo_changed),
                   control_id, &combo));
    combo_bindings_.push_back(std::move(binding));
  };

  auto row = create_combo_row(label, options, config);
  add_row(*row);
}

void ImageControls::on_slider_value_changed(int control_id, Gtk::Scale *scale) {
  if (!device_ || !scale)
    return;

  auto *control = v4l2core_get_control_by_id(device_, control_id);
  if (!control)
    return;

  const int new_value = static_cast<int>(scale->get_value());
  if (control->value == new_value)
    return;

  control->value = new_value;
  if (v4l2core_set_control_value_by_id(device_, control_id) != 0) {
    std::cerr << "Falha ao atualizar controle 0x" << std::hex << control_id
              << std::dec << '\n';
  }
  refresh_controls_state();
}

void ImageControls::on_check_toggled(int control_id,
                                     Gtk::CheckButton *button) {
  if (!device_ || !button)
    return;

  auto *control = v4l2core_get_control_by_id(device_, control_id);
  if (!control)
    return;

  const int new_value = button->get_active() ? 1 : 0;
  if (control->value == new_value)
    return;

  control->value = new_value;
  if (v4l2core_set_control_value_by_id(device_, control_id) != 0) {
    std::cerr << "Falha ao atualizar controle 0x" << std::hex << control_id
              << std::dec << '\n';
  }
  refresh_controls_state();
}

void ImageControls::on_combo_changed(int control_id,
                                     Gtk::ComboBoxText *combo) {
  if (!device_ || !combo)
    return;

  auto *binding = find_combo_binding(control_id, combo);
  if (!binding)
    return;

  const int index = combo->get_active_row_number();
  if (index < 0 ||
      static_cast<size_t>(index) >= binding->values.size())
    return;

  auto *control = v4l2core_get_control_by_id(device_, control_id);
  if (!control)
    return;

  const int new_value = binding->values[static_cast<size_t>(index)];
  if (control->value == new_value)
    return;

  control->value = new_value;
  if (v4l2core_set_control_value_by_id(device_, control_id) != 0) {
    std::cerr << "Falha ao atualizar controle 0x" << std::hex << control_id
              << std::dec << '\n';
  }
  refresh_controls_state();
}

ImageControls::ComboBinding *
ImageControls::find_combo_binding(int control_id, Gtk::ComboBoxText *combo) {
  auto it = std::find_if(combo_bindings_.begin(), combo_bindings_.end(),
                         [&](const ComboBinding &binding) {
                           return binding.control_id == control_id &&
                                  binding.combo == combo;
                         });
  return it != combo_bindings_.end() ? &(*it) : nullptr;
}

void ImageControls::refresh_controls_state() {
  if (!device_)
    return;

  if (has_reset_button())
    reset_button().set_sensitive(true);

  for (auto &binding : slider_bindings_) {
    auto *control = refresh_control(binding.control_id);
    if (!control)
      continue;

    const bool active = control_is_active(*control);
    if (binding.row)
      binding.row->set_sensitive(active);
    if (binding.scale)
      binding.scale->set_sensitive(active);

    if (!binding.adjustment)
      continue;

    binding.handler.block(true);
    binding.adjustment->set_lower(control->control.minimum);
    binding.adjustment->set_upper(control->control.maximum);
    binding.adjustment->set_step_increment(resolve_step(*control));
    binding.adjustment->set_page_increment(resolve_step(*control) * 5.0);
    binding.adjustment->set_value(control->value);
    binding.handler.block(false);
  }

  for (auto &binding : check_bindings_) {
    auto *control = refresh_control(binding.control_id);
    if (!control || !binding.button)
      continue;

    const bool active = control_is_active(*control);
    if (binding.row)
      binding.row->set_sensitive(active);

    binding.handler.block(true);
    binding.button->set_sensitive(active);
    binding.button->set_active(control->value != 0);
    binding.handler.block(false);
  }

  for (auto &binding : combo_bindings_) {
    auto *control = refresh_control(binding.control_id);
    if (!control || !binding.combo)
      continue;

    const bool active = control_is_active(*control);
    if (binding.row)
      binding.row->set_sensitive(active);
    binding.combo->set_sensitive(active);

    int active_index = 0;
    for (size_t i = 0; i < binding.values.size(); ++i) {
      if (binding.values[i] == control->value) {
        active_index = static_cast<int>(i);
        break;
      }
    }

    binding.handler.block(true);
    binding.combo->set_active(active_index);
    binding.handler.block(false);
  }
}

void ImageControls::on_reset_clicked() {
  if (!device_)
    return;

  auto *control = v4l2core_get_control_list(device_);
  for (; control != nullptr; control = control->next) {
    const int id = control->control.id;

    if (control->control.flags & V4L2_CTRL_FLAG_READ_ONLY)
      continue;

    const bool active = control_is_active(*control);
    if (!active && !should_force_reset(id))
      continue;

    switch (control->control.type) {
    case V4L2_CTRL_TYPE_INTEGER:
    case V4L2_CTRL_TYPE_BOOLEAN:
    case V4L2_CTRL_TYPE_MENU:
    case V4L2_CTRL_TYPE_INTEGER_MENU:
    case V4L2_CTRL_TYPE_BITMASK:
      control->value = control->control.default_value;
      if (v4l2core_set_control_value_by_id(device_, id) != 0) {
        // ignore failures for controls that remain inactive in their default state
      }
      break;
    default:
      break;
    }
  }

  refresh_controls_state();
}

bool ImageControls::should_force_reset(int control_id) const {
  switch (control_id) {
  case V4L2_CID_AUTO_WHITE_BALANCE:
  case V4L2_CID_EXPOSURE_AUTO:
  case V4L2_CID_EXPOSURE_AUTO_PRIORITY:
    return true;
  default:
    return false;
  }
}
