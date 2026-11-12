#include <sensorfusion_inferencing.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h" 
#include <math.h>
#include <Preferences.h> // ESP32 Kalıcı hafıza için eklendi

// --- PİN TANIMLAMALARI ---
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9
#define GSR_PIN     1
#define NTC_PIN     11
#define MOTOR_PIN   3 // Titreşim motoru pinini kendi bağlantına göre değiştir
#define LED_PIN     7 // LED pinini kendi bağlantına göre değiştir (PWM destekli pin olmalı)

// --- NTC MATEMATİK SABİTLERİ ---
const float B_COEFFICIENT = 3950.0;
const float SERIES_RESISTOR = 124000.0;

// --- EDGE IMPULSE BELLEK DEĞİŞKENLERİ ---
const size_t expected_features = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
float *features = nullptr;
size_t feature_ix = 0; 

// --- KALİBRASYON DEĞİŞKENLERİ ---
Preferences preferences;
bool isCalibrating = true;
unsigned long calibrationStartTime = 0;
const unsigned long CALIBRATION_DURATION = 5 * 60 * 1000UL; // 5 dakika (Milisaniye)

int calibSampleCount = 0;
float calibEdaSum = 0;
float calibTempSum = 0;
float currentEdaAvg = 0;
float currentTempAvg = 0;

float baselineEda = 0.0;
float baselineTemp = 0.0;

// --- ÖRNEKLEME & ANOMALİ ZAMANLAYICILARI ---
unsigned long last_sample_time = 0;
unsigned long sample_interval_ms = 1000 / EI_CLASSIFIER_FREQUENCY; 
bool anomaliTetiklendi = false;
unsigned long anomaliZamanlayici = 0;

// --- MAX30102 DEĞİŞKENLERİ ---
MAX30105 particleSensor;
const byte RATE_SIZE = 4;
byte rates[RATE_SIZE]; 
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial);

    pinMode(MOTOR_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(MOTOR_PIN, LOW);
    analogWrite(LED_PIN, 0);

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN); 
    particleSensor.setup(); 
    particleSensor.setPulseAmplitudeRed(0x0A); 

    // Hafızayı başlat ve eski baselineları çek (ilk 5 dk yine de kalibre edeceğiz)
    preferences.begin("kalibrasyon", false);
    baselineEda = preferences.getFloat("baseEda", 0.0);
    baselineTemp = preferences.getFloat("baseTemp", 0.0);
    calibrationStartTime = millis(); // 5 dakikalık kalibrasyon sayacını başlat

    features = (float*)malloc(expected_features * sizeof(float));
    if (features == nullptr) {
        Serial.println("Hata: Bellek yetersiz!");
        while (1);
    }
}

void loop() {
    // --- 0. GERİ BİLDİRİM (HAPTİK & OPTİK) KONTROLÜ ---
    // Bu blok delay içermez, işlemciyi dondurmadan sürekli çalışır
    if (anomaliTetiklendi) {
        // Anomali süresini 5 saniye (5000 ms) olarak belirledik
        if (millis() - anomaliZamanlayici < 5000) {
            
            // MOTOR: Kesik kesik çalışma (300ms AÇIK, 300ms KAPALI)
            if ((millis() % 600) < 300) {
                digitalWrite(MOTOR_PIN, HIGH);
            } else {
                digitalWrite(MOTOR_PIN, LOW);
            }

            // LED: Yavaşça nefes alma efekti (Sinüs dalgası matametiği)
            // millis() değerini kullanarak PWM dalgasını 0 ile 255 arasında yumuşakça gezdirir
            int nefesEfekti = 127 + 127 * sin(millis() / 400.0); 
            analogWrite(LED_PIN, nefesEfekti);

        } else {
            // 5 saniye dolduğunda sistemi sustur
            anomaliTetiklendi = false;
            digitalWrite(MOTOR_PIN, LOW);
            analogWrite(LED_PIN, 0);
        }
    }

    // --- 1. SÜREKLİ NABIZ KONTROLÜ ---
    long irValue = particleSensor.getIR();
    
    if (checkForBeat(irValue) == true) {
        long delta = millis() - lastBeat;
        lastBeat = millis();
        beatsPerMinute = 60 / (delta / 1000.0);
        
        if (beatsPerMinute < 255 && beatsPerMinute > 20) {
            rates[rateSpot++] = (byte)beatsPerMinute; 
            rateSpot %= RATE_SIZE; 
            beatAvg = 0;
            for (byte x = 0 ; x < RATE_SIZE ; x++) {
                beatAvg += rates[x];
            }
            beatAvg /= RATE_SIZE;
        }
    }
    if (irValue < 50000) {
        beatAvg = 0;
    }

    // --- 2. EDGE IMPULSE ZAMANLAYICISI ---
    if (millis() - last_sample_time >= sample_interval_ms) {
        last_sample_time = millis();

        int gsrDegeri = analogRead(GSR_PIN);
        float sicaklikC = 0.0;
        int numSamples = 10;
  long totalAnalog = 0.0;
  for (int i = 0; i < numSamples; i++) {
    totalAnalog += analogRead(NTC_PIN);
    delay(10);}
    float ntcAnalog = (float)totalAnalog / numSamples;
  float ntc_V = ntcAnalog / 1000.0; // Milivoltu volta çevir (örn: 1.16V)

    // NTC kısa devre veya açık devre değilse
if (ntcAnalog > 0.0 && ntcAnalog < 4095.0) { 
    // Matematik artık 100k yerine kalibre edilmiş 124k sabiti üzerinden çalışır
    float ntcDirenc = SERIES_RESISTOR / (4095.0 / ntcAnalog - 1.0);
    
    float sicaklikK = 1.0 / (1.0 / 298.15 + (1.0 / B_COEFFICIENT) * log(ntcDirenc / 100000.0));
    sicaklikC = sicaklikK - 273.15;
            Serial.print("Ham ADC: ");
Serial.print(ntcAnalog);
Serial.print(" | Hesaplanan Direnc: ");
Serial.println(ntcDirenc);

        }
float temp_yapay_zeka = sicaklikC;
        float eda_yapay_zeka = 1.0 + ((float)gsrDegeri / 4095.0) * 2.0; 

        if (isCalibrating) {
            // İLK 5 DAKİKA - SADECE KALİBRASYON (AI KAPALI)
            if (millis() - calibrationStartTime < CALIBRATION_DURATION) {
                // Değerler arası fark çoksa (Outlier) ortalamanın dışında tutma kontrolü
                bool isOutlier = false;
                if (calibSampleCount > 5) { // İlk 5 ölçümden sonra filtreleme başlasın
                    // Mevcut ortalamadan %50'den veya 5 dereceden fazla fark varsa outlier kabul et
                    if (abs(eda_yapay_zeka - currentEdaAvg) > currentEdaAvg * 0.5) isOutlier = true;
                    if (abs(temp_yapay_zeka - currentTempAvg) > 5.0) isOutlier = true;
                }
                
                if (!isOutlier) {
                    calibEdaSum += eda_yapay_zeka;
                    calibTempSum += temp_yapay_zeka;
                    calibSampleCount++;
                    currentEdaAvg = calibEdaSum / calibSampleCount;
                    currentTempAvg = calibTempSum / calibSampleCount;
                }

                static unsigned long lastCalibPrint = 0;
                if (millis() - lastCalibPrint > 1000) {
                    Serial.print("Kalibrasyon suruyor... Kalan sure: ");
                    Serial.print((CALIBRATION_DURATION - (millis() - calibrationStartTime)) / 1000);
                    Serial.println(" saniye");
                    lastCalibPrint = millis();
                }
            } else {
                // Kalibrasyon süresi bitti, sonuçları EEPROM'a (Preferences) kaydet
                isCalibrating = false;
                if (calibSampleCount > 0) {
                    baselineEda = currentEdaAvg;
                    baselineTemp = currentTempAvg;
                    
                    preferences.putFloat("baseEda", baselineEda);
                    preferences.putFloat("baseTemp", baselineTemp);
                    
                    Serial.println("Kalibrasyon bitti ve ESP'nin kalici hafizasina kaydedildi.");
                    Serial.print("Base EDA: "); Serial.println(baselineEda);
                    Serial.print("Base TEMP: "); Serial.println(baselineTemp);
                } else {
                    Serial.println("Hata: Kalibrasyon sirasinda yeterli gecerli veri alinamadi.");
                }
            }
        } else {
            // DİZİYİ DOLDUR (Eğitim Verisi Formatına Dönüştürerek) - AI AÇIK
            if (feature_ix + 5 < expected_features) {
                
                // --- 1. ACC (Sahte İvmeölçer) Dönüşümü ---
                float acc_x = random(-30, 80); 
                float acc_y = random(-30, 30);
                float acc_z = random(20, 50);
                
                // --- 2 & 3. EDA ve TEMP YÜZDELİK ORAN HESAPLAMASI ---
                // (Anlık - Baseline) / Baseline
                float eda_percentage = (baselineEda > 0) ? ((eda_yapay_zeka - baselineEda) / baselineEda) : 0;
                float temp_percentage = (baselineTemp > 0) ? ((temp_yapay_zeka - baselineTemp) / baselineTemp) : 0;

                // --- 4. BVP (Nabız Dalga Formu) Dönüşümü ---
                float bvp_yapay_zeka = 0.0;
                if (beatAvg > 20) {
                    float frekans = beatAvg / 60.0; 
                    float zaman_sn = millis() / 1000.0;
                    bvp_yapay_zeka = 60.0 * sin(2 * PI * frekans * zaman_sn); 
                }
                
                // AI'a verilerin basılması
                features[feature_ix++] = acc_x;
                features[feature_ix++] = acc_y;
                features[feature_ix++] = acc_z;
                features[feature_ix++] = eda_percentage; // Ham değil, oransal
                features[feature_ix++] = temp_percentage; // Ham değil, oransal
                features[feature_ix++] = bvp_yapay_zeka;

                Serial.print("EDA (Oran): "); Serial.print(eda_percentage); 
                Serial.print(" | TEMP (Oran): "); Serial.print(temp_percentage); 
                Serial.print(" | BVP: "); Serial.println(bvp_yapay_zeka);
            }

            if (feature_ix >= expected_features) {
                signal_t features_signal;
                int err = numpy::signal_from_buffer(features, expected_features, &features_signal);
                if (err == 0) {
                    ei_impulse_result_t result = { 0 };
                    err = run_classifier(&features_signal, &result, false);
                    
                    if (err == EI_IMPULSE_OK) {
                        
                        // --- ANOMALİ (KRİZ) TETİKLEYİCİSİ ---
                        #if EI_CLASSIFIER_HAS_ANOMALY == 1
                        if (result.anomaly > 0.3) {
                            anomaliTetiklendi = true;
                            anomaliZamanlayici = millis(); 
                            Serial.println("DIKKAT: Anomali (Kriz) Algilandi! Haptik uyari baslatildi.");
                        }
                        #endif
                    }
                }
                feature_ix = 0; 
            }
        }
    }
}