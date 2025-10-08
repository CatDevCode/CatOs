#define FIRMWARE_VERSION "v0.1.4R"
// настройка дисплея
#define RES 17
#define DC 16
#define CS 5

#define OLED_SPI_SPEED 8000000ul // скорость(макс)
//------------------------
// библиотеки и переменные
#include <Arduino.h>        // стандартная либа
#include <GyverOLED.h>      // либа дисплея
#include "GyverButton.h"    // кнопки
#include <GyverDBFile.h>    // для настроек и wifi
#include <LittleFS.h>       // для хранения данных в little fs
#include <SettingsGyver.h>  // либа веб морды
#include <GyverTimer.h>     // Либа таймера
#include <Random16.h>       // умный рандом
#include <map>
#include "tetris.h"         // переменные для тетриса
#include "bitmaps_oled.h"   // битмапы
#include "menu_oled.h"      // переменные для меню
#include "pong.h"           // понг
#include "flappy_bird.h"    // птичка
#include "arkanoid.h"       // арнакоид
#include "space_invaders.h"
// ------------------
bool alert_f;               // показ ошибки в вебморде
bool wifiConnected = false; // для wifi морды
// пины
#define FREE_PIN 25         // свободный пин
#define batteryPin 34       // пин для измерения % батареи
//кнопки
#define UP_PIN 26           // верх
#define DOWN_PIN 21         // вниз
#define RIGHT_PIN 12        // право
#define LEFT_PIN 14         // влево
#define OK_PIN 22           // ОК
//----------------------------
// калькулятор
#define CALCUL_TYPE int64_t // Тип переменной зачений в калькуляторе
CALCUL_TYPE a, b, result;
uint8_t thisval, sign;      // 0-+ 1-- 2-* 3-/ 4-^
bool isdraw;
//----------------------
// ардуино дино
#define DINO_GROUND_Y 47    // Позиция динозавра по вертикали
#define DINO_GRAVITY  0.195f// Значение гравитации динозавра
#define DINO_GAME_FPS 30    // Скорость обновления дисплея
//----------------------------
// флаги
bool flag_pin_free = false;
bool flag_pin_built_in = false;
//----------------------------
// настройки
bool settings_auto_bright;
//читалка
byte cursor = 0;            // Указатель (курсор) меню
byte files = 0;             // Количество файлов
const int MAX_PAGE_HISTORY = 150;         // сколько страниц может отображать читалка(можно изменить). но, если очень много указать, то может крашнутся esp32
long pageHistory[MAX_PAGE_HISTORY] = {0};
int currentHistoryIndex = -1;
int totalPages = 0;
//-------------
//показ заряда
#define REF_VOLTAGE     3.3 // Опорное напряжение ADC (3.3V для ESP32)
#define ADC_RESOLUTION  12  // 12 бит (0-4095)
#define VOLTAGE_DIVIDER 2.0
// Напряжения для расчета процента заряда (калибровка под ваш аккумулятор). Настройки в serv меню без перепрошивки
#define BAT_NOMINAL_VOLTAGE 3.7 // Номинальное напряжение (3.7V)
#define BATTERY_PIN        34   // GPIO34 (ADC1_CH6) для измерения напряжения
float batteryVoltage = 0;
int batteryPercentage = 0;
//----------------------------------
//объекты
GyverOLED<SSD1306_128x64, OLED_BUFFER, OLED_SPI, CS, DC, RES> oled; //дисплей
// кнопки
GButton up(UP_PIN);
GButton down(DOWN_PIN);
GButton right(RIGHT_PIN);
GButton left(LEFT_PIN);
GButton ok(OK_PIN);  
//------------------                                
GyverDBFile db(&LittleFS, "/data.db");              //файл где хранятся настройки
SettingsGyver sett("CatOS " FIRMWARE_VERSION, &db); // веб морда
Random16 rnd;                                       // рандом
GTimer_ms gameTimer(GAME_SPEED);                    // Таймер игр
GTimer_ms animTimer(200);                           // Таймер анимации птицы
//отрисовка батареи
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
//------------------------
// ключи для веб морды (сохранение настроек и т.д.)
DB_KEYS(
  kk,
  OLED_BRIGHTNESS,
  BAT_MIN_VOLTAGE,
  BAT_MAX_VOLTAGE,
  AP_SSID,
  AP_PASS,
  wifi_enabled,
  wifi_ssid,
  wifi_pass,
  serial_num,
  auto_bright,
  apply
);
int getBattery() {
  int adcValue = analogRead(BATTERY_PIN);
  float bat_min = db[kk::BAT_MIN_VOLTAGE].toFloat();
  float bat_max = db[kk::BAT_MAX_VOLTAGE].toFloat();
  
  batteryVoltage = (adcValue / (pow(2, ADC_RESOLUTION) - 1)) * REF_VOLTAGE * VOLTAGE_DIVIDER;
  batteryPercentage = mapFloat(batteryVoltage, bat_min, bat_max, 0, 100);
  batteryPercentage = constrain(batteryPercentage, 0, 100);
  return batteryPercentage;
}
int getVoltage() {
  int adcValue = analogRead(BATTERY_PIN);
  batteryVoltage = (adcValue / (pow(2, ADC_RESOLUTION) - 1)) * REF_VOLTAGE * VOLTAGE_DIVIDER;
  return batteryVoltage;
}
void drawbattery() {
  int battery = getBattery(); // Обновляем заряд
  if (battery <= 10 and settings_auto_bright){
    oled.setContrast(50);
  } else {
    oled.setContrast(db[kk::OLED_BRIGHTNESS].toInt());
  }

  oled.clear(110, 0, 127, 6); 
  
  // Выбираем битмап в зависимости от уровня заряда
  if (battery <= 10) {
    oled.drawBitmap(110, 0, bmp_0, 20, 7);
  } else if (battery <= 30) {
    oled.drawBitmap(110, 0, bmp_20, 20, 7);
  } else if (battery <= 60) {
    oled.drawBitmap(110, 0, bmp_50, 20, 7);
  } else {
    oled.drawBitmap(110, 0, bmp_100, 20, 7);
  }
  oled.update();
}
//---------------------------------------------------
// красивая рамка
void ui_rama(const char* name, bool draw_b, bool draw_l, bool cleardisplay) {
  if (cleardisplay == true) {
    oled.clear(); // чити
  }
  oled.home();  // домой
  oled.setScale(1); // размер 1
  oled.print(name);  // надпись
  oled.setScale(0); // вернуть размер назад (костыль)
  if (draw_b == true) {
    drawbattery();
  }
  if (draw_l == true) {
    oled.line(0, 10, 127, 10);   // Линия
  }
  oled.update();
}
//------------------------
String constrainString(String str, uint8_t minLen, uint8_t maxLen) {
  if (str.length() < minLen) {
      return String("12345678"); // Возвращаем минимально допустимый
  }
  if (str.length() > maxLen) {
      str = str.substring(0, maxLen);
  }
  return str;
}
String getChipID() { // генерация фейк серинового номера
  uint64_t chipid = ESP.getEfuseMac();
  char sn[17];
  snprintf(sn, sizeof(sn), "%04X%08X", 
         (uint16_t)(chipid >> 32), 
         (uint32_t)chipid);
  return String(sn);
}
// В функции обновления яркости:
void update(sets::Updater& upd) {
  static uint8_t lastBrightness = db[kk::OLED_BRIGHTNESS].toInt();
  if(lastBrightness != db[kk::OLED_BRIGHTNESS].toInt()) {
      lastBrightness = db[kk::OLED_BRIGHTNESS].toInt();
      oled.setContrast(lastBrightness);
      db.update(); // Сохраняем изменение
  }
  if (alert_f) {
    alert_f = false;
    upd.alert("Пароль должен быть не менее 8 символов!");
  }
    // Проверка корректности напряжений
    float bat_min = db[kk::BAT_MIN_VOLTAGE].toFloat();
    float bat_max = db[kk::BAT_MAX_VOLTAGE].toFloat();
    
    if(bat_min >= bat_max) {
      upd.alert("Мин. напряжение должно быть меньше макс.!");
      db[kk::BAT_MIN_VOLTAGE] = 2.32;
      db[kk::BAT_MAX_VOLTAGE] = 3.45;
      db.update();
    }
}
bool connectToWiFi() {
  String ssid = db[kk::wifi_ssid];
  String pass = db[kk::wifi_pass];
  WiFi.mode(WIFI_AP_STA);
  if(ssid.isEmpty()) {
      Serial.println("WiFi SSID not configured!");
      return false;
  }
 
  // Сброс предыдущих статусов
  wifiConnected = false;
  WiFi.begin(ssid, pass);
  oled.autoPrintln(true);
  oled.clear();
  oled.home();
  oled.print("Подключение к");
  oled.setCursor(0, 2);
  oled.print(ssid);
  oled.setCursor(0, 3);
  oled.print("Статус:");
  oled.update();
  // Ожидание подключения с визуализацией
  uint32_t startTime = millis();
  // Прерываем ожидание если подключились
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    oled.print(".");
    oled.update();
    if (millis() - startTime > 10000) { // 10 second timeout
      oled.print("Не подключено! Запуск AP");
      oled.update();
      delay(1000);
      oled.autoPrintln(false);
      return false; // Return false if connection fails
    }
  }
  oled.autoPrintln(false);
  return true; // Return true if connected successfully
}
 


void startAP() {
  // Используем AP_SSID и AP_PASS вместо ap_ssid/ap_pass
  String ssid = db[kk::AP_SSID];
  String pass = db[kk::AP_PASS];
 
  if (ssid.length() == 0) {
      ssid = "CatOs";
      db[kk::AP_SSID] = ssid;
  }
  if (pass.length() < 8) {
      pass = "12345678";
      db[kk::AP_PASS] = pass;
  }
  WiFi.softAP(
    db[kk::AP_SSID].toString().c_str(),
    db[kk::AP_PASS].toString().c_str()
  );
 
}
void stopWiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi completely stopped");
}
//----------
void build(sets::Builder& b) { // страница веб морды
  // Секция настроек дисплея
  {
      sets::Group g(b, "Дисплей");
      b.Slider(kk::OLED_BRIGHTNESS, "Яркость дисплея", 0, 255, 1, "", nullptr, sets::Colors::Blue);
  }

  // Секция WiFi
  {
      sets::Group g(b, "WiFi");
      // Переключатель WiFi с привязкой к значению из БД
      bool wifiEnabled = db[kk::wifi_enabled].toInt();
      b.Switch(kk::wifi_enabled, "WiFi подключение", &wifiEnabled);
      b.Input(kk::wifi_ssid, "SSID WiFi");
      b.Pass(kk::wifi_pass, "Пароль WiFi");
  }

  // Секция точки доступа
  {
      sets::Group g(b, "Точка доступа");
      b.Input(kk::AP_SSID, "SSID точки");
      b.Pass(kk::AP_PASS, "Пароль точки");
  }
  {
    sets::Group g(b, "Калибровка АКБ");
    b.Number(kk::BAT_MIN_VOLTAGE, "Мин. напряжение");
    b.Number(kk::BAT_MAX_VOLTAGE, "Макс. напряжение");
  }
  // Секция управления
  {
      sets::Group g(b, "Управление");
      if(b.Button("Применить сетевые настройки")) {
        if (db[kk::AP_PASS].toString().length() >= 8) {
            db.update();
            sett.reload(true);
        } else {
            alert_f = true;  // Устанавливаем флаг
        }
    }
  }
}

// Новая функция инициализации настроек (проверяет есть ли в ключах значение)
void initSettings() {
  // Существующие настройки
  if (!db.has(kk::OLED_BRIGHTNESS)) db.init(kk::OLED_BRIGHTNESS, 128);
  if (!db.has(kk::BAT_MIN_VOLTAGE)) 
    db.init(kk::BAT_MIN_VOLTAGE, 2.32f);
  if (!db.has(kk::BAT_MAX_VOLTAGE)) 
    db.init(kk::BAT_MAX_VOLTAGE, 3.45f);
  if (!db.has(kk::AP_SSID)) {
    db.init(kk::AP_SSID, "CatOs");
  }
  if (!db.has(kk::AP_PASS)) {
      db.init(kk::AP_PASS, "12345678");
  }
  if (!db.has(kk::wifi_enabled)) {
      db.init(kk::wifi_enabled, 0);
  }
  if (!db.has(kk::serial_num)) {
    db.init(kk::serial_num, getChipID());
  }
  if (!db.has(kk::wifi_ssid)) db.init(kk::wifi_ssid, "");
  if (!db.has(kk::wifi_pass)) db.init(kk::wifi_pass, "");

  if (!db.has(kk::auto_bright)) db.init(kk::auto_bright, false);
  db.update();
}
void drawStaticMenu() {
  oled.clear();
  drawbattery();
  for (uint8_t i = 0; i < VISIBLE_ITEMS; i++) {
      oled.setCursor(1, i);
      oled.print(menu_items[i + top_item]);
  }
  oled.update();
}
void updatePointer() {
  static int8_t last_pointer = -1;
  static int8_t last_top = -1;

  // Если изменился видимый диапазон - полная перерисовка
  if (last_top != top_item) {
      drawStaticMenu();
      last_top = top_item;
  }

  // Стереть старый указатель
  if (last_pointer != -1) {
      oled.setCursor(0, last_pointer - top_item);
      oled.print(" ");
  }

  // Нарисовать новый указатель
  oled.setCursor(0, pointer - top_item);
  oled.print(">");
  drawbattery();
  // Обновить только измененные области
  oled.update();
  last_pointer = pointer;
}

void resetButtons() {
  ok.resetStates();
  down.resetStates();
  up.resetStates();
  left.resetStates();
  right.resetStates();
}
void buttons_tick() {
  up.tick();
  down.tick();
  right.tick();
  left.tick();
  ok.tick();
}
void exit(bool with_update = true) {
  oled.clear();
  resetButtons();
  oled.setScale(1);
  if (with_update) {
    drawStaticMenu();
    updatePointer();
  }
  drawbattery();
  oled.update();
}
bool draw_logo() {
  uint32_t tmr = millis();
  bool btn_pressed = false;
  
  oled.clear();
  oled.drawBitmap(50, 16, logo, 32, 32);
  oled.setCursor(0,0);
  oled.print("By CatDevCode " FIRMWARE_VERSION);
  oled.setCursor(0,7);
  oled.print("         CatOs");
  oled.update();

  while (millis() - tmr < 2000) {
      ok.tick(); // Опрашиваем кнопку
      if (ok.isClick()) {
          btn_pressed = true;
          break;
      }
      delay(10); // Небольшая задержка для стабильности
  }
  oled.clear();
  oled.home();
  oled.update();
  return btn_pressed;
}
//старый тест % батареи. используйте новый
//void testBattery() {
//  oled.clear();
//  while (true) {
//    int voltage = getVoltage();
//    int battery_proc = getBattery();
//    oled.setCursor(0,0);
//    oled.print("Вольтаж: ~");
//    oled.setCursor(0,1);
//    oled.print(voltage);
//    oled.setCursor(0,2);
//    oled.print("Проценты заряда: ~");
//    oled.setCursor(0,3);
//    oled.print(battery_proc);
//    oled.setCursor(0,4);
//    oled.print("Пин для измерения:");
//    oled.setCursor(0,5);
//    oled.print(BATTERY_PIN);
//    oled.setCursor(0,6);
//    oled.print("Минимальный вольтаж:");
//    oled.setCursor(0,7);
//    oled.print(BAT_MIN_VOLTAGE);
//    oled.setCursor(0,8);
//    oled.print("Максимальный вольтаж:");
//    oled.setCursor(0,9);
//    oled.print(BAT_MAX_VOLTAGE);
//    joy.read();
//    static uint32_t batteryTimer = millis(); // Таймер для батареи
//    buttons();
//
//    // Автоматическое обновление батареи каждые 30 секунд
//    if (millis() - batteryTimer > 5000) {
//        batteryTimer = millis();
//        drawbattery();
//    }
//    oled.update();
//  }
//}
void testBattery() {
  ui_rama("CatOS " FIRMWARE_VERSION, true, true, true);
  float bat_min = db[kk::BAT_MIN_VOLTAGE].toFloat();
  float bat_max = db[kk::BAT_MAX_VOLTAGE].toFloat();
  while (true) {
    int adcValue = analogRead(BATTERY_PIN);
    
    batteryVoltage = (adcValue / (pow(2, ADC_RESOLUTION) - 1)) * REF_VOLTAGE * VOLTAGE_DIVIDER;
    batteryPercentage = mapFloat(batteryVoltage, bat_min, bat_max, 0, 100);
    batteryPercentage = constrain(batteryPercentage, 0, 100);
    oled.setCursor(0,2);
    oled.print("V ~: ");
    oled.print(batteryVoltage);
    
    oled.setCursor(0,3);
    oled.print("Уровень заряда ~: ");
    oled.print(batteryPercentage);
    oled.print("%");
    
    oled.setCursor(0,4);
    oled.print("Пин: ");
    oled.print(BATTERY_PIN);
    
    oled.setCursor(0,5);
    oled.print("Мин V: ");
    oled.print(bat_min);
    
    oled.setCursor(0,6);
    oled.print("Макс V: ");
    oled.print(bat_max);

    static uint32_t batteryTimer = millis();
    buttons_tick();
    if (millis() - batteryTimer > 5000) {
        batteryTimer = millis();
        drawbattery();
    }
    oled.update();
    
    if(ok.isClick()) {
      return;
    }
  }
}
//void sound_test() {
//  oled.clear();
//  oled.print("Sound test");
//  oled.update();
//  resetButtons();
//  while (true){
//    for (int freq = 200; freq <= 2000; freq += 100) {
//      buttons_tick();
//      tone(SOUND_PIN, freq);
//      delay(150);
//      noTone(SOUND_PIN);
//      if (ok.isClick()) {
//        oled.clear();
//        oled.home();
//        oled.update();
//        return;
//      }
//    }
//  }
//}
void sysInfo() {
  oled.autoPrintln(true);
  ui_rama("CatOS " FIRMWARE_VERSION, true, true, true);
  oled.setCursor(0,2);
  oled.printf("Опиративка: %d \n", ESP.getFreeHeap());
  oled.setCursor(0,3);
  oled.printf("Размер прош.:%u \n", ESP.getSketchSize());
  oled.setCursor(0,4);
  oled.printf("Модель ESP: %s\n", ESP.getChipModel());
  oled.update();
  while(true){
    buttons_tick();
    if (ok.isClick()) {
      resetButtons();
      oled.clear();
      oled.home();
      oled.update();
      oled.autoPrintln(false);
      return;
    }
  } 
}
bool deleteSettings() {
  if (LittleFS.remove("/data.db")) {   // Удаляем файл с настройками
    db.begin();                        // Переинициализируем базу
    initSettings();                    // Создаём настройки по умолчанию
    return true;
  }
  return false;
}
void deleteSettings_ui() {
  oled.clear();
  if (deleteSettings()) {
    oled.setScale(2);
    oled.setCursorXY(10, 16);
    oled.print(F("настройки"));
    oled.setCursorXY(16, 32);
    oled.print(F("сброшены"));
    oled.setScale(1);
    oled.update();
  } else {
    oled.setScale(2);
    oled.setCursorXY(10, 16);
    oled.print(F("Ошибочка!"));
    oled.setScale(1);
    oled.update();
  }
  delay(1500);
  ESP.restart();
}
void formatFS() {
  oled.clear();
  oled.setCursor(0, 2);
  oled.print("Подтвердите сброс:");
  oled.setCursor(0, 4);
  oled.print("Удерживайте OK");
  oled.update();
  
  // Ожидание подтверждения 3 секунды
  uint32_t tmr = millis();
  while(millis() - tmr < 3000) {
    ok.tick();
    if(ok.isHold()) {
      LittleFS.format();                   // Форматируем файловую систему
      oled.clear();
      oled.setScale(2);
      oled.setCursorXY(34, 16);
      oled.print(F("файлы"));
      oled.setCursorXY(22, 32);
      oled.print(F("удалены"));
      oled.setScale(1);
      oled.update();
      delay(1500);
      ESP.restart();
    }
  } 
}
void batteryCalibration() {
  enum CalState { MIN_VOLTAGE, MAX_VOLTAGE, SAVE }; // Объявляем перечисление
  CalState state = MIN_VOLTAGE; // Используем CalState как тип
  float bat_min = db[kk::BAT_MIN_VOLTAGE].toFloat();
  float bat_max = db[kk::BAT_MAX_VOLTAGE].toFloat();
  float* current_val = &bat_min;
  
  while(true) {
    oled.clear();
    oled.setCursor(0, 0);
    oled.print("Калибровка АКБ:");
    
    // Отображение текущих значений
    oled.setCursor(0, 2);
    oled.print(state == MIN_VOLTAGE ? ">" : " ");
    oled.print("Мин: ");
    oled.print(bat_min, 2);
    oled.print("V");
    
    oled.setCursor(0, 3);
    oled.print(state == MAX_VOLTAGE ? ">" : " ");
    oled.print("Макс: ");
    oled.print(bat_max, 2);
    oled.print("V");
    
    oled.setCursor(0, 5);
    oled.print(state == SAVE ? ">" : " ");
    oled.print("Сохранить");
    
    oled.setCursor(0, 7);
    oled.print("Удержать OK - выход");
    
    oled.update();

    buttons_tick();
    
    // Навигация
    if(up.isClick()) {
      if(state > MIN_VOLTAGE) state = (CalState)(state - 1);
    }
    if(down.isClick()) {
      if(state < SAVE) state = (CalState)(state + 1);
    }
    
    // Регулировка значений
    if(ok.isClick()) {
      switch(state) {
        case MIN_VOLTAGE: current_val = &bat_min; break;
        case MAX_VOLTAGE: current_val = &bat_max; break;
        case SAVE: 
          if(bat_min < bat_max) {
            db[kk::BAT_MIN_VOLTAGE] = bat_min;
            db[kk::BAT_MAX_VOLTAGE] = bat_max;
            db.update();
            return;
          }
          break;
      }
    }
    
    // Изменение значений
    if(state != SAVE) {
      if(right.isClick()) *current_val += 0.01;
      if(left.isClick()) *current_val -= 0.01;
      
      // Ограничения
      *current_val = constrain(*current_val, 2.0, 4.9);
    }
    
    // Выход по удержанию
    if(ok.isHold()) {
      if(bat_min < bat_max) {
        db[kk::BAT_MIN_VOLTAGE] = bat_min;
        db[kk::BAT_MAX_VOLTAGE] = bat_max;
        db.update();
      }
      return;
    }
  }
}
void servmode() {
  const char* serv_apps[] = {
    "Cброс настроек",
    "Форматирование",
    "Тест % батареи",
    "Калибровка АКБ",
    "Информация о системе",
    "Выход"
  };
  const uint8_t serv_apps_count = sizeof(serv_apps)/sizeof(serv_apps[0]);
  int8_t serv_apps_ptr = 0;
  const uint8_t header_height = 16; // Высота заголовка с линией

  ui_rama("CatOS " FIRMWARE_VERSION, true, true, true);
  
  while(true) {
    static uint32_t timer = 0;
    // Очищаем только область меню (начиная с 3 строки)
    oled.clear(0, header_height, 127, 63);
    
    // Рисуем только первый видимый пункт
    oled.setCursor(2, header_height/8 + 0); // 3 строка (16px)
    oled.print(serv_apps_ptr == 0 ? ">" : " ");
    oled.print(serv_apps[0]);

    // Рисуем остальные пункты если есть
    if(serv_apps_count > 1) {
      for(uint8_t i = 1; i < serv_apps_count; i++) {
        oled.setCursor(2, header_height/8 + i); // Следующие строки
        oled.print(serv_apps_ptr == i ? ">" : " ");
        oled.print(serv_apps[i]);
      }
    }
    
    oled.update();

    buttons_tick();

    if(up.isClick() && serv_apps_ptr < 5|| (up.isHold() && millis() - timer > 150)) {
      if (serv_apps_ptr < 1) {
          serv_apps_ptr++;
      }
      timer = millis();
      serv_apps_ptr--;
    }
    if(down.isClick() && serv_apps_ptr < 5|| (down.isHold() && millis() - timer > 150)) {
      if (serv_apps_ptr >= 5) {
          serv_apps_ptr--;
      }
      timer = millis();
      serv_apps_ptr++;
    }

    if(ok.isClick()) {
      switch(serv_apps_ptr) {
        case 0: deleteSettings_ui(); break;
        case 1: formatFS(); break;
        case 2: testBattery(); break;
        case 3: batteryCalibration(); break;
        case 4: sysInfo(); break;
        case 5: ESP.restart(); // Выход
      }
      // Перерисовываем интерфейс после возврата
      ui_rama("CatOS " FIRMWARE_VERSION, true, true, true);
    }
  }
}
uint32_t generateSeed() {
    uint32_t seed = 0;
    seed ^= getBattery();
    seed ^= millis();
    seed ^= esp_random();
    seed ^= analogRead(27);
    return seed;
}
uint8_t parseHFile(uint8_t *img, File &file) {
  int imgLen = 0;
  memset(img, 0, 1024); // Очистка буфера

  // Пропускаем все символы до '{'
  while (file.available()) {
    if (file.read() == '{') break;
  }

  // Читаем данные до '}' или конца файла
  while (file.available() && imgLen < 1024) {
    char c = file.read();
    if (c == '}') break; // Конец данных
    
    // Парсим HEX-значения вида 0xXX
    if (c == '0' && file.peek() == 'x') {
      file.read(); // Пропускаем 'x'
      char hex[3] = {0};
      hex[0] = file.read();
      hex[1] = file.read();
      img[imgLen++] = strtoul(hex, NULL, 16); // Конвертируем HEX в байт
    }
    yield(); // Для стабильности ESP
  }

  return (imgLen == 1024) ? 0 : 1; // 0 = успех, 1 = ошибка
}
void setup() {
    Serial.begin(115200);
    oled.init(); 
    oled.clear(); 
    oled.setScale(1);
    Wire.setClock(800000L);
    ok.tick();
    analogReadResolution(ADC_RESOLUTION);
    analogReadResolution(27);
    #ifdef ESP32
      LittleFS.begin(true);
    #else
      LittleFS.begin();
    #endif
    db.begin();  // Читаем данные из файла
    
    // Инициализируем ключи с проверкой значений
    initSettings();
    settings_auto_bright = db[kk::auto_bright].toBool();
    // Применяем настройки
    oled.setContrast(db[kk::OLED_BRIGHTNESS].toInt());
        
    // Проверка наличия кастомного логотипа
    bool customLogoLoaded = false;
    if (LittleFS.exists("/custom_catos.h")) {
        customLogoLoaded = true;
        
        // Пытаемся загрузить кастомный логотип
        File file = LittleFS.open("/custom_catos.h", "r");
        if (file) {
            uint8_t *img = new uint8_t[1024];
            if (parseHFile(img, file) == 0) { // Успешный парсинг
                oled.clear();
                oled.drawBitmap(0, 0, img, 128, 64);
                oled.update();
                delete[] img;
                
                // Ожидание с возможностью прерывания
                uint32_t tmr = millis();
                while (millis() - tmr < 2000) {
                    ok.tick();
                    if (ok.isClick()) {
                        file.close();
                        servmode();
                        return;
                    }
                    delay(10);
                }
            } else {
                delete[] img;
                customLogoLoaded = false;
            }
            file.close();
        }
    }
    // Если во время показа логотипа нажали OK - войти в сервисный режим
    // Если кастомный логотип не загружен - стандартный экран
    if (!customLogoLoaded) {
      if (draw_logo()) {
        servmode();
      }
    }
    drawStaticMenu();
    updatePointer();
    pinMode(FREE_PIN, OUTPUT);
    pinMode(2, OUTPUT);
    uint32_t seed = generateSeed();
    rnd.setSeed(seed);
    randomSeed(seed);
    setCpuFrequencyMhz(80);
}

void snake() {
  // Настройки змейки
  const int BLOCK_SIZE = 4; // Размер блока
  const int MOVE_DELAY = 200; // Задержка движения

  // Переменные
  int snakeLength = 4;
  int snakeX[128];
  int snakeY[64];
  int foodX = rnd.get(2, 30) * BLOCK_SIZE;
  int foodY = rnd.get(2, 14) * BLOCK_SIZE;
  int headX = 20;
  int headY = 20;
  int dirX = BLOCK_SIZE;
  int dirY = 0;
  
  uint32_t snakeMoveTmr;

  // Инициализация змейки
  for (int i = 0; i < snakeLength; i++) {
    snakeX[i] = headX - i * BLOCK_SIZE;
    snakeY[i] = headY;
  }

  while (true) {
    buttons_tick();
    // Управление
    if (left.isClick() && dirX == 0) {
      dirX = -BLOCK_SIZE;
      dirY = 0;
    } else if (right.isClick() && dirX == 0) {
      dirX = BLOCK_SIZE;
      dirY = 0;
    } else if (down.isClick() && dirY == 0) {
      dirX = 0;
      dirY = BLOCK_SIZE;
    } else if (up.isClick() && dirY == 0) {
      dirX = 0;
      dirY = -BLOCK_SIZE;
    }

    // Движение
    if (millis() - snakeMoveTmr >= MOVE_DELAY) {
      snakeMoveTmr = millis();
      
      // Обновление хвоста
      for (int i = snakeLength; i > 0; i--) {
        snakeX[i] = snakeX[i - 1];
        snakeY[i] = snakeY[i - 1];
      }
      
      // Обновление головы
      headX += dirX;
      headY += dirY;
      snakeX[0] = headX;
      snakeY[0] = headY;

      // Проверка на съедение
      if (headX == foodX && headY == foodY) {
        snakeLength++;
        foodX = rnd.get(2, 30) * BLOCK_SIZE;
        foodY = rnd.get(2, 14) * BLOCK_SIZE;
      }
      
      // Отрисовка в буфер
      oled.clear(); // Очищаем экран перед отрисовкой

      // Змейка
      for (int i = 0; i < snakeLength; i++) {
        oled.rect(snakeX[i], snakeY[i], 
                 snakeX[i] + BLOCK_SIZE - 1, 
                 snakeY[i] + BLOCK_SIZE - 1);
      }
      
      // Еда
      oled.rect(foodX, foodY, 
               foodX + BLOCK_SIZE - 1, 
               foodY + BLOCK_SIZE - 1);
      
      oled.update(); // Обновляем экран только один раз за итерацию
    }

    // Выход
    if (ok.isHold()){
      exit(false);
      return;
    }  
}
}

// Space Invaders Game by VoltimeterXPayalnuk

void spaceInvadersGame() {
  resetButtons();
  
  // Переменные
  bool _is_menu_rendered = false;
  bool _is_difficulty_rendered = false;
  bool _is_game_started = false;
  unsigned long _last_time = millis();
  bool _game_over_anim = false;
  int _gamemode = GAMEMODE_CLASSIC;
  int _state = INVADERS_STATE_MENU;
  int _score = 0;
  int _lives = 3;
  int _player_x = (DISPLAY_WIDTH - PLAYER_WIDTH) / 2;
  unsigned long _enemy_spawn_time = millis();
  unsigned long _game_over_time = 0;
  
  // Настройка скорости игрока (можно менять от 1 до 5)
  int playerSpeed = 1;
  
  // Таймер для звезд (30 FPS)
  unsigned long starsLastUpdate = 0;
  const uint8_t starsInterval = 33; // 30 FPS в мс

  // Структуры
  struct Star {
    int x, y, brightness;
  };
  struct Bullet {
    float x, y;
    bool active;
  };
  struct EnemyBullet {
    float x, y;
    bool active;
  };
  struct Enemy {
    float x, y;
    bool active;
    float vx, vy;
    unsigned long last_shot_time;
  };
  
  // Массивы
  Star _stars[MAX_STARS];
  Bullet _bullets[MAX_BULLETS];
  EnemyBullet _enemy_bullets[MAX_ENEMY_BULLETS];
  Enemy _enemies[MAX_ENEMIES];
  
  // Инициализация звезд
  for (int i = 0; i < MAX_STARS; i++) {
    _stars[i].x = random(0, DISPLAY_WIDTH);
    _stars[i].y = random(0, DISPLAY_HEIGHT);
    _stars[i].brightness = random(0, 3);
  }
  
  // Инициализация массивов
  for (int i = 0; i < MAX_BULLETS; i++) _bullets[i].active = false;
  for (int i = 0; i < MAX_ENEMIES; i++) _enemies[i].active = false;
  for (int i = 0; i < MAX_ENEMY_BULLETS; i++) _enemy_bullets[i].active = false;
  
  // Создание начальных врагов
  int initialEnemies = _gamemode == GAMEMODE_CLASSIC ? 3 : 5;
  for (int i = 0; i < initialEnemies; i++) {
    for (int j = 0; j < MAX_ENEMIES; j++) {
      if (!_enemies[j].active) {
        _enemies[j].x = random(0, DISPLAY_WIDTH - ENEMY_WIDTH);
        _enemies[j].y = 0;
        _enemies[j].active = true;
        _enemies[j].vx = (random(0, 2) ? 0 : (random(0, 2) * 2 - 1) * 0.3);
        _enemies[j].vy = (_gamemode == GAMEMODE_CLASSIC) ? 0 : 0.02;
        _enemies[j].last_shot_time = millis();
        break;
      }
    }
  }
  
  // Главный цикл игры
  while(true) {
    buttons_tick();

    // Обновление звезд с фиксированным интервалом
    if (millis() - starsLastUpdate > starsInterval) {
      starsLastUpdate = millis();
      for (int i = 0; i < MAX_STARS; i++) {
        _stars[i].y += (_stars[i].brightness + 1);
        if (_stars[i].y >= DISPLAY_HEIGHT) {
          _stars[i].y = 0;
          _stars[i].x = random(0, DISPLAY_WIDTH);
          _stars[i].brightness = random(0, 3);
        }
      }
    }

    switch (_state) {
      case INVADERS_STATE_MENU: {
        if (!_is_menu_rendered) {
          oled.clear();
          
          // Отрисовка звезд
          for (int i = 0; i < MAX_STARS; i++) {
            if (_stars[i].brightness == 0) oled.dot(_stars[i].x, _stars[i].y);
            else if (_stars[i].brightness == 1) {
              oled.dot(_stars[i].x, _stars[i].y);
              oled.dot(_stars[i].x + 1, _stars[i].y);
              oled.dot(_stars[i].x, _stars[i].y + 1);
              oled.dot(_stars[i].x + 1, _stars[i].y + 1);
            } else {
              oled.rect(_stars[i].x, _stars[i].y, _stars[i].x + 1, _stars[i].y + 1, true);
            }
          }
          
          // Заголовок
          oled.setScale(2);
          oled.setCursorXY((DISPLAY_WIDTH - 12*6) / 2, 10);
          oled.print(F("SPACE"));
          oled.setCursorXY((DISPLAY_WIDTH - 12*6) / 2, 30);
          oled.print(F("INVADERS"));
          
          // Отрисовка корабля и врагов
          int playerX = DISPLAY_WIDTH/2 - PLAYER_WIDTH/2;
          int playerY = 48;
          oled.line(playerX + PLAYER_WIDTH/2, playerY, playerX, playerY + PLAYER_HEIGHT);
          oled.line(playerX + PLAYER_WIDTH/2, playerY, playerX + PLAYER_WIDTH, playerY + PLAYER_HEIGHT);
          oled.line(playerX, playerY + PLAYER_HEIGHT, playerX + PLAYER_WIDTH, playerY + PLAYER_HEIGHT);
          oled.rect(playerX + 2, playerY + PLAYER_HEIGHT - 3, playerX + PLAYER_WIDTH - 2, playerY + PLAYER_HEIGHT, true);
          
          // Враги
          oled.rect(20 + 2, 50, 20 + ENEMY_WIDTH - 2, 50 + 2, true);
          oled.line(20, 50 + 2, 20 + ENEMY_WIDTH, 50 + 2);
          oled.rect(20, 50 + 3, 20 + ENEMY_WIDTH, 50 + ENEMY_HEIGHT - 2, true);
          
          oled.rect(DISPLAY_WIDTH - 20 - ENEMY_WIDTH + 2, 50, DISPLAY_WIDTH - 20 - 2, 50 + 2, true);
          oled.line(DISPLAY_WIDTH - 20 - ENEMY_WIDTH, 50 + 2, DISPLAY_WIDTH - 20, 50 + 2);
          oled.rect(DISPLAY_WIDTH - 20 - ENEMY_WIDTH, 50 + 3, DISPLAY_WIDTH - 20, 50 + ENEMY_HEIGHT - 2, true);
          
          // Инструкции
          oled.setScale(1);
          oled.setCursor(0, 5);
          oled.print(F("OK:START"));
          oled.setCursor(80, 5);
          oled.print(F("UP:EXIT"));
          
          oled.update();
          _is_menu_rendered = true;
        }
        
        if (ok.isClick() || down.isClick()) {
          _state = INVADERS_STATE_DIFFICULTY;
          _is_difficulty_rendered = false;
        } else if (up.isClick()) {
          resetButtons();
          return;
        }
        break;
      }
        
      case INVADERS_STATE_DIFFICULTY: {
        if (!_is_difficulty_rendered) {
          oled.clear();
          
          // Отрисовка звезд
          for (int i = 0; i < MAX_STARS; i++) {
            if (_stars[i].brightness == 0) oled.dot(_stars[i].x, _stars[i].y);
            else if (_stars[i].brightness == 1) {
              oled.dot(_stars[i].x, _stars[i].y);
              oled.dot(_stars[i].x + 1, _stars[i].y);
              oled.dot(_stars[i].x, _stars[i].y + 1);
              oled.dot(_stars[i].x + 1, _stars[i].y + 1);
            } else {
              oled.rect(_stars[i].x, _stars[i].y, _stars[i].x + 1, _stars[i].y + 1, true);
            }
          }
          
          oled.setScale(2);
          oled.setCursorXY((DISPLAY_WIDTH - 12*6) / 2, 10);
          oled.print(F("GAME"));
          oled.setCursorXY((DISPLAY_WIDTH - 8*6) / 2, 25);
          oled.print(F("MODE"));
          
          oled.setScale(1);
          
          // Опция "Классический режим"
          int classicY = 50;
          oled.setCursorXY((DISPLAY_WIDTH - 8*6) / 2, classicY);
          oled.print(_gamemode == GAMEMODE_CLASSIC ? F("> CLASSIC") : F("> CLASSIC"));
          
          
          // Инструкция
          oled.setCursor(15, 5);
          oled.print(F("OK:SELECT  UP:BACK"));
          
          oled.update();
          _is_difficulty_rendered = true;
        }
        
        
        if (ok.isClick()) {
          // Сброс игры
          _score = 0;
          _lives = 3;
          _player_x = (DISPLAY_WIDTH - PLAYER_WIDTH) / 2;
          _game_over_anim = false;
          _enemy_spawn_time = millis();
          
          for (int i = 0; i < MAX_BULLETS; i++) _bullets[i].active = false;
          for (int i = 0; i < MAX_ENEMIES; i++) _enemies[i].active = false;
          if (_gamemode == GAMEMODE_EXTREME) {
            for (int i = 0; i < MAX_ENEMY_BULLETS; i++) _enemy_bullets[i].active = false;
          }
          
          // Создание начальных врагов
          int initialEnemies = _gamemode == GAMEMODE_CLASSIC ? 3 : 5;
          for (int i = 0; i < initialEnemies; i++) {
            for (int j = 0; j < MAX_ENEMIES; j++) {
              if (!_enemies[j].active) {
                if (_gamemode == GAMEMODE_CLASSIC) {
                  _enemies[j].x = random(0, DISPLAY_WIDTH - ENEMY_WIDTH);
                  _enemies[j].y = 0;
                  _enemies[j].active = true;
                  _enemies[j].vx = (random(0, 2) ? 0 : (random(0, 2) * 2 - 1) * 0.3);
                  _enemies[j].vy = 0;
                } else {
                  _enemies[j].x = random(10, DISPLAY_WIDTH - ENEMY_WIDTH - 10);
                  _enemies[j].y = random(5, 15);
                  _enemies[j].active = true;
                  _enemies[j].last_shot_time = millis();
                  _enemies[j].vx = random(2) ? 0.3 : -0.3;
                  _enemies[j].vy = 0.02;
                }
                break;
              }
            }
          }
          
          _state = INVADERS_STATE_PLAYING;
          _is_game_started = true;
        } else if (up.isClick()) {
          _state = INVADERS_STATE_MENU;
          _is_menu_rendered = false;
        }
        break;
      }
        
      case INVADERS_STATE_PLAYING: {
        buttons_tick();
    
        // Управление - измененная скорость движения
        if (left.state()) _player_x = constrain(_player_x - playerSpeed, 0, DISPLAY_WIDTH - PLAYER_WIDTH); delay(5);
        if (right.state()) _player_x = constrain(_player_x + playerSpeed, 0, DISPLAY_WIDTH - PLAYER_WIDTH); delay(5);
        
        
        // Стрельба
        if (ok.isClick()) {
          for (int i = 0; i < MAX_BULLETS; i++) {
            if (!_bullets[i].active) {
              _bullets[i].x = _player_x + PLAYER_WIDTH / 2;
              _bullets[i].y = DISPLAY_HEIGHT - PLAYER_HEIGHT - 2;
              _bullets[i].active = true;
              break;
            }
          }
        }
        
        // Выход из игры
        if (up.isHold()) {
          resetButtons();
          return;
        }

        // Отрисовка
        oled.clear();
        
        // Отрисовка звезд
        for (int i = 0; i < MAX_STARS; i++) {
          if (_stars[i].brightness == 0) oled.dot(_stars[i].x, _stars[i].y);
          else if (_stars[i].brightness == 1) {
            oled.dot(_stars[i].x, _stars[i].y);
            oled.dot(_stars[i].x + 1, _stars[i].y);
            oled.dot(_stars[i].x, _stars[i].y + 1);
            oled.dot(_stars[i].x + 1, _stars[i].y + 1);
          } else {
            oled.rect(_stars[i].x, _stars[i].y, _stars[i].x + 1, _stars[i].y + 1, true);
          }
        }
        
        oled.setScale(1);
        oled.setCursor(5, 0);
        oled.print(F("Score:"));
        oled.print(_score);
        
        oled.setCursor(80, 0);
        oled.print(F("Lives:"));
        oled.print(_lives);
        
        if (_gamemode == GAMEMODE_EXTREME) {
          oled.setCursor(60, 0);
          oled.print(F("X"));
        }
        
        // Отрисовка врагов
        for (int i = 0; i < MAX_ENEMIES; i++) {
          if (_enemies[i].active) {
            int enemyX = _enemies[i].x;
            int enemyY = _enemies[i].y;
            oled.rect(enemyX + 2, enemyY, enemyX + ENEMY_WIDTH - 2, enemyY + 2, true);
            oled.line(enemyX, enemyY + 2, enemyX + ENEMY_WIDTH, enemyY + 2);
            oled.rect(enemyX, enemyY + 3, enemyX + ENEMY_WIDTH, enemyY + ENEMY_HEIGHT - 2, true);
            oled.dot(enemyX + 2, enemyY + 4);
            oled.dot(enemyX + ENEMY_WIDTH - 2, enemyY + 4);
          }
        }
        
        // Отрисовка пуль игрока
        for (int i = 0; i < MAX_BULLETS; i++) {
          if (_bullets[i].active) {
            oled.line(_bullets[i].x, _bullets[i].y, _bullets[i].x, _bullets[i].y + BULLET_HEIGHT);
          }
        }
        
        if (_gamemode == GAMEMODE_EXTREME) {
          for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
            if (_enemy_bullets[i].active) {
              oled.line(_enemy_bullets[i].x, _enemy_bullets[i].y, _enemy_bullets[i].x, _enemy_bullets[i].y + ENEMY_BULLET_HEIGHT);
            }
          }
        }
        
        // Отрисовка игрока
        int playerX = _player_x;
        int playerY = DISPLAY_HEIGHT - PLAYER_HEIGHT;
        oled.line(playerX + PLAYER_WIDTH/2, playerY, playerX, playerY + PLAYER_HEIGHT);
        oled.line(playerX + PLAYER_WIDTH/2, playerY, playerX + PLAYER_WIDTH, playerY + PLAYER_HEIGHT);
        oled.line(playerX, playerY + PLAYER_HEIGHT, playerX + PLAYER_WIDTH, playerY + PLAYER_HEIGHT);
        oled.rect(playerX + 2, playerY + PLAYER_HEIGHT - 3, playerX + PLAYER_WIDTH - 2, playerY + PLAYER_HEIGHT, true);
        
        oled.update();

        // Логика игры с фиксированным интервалом
        if (millis() - _last_time <= 45) break;
        _last_time = millis();

        // Классический режим
        if (_gamemode == GAMEMODE_CLASSIC) {
          // Обновление пуль
          for (int i = 0; i < MAX_BULLETS; i++) {
            if (_bullets[i].active) {
              _bullets[i].y -= 2;
              if (_bullets[i].y < 0) _bullets[i].active = false;
            }
          }
          
          // Создание новых врагов
          if (millis() - _enemy_spawn_time > 2000) {
            for (int i = 0; i < MAX_ENEMIES; i++) {
              if (!_enemies[i].active) {
                _enemies[i].x = random(0, DISPLAY_WIDTH - ENEMY_WIDTH);
                _enemies[i].y = 0;
                _enemies[i].active = true;
                _enemies[i].vx = (random(0, 2) ? 0 : (random(0, 2) * 2 - 1) * 0.3);
                break;
              }
            }
            _enemy_spawn_time = millis();
          }
          
          // Обновление врагов
          for (int i = 0; i < MAX_ENEMIES; i++) {
            if (_enemies[i].active) {
              _enemies[i].y += 0.5;
              _enemies[i].x += _enemies[i].vx;
              
              if (_enemies[i].x <= 0 || _enemies[i].x + ENEMY_WIDTH >= DISPLAY_WIDTH) {
                _enemies[i].vx = -_enemies[i].vx;
              }
              
              if (_enemies[i].y > DISPLAY_HEIGHT) {
                _enemies[i].active = false;
                _lives--;
                if (_lives <= 0) {
                  _state = INVADERS_STATE_GAMEOVER;
                  _game_over_time = millis();
                }
              }
            }
          }
          
          // Проверка столкновений
          for (int i = 0; i < MAX_BULLETS; i++) {
            if (_bullets[i].active) {
              for (int j = 0; j < MAX_ENEMIES; j++) {
                if (_enemies[j].active) {
                  if (_bullets[i].x >= _enemies[j].x && 
                      _bullets[i].x <= _enemies[j].x + ENEMY_WIDTH &&
                      _bullets[i].y >= _enemies[j].y && 
                      _bullets[i].y <= _enemies[j].y + ENEMY_HEIGHT) {
                    _bullets[i].active = false;
                    _enemies[j].active = false;
                    _score += 10;
                    break;
                  }
                }
              }
            }
          }
          
          // Столкновение игрока с врагами
          for (int i = 0; i < MAX_ENEMIES; i++) {
            if (_enemies[i].active) {
              if (_player_x + 2 < _enemies[i].x + ENEMY_WIDTH &&
                  _player_x + PLAYER_WIDTH - 2 > _enemies[i].x &&
                  DISPLAY_HEIGHT - PLAYER_HEIGHT < _enemies[i].y + ENEMY_HEIGHT &&
                  DISPLAY_HEIGHT > _enemies[i].y) {
                _enemies[i].active = false;
                _lives--;
                if (_lives <= 0) {
                  _state = INVADERS_STATE_GAMEOVER;
                  _game_over_time = millis();
                }
              }
            }
          }
        } else {
          // Экстремальный режим
          for (int i = 0; i < MAX_BULLETS; i++) {
            if (_bullets[i].active) {
              _bullets[i].y -= 2;
              if (_bullets[i].y < 0) _bullets[i].active = false;
            }
          }
          
          for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
            if (_enemy_bullets[i].active) {
              _enemy_bullets[i].y += 1.5;
              if (_enemy_bullets[i].y >= DISPLAY_HEIGHT) _enemy_bullets[i].active = false;
            }
          }
          
          int activeEnemies = 0;
          for (int i = 0; i < MAX_ENEMIES; i++) if (_enemies[i].active) activeEnemies++;
          
          if (activeEnemies < 5 && millis() - _enemy_spawn_time > 2000) {
            for (int i = 0; i < MAX_ENEMIES; i++) {
              if (!_enemies[i].active) {
                _enemies[i].x = random(10, DISPLAY_WIDTH - ENEMY_WIDTH - 10);
                _enemies[i].y = random(5, 15);
                _enemies[i].active = true;
                _enemies[i].last_shot_time = millis();
                _enemies[i].vx = random(2) ? 0.3 : -0.3;
                _enemies[i].vy = 0.02;
                break;
              }
            }
            _enemy_spawn_time = millis();
          }
          
          for (int i = 0; i < MAX_ENEMIES; i++) {
            if (_enemies[i].active) {
              _enemies[i].x += _enemies[i].vx;
              _enemies[i].y += _enemies[i].vy;
              
              if (_enemies[i].x <= 0 || _enemies[i].x + ENEMY_WIDTH >= DISPLAY_WIDTH) {
                _enemies[i].vx = -_enemies[i].vx;
              }
              
              if (_enemies[i].y + ENEMY_HEIGHT >= DISPLAY_HEIGHT) {
                _enemies[i].active = false;
                _score = max(0, _score - 100);
              }
              
              // Стрельба врагов
              if (random(100) < 1 && millis() - _enemies[i].last_shot_time > 2000) {
                for (int j = 0; j < MAX_ENEMY_BULLETS; j++) {
                  if (!_enemy_bullets[j].active) {
                    _enemy_bullets[j].x = _enemies[i].x + ENEMY_WIDTH / 2;
                    _enemy_bullets[j].y = _enemies[i].y + ENEMY_HEIGHT;
                    _enemy_bullets[j].active = true;
                    break;
                  }
                }
                _enemies[i].last_shot_time = millis();
              }
            }
          }
          
          // Проверка столкновений
          for (int i = 0; i < MAX_BULLETS; i++) {
            if (_bullets[i].active) {
              for (int j = 0; i < MAX_ENEMIES; j++) {
                if (_enemies[j].active) {
                  if (_bullets[i].x >= _enemies[j].x && 
                      _bullets[i].x <= _enemies[j].x + ENEMY_WIDTH &&
                      _bullets[i].y >= _enemies[j].y && 
                      _bullets[i].y <= _enemies[j].y + ENEMY_HEIGHT) {
                    _bullets[i].active = false;
                    _enemies[j].active = false;
                    _score += 10;
                    break;
                  }
                }
              }
            }
          }
          
          for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
            if (_enemy_bullets[i].active) {
              if (_enemy_bullets[i].x >= _player_x && 
                  _enemy_bullets[i].x <= _player_x + PLAYER_WIDTH &&
                  _enemy_bullets[i].y >= DISPLAY_HEIGHT - PLAYER_HEIGHT &&
                  _enemy_bullets[i].y <= DISPLAY_HEIGHT) {
                _enemy_bullets[i].active = false;
                _lives--;
                if (_lives <= 0) {
                  _state = INVADERS_STATE_GAMEOVER;
                  _game_over_time = millis();
                }
              }
            }
          }
          
          for (int i = 0; i < MAX_ENEMIES; i++) {
            if (_enemies[i].active) {
              if (_player_x + 2 < _enemies[i].x + ENEMY_WIDTH &&
                  _player_x + PLAYER_WIDTH - 2 > _enemies[i].x &&
                  DISPLAY_HEIGHT - PLAYER_HEIGHT < _enemies[i].y + ENEMY_HEIGHT &&
                  DISPLAY_HEIGHT > _enemies[i].y) {
                _enemies[i].active = false;
                _lives--;
                if (_lives <= 0) {
                  _state = INVADERS_STATE_GAMEOVER;
                  _game_over_time = millis();
                }
              }
            }
          }
        }
        break;
      }
        
      case INVADERS_STATE_GAMEOVER: {
        if (!_game_over_anim && millis() - _game_over_time > 800) {
          resetButtons();
          oled.clear();
          
          for (int i = 0; i < MAX_STARS; i++) {
            if (_stars[i].brightness == 0) oled.dot(_stars[i].x, _stars[i].y);
            else if (_stars[i].brightness == 1) {
              oled.dot(_stars[i].x, _stars[i].y);
              oled.dot(_stars[i].x + 1, _stars[i].y);
              oled.dot(_stars[i].x, _stars[i].y + 1);
              oled.dot(_stars[i].x + 1, _stars[i].y + 1);
            } else {
              oled.rect(_stars[i].x, _stars[i].y, _stars[i].x + 1, _stars[i].y + 1, true);
            }
          }
          
          oled.setScale(2);
          oled.setCursorXY((DISPLAY_WIDTH - 9*6) / 2, 10);
          oled.print(F("GAME"));
          oled.setCursorXY((DISPLAY_WIDTH - 8*6) / 2, 30);
          oled.print(F("OVER"));
          
          oled.setScale(1);
          oled.setCursorXY((DISPLAY_WIDTH - 8*6 - String(_score).length()*6) / 2, 50);
          oled.print(F("Score: "));
          oled.print(_score);
          
          oled.setCursor(10, 5);
          oled.print(F("OK:RETRY  UP:EXIT"));
          
          oled.update();
          _game_over_anim = true;
        }
        
        if (_game_over_anim && (ok.isClick() || down.isClick())) {
          _score = 0;
          _lives = 3;
          _player_x = (DISPLAY_WIDTH - PLAYER_WIDTH) / 2;
          _game_over_anim = false;
          _enemy_spawn_time = millis();
          
          for (int i = 0; i < MAX_BULLETS; i++) _bullets[i].active = false;
          for (int i = 0; i < MAX_ENEMIES; i++) _enemies[i].active = false;
          if (_gamemode == GAMEMODE_EXTREME) {
            for (int i = 0; i < MAX_ENEMY_BULLETS; i++) _enemy_bullets[i].active = false;
          }
          
          int initialEnemies = _gamemode == GAMEMODE_CLASSIC ? 3 : 5;
          for (int i = 0; i < initialEnemies; i++) {
            for (int j = 0; j < MAX_ENEMIES; j++) {
              if (!_enemies[j].active) {
                if (_gamemode == GAMEMODE_CLASSIC) {
                  _enemies[j].x = random(0, DISPLAY_WIDTH - ENEMY_WIDTH);
                  _enemies[j].y = 0;
                  _enemies[j].active = true;
                  _enemies[j].vx = (random(0, 2) ? 0 : (random(0, 2) * 2 - 1) * 0.3);
                  _enemies[j].vy = 0;
                } else {
                  _enemies[j].x = random(10, DISPLAY_WIDTH - ENEMY_WIDTH - 10);
                  _enemies[j].y = random(5, 15);
                  _enemies[j].active = true;
                  _enemies[j].last_shot_time = millis();
                  _enemies[j].vx = random(2) ? 0.3 : -0.3;
                  _enemies[j].vy = 0.02;
                }
                break;
              }
            }
          }
          
          _state = INVADERS_STATE_PLAYING;
        } else if (_game_over_anim && up.isClick()) {
          _state = INVADERS_STATE_MENU;
          _is_menu_rendered = false;
        }
        break;
      }
    }
  }
}
void PlayDinosaurGame(void) {
  down.setTimeout(160);         // Настраиваем удобные таймауты удержания
  ok.setTimeout(160);
  ok.setStepTimeout(160);

startDinoGame:                         // Начало игры
  uint8_t gameSpeed = 10;              // Скорость игры
  uint16_t score = 0;                  // Текущий счет
  uint16_t bestScore = 0;              // Рекорд
  int8_t oldEnemyPos = 128;            // Позиция старого противника (тот, что уже заходит за горизонт)
  int8_t oldEnemyType = 0;             // Тип старого противника (тот, что уже заходит за горизонт)
  int8_t newEnemyPos = 128;            // Позиция нового противника (тот, что только выходит изза горизонта)
  int8_t newEnemyType = rnd.get(0, 3);  // Тип нового противника - определяем случайно
  bool dinoStand = true;               // Динозавр стоит на земле
  bool legFlag = true;                 // Флаг переключения ног динозавра
  bool birdFlag = true;                // Флаг взмахов птицы
  int8_t dinoY = DINO_GROUND_Y;        // Позиция динозавра по вертикали (изначально на земле)
  float dinoU = 0.0;                   // Скорость динозавра (вектор направлен вниз)


  while (1) {      
    buttons_tick();                                             // Бесконечный цикл игры
    if (left.isClick()) { // Клик кнопки влево мгновенно возвращает нас в игровое меню
      down.setTimeout(300);
      ok.setTimeout(300);
      ok.setStepTimeout(400);
      exit(false);
      return;
    }                               

    /* ------------------ User input ------------------ */
    if ((ok.isClick()) and dinoY == DINO_GROUND_Y) {                           // Клик по ОК и динозавр стоит на земле (слабый прыжок)
      dinoU = -2.8;                                                          // Прибавляем скорости по направлению вверх
      dinoY -= 4;                                                            // Подкидываем немного вверх
    } else if ((ok.isHolded() or ok.isStep()) and dinoY == DINO_GROUND_Y) {  // Удержание ОК и динозавр стоит на земле (сильный прыжок)
      dinoU = -3.4;                                                          // Прибавляем скорости по направлению вверх
      dinoY -= 4;                                                            // Подкидываем немного вверх
    } else if (down.state()) {                                               // Нажатие ВНИЗ
      dinoU = 3.2;                                                           // Прибавляем скорости по направлению к земле
      if (dinoY >= DINO_GROUND_Y) {                                          // Если динозавр коснулся земли
        dinoY = DINO_GROUND_Y;                                               // Ставим его на землю
        dinoU = 0.0;                                                         // Обнуляем скорость
      }
    }

    if (down.isHold() and dinoY >= DINO_GROUND_Y) {                          // Удержание ВНИЗ и дино стоит на земле
      dinoStand = false;                                                     // Переходим в присяд
    } else {
      dinoStand = true;                                                      // Иначе встаем обратно
    }

    /* ------------------ Game processing ------------------ */
    static uint32_t scoreTimer = millis();                                   // Таймер подсчета очков
    if (millis() - scoreTimer >= 100) {
      scoreTimer = millis();
      score++;                                                               // Увеличиваем счет
      gameSpeed = constrain(map(score, 1000, 0, 4, 10), 4, 10);              // Увеличиваем скорость игры! (10 - медленно, 4 - очень быстро)
    }

    static uint32_t enemyTimer = millis();                                   // Таймер кинематики противников
    if (millis() - enemyTimer >= gameSpeed) {                                // Его период уменьшается с ростом счета
      enemyTimer = millis();
      if (--newEnemyPos < 16) {                                              // Как только НОВЫЙ противник приближается к динозавру
        oldEnemyPos = newEnemyPos;                                           // Новый противник становится старым
        oldEnemyType = newEnemyType;                                         // И копирует тип нового к себе
        newEnemyPos = 128;                                                   // Между тем новый противник выходит изза горизонта
        newEnemyType = rnd.get(0, 3);                                         // Имея каждый раз новый случайный тип
      }
      if (oldEnemyPos >= -24) {                                              // Двигаем старый пока он полностью не скроется за горизонтом
        oldEnemyPos--;                                                       // Двигаем старый
      }
    }

    static uint32_t legTimer = millis();                                     // Таймер анимации ног динозавра
    if (millis() - legTimer >= 130) {
      legTimer = millis();
      legFlag = !legFlag;                                                    // Он просто переключает флаг
    }

    static uint32_t birdTimer = millis();                                    // Таймер анимации взмахов птицы
    if (millis() - birdTimer >= 200) {
      birdTimer = millis();
      birdFlag = !birdFlag;                                                  // Он тоже просто переключает флаг!
    }

    static uint32_t dinoTimer = millis();                                    // Таймер кинематики динозавра
    if (millis() - dinoTimer >= 15) {                                        // С периодом DT
      dinoTimer = millis();
      dinoU += (float)DINO_GRAVITY;                                          // Увеличиваем скорость
      dinoY += (float)dinoU;                                                 // И соответственно координату (динозавр падает)
      if (dinoY >= DINO_GROUND_Y) {                                          // При касании с землей
        dinoY = DINO_GROUND_Y;                                               // Ставим динозвра на землю
        dinoU = 0.0;                                                         // Тормозим его до нуля
      }
    }

    /* ------------------ Drawing ------------------ */
    static uint32_t oledTimer = millis();                                    // Таймер отрисовки игры!
    if (millis() - oledTimer >= (1000 / DINO_GAME_FPS)) {                    // Привязан к FPS игры
      oledTimer = millis();

      oled.clear();                                                                                     // Чистим дисплей                                                                                  // Проверка и рисование батарейки
      oled.setCursor(0, 0); oled.print("HI");                                                           // Выводим рекорд
      oled.setCursor(13, 0); oled.print(bestScore); oled.print(":"); oled.print(score);                 // Рекорд:текущий счет
      oled.line(0, 63, 127, 63);                                                                        // Рисуем поверхность земли (линия)

      switch (oldEnemyType) {                                                                           // Выбираем старого противника
        case 0: oled.drawBitmap(oldEnemyPos, 48, CactusSmall_bmp, 16, 16);                   break;     // Рисуем маленький кактус
        case 1: oled.drawBitmap(oldEnemyPos, 48, CactusBig_bmp, 24, 16);                     break;     // Рисуем большой кактус
        case 2: oled.drawBitmap(oldEnemyPos, 35, birdFlag ? BirdL_bmp : BirdR_bmp, 24, 16);  break;     // Рисуем птицу (выбираем одну из двух картинок для анимации)
      }

      switch (newEnemyType) {                                                                           // Выбираем нового противника
        case 0: oled.drawBitmap(newEnemyPos, 48, CactusSmall_bmp, 16, 16);                     break;   // Рисуем маленький кактус
        case 1: oled.drawBitmap(newEnemyPos, 48, CactusBig_bmp, 24, 16);                       break;   // Рисуем большой кактус
        case 2: oled.drawBitmap(newEnemyPos, 35, birdFlag ? BirdL_bmp : BirdR_bmp, 24, 16);    break;   // Рисуем птицу (выбираем одну из двух картинок для анимации)
      }

      if (oldEnemyPos <= 16 and oldEnemyPos >= (oldEnemyType > 0 ? -24 : -16)) {                        // Если противник в опасной зоне (Отслеживаем столкновения)
        if (oldEnemyType != 2 ? dinoY > 32 : dinoStand and dinoY > 19) {                                // Выбираем условие столкновения в зависимости от типа противника 
          oled.drawBitmap(0, dinoY, DinoStandDie_bmp, 16, 16);                                          // Столкнулись - рисуем погибшего динозавра :(  
          oled.roundRect(0, 10, 127, 40, OLED_CLEAR); oled.roundRect(0, 10, 127, 40, OLED_STROKE);      // Очищаем и обводим область
          oled.setScale(2); oled.setCursor(7, 2); oled.print(F("GAME OVER!"));                          // Выводим надпись   
          oled.setScale(1); oled.setCursor(3, 4); oled.print(F("<- MENU"));                             // Выводим подсказку
          oled.setCursor(83, 4); oled.print(F("PLAY ->"));                                              // Выводим подсказку
          oled.update();                                                                                // Отрисовка картинки на дисплей                                     // Если новый рекорд - обновляем его
          while (1) {        
            buttons_tick();                                                                           // Бесконечный цикл 
            if (right.isClick()) goto startDinoGame;                                                    // Нажали на правую - начинаем сначала
            if (left.isClick()) {
              down.setTimeout(300);
              ok.setTimeout(300);
              ok.setStepTimeout(400);
              exit(false);
              return;
            }                                                            // Нажали на левую - вернулись в меню                                                                          // Уходим в сон по необходимости
            }
          }
        }
      }

      if (dinoStand) {                                                                                  // Если все окей, столкновения нет и дино стоит в полный рост
        oled.drawBitmap(0, dinoY, legFlag ? DinoStandL_bmp : DinoStandR_bmp, 16, 16);                   // Выводим в полный рост с анимацией переступания  
      } else {                                                                                          // Дино пригнулся
        oled.drawBitmap(0, 56, legFlag ? DinoCroachL_bmp : DinoCroachR_bmp, 16, 8);                     // Выводим пригнувшимся, тоже с анимацией ног
      }

      oled.update();                                                                                    // Финальная отрисовка на дисплей
    }
}

void control_gpio(bool pin) { // 0(false) - Встроенный (синий), 1(true) - Свободный FREE_PIN. Так сделанно для экономии пямяти
  String free_pin_str = "Управ. " + String(FREE_PIN) + " пином";
  if(pin) {
    ui_rama(free_pin_str.c_str(), true, true, true);
    oled.setScale(4);
    oled.setCursor(0,2);
    if (!flag_pin_free){
      oled.print("ВЫКЛ");
    } else {
      oled.print("ВКЛ ");
    }
    oled.update();
    while (true)
    {
      buttons_tick();
      if (ok.isClick()) {
        flag_pin_free = !flag_pin_free;
        oled.setScale(4);
        oled.setCursor(0,2);
        if (!flag_pin_free){
          digitalWrite(FREE_PIN, LOW);
          oled.print("ВЫКЛ");
        } else {
          digitalWrite(FREE_PIN, HIGH);
          oled.print("ВКЛ ");
        }
        oled.update();
      }
      if (ok.isHold()){
        exit(false);
        return;
      }
    }
  } else {
    ui_rama("Управ. встр. пин.", true, true, true); 
    oled.setScale(4);
    oled.setCursor(0,2);
    if (!flag_pin_built_in){
      oled.print("ВЫКЛ");
    } else {
      oled.print("ВКЛ ");
    }
    oled.update();
    while (true)
    {
      buttons_tick();
      if (ok.isClick()) {
        flag_pin_built_in = !flag_pin_built_in;
        oled.setScale(4);
        oled.setCursor(0,2);
        if (!flag_pin_built_in){
          digitalWrite(2, LOW);
          oled.print("ВЫКЛ");
        } else {
          digitalWrite(2, HIGH);
          oled.print("ВКЛ ");
        }
        oled.update();
      }
      if (ok.isHold()){
        exit(false);
        return;
      }
    }
  }
}

void choice_gpio() {
  String free_pin_str = "Свободный (" + String(FREE_PIN) + ")";
  const char* choice_gpio_items[] = {
    "Встроенный (синий)",
    free_pin_str.c_str(),
    "Выход"
  };
  const uint8_t choice_gpio_count = sizeof(choice_gpio_items)/sizeof(choice_gpio_items[0]);
  int8_t choice_gpio_ptr = 0;
  const uint8_t header_height = 16; // Высота заголовка с линией

  ui_rama("Выбор GPIO", true, true, true);  
  while(true) {
    // Очищаем только область меню (начиная с 3 строки)
    oled.clear(0, header_height, 127, 63);
    
    // Рисуем только первый видимый пункт
    oled.setCursor(2, header_height/8 + 0); // 3 строка (16px)
    oled.print(choice_gpio_ptr == 0 ? ">" : " ");
    oled.print(choice_gpio_items[0]);

    // Рисуем остальные пункты если есть
    if(choice_gpio_count > 1) {
      for(uint8_t i = 1; i < choice_gpio_count; i++) {
        oled.setCursor(2, header_height/8 + i); // Следующие строки
        oled.print(choice_gpio_ptr == i ? ">" : " ");
        oled.print(choice_gpio_items[i]);
      }
    }
    
    oled.update();

    buttons_tick();

    if(up.isClick() && choice_gpio_ptr > 0) {
      choice_gpio_ptr--;
    }
    if(down.isClick() && choice_gpio_ptr < choice_gpio_count - 1) {
      choice_gpio_ptr++;
    }
    if(ok.isClick()) {
      switch(choice_gpio_ptr) {
        case 0: control_gpio(false); break;
        case 1: control_gpio(true); break;
        case 2: exit(false); return;
      }
      // Перерисовываем интерфейс после возврата
      ui_rama("Выбор GPIO", true, true, true);
    }
  }
}
/* Написано Матвеем и Андреем Бородиными, потому что мы можем в Си. */
// https://github.com/Nich1con/microReader/pull/11
// https://github.com/x4m
double display = 0;      // Число на экране
double accumulator = 0;  // Аккумулятор для +, -, *, /
int op = 0;              // Операции 1 +, 2 -, 3 *, 4 /
int button_x = 0;        // Выбранная кнопка по горизонтали
int button_y = 0;        // Выбранная кнопка по вертикали

void drawDisplay() {
  oled.clear();
  // Число из display (может нарисовать и аккумулятор с операций? места на экране ещё полно)
  oled.setCursor(1, 1);
  oled.print(display);

  // Рисуем клавиатуру
  oled.setCursor(5, 3);
  oled.print(F("C = + -"));
  oled.setCursor(5, 4);
  oled.print(F("1 2 3 *"));
  oled.setCursor(5, 5);
  oled.print(F("4 5 6 /"));
  oled.setCursor(5, 6);
  oled.print(F("7 8 9 0"));

  oled.roundRect(3 + button_y * 12, 22 + button_x * 8, 13 + button_y * 12, 32 + button_x * 8, OLED_STROKE);
  oled.update();
}

void okButtonClick() {
  // Ифы, ифчики, ифята
  if (button_x == 0) {
    if (button_y == 0) {
      // C
      display = 0;
    } else if (button_y == 1) {
      // =
      if (op == 1) {
        display = display + accumulator;
      }
      if (op == 2) {
        display = accumulator - display;
      }
      if (op == 3) {
        display = accumulator * display;
      }
      if (op == 4) {
        display = accumulator / display;
      }
      op = 0;
    } else if (button_y == 2) {
      // +
      accumulator = display;
      display = 0;
      op = 1;
    } else if (button_y == 3) {
      // -
      accumulator = display;
      display = 0;
      op = 2;
    }
  } else if (button_x == 1) {
    if (button_y == 0) {
      //1
      display = display * 10 + 1;
    } else if (button_y == 1) {
      //2
      display = display * 10 + 2;
    } else if (button_y == 2) {
      //3
      display = display * 10 + 3;
    } else if (button_y == 3) {
      //*
      accumulator = display;
      display = 0;
      op = 3;
    }

  } else if (button_x == 2) {
    if (button_y == 0) {
      //4
      display = display * 10 + 4;
    } else if (button_y == 1) {
      //5
      display = display * 10 + 5;
    } else if (button_y == 2) {
      //6
      display = display * 10 + 6;
    } else if (button_y == 3) {
      // /
      accumulator = display;
      display = 0;
      op = 4;
    }

  } else if (button_x == 3) {
    if (button_y == 0) {
      //7
      display = display * 10 + 7;
    } else if (button_y == 1) {
      //8
      display = display * 10 + 8;
    } else if (button_y == 2) {
      //9
      display = display * 10 + 9;
    } else if (button_y == 3) {
      // 0
      display = display * 10;
    }
  }
  // Элсы, элсики, элсята
}

void calc(void) {
  while (true) {
    oled.clear();
    oled.update();
    drawDisplay();
    while (true) {
      buttons_tick();
      
      if (up.isClick()) {
        button_x--;
        if (button_x < 0) button_x = 3;
      }
      if (down.isClick()) {
        button_x++;
        if (button_x > 3) button_x = 0;
      }
      if (left.isClick()) {
        button_y--;
        if (button_y < 0) button_y = 3;
      }
      if (right.isClick()) {
        button_y++;
        if (button_y > 3) button_y = 0;
      }
      
      if (ok.isClick()) {
        okButtonClick();
      }

      if (ok.isHold()) {
        exit();
        return;
      }

      drawDisplay();
      yield();
    }
  }
}
void test(void) {
  resetButtons();
  oled.clear();
  oled.home();
  oled.setCursor(0,5);
  oled.print("Зажмите на OK для");
  oled.setCursor(0,6);
  oled.print("выхода");
  oled.update();
  oled.setScale(2);
  delay(100);
  while (1) {
    buttons_tick();
    if (up.isPress()){
      oled.clear();
      oled.setCursor(0, 2);
      oled.print("Верх!");
      oled.update();
    }
    if (down.isPress()){
      oled.clear();
      oled.setCursor(0, 2);
      oled.print("Вниз!");
      oled.update();
    }
    if (left.isPress()){
      oled.clear();
      oled.setCursor(0, 2);
      
      oled.print("<- Влево!");
      oled.update();
    }
    if (right.isPress()){
      oled.clear();
      oled.setCursor(0, 2);
      
      oled.print("Вправо! ->");
      oled.update();
    }
    if (ok.isPress()){
      oled.clear();
      oled.setCursor(0, 2);
      oled.print("ОК!");
      oled.update();
    }
    if (ok.isHold()){
      exit();
      return;
    }  
  }
}
void create_settings() {
  String ssid = db[kk::wifi_ssid];
  bool wifi_connected = false;
  
    // Замените старый код подключения WiFi на:
    if(db[kk::wifi_enabled].toInt()) {
      connectToWiFi();
      if(ssid.isEmpty()) {
        Serial.println("WiFi SSID not configured!");
        startAP();
      }
      if(WiFi.status() == WL_CONNECTED) {
          // Успешное подключение
          Serial.print("Connected! IP: ");
          Serial.println(WiFi.localIP());
          wifiConnected = true;
      } else {
          // Не удалось подключиться - запуск AP
          startAP();
      }
  } else {
      // WiFi отключен в настройках
      startAP();
  }
 
  // Основной интерфейс настроек
  oled.clear();
  ui_rama("WiFi Веб", true, true, true);
  oled.setCursor(0, 2);
  if (wifiConnected) {
    oled.print("IP:");
    oled.print(WiFi.localIP());
    oled.setCursor(0, 3);
    oled.print("Сеть: ");
    oled.print(ssid);
  } else {
    oled.setCursor(0, 2);
    oled.print("IP: " + WiFi.softAPIP().toString());
    oled.setCursor(0, 3);
    oled.print("Сеть: " + db[kk::AP_SSID].toString());
    oled.setCursor(0, 4);
    oled.print("Пароль: " + db[kk::AP_PASS].toString());
    oled.update();
  }
  oled.setCursor(0, 6);
  oled.print("OK - Перезагрузка");
  oled.update();
  sett.setVersion(FIRMWARE_VERSION);
  // Запуск веб-сервера
  sett.begin();
  sett.onBuild(build);
 
  while(true) {
      sett.tick();
      delay(10);
      buttons_tick();
      if(ok.isClick()) {
          ESP.restart();
      }
  }
}
boolean checkButtons() {
  if (up.isClick()) button_rev = 3;
  if (down.isClick()) button_rev = 1;
  if (ok.isClick()) button_rev = 0;

  if (button_rev != 4) return true;
  return false;
}
// новый раунд
void newGameTetris() {
  //Serial.println("lolkek");   // без этого работает некорректно! магия  // Работает и так
  delay(10);
  button_rev = 4;
  height = HEIGHT;    // высота = высоте дисплея
  pos = WIDTH / 2;    // фигура появляется в середине
  fig = rnd.get(7);    // выбираем слулчайно фигуру
  ang = rnd.get(4);    // и угол поворота
  color = 2;

  // возвращаем обычную скорость падения
  gameTimer.setInterval(GAME_SPEED);
  down_flag = false;  // разрешаем ускорять кнопкой "вниз"
  delay(10);
}
// функция, отрисовывающая фигуру заданным цветом и под нужным углом
void drawFigure(byte figure, byte angle, byte x, byte y, uint32_t color) {
  oledbuf[x + y * WIDTH] = color; // записать в массив
  oled.rect((SEGMENT * (HEIGHT - 1 - y)), X0 + (SEGMENT * x), (SEGMENT * (HEIGHT - 1 - y) + SEGMENT) - 1, X0 + (SEGMENT * x + SEGMENT) - 1, color >= 1 ? OLED_STROKE : OLED_CLEAR); // рисуем точку начала координат фигуры
  int8_t X, Y;                      // вспомогательные
  for (byte i = 0; i < 3; i++) {    // рисуем 4 точки фигуры
    // что происходит: рисуем фигуру относительно текущей координаты падающей точки
    // просто прибавляем "смещение" из массива координат фигур
    // для этого идём в прогмем (функция pgm_read_byte)
    // обращаемся к массиву по адресу &figures
    // преобразовываем число в int8_t (так как progmem работает только с "unsigned"
    // angle * 3 + i - обращаемся к координатам согласно текущему углу поворота фигуры

    X = x + (int8_t)pgm_read_byte(&figures[figure][angle * 3 + i][0]);
    Y = y + (int8_t)pgm_read_byte(&figures[figure][angle * 3 + i][1]);
    if (Y > HEIGHT - 1) continue;   // если выходим за пределы поля, пропустить отрисовку
    oledbuf[X + Y * WIDTH] = color; // записать в массив
    oled.rect((SEGMENT * (HEIGHT - 1 - Y)), X0 + (SEGMENT * X), (SEGMENT * (HEIGHT - 1 - Y) + SEGMENT) - 1, X0 + (SEGMENT * X + SEGMENT) - 1, color >= 1 ? OLED_STROKE : OLED_CLEAR); // нарисовать
  }
}
// поиск и очистка заполненных уровней
void checkAndClear() {
  linesToClear = 1;                 // счётчик заполненных строк по вертикали. Искусственно принимаем 1 для работы цикла
  boolean full_flag = true;         // флаг заполненности
  while (linesToClear != 0) {       // чисти чисти пока не будет чисто!
    linesToClear = 0;
    byte lineNum = 255;       // высота, с которой начинаются заполненные строки (искусственно увеличена)
    for (byte Y  = 0; Y < HEIGHT; Y++) {   // сканируем по высоте
      full_flag = true;                   // поднимаем флаг. Будет сброшен, если найдём чёрный пиксель
      for (byte X = 0; X < WIDTH; X++) {  // проходимся по строкам
        if (oledbuf[X + Y * WIDTH] == 0) { // если хоть один пиксель чёрный
          full_flag = false;                                 // считаем строку неполной
        }
      }
      if (full_flag) {        // если нашлась заполненная строка
        linesToClear++;       // увеличиваем счётчик заполненных строк
        if (lineNum == 255)   // если это первая найденная строка
          lineNum = Y;        // запоминаем высоту. Значение 255 было просто "заглушкой"
      } else {                // если строка не полная
        if (lineNum != 255)   // если lineNum уже не 255 (значит строки были найдены!!)
          break;              // покинуть цикл
      }
    }
    if (linesToClear > 0) {             // если найденных полных строк больше 1
      lineCleanCounter += linesToClear;   // суммируем количество очищенных линий (игровой "счёт")

      // заполняем весь блок найденных строк белым цветом слева направо
      for (byte X = 0; X < WIDTH; X++) {
        for (byte i = 0; i < linesToClear; i++) {
          oled.clear((SEGMENT * (HEIGHT - 1 - (lineNum + i))), X0 + (SEGMENT * X), (SEGMENT * (HEIGHT - 1 - (lineNum + i)) + SEGMENT) - 1, X0 + (SEGMENT * X + SEGMENT) - 1); // стираем его
          oledbuf[X + (lineNum + i)*WIDTH] = 0;
        }
        oled.update();
        delay(5);     // задержка между пикселями слева направо
      }
      delay(1000);

      // и теперь смещаем вниз все пиксели выше уровня с первой найденной строкой
      for (byte i = 0; i < linesToClear; i++) {
        for (byte Y = lineNum; Y < HEIGHT - 1; Y++) {
          for (byte X = 0; X < WIDTH; X++) {
            oled.rect((SEGMENT * (HEIGHT - 1 - Y)), X0 + (SEGMENT * X), (SEGMENT * (HEIGHT - 1 - Y) + SEGMENT) - 1, X0 + (SEGMENT * X + SEGMENT) - 1, oledbuf[X + (Y + 1)*WIDTH] == 0 ? OLED_CLEAR : OLED_STROKE); // сдвигаем вниз
            oledbuf[X + Y * WIDTH] = oledbuf[X + (Y + 1) * WIDTH]; // сдвигаем вниз
          }
          oled.update();
        }
        delay(100);       // задержка между "сдвигами" всех пикселей на один уровень
      }
    }
  }
  gameTimer.reset();
}
// функция, которая удаляет текущую фигуру (красит её чёрным)
// а затем перерисовывает её в новом положении
void redrawFigure(int8_t clr_ang, int8_t clr_pos, int8_t clr_height) {
  drawFigure(fig, clr_ang, clr_pos, clr_height, 0);            // стереть фигуру
  drawFigure(fig, ang, pos, height, color);                           // отрисовать
  oled.update();
}
// проигрыш
void gameOverTetris() {
  oled.clear();
  oled.update();
  for (int i = 0; i < MAX_WIDTH * MAX_HEIGHT; i++) oledbuf[i] = 0;

  // тут можно вывести счёт lineCleanCounter
  oled.home();
  oled.print(lineCleanCounter);
  oled.update();
  delay(1000);
  lineCleanCounter = 0;   // сброс счёта
  delay(20);
  newGameTetris();
}
// проверка на столкновения
boolean checkArea(int8_t check_type) {
  // check type:
  // 0 - проверка лежащих фигур и пола
  // 1 - проверка стенки справа и фигур
  // 2 - проверка стенки слева и фигур
  // 3 - проверка обеих стенок и пола

  boolean flag = true;
  int8_t X, Y;
  boolean offset = 1;
  int8_t this_ang = ang;

  // этот режим для проверки поворота. Поэтому "поворачиваем"
  // фигуру на следующий угол чтобы посмотреть, не столкнётся ли она с чем
  if (check_type == 3) {
    this_ang = ++this_ang % 4;
    offset = 0;   // разрешаем оказаться вплотную к стенке
  }

  for (byte i = 0; i < 4; i++) {
    // проверяем точки фигуры
    // pos, height - координаты главной точки фигуры в ГЛОБАЛЬНОЙ системе координат
    // X, Y - координаты остальных трёх точек в ГЛОБАЛЬНОЙ системе координат
    if (i == 0) {   // стартовая точка фигуры (начало отсчёта)
      X = pos;
      Y = height;
    } else {        // остальные три точки
      // получаем глобальные координаты точек, прибавив их положение в
      // системе координат главной точки к координатам самой главной
      // точки в глобальной системе координат. Если ты до сюда дочитал,
      // то стукни мне [Гайверу] на почту alex@alexgyver.ru . Печенек не дам, но ты молодец!
      X = pos + (int8_t)pgm_read_byte(&figures[fig][this_ang * 3 + i - 1][0]);
      Y = height + (int8_t)pgm_read_byte(&figures[fig][this_ang * 3 + i - 1][1]);

      // дичь дикая! Но на самом деле просто восстанавливаем из прогмема данные
      // и берём нужное число в массиве. Откуда все эти * 3 -1 ... можно додуматься
    }

    // границы поля
    if (check_type == 1 || check_type == 3) {
      if (X + 1 > WIDTH - 1) flag = false;    // смотрим следующий справа
      uint32_t getColor;
      if (Y < HEIGHT)
        getColor = oledbuf[X + offset + Y * WIDTH];
      if (getColor != 2 && getColor != 0) {
        flag = false;         // если не СВОЙ цвет и не чёрный
      }
    }

    if (check_type == 2 || check_type == 3) {
      if (X - 1 < 0) flag = false;    // смотрим следующий слева
      uint32_t getColor;
      if (Y < HEIGHT)
        getColor = oledbuf[X - offset + Y * WIDTH];
      if (getColor != 2 && getColor != 0) {
        flag = false;         // если не СВОЙ цвет и не чёрный
      }
    }

    if (check_type == 0 || check_type == 3) {
      uint32_t getColor;
      if (Y < HEIGHT) {
        getColor = oledbuf[X + (Y - 1) * WIDTH];
        if (getColor != 2 && getColor != 0) {
          flag = false;         // если не СВОЙ цвет и не чёрный
        }
      }
    }
  }
  return flag;    // возвращаем глобальный флаг, который говорит о том, столкнёмся мы с чем то или нет
}
// управление фигурами вправо и влево
void stepRight() {
  if (checkArea(1)) {
    prev_pos = pos;
    if (++pos > WIDTH) pos = WIDTH;
    redrawFigure(ang, prev_pos, height);
  }
}
void stepLeft() {
  if (checkArea(2)) {
    prev_pos = pos;
    if (--pos < 0) pos = 0;
    redrawFigure(ang, prev_pos, height);
  }
}
// функция фиксации фигуры
void fixFigure() {
  color = 1;                   // чутка перекрасить
  redrawFigure(ang, pos, prev_height);  // перерисовать
}
void tetrisRoutine() {
  if (loadingFlag) {
    oled.clear();
    loadingFlag = false;
    newGameTetris();
  }

  if (checkButtons()) {

    if (button_rev == 3) {   // кнопка нажата
      button_rev = 4;
      stepLeft();
    }

    if (button_rev == 1) {
      button_rev = 4;
      stepRight();
    }

    if (button_rev == 0) {
      button_rev = 4;
      if (checkArea(3)) {       // проверка возможности поворота
        prev_ang = ang;         // запоминаем старый угол
        ang = ++ang % 4;        // изменяем ang от 0 до 3 (да, хитро)
        redrawFigure(prev_ang, pos, height);    // перерисовать фигуру
      }
    }
    /*
        if (buttons == 2) {             // кнопка вниз удерживается
          buttons = 4;
          gameTimer.setInterval(FAST_SPEED);  // увеличить скорость
        }*/
  }


  if (up.isStep()) {    // кнопка нажата и удерживается
    stepLeft();
  }
  if (down.isStep()) {
    stepRight();
  }


  if (gameTimer.isReady()) {        // главный таймер игры
    prev_height = height;

    if (!checkArea(0)) {            // проверяем столкновение с другими фигурами
      if (height >= HEIGHT - 2) {   // проиграл по высоте
        gameOverTetris();                 // игра окончена, очистить всё
        newGameTetris();                 // новый раунд
      } else {                      // если не достигли верха
        fixFigure();                // зафиксировать
        checkAndClear();            // проверить ряды и очистить если надо
        newGameTetris();                 // новый раунд
      }
    } else if (height == 0) {       // если достигли дна
      fixFigure();                  // зафиксировать
      checkAndClear();              // проверить ряды и очистить если надо
      newGameTetris();                   // новый раунд
    } else {                        // если путь свободен
      height--;                             // идём вниз
      redrawFigure(ang, pos, prev_height);  // перерисовка
    }
  }
}

void playTetrisGame() {
  //loadingFlag = true;
  oled.clear();
  lineCleanCounter = 0;
  for (int i = 0; i < MAX_WIDTH * MAX_HEIGHT; i++) oledbuf[i] = 0;
  while (1) {
    ok.tick();
    up.tick();
    down.tick();

    drawbattery();                                                                                      // Рисуем индикатор
    oled.line(0, 10, 127, 10);   // Линия
    //oled.line(127, 63, 127, 16);  // Опять линия
    oled.rect(0, 16, 127, 63, OLED_STROKE);   // Рамка
    oled.setCursor(0, 0); oled.print("ТЕТРИС");                                                                    // Выводим рекорд
    if (ok.isHold()) {
      lineCleanCounter = 0;
      exit(false);
      break;
    }
    tetrisRoutine();
    yield();
  }
}










/*
  // аналогично, но координаты 0..128/0..64 и размер сегмента == 4
  void drawFigureRaw(byte figure, byte angle, byte x, byte y) {
  oledbuf[x + y * WIDTH] = color; // записать в массив
  oled.rect(y, x, y + 3, x + 3, OLED_STROKE); // рисуем точку начала координат фигуры
  int8_t X, Y;                      // вспомогательные
  for (byte i = 0; i < 3; i++) {    // рисуем 4 точки фигуры
    X = x + (int8_t)pgm_read_byte(&figures[figure][angle * 3 + i][0]) * 4;
    Y = y + (int8_t)pgm_read_byte(&figures[figure][angle * 3 + i][1]) * 4;
    if (Y > HEIGHT - 1) continue;   // если выходим за пределы поля, пропустить отрисовку
    oled.rect(Y, X, Y + 3, X + 3, OLED_STROKE); // нарисовать
  }
  }
*/
void start_tetris_r() {
  oled.line(0, 10, 127, 10);   // Линия
  //oled.line(127, 63, 127, 16);  // Опять линия
  oled.rect(0, 16, 127, 63, OLED_STROKE);   // Рамка
  oled.setCursor(0, 0); oled.print("ТЕТРИС"); 
  playTetrisGame();                                                                // Запускаем игру
  loadingFlag = true;
}
/* ========================== Работа с файлами =========================== */
int getFilesCount() {
  File root = LittleFS.open("/");
  int count = 0;
  File file = root.openNextFile();
  while (file) {
    String filename = file.name();
    if (filename.endsWith(".txt") || filename.endsWith(".h") || filename.endsWith(".catos")) {
      count++;
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
  return count;
}

String getFilenameByIndex(int idx) {
  File root = LittleFS.open("/");
  int i = 0;
  File file = root.openNextFile();
  while (file) {
    String filename = file.name();
    if (filename.endsWith(".txt") || filename.endsWith(".h") || filename.endsWith(".catos")) {
      if (i == idx) {
        String name = "/" + filename;
        file.close();
        root.close();
        return name;
      }
      i++;
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
  return "";
}
/* ======================================================================= */
/* ============================ Главное меню ============================= */
void update_cursor() {
  for (uint8_t i = 0; i < 6 && i < files; i++) {    // Проходимся от 2й до 8й строки оледа
    oled.setCursor(0, i + 2);                       // Ставим курсор на нужную строку
    oled.print(getFilenameByIndex(cursor < 6 ? i : i + (cursor - 5)));  // Выводим имя файла
  }
  oled.setCursor(0, constrain(cursor, 0, 5) + 2); oled.print(" ");      // Чистим место под указатель
  oled.setCursor(0, constrain(cursor, 0, 5) + 2); oled.print(">");      // Выводим указатель на нужной строке
  oled.update();                                    // Выводим картинку
}
bool drawMainMenu(void) {                           // Отрисовка главного меню
  oled.clear();                                     // Очистка
  oled.home();                                      // Возврат на 0,0
  oled.line(0, 10, 127, 10);                        // Линия
  if (files == 0) {
    oled.clear();
    oled.setScale(2);
    oled.setCursorXY(34, 16);
    oled.print(F("файлов"));
    oled.setCursorXY(22, 32);
    oled.print(F("нету :("));
    oled.setScale(1);
    oled.update();
    delay(1500);
    return false;
  }
  oled.print("НАЙДЕННЫЕ ФАЙЛЫ: "); oled.print(files);   // Выводим кол-во файлов
  update_cursor();
  return true;
}
/* ======================================================================= */
/* ============================ Чтение файла ============================= */
void enterToReadBmpFile(String filename) {
  File file = LittleFS.open(filename, "r");
  if (!file) {
    files = getFilesCount();
    drawMainMenu();
    file.close();
    return;
  }

  uint8_t *img = new uint8_t[1024]; // 128x64 бит = 1024 байт
  if (parseHFile(img, file)) {
    delete[] img;
    file.close();
    files = getFilesCount();
    drawMainMenu();
    return;
  }

  oled.clear();
  oled.drawBitmap(0, 0, img, 128, 64);
  oled.update();
  delete[] img;
  setCpuFrequencyMhz(80);
  while (true) {
    ok.tick();
    if (ok.isClick()) {
      setCpuFrequencyMhz(240);
      file.close();
      files = getFilesCount();
      drawMainMenu();
      setCpuFrequencyMhz(80);
      return;
    }
    yield();
  }
}

void drawPage(File &file, bool storeHistory = true) {
  if(storeHistory) {
    // Сохраняем текущую позицию перед отрисовкой
    currentHistoryIndex = (currentHistoryIndex + 1) % MAX_PAGE_HISTORY;
    pageHistory[currentHistoryIndex] = file.position();
    totalPages = min(totalPages + 1, MAX_PAGE_HISTORY);
  }
  oled.clear();
  oled.home();
  
  const uint8_t maxLineLength = 37;
  uint8_t currentLine = 0;
  String buffer = "";
  String word = "";
  uint8_t spaceLeft = maxLineLength;

  while (file.available() && currentLine < 8) {
    char c = file.read();
    
    if (c == '\n' || c == '\r') {
      if (!buffer.isEmpty()) {
        oled.println(buffer);
        currentLine++;
        buffer = "";
        spaceLeft = maxLineLength;
      }
      continue;
    }

    if (c == ' ') {
      if (buffer.length() + word.length() + 1 <= maxLineLength) {
        buffer += word;
        buffer += ' ';
        spaceLeft = maxLineLength - buffer.length();
        word = "";
      } else {
        oled.println(buffer);
        currentLine++;
        buffer = word + ' ';
        spaceLeft = maxLineLength - buffer.length();
        word = "";
      }
      continue;
    }

    word += c;

    if (word.length() > spaceLeft) {
      String part = word.substring(0, spaceLeft);
      buffer += part;
      oled.println(buffer);
      currentLine++;
      
      word = word.substring(spaceLeft);
      buffer = "";
      spaceLeft = maxLineLength;
      
      if (currentLine >= 8) break;
    }
  }
  // Вывод остатков
  if (!buffer.isEmpty() && currentLine < 8) {
    oled.println(buffer);
    currentLine++;
  }
  if (!word.isEmpty() && currentLine < 8) {
    oled.println(word);
  }

  oled.update();
}
void enterToReadTxtFile(String filename){
  File file = LittleFS.open(getFilenameByIndex(cursor), "r"); // Чтение имени файла по положению курсора и открытие файла
  if (!file) {                                      // Если сам файл не порядке
    files = getFilesCount(); drawMainMenu();        // Смотрим количество файлов и рисуем главное меню
    file.close(); return;                           // Закрываем файл и выходим
  }
  memset(pageHistory, 0, sizeof(pageHistory));
  currentHistoryIndex = -1;
  totalPages = 0;

  drawPage(file);                                   // Если с файлом все ок - рисуем первую страницу
  setCpuFrequencyMhz(80);
  while (1) {                                       // Бесконечный цикл
    up.tick();                                      // Опрос кнопок
    ok.tick();
    down.tick();
    if (ok.isClick()) {                             // Если ок нажат
      setCpuFrequencyMhz(240);
      files = getFilesCount(); drawMainMenu();      // Смотрим количество файлов и рисуем главное меню
      file.close(); 
      setCpuFrequencyMhz(80);
      return;                         // Закрываем файл и выходим
    } else if (up.isClick() or up.isHold()) {       // Если нажата или удержана вверх
      if(totalPages > 0) {
        totalPages--;
        currentHistoryIndex = (currentHistoryIndex - 1 + MAX_PAGE_HISTORY) % MAX_PAGE_HISTORY;
        file.seek(pageHistory[currentHistoryIndex]);
        drawPage(file, false);                      // Не сохраняем в историю
      }                                             // Устанавливаем указатель файла
    } else if (down.isClick() or down.isHold()) {   // Если нажата или удержана вниз
      drawPage(file);                               // Рисуем страницу
    }
    yield();                                        // Внутренний поллинг ESP
  }
}

// ========================= CAT OS SCRIPT ================================
void runCatosApp(String filename) {
  File file = LittleFS.open(filename, "r");
  if (!file) {
    oled.clear();
    oled.setCursor(0, 3);
    oled.print("Ошибка открытия");
    oled.update();
    delay(2000);
    return;
  }

  resetButtons();
  oled.clear();
  oled.setScale(1);
  
  String cmd;
  String arg1, arg2;
  bool inLoop = false;
  int loopStart = 0;

  struct VarValue {
    enum { BOOL, INT } type;
    union {
      bool bVal;
      int iVal;
    };
    VarValue() : type(INT), iVal(0) {}
  };

  std::map<String, VarValue> variables;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    
    if (line.length() == 0 || line.startsWith("//")) continue;

    int space1 = line.indexOf(' ');
    cmd = (space1 == -1) ? line : line.substring(0, space1);
    cmd.toLowerCase();

    String rest = (space1 == -1) ? "" : line.substring(space1 + 1);
    if (cmd == "var") {
      // Объявление переменной
      // Формат: var <type> <name>
      int space2 = rest.indexOf(' ');
      if(space2 == -1) continue; // Ошибка синтаксиса
      String typeStr = rest.substring(0, space2);
      String varName = rest.substring(space2 + 1);
      varName.trim();
      typeStr.toLowerCase();

      if (variables.find(varName) != variables.end()) {
        // Переменная уже объявлена - можно вывести ошибку или пропустить
        continue;
      }

      VarValue v;
      if (typeStr == "bool") {
        v.type = VarValue::BOOL;
        v.bVal = false;
      } else if (typeStr == "int") {
        v.type = VarValue::INT;
        v.iVal = 0;
      } else {
        // Неизвестный тип
        continue;
      }
      variables[varName] = v;
    }
    else if (cmd == "set") {
      // Формат: set <varName> <value>
      int space2 = rest.indexOf(' ');
      if(space2 == -1) continue;
      String varName = rest.substring(0, space2);
      String valueStr = rest.substring(space2 + 1);
      varName.trim();
      valueStr.trim();

      auto it = variables.find(varName);
      if (it == variables.end()) continue; // Переменная не найдена

      VarValue &v = it->second;
      if (v.type == VarValue::BOOL) {
        // Принять "true"/"false" или "1"/"0"
        valueStr.toLowerCase();
        if (valueStr == "true" || valueStr == "1") v.bVal = true;
        else if (valueStr == "false" || valueStr == "0") v.bVal = false;
      } else if (v.type == VarValue::INT) {
        v.iVal = valueStr.toInt();
      }
    }
    else if (cmd == "get") {
      // Формат: get <varName>
      String varName = rest;
      varName.trim();

      auto it = variables.find(varName);
      if (it == variables.end()) continue; // Переменная не найдена

      VarValue &v = it->second;
      oled.setCursor(0, 0); // текущая строка
      if (v.type == VarValue::BOOL) {
        oled.print(varName + ": " + (v.bVal ? "true" : "false"));
      } else if (v.type == VarValue::INT) {
        oled.print(varName + ": " + String(v.iVal));
      }
     }
    else if (cmd == "add") {
      // Формат: add <varName> <value>
      int space2 = rest.indexOf(' ');
      if(space2 == -1) continue;
      String varName = rest.substring(0, space2);
      String valueStr = rest.substring(space2 + 1);
      varName.trim();
      valueStr.trim();
      
      auto it = variables.find(varName);
      if (it == variables.end()) continue; // Переменная не найдена

      VarValue &v = it->second;
      if (v.type == VarValue::INT) {
        v.iVal += valueStr.toInt();
      }
    }
    else if (cmd == "sub") {
      // Формат: sub <varName> <value>
      int space2 = rest.indexOf(' ');
      if(space2 == -1) continue;
      String varName = rest.substring(0, space2);
      String valueStr = rest.substring(space2 + 1);
      varName.trim();
      valueStr.trim();

      auto it = variables.find(varName);
      if (it == variables.end()) continue; // Переменная не найдена

      VarValue &v = it->second;
      if (v.type == VarValue::INT) {
        v.iVal -= valueStr.toInt();
      }
    }
    else if (cmd == "mul") {
      // Формат: mul <varName> <value>
      int space2 = rest.indexOf(' ');
      if(space2 == -1) continue;
      String varName = rest.substring(0, space2);
      String valueStr = rest.substring(space2 + 1);
      varName.trim();
      valueStr.trim();

      auto it = variables.find(varName);
      if (it == variables.end()) continue; // Переменная не найдена

      VarValue &v = it->second;
      if (v.type == VarValue::INT) {
        v.iVal *= valueStr.toInt();
      }
    }
    else if (cmd == "div") {
      // Формат: div <varName> <value>
      int space2 = rest.indexOf(' ');
      if(space2 == -1) continue;
      String varName = rest.substring(0, space2);
      String valueStr = rest.substring(space2 + 1);
      varName.trim();
      valueStr.trim();

      auto it = variables.find(varName);
      if (it == variables.end()) continue; // Переменная не найдена

      VarValue &v = it->second;
      if (v.type == VarValue::INT) {
        int divisor = valueStr.toInt();
        if (divisor != 0) {
          v.iVal /= divisor;
        } else {
          // Обработка деления на ноль (можно вывести ошибку или игнорировать)
          continue; // Игнорируем операцию
        }
      }
    }
    else if (cmd == "printvar") {
      // Формат: printvar <varName>
      String varName = rest;
      varName.trim();

      auto it = variables.find(varName);
      if (it == variables.end()) continue; // Переменная не найдена

      VarValue &v = it->second;
      if (v.type == VarValue::BOOL) {
        oled.print(v.bVal ? "true" : "false");
      } else if (v.type == VarValue::INT) {
        oled.print(String(v.iVal));
      }
    }
    else if (cmd == "rnd") {
      // Формат: rnd <varName> <min> <max>
      int space1 = rest.indexOf(' ');
      if(space1 == -1) continue; 
      String varName = rest.substring(0, space1);
      String rangeStr = rest.substring(space1 + 1);
      varName.trim();
      rangeStr.trim();

      int space2 = rangeStr.indexOf(' ');
      if(space2 == -1) continue; 
      String minStr = rangeStr.substring(0, space2);
      String maxStr = rangeStr.substring(space2 + 1);
      
      int min = minStr.toInt();
      int max = maxStr.toInt();


      int randomValue = rnd.get(min, max); 

      VarValue v;
      v.type = VarValue::INT;
      v.iVal = randomValue;

      variables[varName] = v; 
    }
    else if (cmd == "if") {
      // Формат: if <varName> <operator> <valueOrVar>
      int space2 = rest.indexOf(' ');
      if(space2 == -1) continue;
      String varName = rest.substring(0, space2);
      String conditionStr = rest.substring(space2 + 1);
      varName.trim();
      conditionStr.trim();
    
      auto it = variables.find(varName);
      if (it == variables.end()) continue; // Переменная не найдена
    
      VarValue &v = it->second;
      bool conditionMet = false;
    
      // Получаем оператор и значение/переменную
      int space3 = conditionStr.indexOf(' ');
      if(space3 == -1) continue; // Ошибка синтаксиса
      String op = conditionStr.substring(0, space3);
      String rightStr = conditionStr.substring(space3 + 1);
      rightStr.trim();
    
      // Определяем, является ли rightStr именем переменной
      auto itRight = variables.find(rightStr);
    
      int rightIntVal = 0;
      bool rightBoolVal = false;
      bool rightIsBool = false;
    
      if (itRight != variables.end()) {
        VarValue &vr = itRight->second;
        if (vr.type == VarValue::INT) {
          rightIntVal = vr.iVal;
          rightIsBool = false;
        } else if (vr.type == VarValue::BOOL) {
          rightBoolVal = vr.bVal;
          rightIsBool = true;
        }
      } else {
        String lowerRight = rightStr;
        lowerRight.toLowerCase();
        if (lowerRight == "true") {
          rightBoolVal = true;
          rightIsBool = true;
        } else if (lowerRight == "false") {
          rightBoolVal = false;
          rightIsBool = true;
        } else {
          rightIntVal = rightStr.toInt();
          rightIsBool = false;
        }
      }
    
      if (v.type == VarValue::INT && !rightIsBool) {
        int leftVal = v.iVal;
        int rightVal = rightIntVal;
      
        if (op == "=" || op == "==") conditionMet = (leftVal == rightVal);
        else if (op == "<") conditionMet = (leftVal < rightVal);
        else if (op == ">") conditionMet = (leftVal > rightVal);
        else if (op == "<=") conditionMet = (leftVal <= rightVal);
        else if (op == ">=") conditionMet = (leftVal >= rightVal);
      } else if (v.type == VarValue::BOOL && rightIsBool) {
        bool leftVal = v.bVal;
        bool rightVal = rightBoolVal;
        if (op == "=" || op == "==") conditionMet = (leftVal == rightVal);
      } else {
        conditionMet = false; // Несовместимые типы
      }
    
      if (!conditionMet) {
        while (file.available()) {
          String skipLine = file.readStringUntil('\n');
          skipLine.trim();
          if (skipLine == "endif") break;
        }
      }
    }

    // Основные команды
    else if (cmd == "print") {
      String text = line.substring(space1 + 1);
      oled.print(text);
    }
    
    else if (cmd == "serial.print") {
      String text = line.substring(space1 + 1);
      Serial.print(text);
    }
    else if (cmd == "serial.println") {
      String text = line.substring(space1 + 1);
      Serial.println(text);
    }
    else if (cmd == "printBattery") {
      oled.print(getBattery());
    }
    else if (cmd == "setcursor") {
      int space2 = line.indexOf(' ', space1 + 1);
      arg1 = line.substring(space1 + 1, space2);
      arg2 = line.substring(space2 + 1);
      oled.setCursor(arg1.toInt(), arg2.toInt());
    }
    else if (cmd == "clear") {
      oled.clear();
    }
    else if (cmd == "drawbattery") {
      drawbattery();
    }
    else if (cmd == "update") {
      oled.update();
    }
    else if (cmd == "loop") {
      inLoop = true;
      loopStart = file.position();
      continue;
    }
    else if (cmd == "endloop") {
      if (inLoop) file.seek(loopStart);
      continue;
    }
    else if (cmd == "delay") {
      arg1 = line.substring(space1 + 1);
      delay(arg1.toInt());
    }
    else if (cmd == "exit") {
      break;
    }

    // Проверка кнопок
    buttons_tick();
    // Обработка условий
    if (cmd == "ifbtn") {
      int space2 = line.indexOf(' ', space1 + 1);
      String buttonName = line.substring(space1 + 1, space2);
      String action = line.substring(space2 + 1);
      
      bool condition = false;
      if (buttonName == "up") condition = (action == "click") ? up.isClick() : up.isHold();
      else if (buttonName == "down") condition = (action == "click") ? down.isClick() : down.isHold();
      else if (buttonName == "left") condition = (action == "click") ? left.isClick() : left.isHold();
      else if (buttonName == "right") condition = (action == "click") ? right.isClick() : right.isHold();
      else if (buttonName == "ok") condition = (action == "click") ? ok.isClick() : ok.isHold();
      
      if (!condition) {
        // Пропускаем следующие команды до endif
        while (file.available()) {
          String skipLine = file.readStringUntil('\n');
          skipLine.trim();
          if (skipLine == "endifbtn") break;
        }
      }
    }
  }
  
  file.close();
  resetButtons();
  files = getFilesCount(); drawMainMenu();
  return;
}
void enterToReadFile(void) { 

  String filename = getFilenameByIndex(cursor);
  if (filename.endsWith(".h")) {
    enterToReadBmpFile(filename);
  } else if(filename.endsWith(".txt")) {
    enterToReadTxtFile(filename);
  } else if(filename.endsWith(".catos")) {
    runCatosApp(filename);
  }                
}
void ShowFilesLittleFS() {
  oled.autoPrintln(false);
  setCpuFrequencyMhz(240);
  files = getFilesCount();                    // Читаем количество файлов
  if (drawMainMenu() == false){               // Рисуем главное меню
    exit();
    setCpuFrequencyMhz(80);
    return;
  }                 
  while (true)
  {
    buttons_tick();                                     // Опрос кнопок
    static uint32_t timer = 0;                          // таймер
    if (up.isClick() || (up.isHold() && millis() - timer > 50)) {                // Если нажата или удержана кнопка вверх
      cursor = constrain(cursor - 1, 0, files - 1);   // Двигаем курсор
      timer = millis();
      update_cursor();    
    } else if (down.isClick() || (down.isHold() && millis() - timer > 50)) {       // Если нажата или удержана кнопка вниз
      cursor = constrain(cursor + 1, 0, files - 1);   // Двигаем курсор
      timer = millis();
      update_cursor();                                 // Обновляем главное меню
    } else if (ok.isHold()) {                         // Если удержана ОК
      setCpuFrequencyMhz(80);
      exit();                                         // Выход                        
      return;                                         // Выход
    } else if (ok.isClick()) {                        // Если нажата ОК
      enterToReadFile();                              // Переходим к чтению файла
    }
  }
  
}
void dice_random() {
  #define x_dice 45
  
  // Настройки анимации (можно менять)
  #define ANIMATION_FRAMES 20     // Общее количество кадров анимации
  #define INITIAL_DELAY 50        // Начальная задержка между кадрами (мс)
  #define FINAL_DELAY 150         // Финальная задержка между кадрами (мс)
  #define ACCELERATION 5          // Ускорение анимации (чем больше - быстрее замедление)

  ui_rama("КУБИК", true, true, true);
  
  while (true) {
    buttons_tick();
    
    if (ok.isClick()) {
      // Ждем отпускания кнопки
      while(ok.isPress()) { buttons_tick(); delay(10); }
      
      // Анимация вращения с замедлением
      int current_delay = INITIAL_DELAY;
      for(int i = 0; i < ANIMATION_FRAMES; i++) {
        // Плавное замедление анимации
        if(i > ANIMATION_FRAMES/2) {
          current_delay = constrain(current_delay + ACCELERATION, INITIAL_DELAY, FINAL_DELAY);
        }
        
        // Показываем случайный кубик
        int rand_frame = rnd.get(1, 7);
        oled.clear(x_dice, 16, x_dice + 32, 16 + 32);
        
        switch(rand_frame) {
          case 1: oled.drawBitmap(x_dice, 16, one_rice_32x32, 32, 32); break;
          case 2: oled.drawBitmap(x_dice, 16, two_rice_32x32, 32, 32); break;
          case 3: oled.drawBitmap(x_dice, 16, three_rice_32x32, 32, 32); break;
          case 4: oled.drawBitmap(x_dice, 16, four_rice_32x32, 32, 32); break;
          case 5: oled.drawBitmap(x_dice, 16, five_rice_32x32, 32, 32); break;
          case 6: oled.drawBitmap(x_dice, 16, six_rice_32x32, 32, 32); break;
        }
        
        oled.update();
        delay(current_delay);
      }
      
      // Финальный результат
      int rand_dice = rnd.get(1, 7);
      ui_rama("КУБИК", true, true, true);
      
      switch(rand_dice) {
        case 1: oled.drawBitmap(x_dice, 16, one_rice_32x32, 32, 32); break;
        case 2: oled.drawBitmap(x_dice, 16, two_rice_32x32, 32, 32); break;
        case 3: oled.drawBitmap(x_dice, 16, three_rice_32x32, 32, 32); break;
        case 4: oled.drawBitmap(x_dice, 16, four_rice_32x32, 32, 32); break;
        case 5: oled.drawBitmap(x_dice, 16, five_rice_32x32, 32, 32); break;
        case 6: oled.drawBitmap(x_dice, 16, six_rice_32x32, 32, 32); break;
      }
      
      
      oled.update();
    }
    
    if(ok.isHold()) {
      exit(false);
      return;
    }
  }
}
void stopwatch() {
  bool running = false;
  unsigned long startTime = 0;
  unsigned long elapsedTime = 0;
  unsigned long lastDraw = 0;
  
  resetButtons();
  oled.clear();
  ui_rama("Секундомер", true, true, true);

  while(1) {
      buttons_tick();
      
      // Управление
      if(ok.isClick()) {
          running = !running;
          if(running) startTime = millis() - elapsedTime;
      }
      
      if(up.isHold()) { // Сброс при удержании
          elapsedTime = 0;
          running = false;
      }
      
      // Обновление времени
      if(running) {
          elapsedTime = millis() - startTime;
      }
      
      // Отрисовка каждые 50 мс
      if(millis() - lastDraw > 50) {
          lastDraw = millis();
          oled.setCursor(0, 3);
          oled.setScale(2);
          
          unsigned long hours = elapsedTime / 3600000;
          unsigned long mins = (elapsedTime / 60000) % 60;
          unsigned long secs = (elapsedTime / 1000) % 60;
          unsigned long ms = elapsedTime % 1000;
          
          oled.print(hours < 10 ? "0" : ""); oled.print(hours);
          oled.print(":");
          oled.print(mins < 10 ? "0" : ""); oled.print(mins);
          oled.print(":");
          oled.print(secs < 10 ? "0" : ""); oled.print(secs);
          oled.print(".");
          oled.print(ms / 100);
          
          oled.setScale(1);
          oled.setCursor(0, 7);
          oled.print(running ? "РАБОТА" : "ПАУЗА ");
          oled.print("  СБРОС->");
          oled.update();
      }
      
      // Выход по долгому нажатию OK
      if(ok.isHold()) {
          exit(false);
          return;
      }
  }
}
void timer_oled() {
  enum {SET_HOURS, SET_MINS, SET_SECS, RUNNING, ALARM} state = SET_HOURS;
  uint8_t hours = 0;
  uint8_t mins = 0;
  uint8_t secs = 0;
  unsigned long targetTime = 0;
  
  resetButtons();
  oled.clear();
  ui_rama("Таймер", true, true, true);

  while(1) {
      buttons_tick();
      
      switch(state) {
          case SET_HOURS:
              // Регулировка часов
              if(up.isClick()) hours = (hours + 1) % 24;
              if(down.isClick()) hours = (hours > 0) ? hours - 1 : 23;
              
              // Отображение
              oled.setCursor(0, 3);
              oled.setScale(2);
              oled.print(hours < 10 ? "0" : ""); oled.print(hours);
              oled.print(":00:00");
              
              // Переход к минутам
              if(ok.isClick()) state = SET_MINS;
              break;
              
          case SET_MINS:
              // Регулировка минут
              if(up.isClick()) mins = (mins + 1) % 60;
              if(down.isClick()) mins = (mins > 0) ? mins - 1 : 59;
              
              // Отображение
              oled.setCursor(0, 3);
              oled.setScale(2);
              oled.print(hours < 10 ? "0" : ""); oled.print(hours);
              oled.print(":");
              oled.print(mins < 10 ? "0" : ""); oled.print(mins);
              oled.print(":00");
              
              // Переход к секундам
              if(ok.isClick()) state = SET_SECS;
              break;
              
          case SET_SECS:
              // Регулировка секунд
              if(up.isClick()) secs = (secs + 1) % 60;
              if(down.isClick()) secs = (secs > 0) ? secs - 1 : 59;
              
              // Отображение
              oled.setCursor(0, 3);
              oled.setScale(2);
              oled.print(hours < 10 ? "0" : ""); oled.print(hours);
              oled.print(":");
              oled.print(mins < 10 ? "0" : ""); oled.print(mins);
              oled.print(":");
              oled.print(secs < 10 ? "0" : ""); oled.print(secs);
              
              // Старт таймера
              if(ok.isClick()) {
                  targetTime = millis() + 
                      (hours * 3600000UL) + 
                      (mins * 60000UL) + 
                      (secs * 1000UL);
                  state = RUNNING;
              }
              break;
              
          case RUNNING: {
              unsigned long remaining = targetTime - millis();
              if(remaining > 86400000UL) { // Переполнение
                  state = ALARM;
                  break;
              }
              
              if(remaining <= 0) {
                  ui_rama("Таймер", true, true, true);
                  oled.clear();
                  state = ALARM;
                  break;
              }
              
              // Отображение оставшегося времени
              uint8_t rem_h = remaining / 3600000;
              remaining %= 3600000;
              uint8_t rem_m = remaining / 60000;
              remaining %= 60000;
              uint8_t rem_s = remaining / 1000;
              
              oled.setCursor(0, 3);
              oled.setScale(2);
              oled.print(rem_h < 10 ? "0" : ""); oled.print(rem_h);
              oled.print(":");
              oled.print(rem_m < 10 ? "0" : ""); oled.print(rem_m);
              oled.print(":");
              oled.print(rem_s < 10 ? "0" : ""); oled.print(rem_s);
              
              // Отмена таймера
              if(ok.isClick()) {
                  state = SET_HOURS;
                  hours = mins = secs = 0;
              }
              break;
          }
              
          case ALARM:
              // Мигающий сигнал
              oled.invertDisplay(millis() % 500 < 250);
              oled.setCursor(0, 3);
              oled.setScale(2);
              oled.print("   ВРЕМЯ");
              
              // Отключение сигнала
              if(ok.isClick()) {
                  oled.invertDisplay(false);
                  state = SET_HOURS;
                  hours = mins = secs = 0;
              }
              break;
      }
      
      oled.setScale(1);
      oled.setCursor(0, 7);
      switch(state) {
          case SET_HOURS: oled.print("Уст. часы     OK->"); break;
          case SET_MINS:  oled.print("Уст. минуты   OK->"); break;
          case SET_SECS:  oled.print("Уст. секунды СТАРТ"); break;
          case RUNNING:   oled.print("  ОСТАНОВКА   ->OK"); break;
          case ALARM:     oled.print("   ОК - СБРОС     "); break;
      }
      oled.update();
      
      // Выход по долгому нажатию OK
      if(ok.isHold() && state != ALARM) {
          exit(false);
          return;
      }
  }
}

void drawPaddles() {
  // Левая ракетка
  oled.rect(0, player1Y - PADDLE_HEIGHT/2, PADDLE_WIDTH-1, player1Y + PADDLE_HEIGHT/2, OLED_FILL);
  // Правая ракетка
  oled.rect(127 - PADDLE_WIDTH, player2Y - PADDLE_HEIGHT/2, 126, player2Y + PADDLE_HEIGHT/2, OLED_FILL);
}

void drawBall() {
  oled.rect(ballX - BALL_SIZE/2, ballY - BALL_SIZE/2, 
           ballX + BALL_SIZE/2, ballY + BALL_SIZE/2, OLED_FILL);
}

void resetBall() {
  ballX = 64;
  ballY = random(16, 48);
  ballSpeedX = random(0, 2) ? 1 : -1;
  
  // Генерируем вертикальную скорость, исключая 0
  do {
    ballSpeedY = random(-2, 3); // Увеличиваем диапазон для большего разнообразия углов
  } while(ballSpeedY == 0);     // Повторяем пока не получим не нулевое значение
}

void moveAI() {
  // Уровень сложности (1-легко, 3-сложно)
  static int difficulty = 2; 
  const int baseSpeed = 2;    // Базовая скорость
  const int maxSpeed = 3;     // Максимальная скорость
  
  // Рассчитываем направление к цели с плавностью
  static float player2Yf = player2Y; // Точная позиция с плавающей точкой
  
  // Вычисляем центр мяча относительно ракетки
  int targetY = ballY - (ballSpeedX > 0 ? 0 : PADDLE_WIDTH*2); 
  
  // Плавное движение с ограничением скорости
  float diff = targetY - player2Yf;
  float move = constrain(diff, -baseSpeed, baseSpeed) * 0.7;
  
  // Применяем движение с инерцией
  player2Yf += move * (0.5 + difficulty*0.2);
  
  // Ограничение позиции и перевод в целочисленные координаты
  player2Yf = constrain(player2Yf, 
                      PADDLE_HEIGHT/2 + 2.0, 
                      63 - PADDLE_HEIGHT/2 - 2.0);
  player2Y = static_cast<int>(player2Yf);

  // Случайная ошибка для реалистичности
  if(random(100) < 10) { // 10% шанс на ошибку
    player2Y += random(-4, 3);
  }
}
void pongGame() {
  resetBall();
  // Настройка кнопок для плавного управления
  up.setTimeout(300);  // Время до начала автоповтора (мс)
  down.setTimeout(300);
  while(true) {
      buttons_tick();
      
      // Управление игроком с автоповтором
      static uint32_t moveTimer = millis();

      if(up.isHold() || up.isStep()) {
          if(millis() - moveTimer > 50) { // Задержка перед автоповтором
              player1Y -= 4;
              moveTimer = millis();
          }
      }

      if(down.isHold() || down.isStep()) {
          if(millis() - moveTimer > 50) {
              player1Y += 4;
              moveTimer = millis();
          }
      }
      if(up.isPress()) {
        player1Y -= 2;
      }
      if(down.isPress()) {
        player1Y += 2;
      }
      player1Y = constrain(player1Y, PADDLE_HEIGHT/2, 63 - PADDLE_HEIGHT/2);
      
      // Движение мяча
      ballX += ballSpeedX;
      ballY += ballSpeedY;
      // Гарантируем, что вертикальная скорость не нулевая
      if(ballSpeedY == 0) {
        ballSpeedY = random(0, 2) ? 1 : -1;
      }
      // Отскок от верхней и нижней стенок
      if(ballY <= BALL_SIZE/2 || ballY >= 63 - BALL_SIZE/2) {
          ballSpeedY = -ballSpeedY;
      }
      
      // Проверка столкновений с ракетками
      if(ballX <= PADDLE_WIDTH + BALL_SIZE/2) {
        if(abs(ballY - player1Y) <= PADDLE_HEIGHT/2) {
            ballSpeedX = -ballSpeedX;
            ballSpeedY = (ballY - player1Y) / 2;
            if(ballSpeedY == 0) ballSpeedY = random(0, 2) ? 1 : -1;
        }
      }
      
      if (ballX >= 127 - PADDLE_WIDTH - BALL_SIZE/2) {
        if (abs(ballY - player2Y) <= PADDLE_HEIGHT/2) {
            ballSpeedX = -ballSpeedX;
            
            // Стратегия направленных ударов
            int targetOffset = (player1Y < 32) ? 20 : -20; // Если игрок вверху, бьем вниз
            ballSpeedY = constrain((ballY - (player2Y + targetOffset)) / 2, -3, 3);
            
            // Усложнение с прогрессирующей скоростью
            ballSpeedX *= 1.1;
            ballSpeedY *= 1.1;
        }
    }
      
      // Проверка голов
      if(ballX < 0) {
          score2++;
          resetBall();
      }
      if(ballX > 127) {
          score1++;
          resetBall();
      }
      
      // Движение ИИ
      moveAI();
      
      // Отрисовка
      oled.clear();
      drawPaddles();
      drawBall();
      
      // Отображение счета
      oled.setCursor(56, 0);
      oled.print(score1);
      oled.print(":");
      oled.print(score2);
      
      // Центральная линия
      for(int i = 0; i < 64; i += 4) {
          oled.dot(64, i);
      }
      
      oled.update();
      
      // Выход по долгому нажатию OK
      if(ok.isHold()) {
          up.setTimeout(300);
          down.setTimeout(300);
          score1 = score2 = 0;
          exit(false);
          return;
      }
      
      delay(30);
  }
}
void initFlappy() {
  birdY = 32;
  birdVelocity = 0;
  score = 0;
  gameOver = false;
  
  // Инициализация труб
  for(uint8_t i = 0; i < 3; i++) {
      pipePositions[i] = 128 + i * PIPE_SPACING;
      pipeHeights[i] = random(10, 40);
      pipePassed[i] = false;
  }
}

void updatePipes() {
  for(uint8_t i = 0; i < 3; i++) {
      pipePositions[i] -= 2;
      
      // Обновление счета
      if(pipePositions[i] + PIPE_WIDTH < BIRD_START_X && !pipePassed[i]) {
          score++;
          pipePassed[i] = true; // Помечаем трубу как пройденную
      }
      
      // Регенерация труб
      if(pipePositions[i] + PIPE_WIDTH < 0) {
          pipePositions[i] = 128 + PIPE_SPACING * 2; // Переносим трубу в конец очереди
          pipeHeights[i] = random(10, 40);
          pipePassed[i] = false; // Сбрасываем флаг прохождения для новой трубы
      }
  }
}
bool checkCollision() {

  if(birdY < 0 || birdY + BIRD_SIZE > 75) return true;

  for(uint8_t i = 0; i < 3; i++) {
      if(pipePositions[i] < BIRD_START_X + BIRD_SIZE && 
         pipePositions[i] + PIPE_WIDTH > BIRD_START_X) {

          if(birdY < pipeHeights[i]) return true;

          if(birdY + BIRD_SIZE > pipeHeights[i] + PIPE_GAP) return true;
      }
  }
  return false;
}

void drawPipes() {
  for(uint8_t i = 0; i < 3; i++) {
      // Верхняя труба
      oled.rect(pipePositions[i], 0, pipePositions[i] + PIPE_WIDTH - 1, pipeHeights[i]);
      
      // Нижняя труба
      oled.rect(pipePositions[i], pipeHeights[i] + PIPE_GAP, 
                   pipePositions[i] + PIPE_WIDTH - 1, 63);
  }
}

void drawBird() {
  // Анимация птицы
  if(animTimer.isReady()) birdFrame = !birdFrame;
  
  const uint8_t* frame;
  if(birdVelocity < -1) frame = midle_bird_16x16;
  else if(birdVelocity > 1) frame = down_bird_16x16;
  else frame = midle_bird_16x16;
  
  oled.drawBitmap(BIRD_START_X, (int)birdY, frame, 16, 16);
}

void flappyGame() {
  initFlappy();
  gameOver = false;
  
  while(true) {
      buttons_tick();
      
      if(!gameOver) {
          // Прыжок
          if(ok.isClick() || up.isClick()) {
              birdVelocity = JUMP_FORCE;
          }
          
          // Обновление физики
          birdVelocity += GRAVITY;
          birdY += birdVelocity;
          
          // Обновление труб
          updatePipes();
          
          // Проверка коллизий
          if(checkCollision()) {
              gameOver = true;
          }
      }
      
      // Отрисовка
      oled.clear();
      drawPipes();
      drawBird();
      
      
      if(gameOver) {
          oled.clear();
          bool screen_gameOver = true;
          oled.setCursor(15, 2);
          oled.setScale(2);
          oled.print("GAME OVER");
          oled.setScale(1);
          oled.setCursor(10, 5);
          oled.print("OK-Restart  <-Menu");
          oled.update();
          while (screen_gameOver) {
            buttons_tick();
            if(ok.isClick()) {
              initFlappy();
              screen_gameOver = false;
              gameOver = false;
            }
            if(left.isClick() || ok.isHold()) {
              exit(false);
              return;
            }
          }
      }
      
      oled.update();
      
      if(left.isClick() || ok.isHold()) {
          exit(false);
          return;
      }
      
      delay(30);
  }
}

void initArkanoid() {
    right.setStepTimeout(20);
    left.setStepTimeout(20);
    paddleX = (128 - PADDLE_WIDTH_ark) / 2;
    ballX_ark = random(20, 108);
    ballY_ark = 40;
    ballSpeedX_ark = random(0, 2) ? 1 : -1;
    ballSpeedY_ark = -1;
    lives = 5;
    score_ark = 0;
    gameRunning = true;

    // Инициализация кирпичей
    for (int row = 0; row < BRICK_ROWS; row++) {
        for (int col = 0; col < BRICK_COLS; col++) {
            bricks[row][col] = true;
        }
    }
}

void drawPaddle() {
    oled.rect(paddleX, 60, paddleX + PADDLE_WIDTH_ark - 1, 60 + PADDLE_HEIGHT_ark - 1, OLED_FILL);
}

void drawBall_ark() {
    oled.rect(ballX_ark - BALL_SIZE_ark/2, ballY_ark - BALL_SIZE_ark/2, 
             ballX_ark + BALL_SIZE_ark/2, ballY_ark + BALL_SIZE_ark/2, OLED_FILL);
}

void drawBricks() {
    for (int row = 0; row < BRICK_ROWS; row++) {
        for (int col = 0; col < BRICK_COLS; col++) {
            if (bricks[row][col]) {
                int x = col * (BRICK_WIDTH + BRICK_SPACING);
                int y = row * (BRICK_HEIGHT + BRICK_SPACING) + 5;
                oled.rect(x, y, x + BRICK_WIDTH - 1, y + BRICK_HEIGHT - 1);
            }
        }
    }
}

void moveBall() {
    // Движение шара
    ballX_ark += ballSpeedX_ark; 
    ballY_ark += ballSpeedY_ark; 

    // Отскок от стен
    if (ballX_ark <= BALL_SIZE_ark/2 || ballX_ark >= 127 - BALL_SIZE_ark/2) {
        ballSpeedX_ark = -ballSpeedX_ark;
    }
    if (ballY_ark <= BALL_SIZE_ark/2) {
        ballSpeedY_ark = -ballSpeedY_ark;
    }

    // Проверка столкновения с платформой
    if (ballY_ark + BALL_SIZE_ark/2 >= 60 && 
        ballY_ark - BALL_SIZE_ark/2 <= 60 + PADDLE_HEIGHT_ark &&
        ballX_ark + BALL_SIZE_ark/2 >= paddleX && 
        ballX_ark - BALL_SIZE_ark/2 <= paddleX + PADDLE_WIDTH_ark) {
        
        // Угол отскока зависит от места удара
        int hitPos = (ballX_ark - paddleX) - PADDLE_WIDTH_ark/2;
        ballSpeedX_ark = constrain(hitPos / 3, -2, 2);
        ballSpeedY_ark = -abs(ballSpeedY_ark);
    }

    // Проверка столкновения с кирпичами
    for (int row = 0; row < BRICK_ROWS; row++) {
        for (int col = 0; col < BRICK_COLS; col++) {
            if (bricks[row][col]) {
                int brickX = col * (BRICK_WIDTH + BRICK_SPACING);
                int brickY = row * (BRICK_HEIGHT + BRICK_SPACING) + 5;
                
                if (ballX_ark + BALL_SIZE_ark/2 > brickX && 
                    ballX_ark - BALL_SIZE_ark/2 < brickX + BRICK_WIDTH &&
                    ballY_ark + BALL_SIZE_ark/2 > brickY && 
                    ballY_ark - BALL_SIZE_ark/2 < brickY + BRICK_HEIGHT) {
                    
                    bricks[row][col] = false;
                    score += 10;
                    
                    // Определяем сторону столкновения
                    int overlapLeft = (ballX_ark + BALL_SIZE_ark/2) - brickX;
                    int overlapRight = (brickX + BRICK_WIDTH) - (ballX_ark - BALL_SIZE_ark/2);
                    int overlapTop = (ballY_ark + BALL_SIZE_ark/2) - brickY;
                    int overlapBottom = (brickY + BRICK_HEIGHT) - (ballY_ark - BALL_SIZE_ark/2);
                    
                    // Находим минимальное перекрытие
                    int minOverlap = min(min(overlapLeft, overlapRight), min(overlapTop, overlapBottom));
                    
                    if (minOverlap == overlapLeft || minOverlap == overlapRight) {
                        ballSpeedX_ark = -ballSpeedX_ark;
                    } else {
                        ballSpeedY_ark = -ballSpeedY_ark;
                    }
                }
            }
        }
    }
    // Проверка проигрыша жизни
    if (ballY_ark >= 64) {
        lives--;
        if (lives > 0) {
            ballX_ark = random(20, 108);
            ballY_ark = 40;
            paddleX = (128 - PADDLE_WIDTH_ark) / 2;
            ballSpeedX_ark = random(0, 2) ? 1 : -1;
            ballSpeedY_ark = -1;
        } else {
            gameRunning = false;
        }
    }
}

bool allBricksDestroyed() {
    for (int row = 0; row < BRICK_ROWS; row++) {
        for (int col = 0; col < BRICK_COLS; col++) {
            if (bricks[row][col]) return false;
        }
    }
    return true;
}

void drawGameStats() {
    oled.setCursor(0, 0);
    oled.print("Жизни:");
    oled.print(lives);
    oled.setCursor(70, 0);
    oled.print("Очки:");
    oled.print(score);
}

void arkanoidGame() {
    initArkanoid();
    
    while (true) {
        buttons_tick();
        
        // Управление платформой
        if (left.isStep() or left.isClick()) {
            paddleX = constrain(paddleX - 3, 0, 128 - PADDLE_WIDTH_ark);
        }
        if (right.isStep() or right.isClick()) {
            paddleX = constrain(paddleX + 3, 0, 128 - PADDLE_WIDTH_ark);
        }
        
        // Выход из игры
        if (ok.isHold()) {
            exit(false);
            left.setStepTimeout(400);
            right.setStepTimeout(400);
            return;
        }
        
        if (gameRunning) {
            moveBall();
            
            // Проверка победы
            if (allBricksDestroyed()) {
                gameRunning = false;
            }
            
            // Отрисовка
            oled.clear();
            drawGameStats();
            drawBricks();
            drawPaddle();
            drawBall_ark();
            oled.update();
        } else {
            // Экран окончания игры
            oled.clear();
            oled.setScale(2);
            
            if (lives > 0) {
                oled.setCursorXY(25, 20);
                oled.print("ПОБЕДА!");
            } else {
                oled.setCursorXY(15, 20);
                oled.print("ПРОИГРЫШ");
                
            }
            
            oled.setScale(1);
            oled.setCursor(30, 6);
            oled.print("Очки: ");
            oled.print(score);
            
            oled.setCursor(10, 7);
            oled.print("OK - меню  <-Играть");
            oled.update();
            
            // Обработка выбора после игры
            while (true) {
                buttons_tick();
                if (ok.isClick()) {
                    exit(false);
                    left.setStepTimeout(400);
                    right.setStepTimeout(400);
                    return;
                }
                if (left.isClick()) {
                    initArkanoid();
                    break;
                }
            }
        }
        
        delay(20);
    }
}
// mini 3d graphics
#define DOOM_MAP_SIZE 8
#define DOOM_FOV 60
#define DOOM_MAX_STEPS 20
#define DOOM_WALL_HEIGHT 1.0f

const uint8_t wallTexture[] = {
  0b11111111,
  0b10000001,
  0b10111101,
  0b10111101,
  0b10111101,
  0b10111101,
  0b10000001,
  0b11111111
};

// 1 - стена, 0 - проход. можно сделать свою карту
uint8_t doomMap[DOOM_MAP_SIZE][DOOM_MAP_SIZE] = {
  {1,1,1,1,1,1,1,1},
  {1,0,0,0,0,0,0,1},
  {1,0,1,0,1,1,0,1},
  {1,0,1,0,0,1,0,1},
  {1,0,0,0,0,0,0,1},
  {1,0,1,1,0,1,0,1},
  {1,0,0,0,0,0,0,1},
  {1,1,1,1,1,1,1,1}
};

float playerX = 1.5f;
float playerY = 1.5f;
float playerAngle = 0.0f;
float moveSpeed = 0.1f;
float rotSpeed = 0.05f;

bool checkCollision(float x, float y) {
  int mapX = (int)x;
  int mapY = (int)y;
  
  if (mapX < 0 || mapX >= DOOM_MAP_SIZE || mapY < 0 || mapY >= DOOM_MAP_SIZE) 
    return true;
  
  return doomMap[mapY][mapX] == 1;
}

// Raycasting 
void castRay(float rayAngle, int column, float* wallDist, uint8_t* wallType) {
  float rayDirX = cos(rayAngle);
  float rayDirY = sin(rayAngle);
  
  float deltaDistX = fabs(1 / rayDirX);
  float deltaDistY = fabs(1 / rayDirY);
  
  int mapX = (int)playerX;
  int mapY = (int)playerY;
  
  float sideDistX, sideDistY;
  int stepX, stepY;
  
  if (rayDirX < 0) {
    stepX = -1;
    sideDistX = (playerX - mapX) * deltaDistX;
  } else {
    stepX = 1;
    sideDistX = (mapX + 1.0 - playerX) * deltaDistX;
  }
  
  if (rayDirY < 0) {
    stepY = -1;
    sideDistY = (playerY - mapY) * deltaDistY;
  } else {
    stepY = 1;
    sideDistY = (mapY + 1.0 - playerY) * deltaDistY;
  }
  
  bool hit = false;
  bool side; // false - X, true - Y
  
  for (int i = 0; i < DOOM_MAX_STEPS && !hit; i++) {
    if (sideDistX < sideDistY) {
      sideDistX += deltaDistX;
      mapX += stepX;
      side = false;
    } else {
      sideDistY += deltaDistY;
      mapY += stepY;
      side = true;
    }
    
    if (mapX < 0 || mapX >= DOOM_MAP_SIZE || mapY < 0 || mapY >= DOOM_MAP_SIZE) 
      break;
      
    if (doomMap[mapY][mapX] == 1) {
      hit = true;
    }
  }
  
  if (hit) {
    if (!side) {
      *wallDist = (mapX - playerX + (1 - stepX) / 2) / rayDirX;
    } else {
      *wallDist = (mapY - playerY + (1 - stepY) / 2) / rayDirY;
    }
    *wallType = side ? 2 : 1;
  } else {
    *wallDist = -1;
  }
}

// Рендер 
void renderDoomScene() {
  oled.clear();
  
  //пол и потолок
  oled.rect(0, 32, 127, 63, OLED_FILL); //пол
  oled.rect(0, 0, 127, 31, OLED_CLEAR); //потолок
  
  // Raycasting
  for (int x = 0; x < 128; x++) {
    float cameraX = 2 * x / 127.0f - 1;
    float rayAngle = playerAngle + atan(cameraX * tan(DOOM_FOV * 3.14159 / 360));
    
    float wallDist;
    uint8_t wallType;
    castRay(rayAngle, x, &wallDist, &wallType);
    
    if (wallDist > 0) {
      int lineHeight = (int)(64 / wallDist);
      if (lineHeight > 64) lineHeight = 64;
      
      int drawStart = (64 - lineHeight) / 2;
      int drawEnd = drawStart + lineHeight;
      
      for (int y = drawStart; y < drawEnd; y++) {
        if (y >= 0 && y < 64) {
          //чередование пикселей
          if ((x + y) % (wallType + 1) == 0) {
            oled.dot(x, y);
          }
        }
      }
    }
  }
  
  // миникарта
  int miniMapSize = 32;
  int miniMapX = 96;
  int miniMapY = 0;
  
  for (int y = 0; y < DOOM_MAP_SIZE; y++) {
    for (int x = 0; x < DOOM_MAP_SIZE; x++) {
      if (doomMap[y][x] == 1) {
        oled.rect(miniMapX + x * 4, miniMapY + y * 4, 
                 miniMapX + x * 4 + 3, miniMapY + y * 4 + 3);
      }
    }
  }
  
  //игрок на миникарте
  int playerMapX = miniMapX + (int)(playerX * 4);
  int playerMapY = miniMapY + (int)(playerY * 4);
  oled.rect(playerMapX - 1, playerMapY - 1, playerMapX + 1, playerMapY + 1, OLED_FILL);

  int lookX = playerMapX + cos(playerAngle) * 6;
  int lookY = playerMapY + sin(playerAngle) * 6;
  oled.line(playerMapX, playerMapY, lookX, lookY);
  
  oled.update();
}

// main функция игры
void miniDoomGame() {
  resetButtons();
  playerX = 1.5f;
  playerY = 1.5f;
  playerAngle = 0.0f;
  
  //настройка кнопок
  up.setStepTimeout(50);
  down.setStepTimeout(50);
  left.setStepTimeout(80);
  right.setStepTimeout(80);
  
  while (true) {
    buttons_tick();
    
    //движение
    if (up.isPress() || up.isStep()) {
      float newX = playerX + cos(playerAngle) * moveSpeed;
      float newY = playerY + sin(playerAngle) * moveSpeed;
      
      if (!checkCollision(newX, playerY)) playerX = newX;
      if (!checkCollision(playerX, newY)) playerY = newY;
    }
    
    if (down.isPress() || down.isStep()) {
      float newX = playerX - cos(playerAngle) * moveSpeed;
      float newY = playerY - sin(playerAngle) * moveSpeed;
      
      if (!checkCollision(newX, playerY)) playerX = newX;
      if (!checkCollision(playerX, newY)) playerY = newY;
    }
    
    if (left.isPress() || left.isStep()) {
      playerAngle -= rotSpeed;
    }
    
    if (right.isPress() || right.isStep()) {
      playerAngle += rotSpeed;
    }
    
    while (playerAngle < 0) playerAngle += 2 * 3.14159;
    while (playerAngle >= 2 * 3.14159) playerAngle -= 2 * 3.14159;
    
    //рендеринг
    renderDoomScene();
    
    //выход
    if (ok.isHold()) {
      up.setStepTimeout(400);
      down.setStepTimeout(400);
      left.setStepTimeout(400);
      right.setStepTimeout(400);
      exit(false);
      return;
    }
    //для стабильности
    delay(30); 
  }
}
void mini_apps_menu() {
  setCpuFrequencyMhz(240);
  const char* mini_apps_pages[][6] = {
    { // Страница 1
      "Кубик",
      "Змейка",
      "Ардуино дино",
      "Тетрис",
      "Понг",
      "След.стр ->"
    },
    { // Страница 2
      "Flappy Bird",
      "Арконоид",
      "Space Invaders",
      "Mini 3d graphics",
      "",
      "<- Пред.стр"
    }
  };
  
  const uint8_t pages_count = 2;
  uint8_t current_page = 0;
  int8_t mini_apps_ptr = 0;
  const uint8_t header_height = 16;

  ui_rama("Мини приложения", true, true, true);

  while(true) {
    static uint32_t timer = 0;
    // Отрисовка текущей страницы
    oled.clear(0, header_height, 127, 63);
    
    for(uint8_t i = 0; i < 6; i++) {
      oled.setCursor(2, header_height/8 + i);
      if(mini_apps_ptr == i) oled.print(">");
      else oled.print(" ");
      oled.print(mini_apps_pages[current_page][i]);
    }

    // Индикатор страниц
    oled.setCursor(56, 7);
    oled.print("[");
    oled.print(current_page + 1);
    oled.print("/");
    oled.print(pages_count);
    oled.print("]");

    oled.update();

    buttons_tick();

    // Навигация между страницами
    if(right.isClick() && current_page < pages_count-1) {
      current_page++;
      mini_apps_ptr = 0;
    }
    if(left.isClick() && current_page > 0) {
      current_page--;
      mini_apps_ptr = 0;
    }

    // Вертикальная навигация
    if(up.isClick() && mini_apps_ptr > 0 || (up.isHold() && millis() - timer > 150)) {
      if (mini_apps_ptr < 1) {
          mini_apps_ptr++;
      }
      timer = millis();
      mini_apps_ptr--;
    }
    if(down.isClick() && mini_apps_ptr < 5|| (down.isHold() && millis() - timer > 150)) {
      if (mini_apps_ptr >= 5) {
          mini_apps_ptr--;
      }
      timer = millis();
      mini_apps_ptr++;
    }

    if(ok.isClick()) {
      // Обработка выбора для текущей страницы
      if(current_page == 0) {
        switch(mini_apps_ptr) {
          case 0: dice_random(); break;
          case 1: snake(); break;
          case 2: PlayDinosaurGame(); break;
          case 3: start_tetris_r(); break;
          case 4: pongGame(); break;
          case 5: current_page++; mini_apps_ptr = 0; break; // Переход на след. страницу
        }
      } 
      else if(current_page == 1) {
        switch(mini_apps_ptr) {
          case 0: flappyGame(); break;
          case 1: arkanoidGame(); break;
          case 2: spaceInvadersGame(); break;
          case 3: miniDoomGame(); break;
          case 4: break;
          case 5: current_page--; mini_apps_ptr = 0; break;
        }
      }
      
      // Обновляем интерфейс после возврата
      ui_rama("Мини приложения", true, true, true);
    }
    
    if(left.isHold() || ok.isHold()) {
      exit();
      setCpuFrequencyMhz(80);
      return;
    }
  }
}
// Настройки сети
void networkSettings_ap() {
  String ssid = db[kk::AP_SSID].toString();
  String pass = db[kk::AP_PASS].toString();
  
  while(true) {
      oled.clear();
      oled.setCursor(0, 0);
      oled.print("Сеть: ");
      oled.print(ssid);
      oled.setCursor(0, 2);
      oled.print("Пароль: ");
      oled.print(pass);
      oled.setCursor(0, 4);
      oled.print("OK - выход");
      oled.update();

      buttons_tick();
      
      if(ok.isClick()) {
          return;
      }
  }
}
//регулировка яркости
void brightnessAdjust() {
  int brightness = db[kk::OLED_BRIGHTNESS].toInt();
  
  while(true) {
      oled.clear();
      oled.setCursor(0, 0);
      oled.print("Яркость: ");
      oled.print(brightness);
      oled.setCursor(0, 2);
      oled.print("OK - сохранить");
      oled.setCursor(0, 4);
      oled.print("Зажать OK - выход");
      oled.update();

      buttons_tick();
      
      if(right.isClick()) brightness = constrain(brightness + 20, 0, 255); oled.setContrast(brightness);
      if(left.isClick()) brightness = constrain(brightness - 20, 0, 255); oled.setContrast(brightness);      
      if(up.isClick()) brightness = constrain(brightness + 10, 0, 255); oled.setContrast(brightness);
      if(down.isClick()) brightness = constrain(brightness - 10, 0, 255); oled.setContrast(brightness);
      if(ok.isClick()) {
          if (brightness == 0) brightness = 1;
          db[kk::OLED_BRIGHTNESS] = brightness;
          oled.setContrast(brightness);
          db[kk::OLED_BRIGHTNESS] = brightness;
          db.update(); 
          return;
      }
      if(ok.isHold()) return;
  }
}
void aboutFirmware() {
  String serial_number = db[kk::serial_num].toString();
  ui_rama("О прошивке", true, true, true);
  
  oled.setCursor(0, 2);
  oled.print("Автор: CatDevCode");
  oled.setCursor(0, 3);
  oled.print("Версия: " FIRMWARE_VERSION);
  oled.setCursor(0, 4);
  #ifdef ESP32
    oled.print("Платформа: ESP32");
  #elif ESP8266
    oled.print("Платформа: ESP8266");
  #else
    oled.print("Платформа: Other");
  #endif
  oled.setCursor(0, 5);
  oled.print("Сборка: " __DATE__);
  oled.setCursor(0, 6);
  oled.print("Серийный номер:");
  oled.setCursor(0, 7);
  oled.print(serial_number);
  oled.update();

  while(true) {
      buttons_tick();
      if(ok.isClick() || ok.isHold()) {
          return;
      }
  }
}
void networkSettings_sta() {
  String ssid = db[kk::wifi_ssid].toString();
  String pass = db[kk::wifi_pass].toString();
  
  while(true) {
      oled.clear();
      oled.setCursor(0, 0);
      oled.print("Сеть: ");
      oled.print(ssid);
      oled.setCursor(0, 2);
      oled.print("Пароль: ");
      oled.print(pass);
      oled.setCursor(0, 4);
      oled.print("OK - выход");
      oled.update();

      buttons_tick();
      
      if(ok.isClick()) {
          return;
      }
  }
}
void auto_brightness_setting() {
  settings_auto_bright = db[kk::auto_bright].toBool();
  
  while(true) {
      oled.clear();
      oled.setCursor(0, 0);
      oled.print("Авто яркость:");
      if (settings_auto_bright){
        oled.print("Вкл");
      } else {
        oled.print("Выкл");
      }
      oled.setCursor(0, 4);
      oled.print("OK - Вкл/Выкл");
      oled.update();

      buttons_tick();
      
      if(ok.isClick()) {
        settings_auto_bright = !settings_auto_bright;
      }
      if(ok.isHold()) {
        db[kk::auto_bright] = settings_auto_bright;
        db.update(); 
        exit(); return;
      }
  }
}
void settingDisplay() {
  const char* settings_items[] = {
    "Яркость",
    "Авто яркость",
    "Назад"
  };
  const uint8_t settings_apps_count = sizeof(settings_items)/sizeof(settings_items[0]);
  int8_t settings_apps_ptr = 0;
  const uint8_t header_height = 16; // Высота заголовка с линией

  ui_rama("Настройки дисплея", true, true, true);
  
  while(true) {
    // Очищаем только область меню (начиная с 3 строки)
    oled.clear(0, header_height, 127, 63);
    
    // Рисуем только первый видимый пункт
    oled.setCursor(2, header_height/8 + 0); // 3 строка (16px)
    oled.print(settings_apps_ptr == 0 ? ">" : " ");
    oled.print(settings_items[0]);

    // Рисуем остальные пункты если есть
    if(settings_apps_count > 1) {
      for(uint8_t i = 1; i < settings_apps_count; i++) {
        oled.setCursor(2, header_height/8 + i); // Следующие строки
        oled.print(settings_apps_ptr == i ? ">" : " ");
        oled.print(settings_items[i]);
      }
    }
    
    oled.update();

    buttons_tick();

    if(up.isClick() && settings_apps_ptr > 0) {
      settings_apps_ptr--;
    }
    if(down.isClick() && settings_apps_ptr < settings_apps_count - 1) {
      settings_apps_ptr++;
    }

    if(ok.isClick()) {
      switch(settings_apps_ptr) {
        case 0: brightnessAdjust(); break;
        case 1: auto_brightness_setting(); break;
        case 2: exit(false); return;
      }
      // Перерисовываем интерфейс после возврата
      ui_rama("Настройки дисплея", true, true, true);
    }

  }
}
void settingsMenu() {
  const char* settings_items[] = {
    "Дисплей",
    "Сеть AP",
    "Сеть STA",
    "О прошивке",
    "Назад"
  };
  const uint8_t settings_apps_count = sizeof(settings_items)/sizeof(settings_items[0]);
  int8_t settings_apps_ptr = 0;
  const uint8_t header_height = 16; // Высота заголовка с линией

  ui_rama("Настройки", true, true, true);
  
  while(true) {
    // Очищаем только область меню (начиная с 3 строки)
    oled.clear(0, header_height, 127, 63);
    
    // Рисуем только первый видимый пункт
    oled.setCursor(2, header_height/8 + 0); // 3 строка (16px)
    oled.print(settings_apps_ptr == 0 ? ">" : " ");
    oled.print(settings_items[0]);

    // Рисуем остальные пункты если есть
    if(settings_apps_count > 1) {
      for(uint8_t i = 1; i < settings_apps_count; i++) {
        oled.setCursor(2, header_height/8 + i); // Следующие строки
        oled.print(settings_apps_ptr == i ? ">" : " ");
        oled.print(settings_items[i]);
      }
    }
    
    oled.update();

    buttons_tick();

    if(up.isClick() && settings_apps_ptr > 0) {
      settings_apps_ptr--;
    }
    if(down.isClick() && settings_apps_ptr < settings_apps_count - 1) {
      settings_apps_ptr++;
    }

    if(ok.isClick()) {
      switch(settings_apps_ptr) {
        case 0: settingDisplay(); break;
        case 1: networkSettings_ap(); break;
        case 2: networkSettings_sta(); break;
        case 3: aboutFirmware(); break;
        case 4: exit(); resetButtons(); return;
      }
      // Перерисовываем интерфейс после возврата
      ui_rama("Настройки", true, true, true);
    }
  }
}
void Utilities_menu() {
  const char* settings_items[] = {
    "Секундомер",
    "Таймер",
    "Управление GPIO",
    "Выход"
  };
  const uint8_t settings_apps_count = sizeof(settings_items)/sizeof(settings_items[0]);
  int8_t settings_apps_ptr = 0;
  const uint8_t header_height = 16; // Высота заголовка с линией

  ui_rama("Утилиты", true, true, true);
  
  while(true) {
    // Очищаем только область меню (начиная с 3 строки)
    oled.clear(0, header_height, 127, 63);
    
    // Рисуем только первый видимый пункт
    oled.setCursor(2, header_height/8 + 0); // 3 строка (16px)
    oled.print(settings_apps_ptr == 0 ? ">" : " ");
    oled.print(settings_items[0]);

    // Рисуем остальные пункты если есть
    if(settings_apps_count > 1) {
      for(uint8_t i = 1; i < settings_apps_count; i++) {
        oled.setCursor(2, header_height/8 + i); // Следующие строки
        oled.print(settings_apps_ptr == i ? ">" : " ");
        oled.print(settings_items[i]);
      }
    }
    
    oled.update();

    buttons_tick();

    if(up.isClick() && settings_apps_ptr > 0) {
      settings_apps_ptr--;
    }
    if(down.isClick() && settings_apps_ptr < settings_apps_count - 1) {
      settings_apps_ptr++;
    }

    if(ok.isClick()) {
      switch(settings_apps_ptr) {
        case 0: stopwatch(); break;
        case 1: timer_oled(); break;
        case 2: choice_gpio(); break;
        case 3: exit(); return;
      }
      // Перерисовываем интерфейс после возврата
      ui_rama("Утилиты", true, true, true);
    }
  }
}
void menu_default() {
  while (true) {
    static uint32_t timer = 0;
    buttons_tick();

    static uint32_t batteryTimer = millis(); // Таймер для батареи
    // Автоматическое обновление батареи каждые 5 секунд
    if (millis() - batteryTimer > 5000) {
        batteryTimer = millis();
        drawbattery();
    }
    if (up.isClick() || (up.isHold() && millis() - timer > 150)) {
        if (pointer > 0) {
            pointer--;
            if (pointer < top_item) top_item--;
        }
        timer = millis();
        updatePointer();
    }

    if (down.isClick() || (down.isHold() && millis() - timer > 150)) {
        if (pointer < ITEMS - 1) {
            pointer++;
            if (pointer >= top_item + VISIBLE_ITEMS) top_item++;
        }
        timer = millis();
        updatePointer();
        
    }
    if (ok.isClick()) {   // Нажатие на ОК - переход в пункт меню
      switch (pointer) {  // По номеру указателей располагаем вложенные функции (можно вложенные меню)
        case 0: mini_apps_menu(); break;  // По нажатию на ОК при наведении на 0й пункт вызвать функцию
        case 1: settingsMenu(); break;
        case 2: ShowFilesLittleFS(); break;
        case 3: calc(); break;
        case 4: create_settings(); break;
        case 5: Utilities_menu(); break;
      }
    }
    }
}
void loop() {
  menu_default();
}