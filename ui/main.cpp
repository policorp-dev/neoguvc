#include <gtkmm/application.h>

#include "MainWindow.hpp"

int main(int argc, char *argv[]) {
  auto app = Gtk::Application::create(argc, argv, "org.guvcview.fork");

  MainWindow window;
  return app->run(window);
}
