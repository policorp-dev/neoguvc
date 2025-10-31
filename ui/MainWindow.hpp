#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/stylecontext.h>
#include <gtkmm/window.h>
#include <glibmm/dispatcher.h>
#include <glibmm/refptr.h>
#include <sigc++/connection.h>

extern "C" {
#include "gviewv4l2core.h"
#include "colorspaces.h"
#include "gviewencoder.h"
#include "audio.h"
}

#include "ControlsBase.hpp"

class MainWindow : public Gtk::Window {
public:
  MainWindow();
  ~MainWindow() override;

private:
  void initialise_device();
  void capture_loop();
  void on_frame_ready();
  void stop_stream();

  Gtk::Box layout_box_{Gtk::ORIENTATION_HORIZONTAL};
  Gtk::Box content_box_{Gtk::ORIENTATION_VERTICAL};
  Gtk::Image image_widget_;
  Gtk::Label status_label_;
  Gtk::Box sidebar_box_{Gtk::ORIENTATION_VERTICAL};
  Gtk::Box spacer_top_{Gtk::ORIENTATION_VERTICAL};
  Gtk::Box spacer_bottom_{Gtk::ORIENTATION_VERTICAL};
  Gtk::Button menu_button_;
  Gtk::Button capture_button_{};
  Gtk::Button record_button_{};
  Gtk::Image *record_button_icon_ = nullptr;
  Glib::RefPtr<Gdk::Pixbuf> record_icon_idle_;
  Glib::RefPtr<Gdk::Pixbuf> record_icon_active_;
  Gtk::Menu menu_popup_;
  Glib::Dispatcher dispatcher_;

  v4l2_dev_t *device_ = nullptr;
  std::thread capture_thread_;
  std::mutex frame_mutex_;
  std::vector<uint8_t> rgb_buffer_;
  int frame_width_ = 0;
  int frame_height_ = 0;
  std::atomic<bool> running_{false};
  std::atomic<bool> recording_{false};
  bool pending_frame_ = false;
  std::mutex encoder_mutex_;

  std::atomic<bool> snapshot_request_{false};
  std::atomic<bool> start_record_request_{false};
  std::atomic<bool> stop_record_request_{false};

  encoder_context_t *encoder_ctx_ = nullptr;
  std::string current_video_path_;

  audio_context_t *audio_ctx_ = nullptr;
  audio_buff_t *audio_buffer_ = nullptr;
  std::thread audio_thread_;
  std::atomic<bool> audio_thread_running_{false};
  int audio_sample_type_ = GV_SAMPLE_TYPE_FLOAT;
  uint32_t audio_fx_mask_ = AUDIO_FX_NONE;

  void on_capture_button_clicked();
  void on_record_button_clicked();
  void on_menu_button_clicked();
  void on_config_menu_item_activated(const std::string &id);
  void on_config_window_hidden(const std::string &id);
  void save_snapshot(v4l2_frame_buff_t *frame);
  void handle_recording_frame(v4l2_frame_buff_t *frame);
  bool start_recording(v4l2_frame_buff_t *frame);
  void stop_recording();
  std::string build_output_path(bool video) const;
  std::string timestamp_string() const;
  void post_status(const std::string &text);
  void initialise_audio();
  void start_audio_capture(int frame_size);
  void stop_audio_capture();
  void audio_capture_loop();

  struct ConfigWindowEntry {
    std::string id;
    Glib::ustring menu_label;
    std::function<std::unique_ptr<ControlsBase>(MainWindow &)> factory;
    std::unique_ptr<ControlsBase> window;
    Gtk::MenuItem *menu_item{nullptr};
    sigc::connection hide_connection;
  };

  std::vector<ConfigWindowEntry> config_windows_;

public:
  v4l2_dev_t *device_handle() const { return device_; }
};
