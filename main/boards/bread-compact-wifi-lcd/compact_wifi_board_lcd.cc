#include "wifi_board.h"
#include "assets.h"
#include "adc_manager.h"
#include "button_manager.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "esplog_display.h"
#include "esp32_music.h"
#include "gpio_manager.h"
#include "imu_manager.h"
#include "tools_manager.h"
#include "wifi_manager.h"
#include "display/lcd_display.h"
#include "lamp_controller.h"
#include "led/single_led.h"
#include "button.h"
#include "application.h"
#include "system_reset.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

#if defined(LCD_TYPE_ILI9341_SERIAL)
#include "esp_lcd_ili9341.h"
#endif

#if defined(LCD_TYPE_GC9A01_SERIAL)
#include "esp_lcd_gc9a01.h"
static const gc9a01_lcd_init_cmd_t gc9107_lcd_init_cmds[] = {
    {0xfe, (uint8_t[]){0x00}, 0, 0}, {0xef, (uint8_t[]){0x00}, 0, 0},
    {0xb0, (uint8_t[]){0xc0}, 1, 0}, {0xb1, (uint8_t[]){0x80}, 1, 0},
    {0xb2, (uint8_t[]){0x27}, 1, 0}, {0xb3, (uint8_t[]){0x13}, 1, 0},
    {0xb6, (uint8_t[]){0x19}, 1, 0}, {0xb7, (uint8_t[]){0x05}, 1, 0},
    {0xac, (uint8_t[]){0xc8}, 1, 0}, {0xab, (uint8_t[]){0x0f}, 1, 0},
    {0x3a, (uint8_t[]){0x05}, 1, 0}, {0xb4, (uint8_t[]){0x04}, 1, 0},
    {0xa8, (uint8_t[]){0x08}, 1, 0}, {0xb8, (uint8_t[]){0x08}, 1, 0},
    {0xea, (uint8_t[]){0x02}, 1, 0}, {0xe8, (uint8_t[]){0x2A}, 1, 0},
    {0xe9, (uint8_t[]){0x47}, 1, 0}, {0xe7, (uint8_t[]){0x5f}, 1, 0},
    {0xc6, (uint8_t[]){0x21}, 1, 0}, {0xc7, (uint8_t[]){0x15}, 1, 0},
};
#endif

#define TAG "CompactWifiBoardLCD"

class CompactWifiBoardLCD : public WifiBoard {
private:
  Button boot_button_;
  LcdDisplay *display_;

  // === Thêm các phần từ Esp32s3SmartSpeaker ===
  void InitializeManagers() {
    ESP_LOGI(TAG, "Initializing managers...");
//    AdcManager::GetInstance().Initialize();
//    ImuManager::GetInstance().Initialize();
//    ButtonManager::GetInstance().Initialize();
    ToolsManager::GetInstance().Initialize();
    WifiManager::GetInstance().Initialize();
    ESP_LOGI(TAG, "All managers initialized successfully");
  }

  void InitializeCodecI2c() { return; }

  // === LCD SPI init ===
  void InitializeSpi() {
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
    buscfg.miso_io_num = GPIO_NUM_NC;
    buscfg.sclk_io_num = DISPLAY_CLK_PIN;
    buscfg.quadwp_io_num = GPIO_NUM_NC;
    buscfg.quadhd_io_num = GPIO_NUM_NC;
    buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
  }

  void InitializeLcdDisplay() {
    esp_lcd_panel_io_handle_t panel_io = nullptr;
    esp_lcd_panel_handle_t panel = nullptr;

    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = DISPLAY_CS_PIN;
    io_config.dc_gpio_num = DISPLAY_DC_PIN;
    io_config.spi_mode = DISPLAY_SPI_MODE;
    io_config.pclk_hz = 40 * 1000 * 1000;
    io_config.trans_queue_depth = 10;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = DISPLAY_RST_PIN;
    panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
    panel_config.bits_per_pixel = 16;

#if defined(LCD_TYPE_ILI9341_SERIAL)
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
#elif defined(LCD_TYPE_GC9A01_SERIAL)
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(panel_io, &panel_config, &panel));
#elif defined(LCD_TYPE_ST7789_SERIAL)
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
#endif

    esp_lcd_panel_reset(panel);
    esp_lcd_panel_init(panel);
    esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
    esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
    esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

    display_ = new SpiLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                 DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                 DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
                                 DISPLAY_SWAP_XY);
  }

  void InitializeButtons() {
    boot_button_.OnClick([this]() {
      auto &app = Application::GetInstance();
      if (app.GetDeviceState() == kDeviceStateStarting &&
          !WifiStation::GetInstance().IsConnected()) {
        ResetWifiConfiguration();
      }
      app.ToggleChatState();
    });
  }

  void InitializeTools() { static LampController lamp(LAMP_GPIO); }

public:
  CompactWifiBoardLCD() : boot_button_(BOOT_BUTTON_GPIO) {
    ESP_LOGI(TAG, "Initializing CompactWifiBoardLCD...");

    // === Nhạc & Codec ===
    music_ = new Esp32Music();

    InitializeCodecI2c();
    InitializeManagers();

    InitializeSpi();
    InitializeLcdDisplay();
    InitializeButtons();
    InitializeTools();

    if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
      GetBacklight()->RestoreBrightness();
    }

    ESP_LOGI(TAG, "CompactWifiBoardLCD initialized successfully");
  }

  virtual ~CompactWifiBoardLCD() = default;

  virtual std::string GetBoardType() override {
    return "compact-wifi-board-lcd";
  }

  virtual AudioCodec *GetAudioCodec() override {
    static NoAudioCodecSimplex audio_codec(
        AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
        AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT,
        AUDIO_MIC_I2S_BCLK, AUDIO_MIC_I2S_WS, AUDIO_MIC_I2S_DIN);
    return &audio_codec;
  }

  virtual Display *GetDisplay() override { return display_; }

  virtual Backlight *GetBacklight() override {
    if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
      static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN,
                                    DISPLAY_BACKLIGHT_OUTPUT_INVERT);
      return &backlight;
    }
    return nullptr;
  }

  virtual Led *GetLed() override {
    static SingleLed led(BUILTIN_LED_GPIO);
    return &led;
  }

  virtual Assets *GetAssets() override {
    static Assets assets(std::string(ASSETS_XIAOZHI_WAKENET_SMALL));
    return &assets;
  }

  virtual std::string GetBoardJson() override {
    char json_buffer[1024];
    snprintf(json_buffer, sizeof(json_buffer),
             "{"
             "\"board_type\":\"compact-wifi-board-lcd\","
             "\"version\":\"%s\","
             "\"features\":[\"audio\",\"imu\",\"pressure\",\"lcd\",\"wifi\",\"led\"]"
             "}",
             SMART_SPEAKER_VERSION);
    return std::string(json_buffer);
  }
};

DECLARE_BOARD(CompactWifiBoardLCD);
