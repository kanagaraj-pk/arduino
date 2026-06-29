#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include "Arduino.h"
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <EEPROM.h>            // read and write from flash memory

// ===========================
// Select camera model in board_config.h
// ===========================
#include "board_config.h"

int pictureNumber = 0;


// ===========================
// Enter your WiFi credentials
// ===========================
const char *ssid = "Vi";
const char *password = "9448894884";


#define MAX_IMAGES 1000


void setup() {
  
  Serial.begin(115200);
  //Serial.setDebugOutput(true);
  //Serial.println();
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_YUV422; 
  
  config.frame_size = FRAMESIZE_SVGA;
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  
  // Init Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  WiFi.begin(ssid, password);
      unsigned long start = millis();
      while (WiFi.status() != WL_CONNECTED &&
          millis() - start < 10000) {
        delay(1000);
        Serial.println("Wifi Connecting");
      }
  
}

void loop() {
  
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("Wifi Connected");
          
          // 1. Capture the frame (Make sure pixel_format is set to PIXFORMAT_YUV422 in config)
          camera_fb_t * fb = esp_camera_fb_get();
          if (!fb) {
              Serial.println("Camera capture failed");
              return;
          }

          HTTPClient http;
          http.begin("https://api.tekfocusminds.com/motor/upload");
          
          // 2. Set headers so the server knows how to process the raw binary payload
          http.addHeader("Content-Type", "application/octet-stream"); // standard binary format
          http.addHeader("X-Image-Format", "YUV422");                 // tells your API it is YUV
          http.addHeader("X-Image-Width", String(fb->width));         // passes width (e.g., 1600)
          http.addHeader("X-Image-Height", String(fb->height));       // passes height (e.g., 1200)
          
          // 3. Send the raw framebuffer pointer and length
          int code = http.POST(fb->buf, fb->len);
          
          Serial.printf("HTTP = %d\n", code);
          if (code == 200) {
              Serial.println("Upload OK");
          }
          else {
              Serial.println(http.getString());
          }
          
          http.end(); 
          esp_camera_fb_return(fb);
          delay(5000); // Wait 5 seconds before next shot
  }

}