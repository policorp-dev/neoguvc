#include "VideoControls.hpp"

#include "MainWindow.hpp"

#include <gtkmm/grid.h>
#include <sigc++/bind.h>

namespace {
ControlsBase::ConstructionOptions make_window_options() {
  ControlsBase::ConstructionOptions options;
  options.title = "Controles de vídeo";
  options.header_title = options.title;
  options.width = 520;
  options.height = 540;
  options.show_reset_button = true;
  options.reset_button_label = "Restaurar padrão";
  return options;
}

// Helper to build filter labels and masks.
const std::vector<std::pair<uint32_t, Glib::ustring>> &video_filters() {
  static const std::vector<std::pair<uint32_t, Glib::ustring>> kFilters = {
      {REND_FX_YUV_MIRROR, "Espelho"},
      {REND_FX_YUV_HALF_MIRROR, "Espelho (a meio)"},
      {REND_FX_YUV_UPTURN, "Invertido"},
      {REND_FX_YUV_HALF_UPTURN, "Invertido (a meio)"},
      {REND_FX_YUV_NEGATE, "Negativo"},
      {REND_FX_YUV_MONOCR, "Cinza"},
      {REND_FX_YUV_PIECES, "Pedaços"},
      {REND_FX_YUV_PARTICLES, "Partículas"},
      {REND_FX_YUV_SQRT_DISTORT, "Lente (Raiz)"},
      {REND_FX_YUV_POW_DISTORT, "Lente (Pot)"},
      {REND_FX_YUV_POW2_DISTORT, "Lente (Pot 2)"},
      {REND_FX_YUV_BLUR, "Embaçamento"},
      {REND_FX_YUV_BLUR2, "Embaçamento maior"},
      {REND_FX_YUV_BINARY, "Binary"}};
  return kFilters;
}
} // namespace

VideoControls::VideoControls(MainWindow &window)
    : ControlsBase(make_window_options()), main_window_(window),
      device_(window.device_handle()) {
  initialise_ui();

  if (has_reset_button())
    reset_button().signal_clicked().connect(
        sigc::mem_fun(*this, &VideoControls::on_reset_clicked));

  refresh_state();
  show_all_children();
}

void VideoControls::initialise_ui() {
  ControlsBase::ComboRowConfig device_config;
  device_config.combo_hexpand = true;
  device_config.on_configure = [this](Gtk::ComboBoxText &combo) {
    device_combo_ = &combo;
    combo.signal_changed().connect(
        sigc::mem_fun(*this, &VideoControls::on_device_changed));
  };
  add_row(*create_combo_row("Dispositivo:", {}, device_config));

  ControlsBase::ComboRowConfig frame_rate_config;
  frame_rate_config.combo_hexpand = true;
  frame_rate_config.on_configure = [this](Gtk::ComboBoxText &combo) {
    frame_rate_combo_ = &combo;
    combo.signal_changed().connect(
        sigc::mem_fun(*this, &VideoControls::on_frame_rate_changed));
  };
  add_row(*create_combo_row("Taxa de imagens:", {}, frame_rate_config));

  ControlsBase::ComboRowConfig resolution_config;
  resolution_config.combo_hexpand = true;
  resolution_config.on_configure = [this](Gtk::ComboBoxText &combo) {
    resolution_combo_ = &combo;
    combo.signal_changed().connect(
        sigc::mem_fun(*this, &VideoControls::on_resolution_changed));
  };
  add_row(*create_combo_row("Resolução:", {}, resolution_config));

  ControlsBase::ComboRowConfig format_config;
  format_config.combo_hexpand = true;
  format_config.on_configure = [this](Gtk::ComboBoxText &combo) {
    format_combo_ = &combo;
    combo.signal_changed().connect(
        sigc::mem_fun(*this, &VideoControls::on_format_changed));
  };
  add_row(*create_combo_row("Saída da câmara:", {}, format_config));

  auto filters_section =
      Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 6));
  filters_section->set_hexpand(true);
  filters_section->set_margin_top(8);
  filters_section->get_style_context()->add_class("controls-row");

  auto filters_title =
      Gtk::manage(new Gtk::Label("---- Filtros de vídeo ----"));
  filters_title->set_halign(Gtk::ALIGN_CENTER);
  filters_title->get_style_context()->add_class("controls-label");
  filters_section->pack_start(*filters_title, Gtk::PACK_SHRINK);

  auto filters_grid = Gtk::manage(new Gtk::Grid());
  filters_grid->set_column_spacing(18);
  filters_grid->set_row_spacing(6);
  filters_grid->set_hexpand(true);

  bind_filter_buttons(video_filters(), *filters_grid);

  filters_section->pack_start(*filters_grid, Gtk::PACK_SHRINK);
  add_row(*filters_section);
}

void VideoControls::bind_filter_buttons(
    const std::vector<std::pair<uint32_t, Glib::ustring>> &filters,
    Gtk::Grid &grid) {
  filter_bindings_.clear();

  for (size_t i = 0; i < filters.size(); ++i) {
    auto button = Gtk::manage(new Gtk::CheckButton(filters[i].second));
    button->get_style_context()->add_class("controls-toggle");
    button->set_halign(Gtk::ALIGN_START);

    const int column = static_cast<int>(i % 3);
    const int row = static_cast<int>(i / 3);
    grid.attach(*button, column, row, 1, 1);

    button->signal_toggled().connect(sigc::bind(
        sigc::mem_fun(*this, &VideoControls::on_filter_toggled), button,
        filters[i].first));

    filter_bindings_.push_back(FilterBinding{button, filters[i].first});
  }

  auto current_mask = main_window_.render_fx_mask();
  for (auto &binding : filter_bindings_) {
    if (binding.mask == REND_FX_YUV_MIRROR && binding.button) {
      binding.button->set_active(true);
      current_mask |= binding.mask;
      break;
    }
  }
  main_window_.set_render_fx_mask(current_mask);
}

void VideoControls::with_update_guard(const std::function<void()> &fn) {
  const bool previous_state = updating_ui_;
  updating_ui_ = true;
  if (fn)
    fn();
  updating_ui_ = previous_state;
}

void VideoControls::populate_devices() {
  if (!device_combo_)
    return;

  with_update_guard([&]() {
    device_combo_->remove_all();
    devices_.clear();

    int current_index = device_ ? v4l2core_get_this_device_index(device_) : -1;
    const int num_devices = v4l2core_get_num_devices();

    if (num_devices <= 0) {
      const char *name = device_ ? v4l2core_get_videodevice(device_) : "";
      Glib::ustring label = name && *name ? name : "Dispositivo atual";
      devices_.push_back(DeviceEntry{label, name ? std::string{name} : ""});
      device_combo_->append(label);
      device_combo_->set_active(0);
      device_combo_->set_sensitive(false);
      return;
    }

    device_combo_->set_sensitive(true);
    for (int i = 0; i < num_devices; ++i) {
      auto *sys_data = v4l2core_get_device_sys_data(i);
      if (!sys_data || !sys_data->name)
        continue;
      Glib::ustring label(sys_data->name);
      if (!sys_data->valid)
        label += " (indisponível)";

      devices_.push_back(
          DeviceEntry{label.raw(), sys_data->device ? sys_data->device : ""});
      device_combo_->append(label);
    }

    if (current_index < 0 || current_index >= static_cast<int>(devices_.size()))
      current_index = 0;
    device_combo_->set_active(current_index);
  });
}

void VideoControls::populate_formats() {
  if (!format_combo_ || !device_)
    return;

  with_update_guard([&]() {
    format_combo_->remove_all();
    formats_.clear();

    auto *formats = v4l2core_get_formats_list(device_);
    const int num_formats = v4l2core_get_number_formats(device_);
    if (!formats || num_formats <= 0) {
      format_combo_->set_sensitive(false);
      return;
    }

    format_combo_->set_sensitive(true);
    const uint32_t current_format =
        v4l2core_get_requested_frame_format(device_);
    int active_index = 0;

    for (int i = 0; i < num_formats; ++i) {
      const bool supported = formats[i].dec_support;
      Glib::ustring label;
      if (formats[i].fourcc)
        label = Glib::ustring(formats[i].fourcc) + " - " +
                Glib::ustring(formats[i].description ? formats[i].description
                                                     : "");
      else
        label = Glib::ustring(formats[i].description ? formats[i].description
                                                     : "Formato");
      if (!supported)
        label += " (não suportado)";

      format_combo_->append(label);
      formats_.push_back(FormatEntry{label,
                                     static_cast<uint32_t>(formats[i].format),
                                     i, supported});

      if (formats[i].format == current_format)
        active_index = i;
    }

    if (!formats_.empty())
      format_combo_->set_active(active_index);
  });
}

void VideoControls::populate_resolutions() {
  if (!resolution_combo_ || !device_ || formats_.empty())
    return;

  with_update_guard([&]() {
    resolution_combo_->remove_all();
    resolutions_.clear();

    int format_row = format_combo_->get_active_row_number();
    if (format_row < 0 || format_row >= static_cast<int>(formats_.size()))
      return;

    const auto format_index = formats_[format_row].index;
    auto *formats = v4l2core_get_formats_list(device_);
    if (!formats || format_index < 0)
      return;

    const auto &format = formats[format_index];
    if (!format.list_stream_cap || format.numb_res <= 0) {
      resolution_combo_->set_sensitive(false);
      return;
    }

    resolution_combo_->set_sensitive(true);
    const int current_width = v4l2core_get_frame_width(device_);
    const int current_height = v4l2core_get_frame_height(device_);
    int active_index = 0;

    for (int i = 0; i < format.numb_res; ++i) {
      const auto &cap = format.list_stream_cap[i];
      Glib::ustring label =
          Glib::ustring::compose("%1x%2", cap.width, cap.height);
      resolution_combo_->append(label);
      resolutions_.push_back(ResolutionEntry{cap.width, cap.height, i});
      if (cap.width == current_width && cap.height == current_height)
        active_index = i;
    }

    if (!resolutions_.empty())
      resolution_combo_->set_active(active_index);
  });
}

void VideoControls::populate_frame_rates() {
  if (!frame_rate_combo_ || !device_ || resolutions_.empty() ||
      formats_.empty())
    return;

  with_update_guard([&]() {
    frame_rate_combo_->remove_all();
    frame_rates_.clear();

    int format_row = format_combo_->get_active_row_number();
    int resolution_row = resolution_combo_->get_active_row_number();
    if (format_row < 0 || resolution_row < 0 ||
        format_row >= static_cast<int>(formats_.size()) ||
        resolution_row >= static_cast<int>(resolutions_.size()))
      return;

    const auto format_index = formats_[format_row].index;
    const auto resolution_index = resolutions_[resolution_row].index;
    auto *formats = v4l2core_get_formats_list(device_);
    if (!formats || format_index < 0)
      return;

    const auto &cap_list = formats[format_index].list_stream_cap;
    if (!cap_list || resolution_index < 0)
      return;

    const auto &cap = cap_list[resolution_index];
    if (!cap.framerate_num || !cap.framerate_denom || cap.numb_frates <= 0) {
      frame_rate_combo_->set_sensitive(false);
      return;
    }

    frame_rate_combo_->set_sensitive(true);
    const int current_num = v4l2core_get_fps_num(device_);
    const int current_denom = v4l2core_get_fps_denom(device_);
    int active_index = 0;

    for (int i = 0; i < cap.numb_frates; ++i) {
      Glib::ustring label =
          Glib::ustring::compose("%1/%2 fps", cap.framerate_denom[i],
                                 cap.framerate_num[i]);
      frame_rate_combo_->append(label);
      frame_rates_.push_back(FrameRateEntry{cap.framerate_num[i],
                                            cap.framerate_denom[i], i});
      if (cap.framerate_num[i] == current_num &&
          cap.framerate_denom[i] == current_denom)
        active_index = i;
    }

    if (!frame_rates_.empty())
      frame_rate_combo_->set_active(active_index);
  });
}

void VideoControls::refresh_state() {
  device_ = main_window_.device_handle();

  if (!device_) {
    if (device_combo_)
      device_combo_->set_sensitive(false);
    if (format_combo_)
      format_combo_->set_sensitive(false);
    if (resolution_combo_)
      resolution_combo_->set_sensitive(false);
    if (frame_rate_combo_)
      frame_rate_combo_->set_sensitive(false);
    update_filter_buttons(main_window_.render_fx_mask());
    return;
  }

  populate_devices();
  populate_formats();
  populate_resolutions();
  populate_frame_rates();
  update_filter_buttons(main_window_.render_fx_mask());
}

void VideoControls::on_device_changed() {
  if (updating_ui_ || !device_combo_)
    return;

  const int index = device_combo_->get_active_row_number();
  if (index < 0 || index >= static_cast<int>(devices_.size()))
    return;

  const auto &entry = devices_[index];
  if (entry.device_path.empty())
    return;

  if (!main_window_.switch_device(entry.device_path))
    return;

  device_ = main_window_.device_handle();
  refresh_state();
}

void VideoControls::on_format_changed() {
  if (updating_ui_ || !device_ || !format_combo_)
    return;

  const int index = format_combo_->get_active_row_number();
  if (index < 0 || index >= static_cast<int>(formats_.size()))
    return;

  const auto &entry = formats_[index];
  if (!entry.supported)
    return;

  const bool success = main_window_.reconfigure_video(
      [entry](v4l2_dev_t *vd) {
        v4l2core_prepare_new_format(vd, entry.fourcc);

        auto *formats = v4l2core_get_formats_list(vd);
        const int format_index =
            v4l2core_get_frame_format_index(vd, entry.fourcc);
        if (!formats || format_index < 0)
          return;

        const auto &format = formats[format_index];
        if (!format.list_stream_cap || format.numb_res <= 0)
          return;

        const auto &cap = format.list_stream_cap[0];
        v4l2core_prepare_new_resolution(vd, cap.width, cap.height);

        if (cap.numb_frates > 0 && cap.framerate_num && cap.framerate_denom)
          v4l2core_define_fps(vd, cap.framerate_num[0],
                              cap.framerate_denom[0]);
      });

  if (success) {
    device_ = main_window_.device_handle();
    refresh_state();
  }
}

void VideoControls::on_resolution_changed() {
  if (updating_ui_ || !device_ || !resolution_combo_)
    return;

  const int index = resolution_combo_->get_active_row_number();
  if (index < 0 || index >= static_cast<int>(resolutions_.size()))
    return;

  const auto res = resolutions_[index];

  const bool success = main_window_.reconfigure_video(
      [res](v4l2_dev_t *vd) {
        v4l2core_prepare_new_resolution(vd, res.width, res.height);

        const int format_index =
            v4l2core_get_frame_format_index(vd,
                                            v4l2core_get_requested_frame_format(
                                                vd));
        auto *formats = v4l2core_get_formats_list(vd);
        if (!formats || format_index < 0)
          return;

        const auto &cap =
            formats[format_index].list_stream_cap[res.index];
        if (cap.numb_frates > 0 && cap.framerate_num && cap.framerate_denom)
          v4l2core_define_fps(vd, cap.framerate_num[0],
                              cap.framerate_denom[0]);
      });

  if (success) {
    device_ = main_window_.device_handle();
    refresh_state();
  }
}

void VideoControls::on_frame_rate_changed() {
  if (updating_ui_ || !device_ || !frame_rate_combo_)
    return;

  const int index = frame_rate_combo_->get_active_row_number();
  if (index < 0 || index >= static_cast<int>(frame_rates_.size()))
    return;

  const auto entry = frame_rates_[index];
  const bool success = main_window_.reconfigure_video(
      [entry](v4l2_dev_t *vd) {
        v4l2core_define_fps(vd, entry.numerator, entry.denominator);
      });

  if (success) {
    device_ = main_window_.device_handle();
    refresh_state();
  }
}

void VideoControls::on_filter_toggled(Gtk::CheckButton *button,
                                      uint32_t mask) {
  if (!button || updating_ui_)
    return;

  uint32_t current_mask = main_window_.render_fx_mask();
  if (button->get_active())
    current_mask |= mask;
  else
    current_mask &= ~mask;

  main_window_.set_render_fx_mask(current_mask);
  update_filter_buttons(current_mask);
}

void VideoControls::update_filter_buttons(uint32_t mask) {
  with_update_guard([&]() {
    for (auto &binding : filter_bindings_) {
      if (!binding.button)
        continue;
      const bool active = (mask & binding.mask) != 0;
      binding.button->set_active(active);
    }
  });
}

void VideoControls::on_reset_clicked() {
  if (!device_)
    return;

  const bool success = main_window_.reconfigure_video([](v4l2_dev_t *vd) {
    v4l2core_prepare_valid_format(vd);
    v4l2core_prepare_valid_resolution(vd);

    const int format_index =
        v4l2core_get_frame_format_index(
            vd, v4l2core_get_requested_frame_format(vd));
    auto *formats = v4l2core_get_formats_list(vd);
    if (!formats || format_index < 0)
      return;

    const auto &format = formats[format_index];
    if (!format.list_stream_cap || format.numb_res <= 0)
      return;

    const auto &cap = format.list_stream_cap[0];
    v4l2core_prepare_new_resolution(vd, cap.width, cap.height);
    if (cap.numb_frates > 0 && cap.framerate_num && cap.framerate_denom)
      v4l2core_define_fps(vd, cap.framerate_num[0], cap.framerate_denom[0]);
  });

  if (success) {
    main_window_.set_render_fx_mask(REND_FX_YUV_MIRROR);
    device_ = main_window_.device_handle();
    refresh_state();
  }
}
