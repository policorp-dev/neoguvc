#include "MainWindow.hpp"
#include "AudioControls.hpp"
#include "ImageControls.hpp"
#include "VideoControls.hpp"

#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <array>
#include <cmath>
#include <vector>
#include <utility>

#include <cairomm/context.h>
#include <cairomm/surface.h>
#include <gdk/gdk.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/dialog.h>
#include <gtkmm/entry.h>
#include <gtkmm/settings.h>

#include <glibmm/fileutils.h>
#include <glibmm/main.h>
#include <glibmm/miscutils.h>
#include <glibmm/spawn.h>
#include <sigc++/bind.h>

extern "C" {
#include <glib.h>
#include <glib/gstdio.h>
}

namespace {
constexpr const char *kDefaultDevice = "/dev/video0";
constexpr std::chrono::milliseconds kRetryDelay{10};

constexpr const char *kProfileExtension = ".gpfl";
constexpr const char *kDefaultProfileName = "Default";
constexpr const char *kDefaultProfileFilename = "Default.gpfl";
constexpr const char *kSystemProfileDirectory = "/usr/share/neoguvc";

enum class IconShape { Circle, RoundedSquare };

Glib::RefPtr<Gdk::Pixbuf> create_control_icon(IconShape shape,
                                              const std::array<double, 4> &inner_color,
                                              const std::array<double, 4> &ring_color) {
  constexpr int kSize = 48;
  constexpr double kRingWidth = 4.0;
  constexpr double kGap = 6.0;
  auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, kSize, kSize);
  auto cr = Cairo::Context::create(surface);

  cr->set_source_rgba(0.0, 0.0, 0.0, 0.0);
  cr->paint();

  const double center = kSize / 2.0;
  const double ring_radius = center - 1.0;
  cr->set_line_width(kRingWidth);
  cr->set_source_rgba(ring_color[0], ring_color[1], ring_color[2], ring_color[3]);
  cr->arc(center, center, ring_radius - kRingWidth * 0.5, 0.0, 2.0 * G_PI);
  cr->stroke();

  cr->set_source_rgba(inner_color[0], inner_color[1], inner_color[2], inner_color[3]);

  if (shape == IconShape::Circle) {
    const double inner_radius = std::max(ring_radius - kRingWidth * 0.5 - kGap, 0.0);
    cr->arc(center, center, inner_radius, 0.0, 2.0 * G_PI);
    cr->fill();
  } else {
    const double target_radius = std::max(ring_radius - kRingWidth * 0.5 - kGap, 0.0);
    const double half_inner = target_radius / std::sqrt(2.0);
    const double corner_radius = std::max(half_inner * 0.25, 3.0);

    cr->begin_new_path();
    cr->move_to(center - half_inner + corner_radius, center - half_inner);
    cr->line_to(center + half_inner - corner_radius, center - half_inner);
    cr->arc(center + half_inner - corner_radius, center - half_inner + corner_radius,
            corner_radius, -G_PI_2, 0.0);
    cr->line_to(center + half_inner, center + half_inner - corner_radius);
    cr->arc(center + half_inner - corner_radius, center + half_inner - corner_radius,
            corner_radius, 0.0, G_PI_2);
    cr->line_to(center - half_inner + corner_radius, center + half_inner);
    cr->arc(center - half_inner + corner_radius, center + half_inner - corner_radius,
            corner_radius, G_PI_2, G_PI);
    cr->line_to(center - half_inner, center - half_inner + corner_radius);
    cr->arc(center - half_inner + corner_radius, center - half_inner + corner_radius,
            corner_radius, G_PI, 3.0 * G_PI_2);
    cr->close_path();
    cr->fill();
  }

  return Gdk::Pixbuf::create(surface, 0, 0, kSize, kSize);
}
} // namespace

MainWindow::MainWindow() {
  set_title("neoguvc");
  set_default_size(960, 720);
  set_resizable(false);
  if (auto settings = Gtk::Settings::get_default())
    settings->set_property("gtk-application-prefer-dark-theme", true);

  get_style_context()->add_class("app-window");
  current_device_path_ = kDefaultDevice;

  dispatcher_.connect(sigc::mem_fun(*this, &MainWindow::on_frame_ready));

  auto css = Gtk::CssProvider::create();
  auto css_path = "/usr/share/neoguvc/style.css";
  css->load_from_path(css_path);
  Gtk::StyleContext::add_provider_for_screen(
      Gdk::Screen::get_default(), css,
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  add(main_container_);
  main_container_.set_orientation(Gtk::ORIENTATION_VERTICAL);
  main_container_.set_spacing(0);
  main_container_.set_hexpand(true);
  main_container_.set_vexpand(true);

  menu_bar_.set_hexpand(true);
  menu_bar_.get_style_context()->add_class("app-menu-bar");

  profiles_root_item_.set_submenu(profiles_menu_);
  directories_root_item_.set_submenu(directories_menu_);

  profiles_menu_.append(save_profile_item_);
  profiles_menu_.append(delete_profile_item_);
  profiles_menu_.append(profiles_separator_);
  profiles_menu_.append(default_profile_item_);

  default_profile_item_.set_sensitive(true);

  directories_menu_.append(images_directory_item_);
  directories_menu_.append(videos_directory_item_);

  menu_bar_.append(profiles_root_item_);
  menu_bar_.append(directories_root_item_);

  save_profile_item_.signal_activate().connect(
      sigc::mem_fun(*this, &MainWindow::on_save_profile_activate));
  delete_profile_item_.signal_activate().connect(
      sigc::mem_fun(*this, &MainWindow::on_delete_profile_activate));
  images_directory_item_.signal_activate().connect(
      sigc::mem_fun(*this, &MainWindow::on_open_images_directory));
  videos_directory_item_.signal_activate().connect(
      sigc::mem_fun(*this, &MainWindow::on_open_videos_directory));

  default_profile_item_.signal_activate().connect(
      sigc::mem_fun(*this, &MainWindow::on_default_profile_activate));

  refresh_profiles_menu();

  main_container_.pack_start(menu_bar_, Gtk::PACK_SHRINK);
  main_container_.pack_start(layout_box_, Gtk::PACK_EXPAND_WIDGET);

  layout_box_.pack_start(content_box_, Gtk::PACK_EXPAND_WIDGET);
  layout_box_.pack_start(sidebar_box_, Gtk::PACK_SHRINK);
  layout_box_.get_style_context()->add_class("content-box");

  content_box_.pack_start(image_widget_, Gtk::PACK_EXPAND_WIDGET);
  content_box_.pack_start(status_label_, Gtk::PACK_SHRINK);
  content_box_.get_style_context()->add_class("content-box");

  status_label_.set_margin_top(6);
  status_label_.set_margin_bottom(6);
  status_label_.set_text("Abrindo dispositivo " + std::string{kDefaultDevice} +
                         "...");
  status_label_.get_style_context()->add_class("status-label");

  sidebar_box_.set_orientation(Gtk::ORIENTATION_VERTICAL);
  sidebar_box_.set_spacing(16);
  sidebar_box_.set_valign(Gtk::ALIGN_FILL);
  sidebar_box_.set_halign(Gtk::ALIGN_CENTER);
  sidebar_box_.set_margin_left(15);
  sidebar_box_.set_margin_right(15);
  sidebar_box_.set_hexpand(false);
  sidebar_box_.get_style_context()->add_class("sidebar");

  auto menu_icon = Gtk::manage(new Gtk::Image());
  menu_icon->set_from_icon_name("open-menu-symbolic", Gtk::ICON_SIZE_MENU);
  menu_button_.set_image(*menu_icon);
  menu_icon->show();
  menu_button_.set_tooltip_text("Mais opções");
  menu_button_.set_relief(Gtk::RELIEF_NONE);
  menu_button_.set_focus_on_click(false);
  menu_button_.set_margin_left(0);
  menu_button_.set_margin_right(0);
  menu_button_.set_halign(Gtk::ALIGN_CENTER);
  menu_button_.get_style_context()->add_class("menu-button");
  menu_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &MainWindow::on_menu_button_clicked));

  menu_popup_.get_style_context()->add_class("controls-popup");

  config_windows_.push_back(
      ConfigWindowEntry{"image_controls", "Controles de imagem",
                        [](MainWindow &window) {
                          return std::make_unique<ImageControls>(
                              window.device_handle());
                        }});
  config_windows_.push_back(
      ConfigWindowEntry{"video_controls", "Controles de vídeo",
                        [](MainWindow &window) {
                          return std::make_unique<VideoControls>(window);
                        }});
  config_windows_.push_back(
      ConfigWindowEntry{"audio_controls", "Controles de áudio",
                        [](MainWindow &window) {
                          return std::make_unique<AudioControls>(window);
                        }});

  for (auto &entry : config_windows_) {
    auto *menu_item = Gtk::manage(new Gtk::MenuItem(entry.menu_label));
    entry.menu_item = menu_item;
    menu_item->signal_activate().connect(sigc::bind(
        sigc::mem_fun(*this, &MainWindow::on_config_menu_item_activated),
        entry.id));
    menu_popup_.append(*menu_item);
  }

  menu_popup_.show_all();

  capture_button_.set_margin_left(0);
  capture_button_.set_margin_right(0);
  capture_button_.set_relief(Gtk::RELIEF_NONE);
  capture_button_.set_focus_on_click(false);
  capture_button_.set_always_show_image(true);
  capture_button_.set_tooltip_text("Capturar foto");
  capture_button_.set_halign(Gtk::ALIGN_CENTER);
  capture_button_.get_style_context()->add_class("capture-button");
  auto capture_icon_pixbuf =
      create_control_icon(IconShape::Circle,
                          {1.0, 1.0, 1.0, 1.0},
                          {1.0, 1.0, 1.0, 1.0});
  auto capture_icon = Gtk::manage(new Gtk::Image(capture_icon_pixbuf));
  capture_button_.set_image(*capture_icon);
  capture_icon->show();

  record_button_.set_margin_left(0);
  record_button_.set_margin_right(0);
  record_button_.set_relief(Gtk::RELIEF_NONE);
  record_button_.set_focus_on_click(false);
  record_button_.set_always_show_image(true);
  record_button_.set_tooltip_text("Iniciar/encerrar gravação");
  record_button_.set_halign(Gtk::ALIGN_CENTER);
  record_button_.get_style_context()->add_class("record-button");
  record_icon_idle_ = create_control_icon(IconShape::Circle,
                                         {0.90, 0.0, 0.0, 1.0},
                                         {1.0, 1.0, 1.0, 1.0});
  record_icon_active_ = create_control_icon(IconShape::RoundedSquare,
                                           {0.90, 0.0, 0.0, 1.0},
                                           {1.0, 1.0, 1.0, 1.0});
  record_button_icon_ = Gtk::manage(new Gtk::Image(record_icon_idle_));
  record_button_.set_image(*record_button_icon_);
  record_button_icon_->show();

  sidebar_box_.pack_start(menu_button_, Gtk::PACK_SHRINK, 0);
  sidebar_box_.pack_start(spacer_top_, Gtk::PACK_EXPAND_WIDGET, 0);
  sidebar_box_.pack_start(capture_button_, Gtk::PACK_SHRINK, 0);
  sidebar_box_.pack_start(record_button_, Gtk::PACK_SHRINK, 0);
  sidebar_box_.pack_start(spacer_bottom_, Gtk::PACK_EXPAND_WIDGET, 0);

  capture_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &MainWindow::on_capture_button_clicked));
  record_button_.signal_clicked().connect(
      sigc::mem_fun(*this, &MainWindow::on_record_button_clicked));

  show_all_children();
  initialise_audio();
  initialise_device();
}

MainWindow::~MainWindow() {
  stop_capture_thread();
  stop_recording();
  stop_stream();
  stop_audio_capture();
  if (audio_ctx_) {
    audio_close(audio_ctx_);
    audio_ctx_ = nullptr;
  }
}

void MainWindow::initialise_device() {
  v4l2core_set_verbosity(0);
  std::string path = current_device_path_.empty() ? std::string{kDefaultDevice}
                                                  : current_device_path_;
  device_ = v4l2core_init_dev(path.c_str());
  if (!device_) {
    status_label_.set_text("Falha ao abrir " + path);
    return;
  }
  current_device_path_ = path;

  v4l2core_prepare_valid_format(device_);
  v4l2core_prepare_valid_resolution(device_);
  if (v4l2core_update_current_format(device_) != E_OK) {
    status_label_.set_text(
        "Não foi possível aplicar formato ao dispositivo");
    return;
  }

  frame_width_ = v4l2core_get_frame_width(device_);
  frame_height_ = v4l2core_get_frame_height(device_);
  if (frame_width_ <= 0 || frame_height_ <= 0) {
    status_label_.set_text(
        "Resolução inválida reportada pelo dispositivo");
    return;
  }

  if (!start_streaming()) {
    status_label_.set_text("Falha ao iniciar captura no dispositivo");
    return;
  }
  status_label_.set_text("Capturando de " + current_device_path_);
}

void MainWindow::stop_stream() {
  if (device_) {
    v4l2core_stop_stream(device_);
    v4l2core_close_dev(device_);
    device_ = nullptr;
  }
}

void MainWindow::stop_capture_thread() {
  running_.store(false, std::memory_order_release);
  if (capture_thread_.joinable())
    capture_thread_.join();
  pending_frame_ = false;
}

void MainWindow::resize_rgb_buffer() {
  if (frame_width_ <= 0 || frame_height_ <= 0) {
    rgb_buffer_.clear();
    return;
  }
  rgb_buffer_.resize(static_cast<size_t>(frame_width_) * frame_height_ * 3);
}

bool MainWindow::start_streaming() {
  if (!device_)
    return false;

  if (v4l2core_start_stream(device_) != E_OK)
    return false;

  frame_width_ = v4l2core_get_frame_width(device_);
  frame_height_ = v4l2core_get_frame_height(device_);
  resize_rgb_buffer();

  running_.store(true, std::memory_order_release);
  capture_thread_ = std::thread(&MainWindow::capture_loop, this);
  return true;
}

bool MainWindow::reconfigure_video(
    const std::function<void(v4l2_dev_t *)> &mutator) {
  if (!device_)
    return false;

  const uint32_t current_format =
      v4l2core_get_requested_frame_format(device_);
  const int current_width = frame_width_;
  const int current_height = frame_height_;
  const int current_fps_num = v4l2core_get_fps_num(device_);
  const int current_fps_den = v4l2core_get_fps_denom(device_);

  auto initializer = [&](v4l2_dev_t *vd) {
    if (current_format)
      v4l2core_prepare_new_format(vd, current_format);
    else
      v4l2core_prepare_valid_format(vd);

    if (current_width > 0 && current_height > 0)
      v4l2core_prepare_new_resolution(vd, current_width, current_height);
    else
      v4l2core_prepare_valid_resolution(vd);

    if (current_fps_num > 0 && current_fps_den > 0)
      v4l2core_define_fps(vd, current_fps_num, current_fps_den);

    if (mutator)
      mutator(vd);
  };

  return reopen_video_device(current_device_path_, initializer);
}

bool MainWindow::switch_device(const std::string &device_path) {
  std::string path =
      device_path.empty() ? std::string{kDefaultDevice} : device_path;
  if (reopen_video_device(path, nullptr)) {
    current_device_path_ = path;
    return true;
  }
  return false;
}

void MainWindow::set_render_fx_mask(uint32_t mask) {
  render_fx_mask_.store(mask, std::memory_order_release);
}

uint32_t MainWindow::render_fx_mask() const {
  return render_fx_mask_.load(std::memory_order_acquire);
}

bool MainWindow::reopen_video_device(
    const std::string &device_path,
    const std::function<void(v4l2_dev_t *)> &initializer) {
  stop_capture_thread();

  if (device_) {
    v4l2core_stop_stream(device_);
    v4l2core_close_dev(device_);
    device_ = nullptr;
  }

  v4l2_dev_t *new_device = v4l2core_init_dev(device_path.c_str());
  if (!new_device) {
    post_status("Falha ao abrir " + device_path);
    return false;
  }

  if (initializer)
    initializer(new_device);
  else {
    v4l2core_prepare_valid_format(new_device);
    v4l2core_prepare_valid_resolution(new_device);
  }

  if (v4l2core_update_current_format(new_device) != E_OK) {
    post_status("Não foi possível aplicar formato ao dispositivo");
    v4l2core_close_dev(new_device);
    return false;
  }

  device_ = new_device;

  if (!start_streaming()) {
    post_status("Falha ao iniciar captura no dispositivo");
    v4l2core_close_dev(device_);
    device_ = nullptr;
    return false;
  }

  current_device_path_ = device_path;
  post_status("Capturando de " + current_device_path_);
  return true;
}

void MainWindow::capture_loop() {
  while (running_) {
    if (!device_) {
      std::this_thread::sleep_for(kRetryDelay);
      continue;
    }

    v4l2_frame_buff_t *frame = v4l2core_get_decoded_frame(device_);
    if (!frame) {
      std::this_thread::sleep_for(kRetryDelay);
      continue;
    }

    const uint32_t fx_mask = render_fx_mask_.load(std::memory_order_relaxed);
    if (fx_mask != REND_FX_YUV_NOFILT)
      render_fx_apply(frame->yuv_frame, frame_width_, frame_height_, fx_mask);

    {
      std::lock_guard<std::mutex> guard(frame_mutex_);
      yu12_to_rgb24(rgb_buffer_.data(), frame->yuv_frame, frame_width_,
                    frame_height_);
      pending_frame_ = true;
    }

    if (snapshot_request_.exchange(false)) {
      save_snapshot(frame);
    }

    if (start_record_request_.exchange(false)) {
      start_recording(frame);
    }

    if (stop_record_request_.exchange(false)) {
      stop_recording();
    }

    if (recording_.load(std::memory_order_acquire)) {
      handle_recording_frame(frame);
    }

    v4l2core_release_frame(device_, frame);
    dispatcher_();
  }
}

void MainWindow::on_menu_button_clicked() {
  auto *widget = dynamic_cast<Gtk::Widget *>(&menu_button_);
  if (!widget)
    return;

  menu_popup_.popup_at_widget(widget, Gdk::GRAVITY_SOUTH,
                              Gdk::GRAVITY_NORTH, nullptr);
}

void MainWindow::on_config_menu_item_activated(const std::string &id) {
  auto it = std::find_if(config_windows_.begin(), config_windows_.end(),
                         [&id](const ConfigWindowEntry &entry) {
                           return entry.id == id;
                         });
  if (it == config_windows_.end())
    return;

  auto &entry = *it;
  if (!entry.window) {
    if (!entry.factory)
      return;

    entry.window = entry.factory(*this);
    if (!entry.window)
      return;

    entry.window->set_transient_for(*this);
    entry.window->set_position(Gtk::WIN_POS_CENTER_ON_PARENT);
    entry.hide_connection = entry.window->signal_hide().connect(
        sigc::bind(sigc::mem_fun(*this, &MainWindow::on_config_window_hidden),
                   entry.id));
  }

  entry.window->present();
}

void MainWindow::on_config_window_hidden(const std::string &id) {
  auto it = std::find_if(config_windows_.begin(), config_windows_.end(),
                         [&id](const ConfigWindowEntry &entry) {
                           return entry.id == id;
                         });
  if (it == config_windows_.end())
    return;

  auto &entry = *it;
  if (entry.hide_connection.connected())
    entry.hide_connection.disconnect();
  entry.window.reset();
}

void MainWindow::on_frame_ready() {
  std::vector<uint8_t> local_copy;
  {
    std::lock_guard<std::mutex> guard(frame_mutex_);
    if (!pending_frame_)
      return;
    local_copy = rgb_buffer_;
    pending_frame_ = false;
  }

  auto pixbuf =
      Gdk::Pixbuf::create(Gdk::COLORSPACE_RGB, false, 8, frame_width_,
                          frame_height_);
  auto *pixels = pixbuf->get_pixels();
  const int rowstride = pixbuf->get_rowstride();

  const size_t src_row_bytes = static_cast<size_t>(frame_width_) * 3;
  for (int y = 0; y < frame_height_; ++y) {
    std::memcpy(pixels + y * rowstride,
                local_copy.data() + y * src_row_bytes, src_row_bytes);
  }

  image_widget_.set(pixbuf);
}

void MainWindow::on_save_profile_activate() {
  Gtk::Dialog dialog("Salvar perfil", *this, true);
  dialog.set_transient_for(*this);
  dialog.set_modal(true);
  dialog.add_button("_Cancelar", Gtk::RESPONSE_CANCEL);
  dialog.add_button("_Salvar", Gtk::RESPONSE_OK);
  dialog.set_default_response(Gtk::RESPONSE_OK);
  dialog.set_resizable(false);

  auto *content = dialog.get_content_area();
  content->set_spacing(8);
  content->set_border_width(12);

  auto *label = Gtk::manage(new Gtk::Label("Nome do perfil:"));
  label->set_halign(Gtk::ALIGN_START);
  label->set_margin_bottom(4);

  Gtk::Entry name_entry;
  name_entry.set_width_chars(24);
  name_entry.set_activates_default(true);
  name_entry.set_text(kDefaultProfileName);

  content->pack_start(*label, Gtk::PACK_SHRINK);
  content->pack_start(name_entry, Gtk::PACK_SHRINK);

  label->show();
  name_entry.show();

  const int response = dialog.run();
  if (response != Gtk::RESPONSE_OK)
    return;

  Glib::ustring profile_input = name_entry.get_text();
  if (profile_input.empty())
    profile_input = kDefaultProfileName;

  const std::string display_name = sanitize_profile_name(profile_input.raw());
  const std::string profile_path = build_profile_path(profile_input.raw());

  if (!ensure_profile_directory())
    return;

  if (!device_) {
    post_status("Nenhum dispositivo disponível para salvar o perfil.");
    return;
  }

  const int result = v4l2core_save_control_profile(device_, profile_path.c_str());
  if (result == E_OK) {
    refresh_profiles_menu();
    post_status(Glib::ustring::compose("Perfil \"%1\" salvo em %2", display_name,
                                       profile_path)
                    .raw());
  } else {
    post_status("Falha ao salvar perfil em " + profile_path);
  }
}

void MainWindow::on_open_images_directory() {
  const char *pictures_dir = g_get_user_special_dir(G_USER_DIRECTORY_PICTURES);
  std::string path;
  if (pictures_dir && *pictures_dir)
    path = pictures_dir;
  else if (const char *home = g_get_home_dir())
    path = std::string(home) + "/Imagens";
  else
    path = "Imagens";

  if (!path.empty() && !Glib::file_test(path, Glib::FILE_TEST_IS_DIR))
    g_mkdir_with_parents(path.c_str(), 0755);

  open_directory(path);
}

void MainWindow::on_open_videos_directory() {
  const char *videos_dir = g_get_user_special_dir(G_USER_DIRECTORY_VIDEOS);
  std::string path;
  if (videos_dir && *videos_dir)
    path = videos_dir;
  else if (const char *home = g_get_home_dir())
    path = std::string(home) + "/Vídeos";
  else
    path = "Vídeos";

  if (!path.empty() && !Glib::file_test(path, Glib::FILE_TEST_IS_DIR))
    g_mkdir_with_parents(path.c_str(), 0755);

  open_directory(path);
}

std::string MainWindow::sanitize_profile_name(const std::string &name) const {
  std::string sanitized;
  sanitized.reserve(name.size());

  for (char ch : name) {
    unsigned char uch = static_cast<unsigned char>(ch);
    if (std::isalnum(uch)) {
      sanitized.push_back(static_cast<char>(uch));
    } else if (ch == '-' || ch == '_') {
      if (sanitized.empty() || sanitized.back() != ch)
        sanitized.push_back(ch);
    } else if (std::isspace(uch)) {
      if (!sanitized.empty() && sanitized.back() != '_')
        sanitized.push_back('_');
    }
  }

  while (!sanitized.empty() && (sanitized.front() == '_' || sanitized.front() == '-'))
    sanitized.erase(sanitized.begin());
  while (!sanitized.empty() && (sanitized.back() == '_' || sanitized.back() == '-'))
    sanitized.pop_back();

  if (sanitized.empty())
    sanitized = "perfil";

  return sanitized;
}

std::string MainWindow::build_profile_path(const std::string &name) const {
  std::string sanitized = sanitize_profile_name(name);
  const std::string filename = sanitized + kProfileExtension;
  const std::string dir = profile_directory();
  return dir + "/" + filename;
}

bool MainWindow::ensure_profile_directory() {
  const std::string dir = profile_directory();
  if (g_mkdir_with_parents(dir.c_str(), 0755) != 0) {
    post_status("Falha ao preparar diretório de perfis: " +
                std::string(std::strerror(errno)));
    return false;
  }
  return true;
}

std::string MainWindow::profile_directory() const {
  if (const char *data_dir = g_get_user_data_dir()) {
    if (*data_dir)
      return std::string(data_dir) + "/neoguvc";
  }
  if (const char *home = g_get_home_dir())
    return std::string(home) + "/.local/share/neoguvc";
  return "neoguvc_profiles";
}

void MainWindow::refresh_profiles_menu() {
  for (auto &entry : profile_entries_) {
    if (entry.handler.connected())
      entry.handler.disconnect();
    if (entry.item) {
      profiles_menu_.remove(*entry.item);
      delete entry.item;
      entry.item = nullptr;
    }
  }
  profile_entries_.clear();

  const std::string dir_path = profile_directory();
  const std::string default_profile_path = dir_path + "/" + kDefaultProfileFilename;
  const bool default_profile_exists =
      Glib::file_test(default_profile_path, Glib::FILE_TEST_IS_REGULAR);
  default_profile_item_.set_sensitive(true);
  if (default_profile_exists)
    default_profile_item_.set_tooltip_text("Carregar perfil salvo \"Default\"");
  else
    default_profile_item_.set_tooltip_text("Restaurar valores padrão");

  if (!Glib::file_test(dir_path, Glib::FILE_TEST_IS_DIR))
    return;

  std::vector<std::pair<std::string, std::string>> profiles;
  const std::size_t extension_length = std::strlen(kProfileExtension);

  GError *error = nullptr;
  GDir *dir = g_dir_open(dir_path.c_str(), 0, &error);
  if (!dir) {
    if (error) {
      post_status("Falha ao listar perfis: " +
                  std::string(error->message ? error->message : "erro desconhecido"));
      g_error_free(error);
    }
    return;
  }

  const char *entry_name = nullptr;
  while ((entry_name = g_dir_read_name(dir))) {
    std::string filename{entry_name};
    if (filename.empty() || filename[0] == '.')
      continue;
    if (filename.size() <= extension_length)
      continue;
    if (filename.compare(filename.size() - extension_length, extension_length,
                         kProfileExtension) != 0)
      continue;

    std::string base = filename.substr(0, filename.size() - extension_length);
    if (base == kDefaultProfileName)
      continue;

    std::string full_path = dir_path + "/" + filename;
    profiles.emplace_back(base, full_path);
  }

  g_dir_close(dir);

  std::sort(profiles.begin(), profiles.end(),
            [](const auto &lhs, const auto &rhs) {
              return Glib::ustring(lhs.first).lowercase() <
                     Glib::ustring(rhs.first).lowercase();
            });

  for (const auto &profile : profiles) {
    auto *item = new Gtk::MenuItem(profile.first);
    auto handler = item->signal_activate().connect(
        sigc::bind(sigc::mem_fun(*this, &MainWindow::on_profile_selected),
                   profile.first, profile.second));
    profiles_menu_.append(*item);
    item->show();

    ProfileMenuEntry entry;
    entry.name = profile.first;
    entry.path = profile.second;
    entry.item = item;
    entry.handler = handler;
    profile_entries_.push_back(entry);
  }

  profiles_menu_.show_all();
}

void MainWindow::on_profile_selected(const std::string &name,
                                     const std::string &path) {
  if (!device_) {
    post_status("Nenhum dispositivo disponível para carregar perfil.");
    return;
  }

  if (!Glib::file_test(path, Glib::FILE_TEST_IS_REGULAR)) {
    post_status(Glib::ustring::compose("Perfil \"%1\" não encontrado.", name).raw());
    return;
  }

  const int result = v4l2core_load_control_profile(device_, path.c_str());
  if (result == E_OK) {
    post_status(Glib::ustring::compose("Perfil \"%1\" carregado.", name).raw());
  } else {
    post_status(
        Glib::ustring::compose("Falha ao carregar perfil \"%1\".", name).raw());
  }
}

void MainWindow::on_delete_profile_activate() {
  refresh_profiles_menu();

  if (profile_entries_.empty()) {
    post_status("Nenhum perfil salvo para excluir.");
    return;
  }

  Gtk::Dialog dialog("Excluir perfil", *this, true);
  dialog.add_button("_Cancelar", Gtk::RESPONSE_CANCEL);
  dialog.add_button("_Excluir", Gtk::RESPONSE_OK);
  dialog.set_default_response(Gtk::RESPONSE_OK);
  dialog.set_resizable(false);
  dialog.set_transient_for(*this);

  auto *content = dialog.get_content_area();
  content->set_spacing(8);
  content->set_border_width(12);

  auto *label = Gtk::manage(
      new Gtk::Label("Selecione o perfil que deseja excluir:"));
  label->set_halign(Gtk::ALIGN_START);
  label->set_margin_bottom(4);
  content->pack_start(*label, Gtk::PACK_SHRINK);

  auto *combo = Gtk::manage(new Gtk::ComboBoxText());
  for (const auto &entry : profile_entries_) {
    combo->append(entry.path, entry.name);
  }
  combo->set_active(0);
  combo->set_hexpand(true);
  content->pack_start(*combo, Gtk::PACK_SHRINK);

  label->show();
  combo->show();

  const int response = dialog.run();
  if (response != Gtk::RESPONSE_OK)
    return;

  const Glib::ustring selected_path = combo->get_active_id();
  const Glib::ustring selected_name = combo->get_active_text();
  if (selected_path.empty()) {
    post_status("Nenhum perfil selecionado para exclusão.");
    return;
  }

  if (::g_remove(selected_path.c_str()) != 0) {
    post_status(Glib::ustring::compose("Falha ao excluir perfil \"%1\": %2",
                                       selected_name, std::strerror(errno))
                    .raw());
  } else {
    post_status(
        Glib::ustring::compose("Perfil \"%1\" excluído.", selected_name).raw());
  }

  refresh_profiles_menu();
}

void MainWindow::on_default_profile_activate() {
  if (!device_) {
    post_status("Nenhum dispositivo disponível para carregar perfil.");
    return;
  }

  const std::string user_default = profile_directory() + "/" + kDefaultProfileFilename;
  const std::string system_default =
      std::string(kSystemProfileDirectory) + "/" + kDefaultProfileFilename;

  std::vector<std::string> candidates;
  if (Glib::file_test(user_default, Glib::FILE_TEST_IS_REGULAR))
    candidates.push_back(user_default);
  if (Glib::file_test(system_default, Glib::FILE_TEST_IS_REGULAR))
    candidates.push_back(system_default);

  for (const auto &candidate : candidates) {
    const int result =
        v4l2core_load_control_profile(device_, candidate.c_str());
    if (result == E_OK) {
      post_status(
          Glib::ustring::compose("Perfil \"Default\" carregado de %1", candidate)
              .raw());
      return;
    }
  }

  v4l2core_set_control_defaults(device_);
  post_status("Perfil \"Default\" carregado (valores padrão do dispositivo).");
}

void MainWindow::open_directory(const std::string &path) {
  if (path.empty())
    return;

  try {
    std::vector<Glib::ustring> argv = {"xdg-open", path};
    Glib::spawn_async("", argv, Glib::SPAWN_SEARCH_PATH);
  } catch (const Glib::Error &error) {
    post_status("Não foi possível abrir diretório: " + std::string(error.what()));
  }
}

void MainWindow::on_capture_button_clicked() { snapshot_request_ = true; }

void MainWindow::on_record_button_clicked() {
  if (recording_.load(std::memory_order_acquire)) {
    stop_record_request_ = true;
  } else {
    start_record_request_ = true;
  }
}

std::string MainWindow::timestamp_string() const {
  auto now = std::chrono::system_clock::now();
  std::time_t tt = std::chrono::system_clock::to_time_t(now);
  std::tm tm;
  localtime_r(&tt, &tm);
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y%m%d-%H%M%S");
  return oss.str();
}

std::string MainWindow::build_output_path(bool video) const {
  const char *special = g_get_user_special_dir(
      video ? G_USER_DIRECTORY_VIDEOS : G_USER_DIRECTORY_PICTURES);
  std::string base;
  if (special && *special)
    base = special;
  else if (const char *home = g_get_home_dir())
    base = std::string(home) + (video ? "/Videos" : "/Pictures");
  else
    base = ".";

  std::string filename =
      std::string("guvcview_") + timestamp_string() +
      (video ? ".mkv" : ".jpg");
  if (!base.empty())
    g_mkdir_with_parents(base.c_str(), 0755);

  if (!base.empty() && base.back() == '/')
    return base + filename;
  return base + "/" + filename;
}

void MainWindow::save_snapshot(v4l2_frame_buff_t *frame) {
  if (!frame)
    return;

  std::string path = build_output_path(false);
  if (v4l2core_save_image(frame, path.c_str(), IMG_FMT_JPG) == E_OK)
    post_status("Foto salva em " + path);
  else
    post_status("Falha ao salvar foto");
}

bool MainWindow::start_recording(v4l2_frame_buff_t *frame) {
  if (recording_.load(std::memory_order_acquire) || !device_ || !frame)
    return false;

  int fps_num = v4l2core_get_fps_num(device_);
  int fps_den = v4l2core_get_fps_denom(device_);
  if (fps_num <= 0 || fps_den <= 0) {
    fps_num = 30;
    fps_den = 1;
  }

  int audio_channels = 0;
  int audio_samprate = 0;
  if (audio_ctx_ && audio_get_api(audio_ctx_) != AUDIO_NONE) {
    audio_channels = audio_get_channels(audio_ctx_);
    if (audio_channels <= 0) {
      audio_channels = 2;
      audio_set_channels(audio_ctx_, audio_channels);
    }
    audio_samprate = audio_get_samprate(audio_ctx_);
    if (audio_samprate <= 0) {
      audio_samprate = 44100;
      audio_set_samprate(audio_ctx_, audio_samprate);
    }
  }

  {
    std::lock_guard<std::mutex> lock(encoder_mutex_);
    encoder_ctx_ =
        encoder_init(v4l2core_get_requested_frame_format(device_), 0, 0,
                     ENCODER_MUX_MKV, frame_width_, frame_height_, fps_num,
                     fps_den, audio_channels, audio_samprate);
    if (!encoder_ctx_) {
      post_status("Falha ao iniciar encoder");
      return false;
    }

    current_video_path_ = build_output_path(true);
    encoder_muxer_init(encoder_ctx_, current_video_path_.c_str());
  }
  recording_.store(true, std::memory_order_release);
  Glib::signal_idle().connect_once([this]() {
    if (record_button_icon_ && record_icon_active_)
      record_button_icon_->set(record_icon_active_);
  });
  post_status("Gravando em " + current_video_path_);
  if (audio_channels > 0) {
    std::lock_guard<std::mutex> lock(encoder_mutex_);
    if (encoder_ctx_ && encoder_ctx_->enc_audio_ctx)
      start_audio_capture(encoder_get_audio_frame_size(encoder_ctx_));
  }
  return true;
}

void MainWindow::handle_recording_frame(v4l2_frame_buff_t *frame) {
  std::lock_guard<std::mutex> lock(encoder_mutex_);
  if (!encoder_ctx_ || !frame)
    return;

  int size = (frame->width * frame->height * 3) / 2;
  uint8_t *input_frame = frame->yuv_frame;

  if (encoder_ctx_->video_codec_ind == 0) {
    switch (v4l2core_get_requested_frame_format(device_)) {
    case V4L2_PIX_FMT_H264:
      input_frame = frame->h264_frame;
      size = static_cast<int>(frame->h264_frame_size);
      break;
    default:
      input_frame = frame->raw_frame;
      size = static_cast<int>(frame->raw_frame_size);
      break;
    }
  }

  encoder_add_video_frame(input_frame, size, frame->timestamp,
                          frame->isKeyframe);
  encoder_process_next_video_buffer(encoder_ctx_);
}

void MainWindow::stop_recording() {
  if (!recording_.load(std::memory_order_acquire))
    return;

  recording_.store(false, std::memory_order_release);
  Glib::signal_idle().connect_once([this]() {
    if (record_button_icon_ && record_icon_idle_)
      record_button_icon_->set(record_icon_idle_);
  });
  stop_audio_capture();
  {
    std::lock_guard<std::mutex> lock(encoder_mutex_);
    if (encoder_ctx_) {
      encoder_flush_video_buffer(encoder_ctx_);
      if (encoder_ctx_->audio_channels > 0 && encoder_ctx_->enc_audio_ctx)
        encoder_flush_audio_buffer(encoder_ctx_);
      encoder_muxer_close(encoder_ctx_);
      encoder_close(encoder_ctx_);
      encoder_ctx_ = nullptr;
    }
  }
  if (!current_video_path_.empty())
    post_status("Vídeo salvo em " + current_video_path_);
  else
    post_status("Gravação finalizada");
  current_video_path_.clear();
}

void MainWindow::post_status(const std::string &text) {
  Glib::signal_idle().connect_once(
      [this, text]() { status_label_.set_text(text); });
}

void MainWindow::initialise_audio() {
  audio_set_verbosity(0);
  recreate_audio_context(AUDIO_PORTAUDIO);
}

bool MainWindow::recreate_audio_context(int api) {
  const bool was_running =
      audio_thread_running_.load(std::memory_order_acquire);
  if (was_running)
    stop_audio_capture();

  if (audio_ctx_) {
    audio_close(audio_ctx_);
    audio_ctx_ = nullptr;
  }

  if (api == AUDIO_NONE) {
    audio_buffer_ = nullptr;
    return true;
  }

  audio_ctx_ = audio_init(api, -1);
  if (!audio_ctx_) {
    audio_buffer_ = nullptr;
    return false;
  }

  if (audio_get_channels(audio_ctx_) <= 0)
    audio_set_channels(audio_ctx_, 2);
  if (audio_get_samprate(audio_ctx_) <= 0)
    audio_set_samprate(audio_ctx_, 44100);
  audio_buffer_ = nullptr;
  if (was_running && encoder_ctx_)
    start_audio_capture(encoder_get_audio_frame_size(encoder_ctx_));

  return true;
}

bool MainWindow::set_audio_device(int index) {
  if (!audio_ctx_)
    return false;

  const int num_devices = audio_get_num_inp_devices(audio_ctx_);
  if (num_devices <= 0)
    return false;

  if (index < 0)
    index = 0;
  if (index >= num_devices)
    index = num_devices - 1;

  const bool was_running =
      audio_thread_running_.load(std::memory_order_acquire);
  if (was_running)
    stop_audio_capture();

  audio_set_device_index(audio_ctx_, index);
  if (was_running && encoder_ctx_)
    start_audio_capture(encoder_get_audio_frame_size(encoder_ctx_));
  return true;
}

void MainWindow::set_audio_samplerate(int samplerate) {
  if (!audio_ctx_)
    return;

  if (samplerate <= 0) {
    const int device_index = audio_get_device_index(audio_ctx_);
    if (device_index >= 0 &&
        device_index < audio_get_num_inp_devices(audio_ctx_))
      samplerate = audio_get_device(audio_ctx_, device_index)->samprate;
  }

  if (samplerate > 0)
    audio_set_samprate(audio_ctx_, samplerate);
}

void MainWindow::set_audio_channels(int channels) {
  if (!audio_ctx_)
    return;

  if (channels <= 0) {
    const int device_index = audio_get_device_index(audio_ctx_);
    if (device_index >= 0 &&
        device_index < audio_get_num_inp_devices(audio_ctx_))
      channels = audio_get_device(audio_ctx_, device_index)->channels;
  }

  if (channels <= 0)
    return;

  const int device_index = audio_get_device_index(audio_ctx_);
  if (device_index >= 0 &&
      device_index < audio_get_num_inp_devices(audio_ctx_)) {
    int max_channels = audio_get_device(audio_ctx_, device_index)->channels;
    if (max_channels > 0 && channels > max_channels)
      channels = max_channels;
  }

  if (channels > 2)
    channels = 2;

  audio_set_channels(audio_ctx_, channels);
}

void MainWindow::set_audio_latency(double latency) {
  if (!audio_ctx_ || latency < 0.0)
    return;
  audio_set_latency(audio_ctx_, latency);
}

void MainWindow::set_audio_fx_mask(uint32_t mask) {
  audio_fx_mask_.store(mask, std::memory_order_release);
}

uint32_t MainWindow::audio_fx_mask() const {
  return audio_fx_mask_.load(std::memory_order_acquire);
}

int MainWindow::audio_api() const {
  return audio_ctx_ ? audio_get_api(audio_ctx_) : AUDIO_NONE;
}

int MainWindow::audio_device_index() const {
  return audio_ctx_ ? audio_get_device_index(audio_ctx_) : -1;
}

void MainWindow::start_audio_capture(int frame_size) {
  if (!audio_ctx_ || audio_get_api(audio_ctx_) == AUDIO_NONE ||
      frame_size <= 0)
    return;

  int channels = audio_get_channels(audio_ctx_);
  if (channels <= 0)
    channels = 2;

  audio_set_cap_buffer_size(audio_ctx_, frame_size * channels);
  if (audio_start(audio_ctx_) != 0)
    return;

  audio_buffer_ = audio_get_buffer(audio_ctx_);
  if (!audio_buffer_) {
    audio_stop(audio_ctx_);
    return;
  }

  audio_sample_type_ = encoder_get_audio_sample_fmt(encoder_ctx_);
  audio_thread_running_.store(true, std::memory_order_release);
  audio_thread_ = std::thread(&MainWindow::audio_capture_loop, this);
}

void MainWindow::stop_audio_capture() {
  audio_thread_running_.store(false, std::memory_order_release);
  if (audio_thread_.joinable())
    audio_thread_.join();

  if (audio_ctx_)
    audio_stop(audio_ctx_);
  audio_buffer_ = nullptr;
}

void MainWindow::audio_capture_loop() {
  while (audio_thread_running_) {
    if (!audio_ctx_ || !audio_buffer_) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      continue;
    }

    int ret = audio_get_next_buffer(audio_ctx_, audio_buffer_,
                                    audio_sample_type_,
                                    audio_fx_mask_.load(std::memory_order_acquire));
    if (ret > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      continue;
    }
    if (ret < 0)
      continue;

    {
      std::lock_guard<std::mutex> lock(encoder_mutex_);
      if (encoder_ctx_ && encoder_ctx_->enc_audio_ctx)
        encoder_ctx_->enc_audio_ctx->pts = audio_buffer_->timestamp;

      if (encoder_ctx_)
        encoder_process_audio_buffer(encoder_ctx_, audio_buffer_->data);
    }
  }
}
