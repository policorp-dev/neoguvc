#include "AudioControls.hpp"

#include "MainWindow.hpp"

#include <algorithm>
#include <gtkmm/grid.h>
#include <sigc++/bind.h>

namespace {
ControlsBase::ConstructionOptions make_window_options() {
  ControlsBase::ConstructionOptions options;
  options.title = "Controles de áudio";
  options.header_title = options.title;
  options.width = 520;
  options.height = 540;
  options.show_reset_button = true;
  options.reset_button_label = "Restaurar padrão";
  return options;
}

const std::vector<std::pair<uint32_t, Glib::ustring>> &audio_filters() {
  static const std::vector<std::pair<uint32_t, Glib::ustring>> kFilters = {
      {AUDIO_FX_ECHO, "Eco"},         {AUDIO_FX_REVERB, "Reverb"},
      {AUDIO_FX_FUZZ, "Ruído"}};
  return kFilters;
}

const std::vector<int> &standard_samplerates() {
  static const std::vector<int> kRates = {
      0,    7350,  8000,  11025, 12000, 16000, 22050,
      24000, 32000, 44100, 48000, 64000, 88200, 96000};
  return kRates;
}
} // namespace

AudioControls::AudioControls(MainWindow &window)
    : ControlsBase(make_window_options()), main_window_(window),
      audio_ctx_(window.audio_context()) {
  initialise_ui();

  if (has_reset_button())
    reset_button().signal_clicked().connect(
        sigc::mem_fun(*this, &AudioControls::on_reset_clicked));

  refresh_state();
  show_all_children();
}

void AudioControls::initialise_ui() {
  ControlsBase::ComboRowConfig api_config;
  api_config.combo_hexpand = true;
  api_config.on_configure = [this](Gtk::ComboBoxText &combo) {
    api_combo_ = &combo;
    combo.signal_changed().connect(
        sigc::mem_fun(*this, &AudioControls::on_api_changed));
  };
  add_row(*create_combo_row("Áudio API:", {}, api_config));

  ControlsBase::ComboRowConfig device_config;
  device_config.combo_hexpand = true;
  device_config.on_configure = [this](Gtk::ComboBoxText &combo) {
    device_combo_ = &combo;
    combo.signal_changed().connect(
        sigc::mem_fun(*this, &AudioControls::on_device_changed));
  };
  add_row(*create_combo_row("Dispositivo de som:", {}, device_config));

  ControlsBase::ComboRowConfig samplerate_config;
  samplerate_config.combo_hexpand = true;
  samplerate_config.on_configure = [this](Gtk::ComboBoxText &combo) {
    samplerate_combo_ = &combo;
    combo.signal_changed().connect(
        sigc::mem_fun(*this, &AudioControls::on_samplerate_changed));
  };
  add_row(*create_combo_row("Frequência:", {}, samplerate_config));

  ControlsBase::ComboRowConfig channels_config;
  channels_config.combo_hexpand = true;
  channels_config.on_configure = [this](Gtk::ComboBoxText &combo) {
    channels_combo_ = &combo;
    combo.signal_changed().connect(
        sigc::mem_fun(*this, &AudioControls::on_channels_changed));
  };
  add_row(*create_combo_row("Canais:", {}, channels_config));

  ControlsBase::SliderRowConfig latency_config;
  latency_config.step = 0.001;
  latency_config.digits = 3;
  latency_config.label_width_chars = 10;
  latency_config.scale_margin_left = 0;
  latency_config.scale_margin_right = 0;
  latency_config.scale_hexpand = true;
  latency_config.on_configure = [this](Gtk::Scale &scale,
                                       const Glib::RefPtr<Gtk::Adjustment> &)
  {
    latency_scale_ = &scale;
    latency_scale_->set_range(0.0, 0.5);
    latency_scale_->signal_value_changed().connect(
        sigc::mem_fun(*this, &AudioControls::on_latency_changed));
  };
  add_row(*create_slider_row("Latência:", 0.030, 0.0, 0.5, latency_config));

  auto filters_section =
      Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 6));
  filters_section->set_hexpand(true);
  filters_section->set_margin_top(8);
  filters_section->get_style_context()->add_class("controls-row");

  auto filters_title = Gtk::manage(new Gtk::Label("---- Filtros de Áudio ----"));
  filters_title->set_halign(Gtk::ALIGN_CENTER);
  filters_title->get_style_context()->add_class("controls-label");
  filters_section->pack_start(*filters_title, Gtk::PACK_SHRINK);

  auto filters_grid = Gtk::manage(new Gtk::Grid());
  filters_grid->set_column_spacing(18);
  filters_grid->set_row_spacing(6);
  filters_grid->set_hexpand(true);

  audio_filter_buttons_.clear();
  const auto &filters = audio_filters();
  for (size_t i = 0; i < filters.size(); ++i) {
    auto button = Gtk::manage(new Gtk::CheckButton(filters[i].second));
    button->get_style_context()->add_class("controls-toggle");
    button->set_halign(Gtk::ALIGN_START);
    filters_grid->attach(*button, static_cast<int>(i), 0, 1, 1);
    button->signal_toggled().connect(sigc::bind(
        sigc::mem_fun(*this, &AudioControls::on_filter_toggled), button,
        filters[i].first));
    audio_filter_buttons_.push_back({button, filters[i].first});
  }

  filters_section->pack_start(*filters_grid, Gtk::PACK_SHRINK);
  add_row(*filters_section);
}

void AudioControls::with_update_guard(const std::function<void()> &fn) {
  const bool previous = updating_ui_;
  updating_ui_ = true;
  if (fn)
    fn();
  updating_ui_ = previous;
}

void AudioControls::populate_api() {
  if (!api_combo_)
    return;

  with_update_guard([&]() {
    api_combo_->remove_all();
    api_combo_->append("Sem áudio");
    api_combo_->append("PORTAUDIO");
    api_combo_->append("PULSEAUDIO");

    int api = main_window_.audio_api();
    if (api < AUDIO_NONE || api > AUDIO_PULSE)
      api = AUDIO_NONE;

    api_combo_->set_active(api);
  });
}

void AudioControls::populate_devices() {
  if (!device_combo_)
    return;

  with_update_guard([&]() {
    device_combo_->remove_all();

    audio_ctx_ = main_window_.audio_context();
    if (!audio_ctx_) {
      device_combo_->set_sensitive(false);
      return;
    }

    const int num_devices = audio_get_num_inp_devices(audio_ctx_);
    if (num_devices <= 0) {
      device_combo_->set_sensitive(false);
      return;
    }

    device_combo_->set_sensitive(true);
    for (int i = 0; i < num_devices; ++i) {
      auto *device = audio_get_device(audio_ctx_, i);
      if (!device)
        continue;
      device_combo_->append(device->description);
    }
    int active = main_window_.audio_device_index();
    if (active < 0 || active >= num_devices)
      active = 0;
    device_combo_->set_active(active);
  });
}

void AudioControls::populate_samplerates() {
  if (!samplerate_combo_)
    return;

  with_update_guard([&]() {
    samplerate_combo_->remove_all();
    samplerate_values_.clear();

    const auto &rates = standard_samplerates();
    for (size_t i = 0; i < rates.size(); ++i) {
      if (i == 0)
        samplerate_combo_->append("Automático");
      else
        samplerate_combo_->append(
            Glib::ustring::compose("%1 Hz", rates[i]));
      samplerate_values_.push_back(rates[i]);
    }

    audio_ctx_ = main_window_.audio_context();
    int current_rate = audio_ctx_ ? audio_get_samprate(audio_ctx_) : 0;
    if (current_rate <= 0)
      samplerate_combo_->set_active(0);
    else {
      auto it = std::find(samplerate_values_.begin(), samplerate_values_.end(),
                          current_rate);
      if (it != samplerate_values_.end())
        samplerate_combo_->set_active(
            static_cast<int>(std::distance(samplerate_values_.begin(), it)));
      else
        samplerate_combo_->set_active(0);
    }

    samplerate_combo_->set_sensitive(audio_ctx_ != nullptr);
  });
}

void AudioControls::populate_channels() {
  if (!channels_combo_)
    return;

  with_update_guard([&]() {
    channels_combo_->remove_all();
    channel_values_.clear();

    channel_values_ = {0, 1, 2};
    channels_combo_->append("Automático");
    channels_combo_->append("Mono");
    channels_combo_->append("Estéreo");

    audio_ctx_ = main_window_.audio_context();
    int current_channels = audio_ctx_ ? audio_get_channels(audio_ctx_) : 0;
    if (current_channels <= 0)
      channels_combo_->set_active(0);
    else if (current_channels == 1)
      channels_combo_->set_active(1);
    else
      channels_combo_->set_active(2);

    channels_combo_->set_sensitive(audio_ctx_ != nullptr);
  });
}

void AudioControls::update_latency() {
  if (!latency_scale_)
    return;

  with_update_guard([&]() {
    audio_ctx_ = main_window_.audio_context();
    if (!audio_ctx_) {
      latency_scale_->set_sensitive(false);
      latency_scale_->set_value(0.0);
      return;
    }

    latency_scale_->set_sensitive(true);
    latency_scale_->set_value(audio_get_latency(audio_ctx_));
  });
}

void AudioControls::update_filters() {
  const uint32_t mask = main_window_.audio_fx_mask();
  with_update_guard([&]() {
    for (auto &binding : audio_filter_buttons_) {
      if (!binding.first)
        continue;
      const bool active = (mask & binding.second) != 0;
      binding.first->set_active(active);
    }
  });
}

void AudioControls::refresh_state() {
  audio_ctx_ = main_window_.audio_context();
  populate_api();
  populate_devices();
  populate_samplerates();
  populate_channels();
  update_latency();
  update_filters();
}

void AudioControls::on_api_changed() {
  if (updating_ui_ || !api_combo_)
    return;

  int api = api_combo_->get_active_row_number();
  if (api < 0)
    api = AUDIO_NONE;
  if (api > AUDIO_PULSE)
    api = AUDIO_NONE;

  if (!main_window_.recreate_audio_context(api)) {
    refresh_state();
    return;
  }

  audio_ctx_ = main_window_.audio_context();
  refresh_state();
}

void AudioControls::on_device_changed() {
  if (updating_ui_ || !device_combo_)
    return;

  int index = device_combo_->get_active_row_number();
  if (index < 0)
    index = 0;

  if (!main_window_.set_audio_device(index)) {
    refresh_state();
    return;
  }

  refresh_state();
}

void AudioControls::on_samplerate_changed() {
  if (updating_ui_ || !samplerate_combo_)
    return;

  int index = samplerate_combo_->get_active_row_number();
  if (index < 0 || index >= static_cast<int>(samplerate_values_.size()))
    index = 0;

  main_window_.set_audio_samplerate(samplerate_values_[index]);
}

void AudioControls::on_channels_changed() {
  if (updating_ui_ || !channels_combo_)
    return;

  int index = channels_combo_->get_active_row_number();
  if (index < 0 || index >= static_cast<int>(channel_values_.size()))
    index = 0;

  main_window_.set_audio_channels(channel_values_[index]);
}

void AudioControls::on_latency_changed() {
  if (updating_ui_ || !latency_scale_)
    return;

  main_window_.set_audio_latency(latency_scale_->get_value());
}

void AudioControls::on_filter_toggled(Gtk::CheckButton *button,
                                      uint32_t mask) {
  if (!button || updating_ui_)
    return;

  uint32_t new_mask = main_window_.audio_fx_mask();
  if (button->get_active())
    new_mask |= mask;
  else
    new_mask &= ~mask;

  main_window_.set_audio_fx_mask(new_mask);
  update_filters();
}

void AudioControls::on_reset_clicked() {
  main_window_.recreate_audio_context(AUDIO_PORTAUDIO);
  main_window_.set_audio_fx_mask(AUDIO_FX_NONE);
  refresh_state();
}
