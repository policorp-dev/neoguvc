#include "MainWindow.hpp"

#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <glibmm/fileutils.h>
#include <glibmm/main.h>
#include <glibmm/miscutils.h>

extern "C" {
#include <glib.h>
#include <glib/gstdio.h>
}

namespace {
constexpr const char *kDefaultDevice = "/dev/video0";
constexpr std::chrono::milliseconds kRetryDelay{10};
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
  sidebar_box_.set_halign(Gtk::ALIGN_FILL);
  sidebar_box_.get_style_context()->add_class("sidebar");

  capture_button_.set_label("Foto");
  capture_button_.set_size_request(60, 50);
  capture_button_.set_margin_left(15);
  capture_button_.set_margin_right(15);
  capture_button_.get_style_context()->add_class("sidebar-button");

  record_button_.set_label("Gravar");
  record_button_.set_size_request(60, 50);
  record_button_.set_margin_left(15);
  record_button_.set_margin_right(15);
  record_button_.get_style_context()->add_class("sidebar-button");

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
  set_record_button_label("Parar");
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
  set_record_button_label("Gravar");
  current_video_path_.clear();
}

void MainWindow::post_status(const std::string &text) {
  Glib::signal_idle().connect_once(
      [this, text]() { status_label_.set_text(text); });
}

void MainWindow::set_record_button_label(const std::string &text) {
  Glib::signal_idle().connect_once(
      [this, text]() { record_button_.set_label(text); });
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
