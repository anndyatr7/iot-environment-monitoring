#define BLYNK_TEMPLATE_ID "TMPL6OkhqezV7"
#define BLYNK_TEMPLATE_NAME "iot environment monitoring"
#define BLYNK_AUTH_TOKEN "ILLSguKfxvCVi4qRlZuxnLakRR-QsMHq"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// =====================================
// 1. KONFIGURASI PIN
// =====================================

#define DHT_PIN 15
#define DHT_TYPE DHT22

#define TRIG_PIN 5
#define ECHO_PIN 18

#define BUZZER_PIN 19
#define BUTTON_PIN 23

// =====================================
// 2. KONEKSI WIFI WOKWI
// =====================================

char ssid[] = "Wokwi-GUEST";
char pass[] = "";

// =====================================
// 3. INISIALISASI KOMPONEN
// =====================================

DHT dht(DHT_PIN, DHT_TYPE);

LiquidCrystal_I2C lcd(0x27, 16, 2);

BlynkTimer timer;

// =====================================
// 4. VARIABEL SENSOR DAN STATUS
// =====================================

float suhu = 0;
float kelembapan = 0;
float jarak = 0;

bool sensorValid = false;
bool objectDetected = false;

String roomStatus = "Memulai...";

// Kontrol buzzer dari Blynk
bool buzzerEnabled = true;

// Kondisi buzzer yang sedang aktif
bool buzzerActive = false;

// Tombol fisik membisukan alarm sampai
// kondisi bahaya selesai
bool alarmMuted = false;

// Variabel pembacaan tombol
bool lastButtonState = HIGH;
bool buttonState = HIGH;
unsigned long lastDebounceTime = 0;

const unsigned long debounceDelay = 50;

// =====================================
// 5. MENERIMA KONTROL DARI BLYNK
// =====================================

BLYNK_WRITE(V4) {
  buzzerEnabled = param.asInt();

  if (!buzzerEnabled) {
    buzzerActive = false;
    noTone(BUZZER_PIN);
  }
}

BLYNK_CONNECTED() {
  Blynk.syncVirtual(V4);
}

// =====================================
// 6. MEMBACA JARAK HC-SR04
// =====================================

float bacaJarak() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  unsigned long durasi = pulseIn(ECHO_PIN, HIGH, 30000);

  if (durasi == 0) {
    return -1;
  }

  // Kecepatan suara sekitar 0,0343 cm/us.
  // Dibagi 2 karena suara pergi dan kembali.
  float hasil = durasi * 0.0343 / 2.0;

  return hasil;
}

// =====================================
// 7. MEMBACA SENSOR
// =====================================

void bacaSensor() {
  suhu = dht.readTemperature();
  kelembapan = dht.readHumidity();
  jarak = bacaJarak();

  sensorValid = !isnan(suhu) && !isnan(kelembapan);

  if (jarak >= 0) {
    objectDetected = (jarak < 80);
  } else {
    objectDetected = false;
  }

  // Tampilkan hasil di Serial Monitor
  Serial.println("======================");

  if (sensorValid) {
    Serial.print("Suhu: ");
    Serial.print(suhu);
    Serial.println(" C");

    Serial.print("Kelembapan: ");
    Serial.print(kelembapan);
    Serial.println(" %");
  } else {
    Serial.println("Sensor DHT22 gagal dibaca!");
  }

  if (jarak >= 0) {
    Serial.print("Jarak: ");
    Serial.print(jarak);
    Serial.println(" cm");
  } else {
    Serial.println("Jarak tidak terbaca");
  }
}

// =====================================
// 8. MENENTUKAN STATUS RUANGAN
// =====================================

void tentukanStatus() {
  if (!sensorValid) {
    roomStatus = "Sensor Error";
    return;
  }

  bool suhuNormal = (suhu >= 18 && suhu <= 25);
  bool kelembapanNormal = (kelembapan > 75);

  bool lingkunganNormal = suhuNormal && kelembapanNormal;

  if (objectDetected && !lingkunganNormal) {
    roomStatus = "Bahaya";
  }
  else if (!suhuNormal) {
    roomStatus = "Suhu Abnormal";
  }
  else if (!kelembapanNormal) {
    roomStatus = "Peringatan RH";
  }
  else if (objectDetected) {
    roomStatus = "Objek Terdeteksi";
  }
  else {
    roomStatus = "Normal";
  }

  Serial.print("Status: ");
  Serial.println(roomStatus);
}

// =====================================
// 9. MENGONTROL BUZZER DAN TOMBOL
// =====================================


void kontrolBuzzer() {
  static bool lastReading = HIGH;
  static unsigned long lastDebounceTime = 0;

  bool reading = digitalRead(BUTTON_PIN);

  // Deteksi perubahan kondisi tombol
  if (reading != lastReading) {
    lastDebounceTime = millis();
  }

  // Tunggu 50 ms agar pembacaan tombol stabil
  if (millis() - lastDebounceTime > 50) {
    static bool stableState = HIGH;

    if (reading != stableState) {
      stableState = reading;

      // Tombol baru saja ditekan
      if (stableState == LOW && objectDetected) {
        alarmMuted = !alarmMuted;
      }
    }
  }

  lastReading = reading;

  // Tentukan apakah buzzer harus berbunyi
  buzzerActive = objectDetected &&
                 buzzerEnabled &&
                 !alarmMuted;

  if (buzzerActive) {
    tone(BUZZER_PIN, 1000);
  } else {
    noTone(BUZZER_PIN);
  }

  // Reset mute setelah objek menjauh
  if (!objectDetected) {
    alarmMuted = false;
  }
}


// =====================================
// 10. MENAMPILKAN INFORMASI DI LCD
// =====================================

void tampilkanLCD() {
  lcd.clear();

  if (!sensorValid) {
    lcd.setCursor(0, 0);
    lcd.print("Sensor Error");

    lcd.setCursor(0, 1);
    lcd.print("Cek DHT22");
    return;
  }

  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(suhu, 1);
  lcd.print("C H:");
  lcd.print(kelembapan, 0);
  lcd.print("%");

  lcd.setCursor(0, 1);
  lcd.print(roomStatus);
}

// =====================================
// 11. MENGIRIM DATA KE BLYNK
// =====================================

void kirimKeBlynk() {
  if (!Blynk.connected()) {
    return;
  }

  if (sensorValid) {
    Blynk.virtualWrite(V0, suhu);
    Blynk.virtualWrite(V1, kelembapan);
  }

  if (jarak >= 0) {
    Blynk.virtualWrite(V2, jarak);
  }

  Blynk.virtualWrite(V3, roomStatus);
  Blynk.virtualWrite(V5, objectDetected ? 1 : 0);
  Blynk.virtualWrite(V6, buzzerActive ? 1 : 0);
}

// =====================================
// 12. SETUP: DIJALANKAN SEKALI
// =====================================

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  dht.begin();

  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Plant Lab IoT");
  lcd.setCursor(0, 1);
  lcd.print("Connecting...");

  // Menghubungkan ESP32 ke Blynk
  Blynk.begin(
    BLYNK_AUTH_TOKEN,
    ssid,
    pass
  );

  // Membaca sensor setiap 2 detik
  timer.setInterval(2000L, bacaSensor);

  // Memperbarui LCD dan status
  timer.setInterval(1000L, []() {
    tentukanStatus();
    kontrolBuzzer();
    tampilkanLCD();
  });

  // Mengirim data ke Blynk setiap 2 detik
  timer.setInterval(2000L, kirimKeBlynk);
}

// =====================================
// 13. LOOP: DIJALANKAN BERULANG
// =====================================

void loop() {
  Blynk.run();
  timer.run();
  kontrolBuzzer();
}
