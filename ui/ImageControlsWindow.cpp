#include "ImageControlsWindow.hpp"

ImageControlsWindow::ImageControlsWindow() {
  set_title("Controles de imagem");
  set_default_size(360, 240);
  set_resizable(false);

  header_bar_.set_show_close_button(true);
  header_bar_.set_title("Controles de imagem");
  set_titlebar(header_bar_);

  add(root_box_);
  root_box_.set_spacing(12);
  root_box_.set_margin_top(16);
  root_box_.set_margin_bottom(16);
  root_box_.set_margin_left(16);
  root_box_.set_margin_right(16);

  placeholder_label_.set_line_wrap(true);
  placeholder_label_.set_justify(Gtk::JUSTIFY_CENTER);
  root_box_.pack_start(placeholder_label_, Gtk::PACK_EXPAND_WIDGET);

  close_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &ImageControlsWindow::on_close_clicked));
  close_button_.set_halign(Gtk::ALIGN_END);
  root_box_.pack_start(close_button_, Gtk::PACK_SHRINK);

  show_all_children();
}

void ImageControlsWindow::on_close_clicked() { hide(); }
