#include "VideoControls.hpp"

#include <gtkmm/grid.h>

namespace {
ControlsBase::ConstructionOptions make_window_options() {
  ControlsBase::ConstructionOptions options;
  options.title = "Controles de vídeo";
  options.header_title = options.title;
  options.width = 520;
  options.height = 540;
  return options;
}
} // namespace

VideoControls::VideoControls() : ControlsBase(make_window_options()) {
  ControlsBase::ComboRowConfig device_config;
  device_config.active_index = 0;
  add_row(*create_combo_row(
      "Dispositivo:",
      {"Positivo Theia Camera: Positivo", "Câmera integrada", "USB Capture"},
      device_config));

  ControlsBase::ComboRowConfig framerate_config;
  framerate_config.active_index = 0;
  add_row(*create_combo_row("Taxa de imagens:",
                            {"30/1 fps", "25/1 fps", "60/1 fps"},
                            framerate_config));

  ControlsBase::ComboRowConfig resolution_config;
  resolution_config.active_index = 0;
  add_row(*create_combo_row("Resolução:",
                            {"1280x720", "1920x1080", "640x480"},
                            resolution_config));

  ControlsBase::ComboRowConfig format_config;
  format_config.active_index = 0;
  add_row(*create_combo_row("Saída da câmara:",
                            {"MJPG - Motion-JPEG", "YUYV - Raw", "H264"},
                            format_config));

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

  const std::vector<Glib::ustring> filters = {
      "Espelho",            "Espelho (a meio)",   "Invertido",
      "Invertido (a meio)", "Negativo",         "Cinza",
      "Pedaços",            "Partículas",        "Lente (Raiz)",
      "Lente (Pot)",        "Lente (Pot 2)",     "Embaçamento",
      "Embaçamento maior",  "Binary"
  };

  for (size_t i = 0; i < filters.size(); ++i) {
    auto button = Gtk::manage(new Gtk::CheckButton(filters[i]));
    button->get_style_context()->add_class("controls-toggle");
    button->set_halign(Gtk::ALIGN_START);
    const int column = static_cast<int>(i % 3);
    const int row = static_cast<int>(i / 3);
    filters_grid->attach(*button, column, row, 1, 1);
  }

  filters_section->pack_start(*filters_grid, Gtk::PACK_SHRINK);
  add_row(*filters_section);

  show_all_children();
}
