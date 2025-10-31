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
#include <array>
#include <cmath>

#include <cairomm/context.h>
#include <cairomm/surface.h>
#include <gdk/gdk.h>

#include <glibmm/fileutils.h>
#include <glibmm/main.h>
#include <glibmm/miscutils.h>
#include <sigc++/bind.h>

extern "C" {
#include <glib.h>
#include <glib/gstdio.h>
}

namespace {
constexpr const char *kDefaultDevice = "/dev/video0";
constexpr std::chrono::milliseconds kRetryDelay{10};

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

  dispatcher_.connect(sigc::mem_fun(*this, &MainWindow::on_frame_ready));

  auto css = Gtk::CssProvider::create();
  auto css_path = "/usr/share/neoguvc/style.css";
  css->load_from_path(css_path);
  Gtk::StyleContext::add_provider_for_screen(
      Gdk::Screen::get_default(), css,
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  add(layout_box_);
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
                        []() { return std::make_unique<ImageControls>(); }});
  config_windows_.push_back(
      ConfigWindowEntry{"video_controls", "Controles de vídeo",
                        []() { return std::make_unique<VideoControls>(); }});
  config_windows_.push_back(
      ConfigWindowEntry{"audio_controls", "Controles de áudio",
                        []() { return std::make_unique<AudioControls>(); }});

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
  running_ = false;
  if (capture_thread_.joinable())
    capture_thread_.join();
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
  device_ = v4l2core_init_dev(kDefaultDevice);
  if (!device_) {
    status_label_.set_text("Falha ao abrir " + std::string{kDefaultDevice});
    return;
  }

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

  if (v4l2core_start_stream(device_) != E_OK) {
    status_label_.set_text("Falha ao iniciar captura no dispositivo");
    return;
  }

  rgb_buffer_.resize(static_cast<size_t>(frame_width_) * frame_height_ * 3);
  running_ = true;
  capture_thread_ = std::thread(&MainWindow::capture_loop, this);
  status_label_.set_text("Capturando de " + std::string{kDefaultDevice});
}

void MainWindow::stop_stream() {
  if (device_) {
    v4l2core_stop_stream(device_);
    v4l2core_close_dev(device_);
    device_ = nullptr;
  }
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

    entry.window = entry.factory();
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
  audio_ctx_ = audio_init(AUDIO_PORTAUDIO, -1);
  if (!audio_ctx_)
    return;

  if (audio_get_channels(audio_ctx_) <= 0)
    audio_set_channels(audio_ctx_, 2);
  if (audio_get_samprate(audio_ctx_) <= 0)
    audio_set_samprate(audio_ctx_, 44100);
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
                                    audio_sample_type_, audio_fx_mask_);
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
