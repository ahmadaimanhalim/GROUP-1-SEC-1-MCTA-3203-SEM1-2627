#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

#if ESP_ARDUINO_VERSION_MAJOR >= 3   // [FIX] stop early with a clear message on the wrong core
#error "Install esp32 core 2.0.17 in Boards Manager. This sketch uses the 2.x LEDC API (ledcSetup/ledcAttachPin)."
#endif

// WiFi network credentials (phone hotspot, must be 2.4 GHz)
const char* ssid = "Pentol";
const char* password = "TYPE_YOUR_HOTSPOT_PASSWORD";

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

int stream_port_number; // Stream server port number

const int servo_pin = 2;      // Servo motor connected to GPIO 2
const int servo_channel = 2;  // [P3] LEDC channel (kept separate from the pin number; ch 0 = camera XCLK)
const int button_pin = 13;    // [P3] pushbutton: GPIO 13 to GND, internal pull-up -> pressed = LOW

// PWM values for servo's min and max positions
// 16-bit duty at 50 Hz: 1 count = 20 ms / 65536 = 0.305 us
const int servo_min_position = 3700;   // 1.13 ms pulse
const int servo_max_position = 7000;   // 2.14 ms pulse
const int servo_step = 20;                      // [P3] duty counts per step
const unsigned long step_interval_ms = 15;      // [P3] 20 counts / 15 ms -> one full sweep in about 2.5 s
unsigned long last_step_ms = 0;
bool last_pressed = false;

int servo_position = servo_min_position;
bool servo_direction = true; // true = forward, false = backward

// ********************************************************
// stream_handler: handles video streaming

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  static int64_t last_frame = 0;
  if (!last_frame) {
    last_frame = esp_timer_get_time();
  }

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      if (fb->width > 400) {
        if (fb->format != PIXFORMAT_JPEG) {
          bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
          esp_camera_fb_return(fb);
          fb = NULL;
          if (!jpeg_converted) {
            Serial.println("JPEG compression failed");
            res = ESP_FAIL;
          }
        } else {
          _jpg_buf_len = fb->len;
          _jpg_buf = fb->buf;
        }
      }
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if (res != ESP_OK) {
      break;
    }
    // [P3] Servo code removed from here: it now runs in loop(), independent of the video stream.
  }
  last_frame = 0;

  return res;
}

// ********************************************************
// web_handler: builds the web page

static esp_err_t web_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Content-Encoding", "identity");
  sensor_t * s = esp_camera_sensor_get();

  char webPage[175] = "";
  strcat(webPage, "<!doctype html> <html> <head> <title id='title'>ESP32-CAM</title> </head> <body> <img id='stream' src='http://");

  // Stream server address (example: 172.20.10.2:81)
  char address[20] = "";
  sprintf(address, "%d.%d.%d.%d:%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3], stream_port_number);
  strcat(webPage, address);
  strcat(webPage, "/stream'> </body> </html>");
  int pageLength = strlen(webPage);

  return httpd_resp_send(req, (const char *)webPage, pageLength);
}

// ********************************************************
// startCameraServer: starts the web and stream servers

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = web_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };

  Serial.printf("Starting web server on port: '%d'\n", config.server_port);
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
  }

  config.server_port += 1;
  config.ctrl_port += 1;
  Serial.printf("Starting stream server on port: '%d'\n", config.server_port);
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }

  stream_port_number = config.server_port;
}

// ********************************************************
// setup: camera initialization and WiFi connection

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("====");

  // Pin configuration for AI Thinker ESP32-CAM
  camera_config_t config = {};               // [FIX] zero-initialise: unset fields were random stack values
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sccb_sda = 26;                  // [FIX] current field name (manual's pin_sscb_sda only gives a warning)
  config.pin_sccb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;         // 640x480; keep width > 400 or this stream_handler sends empty frames
  config.jpeg_quality = 10;
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_PSRAM;   // [FIX] explicit frame buffers in PSRAM
  config.grab_mode = CAMERA_GRAB_LATEST;     // [FIX] always send the newest frame (lower latency)

  // Camera initialization
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();

  // Servo motor initialization
  ledcSetup(servo_channel, 50, 16);         // channel, frequency, resolution
  ledcAttachPin(servo_pin, servo_channel);  // pin, channel
  ledcWrite(servo_channel, servo_position); // [P3] park at the start position

  pinMode(button_pin, INPUT_PULLUP);        // [P3] no external resistor needed

  Serial.println("");
  Serial.print("Connecting to WiFi network: ");
  Serial.println(ssid);
  delay(100);

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);                      // [FIX] modem sleep off: smoother stream
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");                       // [FIX] visible progress; endless dots = wrong SSID/password or 5 GHz hotspot
  }

  Serial.println("");
  Serial.println("WiFi connected");

  delay(100);

  startCameraServer();

  Serial.print("Camera ready. Access it at: http://");
  Serial.print(WiFi.localIP());
  Serial.println("");
}

// ********************************************************
// [P3] loop: pan back and forth only while the button is held

void loop() {
  bool pressed = (digitalRead(button_pin) == LOW);   // active-LOW with INPUT_PULLUP

  if (pressed != last_pressed) {                     // report state changes once
    last_pressed = pressed;
    Serial.println(pressed ? "Button pressed: panning" : "Button released: holding position");
  }

  if (pressed && (millis() - last_step_ms >= step_interval_ms)) {
    last_step_ms = millis();
    if (servo_direction) {
      servo_position += servo_step;
      if (servo_position >= servo_max_position) {
        servo_position = servo_max_position;
        servo_direction = false;
      }
    } else {
      servo_position -= servo_step;
      if (servo_position <= servo_min_position) {
        servo_position = servo_min_position;
        servo_direction = true;
      }
    }
    ledcWrite(servo_channel, servo_position);
  }

  delay(2);
}
