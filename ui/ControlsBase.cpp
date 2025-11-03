#include "ControlsBase.hpp"

#include <algorithm>

#include <sigc++/functors/mem_fun.h>

namespace {
double resolve_step(double step) { return step == 0.0 ? 1.0 : step; }
} // namespace

ControlsBase::ControlsBase(const ConstructionOptions &options)
    : options_(options), close_button_(options.close_button_label) {
  if (options_.header_title.empty())
    options_.header_title = options_.title;

  set_title(options_.title);
  if (options_.apply_size_request)
    set_size_request(options_.width, options_.height);
  set_resizable(options_.resizable);
  get_style_context()->remove_class("controls-window-root");
  apply_style_classes(*this, options_.window_style_classes);

  header_bar_.set_show_close_button(true);
  header_bar_.set_title(options_.header_title);
  header_bar_.get_style_context()->add_class("controls-header");
  set_titlebar(header_bar_);

  add(root_box_);
  root_box_.set_spacing(options_.root_spacing);
  root_box_.set_margin_top(options_.root_margin_top);
  root_box_.set_margin_bottom(options_.root_margin_bottom);
  root_box_.set_margin_left(options_.root_margin_left);
  root_box_.set_margin_right(options_.root_margin_right);
  apply_style_classes(root_box_, options_.root_box_style_classes);

  scroll_.set_policy(options_.h_scroll_policy, options_.v_scroll_policy);
  scroll_.set_hexpand(true);
  scroll_.set_vexpand(true);
  scroll_.add(controls_container_);
  apply_style_classes(scroll_, options_.scroll_style_classes);

  root_box_.pack_start(scroll_, Gtk::PACK_EXPAND_WIDGET);

  controls_container_.set_spacing(options_.container_spacing);
  controls_container_.set_margin_bottom(options_.container_margin_bottom);
  controls_container_.set_margin_right(options_.container_margin_right);
  apply_style_classes(controls_container_, options_.container_style_classes);

  if (options_.show_close_button || options_.show_reset_button) {
    footer_box_.set_spacing(6);
    footer_box_.set_margin_top(8);
    footer_box_.set_hexpand(true);
    footer_box_.set_halign(Gtk::ALIGN_FILL);
    root_box_.pack_start(footer_box_, Gtk::PACK_SHRINK);
  }

  if (options_.show_reset_button) {
    reset_button_.set_label(options_.reset_button_label);
    apply_style_classes(reset_button_, options_.reset_button_style_classes);
    reset_button_.set_halign(Gtk::ALIGN_START);
    footer_box_.pack_start(reset_button_, Gtk::PACK_SHRINK);
  }

  if (options_.show_close_button) {
    apply_style_classes(close_button_, options_.close_button_style_classes);
    close_button_.set_halign(Gtk::ALIGN_END);
    close_button_.signal_clicked().connect(
        sigc::mem_fun(*this, &ControlsBase::on_close_clicked));
    footer_box_.pack_end(close_button_, Gtk::PACK_SHRINK);
  }
}

Gtk::Box &ControlsBase::root_box() { return root_box_; }

Gtk::Box &ControlsBase::controls_container() {
  return controls_container_;
}

Gtk::ScrolledWindow &ControlsBase::scrolled_window() { return scroll_; }

Gtk::HeaderBar &ControlsBase::header_bar() { return header_bar_; }

Gtk::Button &ControlsBase::close_button() { return close_button_; }

Gtk::Button &ControlsBase::reset_button() { return reset_button_; }

Gtk::Box &ControlsBase::footer_box() { return footer_box_; }

bool ControlsBase::has_reset_button() const { return options_.show_reset_button; }

Gtk::Box &ControlsBase::body_container() { return controls_container_; }

void ControlsBase::add_row(Gtk::Widget &widget, Gtk::PackOptions options,
                                 guint padding) {
  controls_container_.pack_start(widget, options, padding);
}

Gtk::Box *ControlsBase::create_slider_row(const Glib::ustring &label,
                                                double initial, double min,
                                                double max) {
  return create_slider_row(label, initial, min, max, SliderRowConfig());
}

Gtk::Box *ControlsBase::create_slider_row(const Glib::ustring &label,
                                                double initial, double min,
                                                double max,
                                                SliderRowConfig config) {
  auto row =
      Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, config.row_spacing));
  row->set_hexpand(config.row_hexpand);
  apply_style_classes(*row, config.row_style_classes);

  auto title = Gtk::manage(new Gtk::Label(label));
  title->set_halign(config.label_halign);
  title->set_valign(config.label_valign);
  title->set_margin_left(config.label_margin_left);
  title->set_margin_right(config.label_margin_right);
  title->set_width_chars(config.label_width_chars);
  title->set_xalign(config.label_xalign);
  apply_style_classes(*title, config.label_style_classes);
  row->pack_start(*title, Gtk::PACK_SHRINK);

  const double step_increment = resolve_step(config.step);
  const double page_increment =
      config.page_increment > 0.0 ? config.page_increment
                                  : step_increment * 5.0;
  const double page_size = std::max(config.page_size, 0.0);

  auto adjustment =
      Gtk::Adjustment::create(initial, min, max, step_increment, page_increment,
                              page_size);
  adjustments_.push_back(adjustment);

  auto scale = Gtk::manage(new Gtk::Scale(adjustment));
  scale->set_hexpand(config.scale_hexpand);
  scale->set_digits(config.digits);
  scale->set_draw_value(config.draw_value);
  scale->set_value_pos(config.value_position);
  scale->set_margin_left(config.scale_margin_left);
  scale->set_margin_right(config.scale_margin_right);
  scale->set_sensitive(config.sensitive);
  apply_style_classes(*scale, config.scale_style_classes);

  row->set_sensitive(config.sensitive);
  row->pack_start(
      *scale, config.scale_hexpand ? Gtk::PACK_EXPAND_WIDGET : Gtk::PACK_SHRINK);

  if (config.on_configure)
    config.on_configure(*scale, adjustment);

  return row;
}

Gtk::Box *ControlsBase::create_check_row(const Glib::ustring &label,
                                               bool active) {
  return create_check_row(label, active, CheckRowConfig());
}

Gtk::Box *ControlsBase::create_check_row(const Glib::ustring &label,
                                               bool active,
                                               CheckRowConfig config) {
  auto row =
      Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, config.row_spacing));
  row->set_hexpand(config.hexpand);
  apply_style_classes(*row, config.row_style_classes);

  auto checkbox = Gtk::manage(new Gtk::CheckButton(label));
  checkbox->set_active(active);
  checkbox->set_halign(Gtk::ALIGN_START);
  apply_style_classes(*checkbox, config.toggle_style_classes);
  row->pack_start(*checkbox, Gtk::PACK_SHRINK);

  if (config.on_configure)
    config.on_configure(*checkbox);

  return row;
}

Gtk::Box *ControlsBase::create_combo_row(
    const Glib::ustring &label, const std::vector<Glib::ustring> &options) {
  return create_combo_row(label, options, ComboRowConfig());
}

Gtk::Box *ControlsBase::create_combo_row(
    const Glib::ustring &label, const std::vector<Glib::ustring> &options,
    ComboRowConfig config) {
  auto row =
      Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, config.row_spacing));
  row->set_hexpand(config.hexpand);
  apply_style_classes(*row, config.row_style_classes);

  auto title = Gtk::manage(new Gtk::Label(label));
  title->set_width_chars(config.label_width_chars);
  title->set_halign(config.label_halign);
  title->set_valign(config.label_valign);
  title->set_xalign(config.label_xalign);
  title->set_margin_left(config.label_margin_left);
  title->set_margin_right(config.label_margin_right);
  apply_style_classes(*title, config.label_style_classes);
  row->pack_start(*title, Gtk::PACK_SHRINK);

  auto combo = Gtk::manage(new Gtk::ComboBoxText());
  combo->set_hexpand(config.combo_hexpand);
  apply_style_classes(*combo, config.combo_style_classes);

  for (const auto &option : options)
    combo->append(option);

  if (config.active_index >= 0)
    combo->set_active(config.active_index);

  row->pack_start(
      *combo, config.combo_hexpand ? Gtk::PACK_EXPAND_WIDGET
                                   : Gtk::PACK_SHRINK);

  if (config.on_configure)
    config.on_configure(*combo);

  return row;
}

void ControlsBase::apply_style_classes(
    Gtk::Widget &widget, const std::vector<Glib::ustring> &classes) {
  auto context = widget.get_style_context();
  if (!context)
    return;

  for (const auto &name : classes)
    context->add_class(name);
}

void ControlsBase::on_close_clicked() { hide(); }
