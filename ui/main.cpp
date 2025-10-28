#include <gtkmm/application.h>

#include "CameraWindow.hpp"

int main(int argc, char *argv[]) {
  auto app = Gtk::Application::create(argc, argv, "org.guvcview.fork");

  CameraWindow window;
  return app->run(window);
}
