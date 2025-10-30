#include "ImageControls.hpp"

namespace {
ControlsBase::ConstructionOptions make_window_options() {
  ControlsBase::ConstructionOptions options;
  options.title = "Controles de imagem";
  options.header_title = options.title;
  options.width = 520;
  options.height = 620;
  return options;
}
} // namespace

ImageControls::ImageControls()
    : ControlsBase(make_window_options()) {
  add_row(*create_slider_row("Brilho:", 50, 0, 100));
  add_row(*create_slider_row("Contraste:", 60, 0, 100));
  add_row(*create_slider_row("Saturação:", 45, 0, 100));
  add_row(*create_slider_row("Matiz:", 50, 0, 100));

  ControlsBase::SliderRowConfig gamma_config;
  gamma_config.step = 5.0;
  add_row(*create_slider_row("Gama:", 300, 100, 500, gamma_config));

  add_row(*create_slider_row("Nitidez:", 50, 0, 100));
  add_row(*create_slider_row("Compensação de Luz:", 0, 0, 100));

  add_row(*create_check_row("Balanço de brancos", true));

  ControlsBase::SliderRowConfig white_balance_config;
  white_balance_config.step = 50.0;
  white_balance_config.sensitive = false;
  add_row(*create_slider_row("Balanço de branco:", 4600, 2500, 7500,
                             white_balance_config));

  ControlsBase::ComboRowConfig exposure_mode_config;
  exposure_mode_config.active_index = 1;
  add_row(*create_combo_row(
      "Exposição automática:",
      {"Manual", "Modo prioridade da abertura", "Modo prioridade do obturador"},
      exposure_mode_config));

  ControlsBase::SliderRowConfig exposure_time_config;
  exposure_time_config.step = 50.0;
  exposure_time_config.sensitive = false;
  add_row(*create_slider_row("Tempo de exposição:", 1250, 1, 20000,
                             exposure_time_config));

  ControlsBase::ComboRowConfig mains_frequency_config;
  mains_frequency_config.active_index = 1;
  add_row(*create_combo_row("Frequência:", {"50 Hz", "60 Hz"},
                            mains_frequency_config));

  add_row(*create_check_row("Exposição", true));

  show_all_children();
}
