#include "AudioControls.hpp"

#include <gtkmm/grid.h>

namespace {
ControlsBase::ConstructionOptions make_window_options() {
  ControlsBase::ConstructionOptions options;
  options.title = "Controles de áudio";
  options.header_title = options.title;
  options.width = 520;
  options.height = 480;
  return options;
}
} // namespace

AudioControls::AudioControls() : ControlsBase(make_window_options()) {
  ControlsBase::ComboRowConfig combo_config;
  combo_config.active_index = 0;

  add_row(*create_combo_row("Áudio API:",
                            {"PORTAUDIO", "ALSA", "PulseAudio"},
                            combo_config));

  ControlsBase::ComboRowConfig device_config;
  device_config.active_index = 1;
  add_row(*create_combo_row("Dispositivo de som:",
                            {"default", "pulse", "hw:0,0"},
                            device_config));

  ControlsBase::ComboRowConfig frequency_config;
  frequency_config.active_index = 0;
  add_row(*create_combo_row("Frequência:",
                            {"Automático", "44100 Hz", "48000 Hz"},
                            frequency_config));

  ControlsBase::ComboRowConfig channels_config;
  channels_config.active_index = 0;
  add_row(*create_combo_row("Canais:",
                            {"Automático", "Mono", "Estéreo"},
                            channels_config));

  ControlsBase::SliderRowConfig latency_config;
  latency_config.step = 0.001;
  latency_config.digits = 3;
  latency_config.label_width_chars = 10;
  latency_config.scale_margin_left = 0;
  latency_config.scale_margin_right = 0;
  add_row(*create_slider_row("Latency:", 0.035, 0.0, 0.2, latency_config));

  auto filters_section =
      Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 6));
  filters_section->set_hexpand(true);
  filters_section->set_margin_top(8);
  filters_section->get_style_context()->add_class("controls-row");

  auto filters_title =
      Gtk::manage(new Gtk::Label("---- Audio Filters ----"));
  filters_title->set_halign(Gtk::ALIGN_CENTER);
  filters_title->get_style_context()->add_class("controls-label");
  filters_section->pack_start(*filters_title, Gtk::PACK_SHRINK);

  auto filters_grid = Gtk::manage(new Gtk::Grid());
  filters_grid->set_column_spacing(18);
  filters_grid->set_row_spacing(6);
  filters_grid->set_hexpand(true);

  const std::vector<Glib::ustring> filters = {
      "Eco", "Reverb", "Ruído", "WhaWah", "Patinho"};

  for (size_t index = 0; index < filters.size(); ++index) {
    auto button = Gtk::manage(new Gtk::CheckButton(filters[index]));
    button->get_style_context()->add_class("controls-toggle");
    button->set_halign(Gtk::ALIGN_START);
    filters_grid->attach(*button, static_cast<int>(index), 0, 1, 1);
  }

  filters_section->pack_start(*filters_grid, Gtk::PACK_SHRINK);
  add_row(*filters_section);

  show_all_children();
}
