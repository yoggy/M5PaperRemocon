#include <M5Unified.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

#include <vector>

#include "config.h"

WiFiClient wifi_client;
void mqtt_sub_callback(char* topic, byte* payload, unsigned int length);
PubSubClient mqtt_client(mqtt_host, mqtt_port, mqtt_sub_callback, wifi_client);

class Button {
public:
  Button(const String &label, const String &topic, const String &arg, const int &x, const int &y, const int &w, const int &h) {
    this->_label = label;
    this->_topic = topic;
    this->_arg = arg;    
    this->_x = x;
    this->_y = y;
    this->_w = w;
    this->_h = h;
    this->_prev_pressed = false;
  }

  void draw(const bool &flag) {
    if (flag == false) {
      M5.Display.fillRect(_x, _y, _w, _h, TFT_WHITE);
      M5.Display.drawRect(_x, _y, _w, _h, TFT_BLACK);
      M5.Lcd.setTextColor(TFT_BLACK, TFT_WHITE);
      M5.Display.drawString(_label, _x + _w / 2 - M5.Display.textWidth(_label) / 2, _y + _h / 2 - 40 / 2);
    } else {
      M5.Display.drawRect(_x, _y, _w, _h, TFT_BLACK);
    }
  }

  void update() {
    auto t = M5.Touch.getDetail();

    // タッチイベントがボタン外の場合
    if (t.base_x < _x || _x + _w < t.base_x || t.base_y < _y || _y + _h < t.base_y) {
      if (_prev_pressed == true) {
        process_released();
        _prev_pressed = false;
      }
      return;
    }

    // タッチイベントがボタン内の場合
    if (t.isPressed() == true) {
      if (_prev_pressed == false) {
        process_pressed();
        _prev_pressed = true;
      }
    } else {
      if (_prev_pressed == true) {
        process_released();
        _prev_pressed = false;
      }
    }
  }

  void process_pressed() {
    Serial.print("process_pressed() : publish topic=");
    Serial.print(_topic);
    Serial.print(", arg=");
    Serial.print(_arg);

    mqtt_client.publish(_topic.c_str(), _arg.c_str());

    draw(true);
  }

  void process_released() {
    Serial.println("process_released()");
    draw(false);
  }

protected:
  String _label;
  String _topic;
  String _arg;
  int _x;
  int _y;
  int _w;
  int _h;
  bool _prev_pressed;
};

std::vector<Button> buttons;

void setup() {
  // 初期設定
  auto cfg = M5.config();
  cfg.serial_baudrate = 9600;
  M5.begin(cfg);

  M5.Display.setTextSize(1);
  M5.Display.setFont(&lgfxJapanGothic_40);
  M5.Display.clear();

  // Wifi
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  WiFi.setSleep(false);
  int count = 0;
  M5.Lcd.setTextColor(TFT_BLACK, TFT_WHITE);

  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    switch (count % 4) {
      case 0:
        M5.Display.drawString("|", M5.Display.width()/2, M5.Display.height()/2);
        break;
      case 1:
        M5.Display.drawString("/", M5.Display.width()/2, M5.Display.height()/2);
        break;
      case 2:
        M5.Display.drawString("-", M5.Display.width()/2, M5.Display.height()/2);
        break;
      case 3:
        M5.Display.drawString("\\", M5.Display.width()/2, M5.Display.height()/2);
        break;
    }
    count++;
    if (count >= 100) reboot();  // 100 / 4 = 60sec
  }
  M5.Display.drawString("WiFi connected!", 0, 16);
  delay(1000);

  // MQTT
  bool rv = false;
  if (mqtt_use_auth == true) {
    rv = mqtt_client.connect(mqtt_client_id, mqtt_username, mqtt_password);
  } else {
    rv = mqtt_client.connect(mqtt_client_id);
  }
  if (rv == false) {
    Serial.println("mqtt connecting failed...");
    reboot();
  }
  M5.Display.drawString("MQTT connected!", 0, 16);
  delay(1000);

  // ボタンの描画
  M5.Display.clear();

  int y = 20, step_y = 120;
  buttons.push_back(Button("でんき", "device/led/power", "", 20, y, M5.Display.width() - 20 * 2, 100));
  y += step_y;
  buttons.push_back(Button("シーリングファン", "device/ceiling-fan/power", "", 20, y, M5.Display.width() - 20 * 2, 100));
  y += step_y;
  y += step_y;
  buttons.push_back(Button("エアコンふつう", "device/aircon/on", "", 20, y, M5.Display.width() - 20 * 2, 100));
  y += step_y;
  buttons.push_back(Button("エアコン弱", "device/aircon/on2", "", 20, y, M5.Display.width() - 20 * 2, 100));
  y += step_y;
  buttons.push_back(Button("エアコン消す", "device/aircon/off", "", 20, y, M5.Display.width() - 20 * 2, 100));

  std::vector<Button>::iterator it;
  for (it = buttons.begin(); it != buttons.end(); ++it) {
    it->draw(false);
  }
}

void reboot() {
  Serial.println("REBOOT!!!!!");
  for (int i = 0; i < 5; ++i) {
    M5.Display.clear();
    delay(200);
    M5.Display.fillRect(0, 0, M5.Display.width(), M5.Display.height(), TFT_BLACK);
    delay(200);
  }

  M5.Display.clear();

  ESP.restart();
}

void loop() {
  mqtt_client.loop();
  if (!mqtt_client.connected()) {
    Serial.println("MQTT disconnected...");
    reboot();
  }

  M5.update();

  for (auto it = buttons.begin(); it != buttons.end(); ++it) {
    it->update();
  }
}

#define BUF_LEN 16
char buf[BUF_LEN];

void mqtt_sub_callback(char* topic, byte* payload, unsigned int length) {
  int len = BUF_LEN - 1 < length ? (BUF_LEN - 1) : length;
  memset(buf, 0, BUF_LEN);
  strncpy(buf, (const char*)payload, len);

  String cmd = String(buf);
  Serial.print("payload=");
  Serial.println(cmd);
}
