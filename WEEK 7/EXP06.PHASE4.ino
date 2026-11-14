#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include <list>
#include <vector>
#include "human_face_detect_msr01.hpp"
#include "human_face_detect_mnp01.hpp"

#if ESP_ARDUINO_VERSION_MAJOR >= 3
#error "Face detection on the plain ESP32 needs esp32 core 2.0.17 (core 3.x disables it on non-S3 chips)."
#endif
#ifndef BOARD_HAS_PSRAM
#error "Select Tools > Board > ESP32 Wrover Module: face detection needs PSRAM."
#endif

// ---------------- WiFi (phone hotspot, 2.4 GHz) ----------------
const char* ssid = "**********";
const char* password = "**********";

// ---------------- Pan/tilt servo settings ----------------
const int PAN_PIN  = 2;
const int TILT_PIN = 14;
const int PAN_CH   = 4;
const int TILT_CH  = 5;
const int SERVO_MIN = 1800;      // 0.55 ms  (mechanical safety limit)
const int SERVO_MAX = 8000;      // 2.44 ms
const int SERVO_MID = 4915;      // 1.50 ms  (centre)
const int SWEEP_MIN = 3277;      // 1.00 ms  (search sweep range)
const int SWEEP_MAX = 6554;      // 2.00 ms
const float KP = 600.0f;         // duty counts per unit of normalised error (about half the needed correction per frame)
const float DEADBAND = 0.12f;    // ignore errors within 12 % of the frame half-width/height
const int PAN_DIR  = -1;         // flip to +1 if the camera turns AWAY from the face
const int TILT_DIR = +1;         // flip to -1 if the camera tilts AWAY from the face
const bool SEARCH_WHEN_IDLE = true;  // slow pan sweep after 3 s without a face

int pan_pos  = SERVO_MID;
int tilt_pos = SERVO_MID;

// ---------------- Face position shared between the stream task and loop() ----------------
volatile int g_face_cx = 0, g_face_cy = 0, g_frame_w = 0, g_frame_h = 0;
volatile uint32_t g_face_seq = 0;

// ---------------- HTTP streaming ----------------
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;
int stream_port_number = 81;

#define FACE_COLOR_YELLOW 0x0000FFFF   // yellow in the BGR888 buffer

// Draw a rectangle around every detected face
static void draw_face_boxes(fb_data_t *fb, std::list<dl::detect::result_t> &results) {
  for (auto &r : results) {
    if (r.box.size() < 4) continue;
    int x = r.box[0], y = r.box[1];
    int w = r.box[2] - x + 1, h = r.box[3] - y + 1;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + w > fb->width)  w = fb->width - x;
    if (y + h > fb->height) h = fb->height - y;
    if (w <= 0 || h <= 0) continue;
    fb_gfx_drawFastHLine(fb, x, y, w, FACE_COLOR_YELLOW);
    fb_gfx_drawFastHLine(fb, x, y + h - 1, w, FACE_COLOR_YELLOW);
    fb_gfx_drawFastVLine(fb, x, y, h, FACE_COLOR_YELLOW);
    fb_gfx_drawFastVLine(fb, x + w - 1, y, h, FACE_COLOR_YELLOW);
  }
}

// Publish the centre of the largest face for loop()
static void report_face(std::list<dl::detect::result_t> &results, int fw, int fh) {
  int best = 0, cx = 0, cy = 0;
  for (auto &r : results) {
    if (r.box.size() < 4) continue;
    int area = (r.box[2] - r.box[0]) * (r.box[3] - r.box[1]);
    if (area > best) {
      best = area;
      cx = (r.box[0] + r.box[2]) / 2;
      cy = (r.box[1] + r.box[3]) / 2;
    }
  }
  if (best > 0) {
    g_face_cx = cx;
    g_face_cy = cy;
    g_frame_w = fw;
    g_frame_h = fh;
    g_face_seq = g_face_seq + 1;
  }
}

// Stream handler: capture -> RGB888 -> face detection -> draw boxes -> JPEG -> send
static esp_err_t stream_handler(httpd_req_t *req) {
  HumanFaceDetectMSR01 s1(0.1F, 0.5F, 10, 0.2F);   // stage 1: candidate face boxes
  HumanFaceDetectMNP01 s2(0.5F, 0.3F, 5);          // stage 2: refine candidates
  char part_buf[64];

  esp_err_t res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  while (true) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
      break;
    }

    int w = fb->width, h = fb->height;
    size_t rgb_len = (size_t)w * h * 3;
    uint8_t *rgb = (uint8_t *)ps_malloc(rgb_len);
    bool ok = (rgb != NULL) && fmt2rgb888(fb->buf, fb->len, fb->format, rgb);
    esp_camera_fb_return(fb);
    if (!ok) {
      if (rgb) free(rgb);
      Serial.println("RGB888 conversion failed");
      res = ESP_FAIL;
      break;
    }

    std::list<dl::detect::result_t> &candidates = s1.infer((uint8_t *)rgb, {h, w, 3});
    std::list<dl::detect::result_t> &results = s2.infer((uint8_t *)rgb, {h, w, 3}, candidates);
    if (results.size() > 0) {
      fb_data_t rfb;
      rfb.width = w;
      rfb.height = h;
      rfb.data = rgb;
      rfb.bytes_per_pixel = 3;
      rfb.format = FB_BGR888;
      draw_face_boxes(&rfb, results);
      report_face(results, w, h);
    }

    uint8_t *jpg_buf = NULL;
    size_t jpg_len = 0;
    ok = fmt2jpg(rgb, rgb_len, w, h, PIXFORMAT_RGB888, 80, &jpg_buf, &jpg_len);
    free(rgb);
    if (!ok) {
      Serial.println("JPEG compression failed");
      res = ESP_FAIL;
      break;
    }

    size_t hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, (unsigned)jpg_len);
    res = httpd_resp_send_chunk(req, part_buf, hlen);
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)jpg_buf, jpg_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    free(jpg_buf);
    if (res != ESP_OK) {
      break;   // browser closed the stream
    }
  }
  return res;
}

// Web page: shows the stream from port 81
static esp_err_t web_handler(httpd_req_t *req) {
  char page[400];
  IPAddress ip = WiFi.localIP();
  snprintf(page, sizeof(page),
           "<!doctype html><html><head><title>ESP32-CAM Face Tracking</title></head>"
           "<body style='background:#111;color:#eee;font-family:sans-serif'>"
           "<h3>ESP32-CAM face tracking</h3>"
           "<img src='http://%d.%d.%d.%d:%d/stream' style='width:640px;max-width:100%%'>"
           "</body></html>",
           ip[0], ip[1], ip[2], ip[3], stream_port_number);
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, page, strlen(page));
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  httpd_uri_t index_uri = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = web_handler,
    .user_ctx = NULL
  };
  httpd_uri_t stream_uri = {
    .uri      = "/stream",
    .method   = HTTP_GET,
    .handler  = stream_handler,
    .user_ctx = NULL
  };

  Serial.printf("Starting web server on port: '%d'\n", config.server_port);
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
  }

  config.server_port += 1;          // 81
  config.ctrl_port += 1;
  config.stack_size = 8192;         // extra headroom for face detection
  Serial.printf("Starting stream server on port: '%d'\n", config.server_port);
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
  stream_port_number = config.server_port;
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  // Camera pins for the AI-Thinker ESP32-CAM
  camera_config_t config = {};
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
  config.pin_sccb_sda = 26;
  config.pin_sccb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;       // 320x240: face detection needs width <= 400 px
  config.jpeg_quality = 10;
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }

  // Servos: 50 Hz, 16-bit (1 count = 0.305 us), start centred
  ledcSetup(PAN_CH, 50, 16);
  ledcAttachPin(PAN_PIN, PAN_CH);
  ledcSetup(TILT_CH, 50, 16);
  ledcAttachPin(TILT_PIN, TILT_CH);
  ledcWrite(PAN_CH, pan_pos);
  ledcWrite(TILT_CH, tilt_pos);

  Serial.print("Connecting to WiFi network: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  startCameraServer();

  Serial.print("Camera ready. Access it at: http://");
  Serial.println(WiFi.localIP());
}

// loop(): proportional pan/tilt controller, one correction per new detection
void loop() {
  static uint32_t last_seq = 0;
  static unsigned long last_face_ms = 0;
  static unsigned long last_sweep_ms = 0;
  static int sweep_dir = 1;
  unsigned long now = millis();

  uint32_t seq = g_face_seq;
  if (seq != last_seq) {                          // a new detection result arrived
    last_seq = seq;
    last_face_ms = now;
    int fw = g_frame_w, fh = g_frame_h;
    int cx = g_face_cx, cy = g_face_cy;
    if (fw > 0 && fh > 0) {
      float ex = (cx - fw / 2.0f) / (fw / 2.0f);  // -1 = left edge  .. +1 = right edge
      float ey = (cy - fh / 2.0f) / (fh / 2.0f);  // -1 = top edge   .. +1 = bottom edge
      if (fabsf(ex) > DEADBAND) pan_pos  += (int)(PAN_DIR  * KP * ex);
      if (fabsf(ey) > DEADBAND) tilt_pos += (int)(TILT_DIR * KP * ey);
      pan_pos  = constrain(pan_pos,  SERVO_MIN, SERVO_MAX);
      tilt_pos = constrain(tilt_pos, SERVO_MIN, SERVO_MAX);
      ledcWrite(PAN_CH, pan_pos);
      ledcWrite(TILT_CH, tilt_pos);
      Serial.printf("face (%d,%d) in %dx%d  err=(%+.2f,%+.2f)  pan=%d tilt=%d\n",
                    cx, cy, fw, fh, ex, ey, pan_pos, tilt_pos);
    }
  } else if (SEARCH_WHEN_IDLE && (now - last_face_ms > 3000) && (now - last_sweep_ms > 40)) {
    last_sweep_ms = now;                          // no face for 3 s: slow search sweep (about 8.7 s per pass)
    pan_pos += 15 * sweep_dir;
    if (pan_pos >= SWEEP_MAX) { pan_pos = SWEEP_MAX; sweep_dir = -1; }
    if (pan_pos <= SWEEP_MIN) { pan_pos = SWEEP_MIN; sweep_dir = +1; }
    ledcWrite(PAN_CH, pan_pos);
  }
  delay(5);
}