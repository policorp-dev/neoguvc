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
#include <gtkmm/menubar.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/separatormenuitem.h>
#include <gtkmm/stylecontext.h>
#include <gtkmm/window.h>
#include <glibmm/dispatcher.h>
#include <glibmm/refptr.h>
#include <sigc++/connection.h>
#include <sigc++/functors/mem_fun.h>

extern "C" {
#include "gviewv4l2core.h"
#include "colorspaces.h"
#include "gviewencoder.h"
#include "audio.h"
#include "gviewrender.h"
#include "render.h"
}

#include "ControlsBase.hpp"

class MainWindow : public Gtk::Window {
public:
  MainWindow();
  ~MainWindow() override;

  v4l2_dev_t *device_handle() const { return device_; }
  audio_context_t *audio_context() const { return audio_ctx_; }

  bool reconfigure_video(const std::function<void(v4l2_dev_t *)> &mutator);
  bool switch_device(const std::string &device_path);

  void set_render_fx_mask(uint32_t mask);
  uint32_t render_fx_mask() const;

  bool recreate_audio_context(int api);
  bool set_audio_device(int index);
  void set_audio_samplerate(int samplerate);
  void set_audio_channels(int channels);
  void set_audio_latency(double latency);
  void set_audio_fx_mask(uint32_t mask);
  uint32_t audio_fx_mask() const;
  int audio_api() const;
  int audio_device_index() const;

private:
  void initialise_device();
  void capture_loop();
  void on_frame_ready();
  void stop_stream();

  void on_save_profile_activate();
  void on_open_images_directory();
  void on_open_videos_directory();
  void refresh_profiles_menu();
  std::string sanitize_profile_name(const std::string &name) const;
  std::string build_profile_path(const std::string &name) const;
  void on_profile_selected(const std::string &name, const std::string &path);
  void on_delete_profile_activate();
  void on_default_profile_activate();
  bool ensure_profile_directory();
  std::string profile_directory() const;
  void open_directory(const std::string &path);

  struct ProfileMenuEntry {
    std::string name;
    std::string path;
    Gtk::MenuItem *item{nullptr};
    sigc::connection handler;
  };
  std::vector<ProfileMenuEntry> profile_entries_;

  Gtk::Box main_container_{Gtk::ORIENTATION_VERTICAL};
  Gtk::MenuBar menu_bar_;
  Gtk::MenuItem profiles_root_item_{"Perfis"};
  Gtk::Menu profiles_menu_;
  Gtk::MenuItem save_profile_item_{"Salvar perfil..."};
  Gtk::MenuItem delete_profile_item_{"Excluir perfil..."};
  Gtk::SeparatorMenuItem profiles_separator_;
  Gtk::MenuItem default_profile_item_{"Default"};
  Gtk::MenuItem directories_root_item_{"Diretórios"};
  Gtk::Menu directories_menu_;
  Gtk::MenuItem images_directory_item_{"Imagens"};
  Gtk::MenuItem videos_directory_item_{"Vídeos"};
  double scaling_factor_{1.0};
  Gtk::Box layout_box_{Gtk::ORIENTATION_HORIZONTAL};
  Gtk::Box content_box_{Gtk::ORIENTATION_VERTICAL};
  Gtk::Image image_widget_;
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
  std::atomic<uint32_t> audio_fx_mask_{AUDIO_FX_NONE};
  std::atomic<uint32_t> render_fx_mask_{REND_FX_YUV_MIRROR};
  std::string current_device_path_;

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
  void stop_capture_thread();
  bool start_streaming();
  void resize_rgb_buffer();
  bool reopen_video_device(const std::string &device_path,
                           const std::function<void(v4l2_dev_t *)> &initializer);

  struct ConfigWindowEntry {
    std::string id;
    Glib::ustring menu_label;
    std::function<std::unique_ptr<ControlsBase>(MainWindow &)> factory;
    std::unique_ptr<ControlsBase> window;
    Gtk::MenuItem *menu_item{nullptr};
    sigc::connection hide_connection;
  };

  std::vector<ConfigWindowEntry> config_windows_;
};
