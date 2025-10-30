#pragma once

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/label.h>
#include <gtkmm/window.h>

class ImageControlsWindow : public Gtk::Window {
public:
  ImageControlsWindow();
  ~ImageControlsWindow() override = default;

private:
  Gtk::Box root_box_{Gtk::ORIENTATION_VERTICAL};
  Gtk::HeaderBar header_bar_;
  Gtk::Label placeholder_label_{
      "Os controles de imagem estarão disponíveis em breve."};
  Gtk::Button close_button_{"Fechar"};

  void on_close_clicked();
};
