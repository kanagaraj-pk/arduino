#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "FS.h"                // SD Card ESP32 File System
#include "SD_MMC.h"            // SD Card hardware driver
#include "soc/soc.h"           // Used to manage brownout loops
#include "soc/rtc_cntl_reg.h"  // Used to manage brownout loops


// ===========================
// Select camera model in board_config.h
// ===========================
#include "board_config.h"

// ===========================
// Enter your WiFi credentials
// ===========================
const char *ssid = "Vi";
const char *password = "9448894884";


#define MAX_IMAGES 1000


// Time to sleep between images (in seconds)
// 5 minutes = 300 seconds
#define TIME_TO_SLEEP  30   
#define uS_TO_S_FACTOR 1000000ULL  // Conversion factor for micro seconds to seconds
#define FLASH_LED_PIN 4 // Turn flash OFF


//void startCameraServer();
//void setupLedFlash();

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  ledcDetach(FLASH_LED_PIN); 
  pinMode(FLASH_LED_PIN, OUTPUT); // Turn flash OFF
  digitalWrite(FLASH_LED_PIN, LOW);
  delay(10);

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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  //config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition

  config.jpeg_quality = 12;
  config.fb_count = 1;
  config.frame_size = FRAMESIZE_XGA;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  // Frame size options: UXGA (1600x1200), SXGA (1280x1024), XGA (1024x768), SVGA (800x600), VGA (640x480)
 

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  delay(1000); 
  // camera init
  esp_err_t err = esp_camera_init(&config);




  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }


    sensor_t *s = esp_camera_sensor_get();

    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_exposure_ctrl(s, 1);
    s->set_gain_ctrl(s, 1);


/*
  sensor_t *s = esp_camera_sensor_get();


  //Serial.printf("PID = 0x%02X\n", s->id.PID);
  //Serial.printf("VER = 0x%02X\n", s->id.VER);
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // up the brightness just a bit
    s->set_saturation(s, -2);  // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }
*/

/*
#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  setupLedFlash();
#endif

*/

  // Initialize MicroSD Card
  Serial.println("Starting SD Card");
  if(!SD_MMC.begin()){
    Serial.println("SD Card Mount Failed");
    return;
  }
  
  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD Card attached");
    return;
  }

 

    uint32_t pictureNumber = 0;

    File counterFile = SD_MMC.open("/counter.txt", FILE_READ);
    if (counterFile) {
        pictureNumber = counterFile.parseInt();
        counterFile.close();
    }
    pictureNumber++;

    if (pictureNumber >= MAX_IMAGES)
    pictureNumber = 0;


for (int i = 0; i < 3; i++) {
    camera_fb_t *tmp = esp_camera_fb_get();
    if (tmp) {
        esp_camera_fb_return(tmp);
    }
    delay(100);
}

 camera_fb_t *fb = esp_camera_fb_get();

  if (!fb) {
      Serial.println("Camera capture failed");
      esp_camera_deinit();
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
      esp_deep_sleep_start();
  }
  uint8_t *jpg_buf = NULL;
  size_t jpg_len = 0;

  bool ok = frame2jpg(fb, 80, &jpg_buf, &jpg_len);
  if (!ok) {
    Serial.println("JPEG conversion failed");
    esp_camera_fb_return(fb);
    esp_camera_deinit();
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
  }
  if (ok) {
      String path = "/picture" + String(pictureNumber) + ".jpg";
      File file = SD_MMC.open(path, FILE_WRITE);
      file.write(jpg_buf, jpg_len);
      file.close();
      Serial.printf("Saved file to path: %s\n", path.c_str());

      WiFi.begin(ssid, password);
      unsigned long start = millis();
      while (WiFi.status() != WL_CONNECTED &&
          millis() - start < 10000) {
        delay(200);
        Serial.println("Wifi Connecting");
      }
      if(WiFi.status() == WL_CONNECTED) {
        Serial.println("Wifi Connected");
          HTTPClient http;
          http.begin("https://api.tekfocusminds.com/motor/upload");
          http.addHeader("Content-Type","image/jpeg");
          int code = http.POST(jpg_buf,jpg_len);
          Serial.printf("HTTP = %d\n", code);
          if (code == 200) {
              Serial.println("Upload OK");
          }
          else {
              Serial.println(http.getString());
          }
          http.end();        
      }
      free(jpg_buf);
  }

  esp_camera_fb_return(fb);
  delay(500); // Small pause for sensor stabilization

    
/*
 String path = "/picture" + String(pictureNumber) + ".jpg";
    fs::FS &fs = SD_MMC;
    Serial.printf("Writing file: %s\n", path.c_str());

    File file = fs.open(path.c_str(), FILE_WRITE);
     if(!file){
        Serial.println("Failed to open file in writing mode");
      } 
    else {
        file.write(fb->buf, fb->len); // Write frame buffer data to SD Card
        Serial.printf("Saved file to path: %s\n", path.c_str());
        
      }
    file.close();

    */

    counterFile = SD_MMC.open("/counter.txt", FILE_WRITE);
    if (counterFile) {
        counterFile.seek(0);
        counterFile.print(pictureNumber);
        counterFile.close();
    }

    


      // Enable sleep timer and start deep sleep
  Serial.printf("Going to sleep for %d seconds...\n", TIME_TO_SLEEP);
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_camera_deinit();
  esp_deep_sleep_start();

 /* WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
  */
}

void loop() {
  // Do nothing. Everything is done in another task by the web server
  delay(10000);
}
