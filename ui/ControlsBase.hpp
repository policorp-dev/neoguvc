#pragma once

#include <functional>
#include <vector>

#include <gtkmm/adjustment.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/label.h>
#include <gtkmm/scale.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/stylecontext.h>
#include <gtkmm/window.h>

class ControlsBase : public Gtk::Window {
public:
  struct ConstructionOptions {
    Glib::ustring title;
    Glib::ustring header_title;
    Glib::ustring close_button_label{"Fechar"};
    bool resizable{true};
    bool apply_size_request{true};
    int width{520};
    int height{620};
    unsigned root_spacing{6};
    unsigned root_margin_top{12};
    unsigned root_margin_bottom{12};
    unsigned root_margin_left{12};
    unsigned root_margin_right{12};
    unsigned container_spacing{12};
    unsigned container_margin_bottom{8};
    unsigned container_margin_right{15};
    Gtk::PolicyType h_scroll_policy{Gtk::POLICY_NEVER};
    Gtk::PolicyType v_scroll_policy{Gtk::POLICY_AUTOMATIC};
    std::vector<Glib::ustring> window_style_classes{"controls-window-root"};
    std::vector<Glib::ustring> root_box_style_classes{"controls-window"};
    std::vector<Glib::ustring> scroll_style_classes{"controls-scroll"};
    std::vector<Glib::ustring> container_style_classes{"controls-container"};
    std::vector<Glib::ustring> close_button_style_classes{"controls-button"};
    bool show_reset_button{false};
    Glib::ustring reset_button_label{"Restaurar"};
    std::vector<Glib::ustring> reset_button_style_classes{"controls-button"};
    bool show_close_button{true};
  };

  struct SliderRowConfig {
    unsigned row_spacing{12};
    double step{1.0};
    double page_increment{0.0};
    double page_size{0.0};
    bool sensitive{true};
    int digits{0};
    int label_width_chars{16};
    Gtk::Align label_halign{Gtk::ALIGN_START};
    Gtk::Align label_valign{Gtk::ALIGN_CENTER};
    float label_xalign{0.0f};
    unsigned label_margin_left{0};
    unsigned label_margin_right{7};
    unsigned scale_margin_left{8};
    unsigned scale_margin_right{4};
    bool row_hexpand{true};
    bool scale_hexpand{true};
    bool draw_value{true};
    Gtk::PositionType value_position{Gtk::POS_RIGHT};
    std::vector<Glib::ustring> row_style_classes{"controls-row"};
    std::vector<Glib::ustring> label_style_classes{"controls-label"};
    std::vector<Glib::ustring> scale_style_classes{"controls-scale"};
    std::function<void(Gtk::Scale &, const Glib::RefPtr<Gtk::Adjustment> &)>
        on_configure;
  };

  struct CheckRowConfig {
    unsigned row_spacing{0};
    bool hexpand{true};
    std::vector<Glib::ustring> row_style_classes{"controls-row"};
    std::vector<Glib::ustring> toggle_style_classes{"controls-toggle"};
    std::function<void(Gtk::CheckButton &)> on_configure;
  };

  struct ComboRowConfig {
    unsigned row_spacing{12};
    int active_index{0};
    bool hexpand{true};
    bool combo_hexpand{true};
    int label_width_chars{24};
    Gtk::Align label_halign{Gtk::ALIGN_START};
    Gtk::Align label_valign{Gtk::ALIGN_CENTER};
    float label_xalign{0.0f};
    unsigned label_margin_left{0};
    unsigned label_margin_right{12};
    std::vector<Glib::ustring> row_style_classes{"controls-row"};
    std::vector<Glib::ustring> label_style_classes{"controls-label"};
    std::vector<Glib::ustring> combo_style_classes{"controls-entry"};
    std::function<void(Gtk::ComboBoxText &)> on_configure;
  };

  explicit ControlsBase(const ConstructionOptions &options);
  ~ControlsBase() override = default;

  Gtk::Box &root_box();
  Gtk::Box &controls_container();
  Gtk::ScrolledWindow &scrolled_window();
  Gtk::HeaderBar &header_bar();
  Gtk::Button &close_button();
  Gtk::Button &reset_button();
  Gtk::Box &footer_box();
  bool has_reset_button() const;

protected:
  Gtk::Box &body_container();

  Gtk::Box *create_slider_row(const Glib::ustring &label, double initial,
                              double min, double max);
  Gtk::Box *create_slider_row(const Glib::ustring &label, double initial,
                              double min, double max, SliderRowConfig config);
  Gtk::Box *create_check_row(const Glib::ustring &label, bool active = false);
  Gtk::Box *create_check_row(const Glib::ustring &label, bool active,
                             CheckRowConfig config);
  Gtk::Box *create_combo_row(const Glib::ustring &label,
                             const std::vector<Glib::ustring> &options);
  Gtk::Box *create_combo_row(const Glib::ustring &label,
                             const std::vector<Glib::ustring> &options,
                             ComboRowConfig config);
  void add_row(Gtk::Widget &widget,
               Gtk::PackOptions options = Gtk::PACK_SHRINK,
               guint padding = 0);

private:
  void apply_style_classes(Gtk::Widget &widget,
                           const std::vector<Glib::ustring> &classes);
  void on_close_clicked();

  ConstructionOptions options_;
  Gtk::Box root_box_{Gtk::ORIENTATION_VERTICAL};
  Gtk::ScrolledWindow scroll_;
  Gtk::Box controls_container_{Gtk::ORIENTATION_VERTICAL};
  Gtk::Box footer_box_{Gtk::ORIENTATION_HORIZONTAL};
  Gtk::HeaderBar header_bar_;
  Gtk::Button close_button_;
  Gtk::Button reset_button_;
  std::vector<Glib::RefPtr<Gtk::Adjustment>> adjustments_;
};
