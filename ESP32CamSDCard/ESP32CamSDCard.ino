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

#include <ArduinoJson.h>

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
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;



  config.pixel_format = PIXFORMAT_YUV422; 

  //800*600
  //config.xclk_freq_hz = 20000000;
  //config.frame_size = FRAMESIZE_SVGA; 
  //config.fb_count = 1;

  //320*240
  config.xclk_freq_hz = 16000000;  
  config.frame_size = FRAMESIZE_QVGA;
  config.fb_count = 2;

  
  // Init Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // --- ADD THIS BLOCK FOR SUNLIGHT CORRECTION ---
  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    // 1. Enable Automatic Exposure Control (AEC) so it actively drops brightness
    s->set_aec_value(s, 0); 
    s->set_exposure_ctrl(s, 1); // 1 = Enable Auto Exposure

    // 2. Enable Automatic Gain Control (AGC) but drop the ceiling to prevent grain/whiteness
    s->set_gain_ctrl(s, 1);     // 1 = Enable Auto Gain
    s->set_agc_gain(s, 0);         // Reset manual gain overrides
    s->set_gainceiling(s, GAINCEILING_2X); // Lower limit (indoor defaults use 8X or 16X)

    // 3. Enable Auto White Balance (AWB) and switch to "Sunny" or "Outdoor" mode
    s->set_whitebal(s, 1);         // 1 = Enable Auto White Balance
    s->set_wb_mode(s, 1);          // 1 = Sunny / Outdoor mode (0=Auto, 2=Cloudy, 3=Office)

    // 4. Adjust basic light levels
    s->set_brightness(s, -1);      // Drop general brightness bias slightly (-2 to 2 scale)
    s->set_contrast(s, 1);         // Boost contrast to preserve details under direct sun
  }

  WiFi.begin(ssid, password);
      unsigned long start = millis();
      while (WiFi.status() != WL_CONNECTED &&
          millis() - start < 10000) {
        delay(1000);
        Serial.println("Wifi Connecting");
      }
  Serial.println("Wifi Connected");
  
}

void loop() {  
  if(WiFi.status() == WL_CONNECTED) {          
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
                        // --- DYNAMIC CONTROL DECODING ---
                String responseBody = http.getString();
                
                // Parse the JSON string configuration map returned from FastAPI
                JsonDocument doc; 
                DeserializationError error = deserializeJson(doc, responseBody);
                
                if (!error) {
                  sensor_t * s = esp_camera_sensor_get();
                  if (s != NULL) {
                    // Read settings out of JSON and write directly to camera registers
                    s->set_brightness(s, doc["brightness"]);    
                    s->set_contrast(s, doc["contrast"]);        
                    s->set_exposure_ctrl(s, doc["exposure_ctrl"]); 
                    s->set_aec_value(s, doc["aec_value"]);
                    
                    s->set_gain_ctrl(s, doc["gain_control"]); 
                    s->set_agc_gain(s, doc["agc_gain"]);
                    // Map integer to internal enum indices (0=2X, 1=4X, 2=8X, etc)
                    s->set_gainceiling(s, (gainceiling_t)doc["gainceiling"].as<int>());

                    // --- NEWLY ADDED GC2145 COMPATIBLE SETTINGS ---
                    s->set_saturation(s, doc["saturation"]); // Fix color washed-out issues (-2 to 2)
                    s->set_whitebal(s, doc["whitebal"]);     // Auto white balance (0 or 1)
                    s->set_wb_mode(s, doc["wb_mode"]);       // 0=Auto, 1=Sunny, 2=Cloudy
                    s->set_hmirror(s, doc["hmirror"]);       // Horizontal flip (0 or 1)
                    s->set_vflip(s, doc["vflip"]);           // Vertical flip (0 or 1)                    
                  }
                }
            }          
          else {
              Serial.println(http.getString());
              }
          
          http.end(); 
          esp_camera_fb_return(fb);
          delay(100); // Wait 5 seconds before next shot
  }
  else
  {
    Serial.println("Wifi not Connected");
  }

}