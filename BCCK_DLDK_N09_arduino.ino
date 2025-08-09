/********************************************************************************/
/*             ĐỒ ÁN HỌC PHẦN ĐO LƯỜNG VÀ ĐIỀU KHIỂN BẰNG MÁY TÍNH              */
/*                                    NHÓM 09                                   */
/*                    THÀNH VIÊN: Nguyễn Duy Tân - Leader                       */
/*                                Đặng Hoàng Khải                               */
/*                                Trương Duy Linh                               */
/*                                Nguyễn Ngọc Tính                              */
/*                                                                              */
/*                          CTU - Cần Thơ - 30/3/2025                           */
/********************************************************************************/

#include <SoftwareSerial.h>
#include <ModbusMaster.h>
#include <ModbusRTUSlave.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>
//---------------------------------------------------
/**
  * Khai báo nút nhấn và module rotary encoder 
  */
#define SETUP_PIN 12
#define PHA1_PIN 2
#define PHA2_PIN 3

//---------------------------------------------------
/**
  * Khai báo địa chỉ EEPROM lưu ID của thiết bị và biến tần.
  * ID của thiết bị được lưu vào -- 0
  * ID của biến tần được lưu vào -- 1
  */
#define DEVICE_ID_ADDRESS_IN_EEPROM 0
#define INVERTER_ID_ADDRESS_IN_EEPROM 1


//---------------------------------------------------
/**
  * Thiết lập lcd 16x2
  */
#define RS_PIN 9
#define EN_PIN 8
#define D4_PIN 7
#define D5_PIN 6
#define D6_PIN 5
#define D7_PIN 4
LiquidCrystal lcd(RS_PIN, EN_PIN, D4_PIN, D5_PIN, D6_PIN, D7_PIN);

//---------------------------------------------------
/**
  * Thiết lập cổng Serial phụ
  * Do Arduino nano chỉ hỗ trợ 1 cổng serial
  * Rx = 10, Tx = 11
  */
SoftwareSerial MODBUS_MASTER_SERIAL(10, 11);

//---------------------------------------------------
/**
  * Thiết lập thông số Modbus Master - Điều khiển biến tần
  * mbm1 điều khiển biến tần
  */
#define MODBUS_MASTER_BAUD 9600
#define MODBUS_MASTER_CONFIG SERIAL_8N1
#define MODBUS_INVERTER_SLAVE_ID EEPROM.read(INVERTER_ID_ADDRESS_IN_EEPROM)
ModbusMaster mbm1;

// Thiết lập thông số Modbus Slave - Nhận lệnh điều khiển từ PC
#define MODBUS_SLAVE_SERIAL Serial
#define MODBUS_SLAVE_BAUD 9600
#define MODBUS_SLAVE_CONFIG SERIAL_8N1
#define MODBUS_SLAVE_INIT_ID EEPROM.read(DEVICE_ID_ADDRESS_IN_EEPROM)
ModbusRTUSlave mbs(MODBUS_SLAVE_SERIAL);

//---------------------------------------------------
/**
  * Địa chỉ Modbus điều khiển biến tần  
  */
#define CONTROL_WORD 8501
#define FREQUENCY_REFERENCE 8502
#define OUTPUT_FREQUENCY 3202
#define CURRENT_IN_THE_MOTOR 3204
#define MOTOR_TORQUE 3205

// Cài đặt các trạng thái điều khiển trong thanh ghi CONTROL_WORD
#define KHOI_DONG 0x06
#define DUNG 0x100F
#define QUAY_THUAN 0x0F
#define QUAY_NGHICH 0x80F

//---------------------------------------------------
/**
  * Các thanh ghi trong MCU
  */
typedef enum {
  iCONTROL_WORD,
  iFREQUENCY_REFERENCE,
  iOUTPUT_FREQUENCY,
  iCURRENT_IN_THE_MOTOR,
  iMOTOR_TORQUE,
  iLENGTH_REGISTER
};
uint16_t registerAddress[iLENGTH_REGISTER];

//---------------------------------------------------
/**
  * Chế độ của thiết bị
  */
typedef enum {
  SETUP,
  RUN,
  SETUP_TO_RUN,
  RUN_TO_SETUP
} deviceStatus;
deviceStatus deviceMode = RUN;

//---------------------------------------------------
/**
  * Chế độ của setup
  * Bao gồm: Cấu hình id cho thiết bị, Cấu hình id cho biến tần
  */
typedef enum {
  SETUP_DEVICE_ID,
  SETUP_INVERTER_ID
} setupStatus;
setupStatus setupMode = SETUP_DEVICE_ID;

//---------------------------------------------------
/**
  * Biến toàn cục
  */
uint16_t lastDataFerq = 0, currentDataFreq = 0;
uint16_t lastDataControl = 0, currentDataControl = 0;
volatile int g_nIDDevice = 1;

//===================================================
// SETUP
//===================================================
void setup() {
  // Cấu hình bảng điều khiển trên board MCU
  pinMode(SETUP_PIN, INPUT_PULLUP);
  pinMode(PHA1_PIN, INPUT);
  pinMode(PHA2_PIN, INPUT);

  // Cấu hình Modbus Master
  MODBUS_MASTER_SERIAL.begin(MODBUS_MASTER_BAUD);
  mbm1.begin(MODBUS_INVERTER_SLAVE_ID, MODBUS_MASTER_SERIAL);

  // Cấu hình Modbus Slave
  mbs.configureHoldingRegisters(registerAddress, iLENGTH_REGISTER);
  MODBUS_SLAVE_SERIAL.begin(MODBUS_SLAVE_BAUD, MODBUS_SLAVE_CONFIG);
  mbs.begin(MODBUS_SLAVE_INIT_ID, MODBUS_SLAVE_BAUD);

  // Cấu hình LCD 16x2
  lcd.begin(16, 2);

  khoiDongDongCo();
}

//===================================================
// LOOP
//===================================================
void loop() {

  // Chế độ hệ thống
  switch (deviceMode) {
    case SETUP:
      {
        switch (setupMode) {
          case SETUP_DEVICE_ID:
            {
              // Hiển thị lên lcd
              static bool s_bEnableLcd = true;
              static int s_nLastIDDevice = g_nIDDevice;
              if (s_bEnableLcd) {
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("SET DEVICE ID:");
                lcd.setCursor(0, 1);
                lcd.print(g_nIDDevice);

                s_bEnableLcd = false;
              }
              if (s_nLastIDDevice != g_nIDDevice) {
                s_nLastIDDevice = g_nIDDevice;
                s_bEnableLcd = true;
              }
              
              // Đọc dữ liệu từ EEPROM
              static bool s_bEnableEEPROM = true;
              if (s_bEnableEEPROM) {
                g_nIDDevice = EEPROM.read(DEVICE_ID_ADDRESS_IN_EEPROM);

                s_bEnableEEPROM = false;
              }

              // Kiểm tra nút nhấn
              if (digitalRead(SETUP_PIN) == LOW) {
                delay(200);
                unsigned long l_nLasttime = millis();
                bool l_bChangeDeviceMode = false;

                // Nhấn giữ 3s
                while (digitalRead(SETUP_PIN) == LOW) {
                  if (millis() - l_nLasttime >= 3000) {
                    deviceMode = SETUP_TO_RUN;
                    l_bChangeDeviceMode = true;
                    break;
                  }
                }

                if (l_bChangeDeviceMode == false) {
                  // Cập nhật ID_Device vào EEPROM
                  EEPROM.update(DEVICE_ID_ADDRESS_IN_EEPROM, g_nIDDevice);
                  // Reset biến toàn cục ID
                  s_bEnableEEPROM = true;
                  s_bEnableLcd = true;
                  setupMode = SETUP_INVERTER_ID;
                  break;

                } else {
                  // Cập nhật ID_Device vào EEPROM
                  EEPROM.update(DEVICE_ID_ADDRESS_IN_EEPROM, g_nIDDevice);
                  // Reset các biến cho phép hoạt động
                  s_bEnableEEPROM = true;
                  s_bEnableLcd = true;
                  break;
                }
              }
              
              delay(10);

              break;
            }


          case SETUP_INVERTER_ID:
            {
              // Hiển thị lên lcd
              static bool s_bEnableLcd = true;
              static int s_nLastIDDevice = g_nIDDevice;
              if (s_bEnableLcd) {
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("SET INVERTER ID:");
                lcd.setCursor(0, 1);
                lcd.print(g_nIDDevice);

                s_bEnableLcd = false;
              }
              if (s_nLastIDDevice != g_nIDDevice) {
                s_nLastIDDevice = g_nIDDevice;
                s_bEnableLcd = true;
              }

              // Đọc dữ liệu từ EEPROM
              static bool s_bEnableEEPROM = true;
              if (s_bEnableEEPROM) {
                g_nIDDevice = EEPROM.read(INVERTER_ID_ADDRESS_IN_EEPROM);

                s_bEnableEEPROM = false;
              }

              // Kiểm tra nút nhấn
              if (digitalRead(SETUP_PIN) == LOW) {
                delay(200);
                unsigned long l_nLasttime = millis();
                bool l_bChangeDeviceMode = false;

                // Nhấn giữ 3s
                while (digitalRead(SETUP_PIN) == LOW) {
                  if (millis() - l_nLasttime >= 3000) {
                    deviceMode = SETUP_TO_RUN;
                    l_bChangeDeviceMode = true;
                    break;
                  }
                }

                if (l_bChangeDeviceMode == false) {
                  // Cập nhật ID_Device vào EEPROM
                  EEPROM.update(INVERTER_ID_ADDRESS_IN_EEPROM, g_nIDDevice);
                  // Reset các biến cho phép hoạt động
                  s_bEnableEEPROM = true;
                  s_bEnableLcd = true;
                  setupMode = SETUP_DEVICE_ID;
                  break;

                } else {
                  // Cập nhật ID_Device vào EEPROM
                  EEPROM.update(INVERTER_ID_ADDRESS_IN_EEPROM, g_nIDDevice);
                  // Reset các biến cho phép hoạt động
                  s_bEnableEEPROM = true;
                  s_bEnableLcd = true;
                  break;
                }
              }

              delay(10);

              break;
            }
        }

        break;
      }

    case SETUP_TO_RUN:
      {
        // Hủy ngắt ngoài
        detachInterrupt(0);

        // Cấu hình lại Modbus Master
        mbm1.begin(MODBUS_INVERTER_SLAVE_ID, MODBUS_MASTER_SERIAL);

        // Cấu hình lại Modbus Slave
        mbs.begin(MODBUS_SLAVE_INIT_ID, MODBUS_SLAVE_BAUD);

        deviceMode = RUN;

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("----RUN--->");

        delay(3000);
        break;
      }


    case RUN:
      {
        // Kiểm tra nút nhấn
        if (digitalRead(SETUP_PIN) == LOW) {
          delay(200);
          unsigned long l_nLasttime = millis();

          // Nhấn giữ 3s
          while (digitalRead(SETUP_PIN) == LOW) {
            if (millis() - l_nLasttime >= 3000) {
              deviceMode = RUN_TO_SETUP;
              break;
            }
          }
        }

        // Chương trình thực thi
        mbs.poll();

        currentDataControl = registerAddress[iCONTROL_WORD];
        if (lastDataControl != currentDataControl) {
          switch (currentDataControl) {
            case QUAY_THUAN:
              quayThuan();
              break;
            
            case QUAY_NGHICH:
              quayNghich();
              break;

            case DUNG:
              dungDongCo();
              break;
          }

          lastDataControl = currentDataControl;
        }

        delay(100);

        currentDataFreq = registerAddress[iFREQUENCY_REFERENCE];
        if (lastDataFerq != currentDataFreq) {
          datTanSo(registerAddress[iFREQUENCY_REFERENCE]);
          lastDataFerq = currentDataFreq;
        }

        delay(100);

        if (mbm1.readHoldingRegisters(OUTPUT_FREQUENCY, 1) == 0) {
          registerAddress[iOUTPUT_FREQUENCY] = mbm1.getResponseBuffer(0);
          mbm1.clearResponseBuffer();

          if (registerAddress[iOUTPUT_FREQUENCY] > 200) {
            registerAddress[iOUTPUT_FREQUENCY] = 65536 - registerAddress[iOUTPUT_FREQUENCY];
          }

        }

        delay(100);

        if (mbm1.readHoldingRegisters(CURRENT_IN_THE_MOTOR, 1) == 0) {
          registerAddress[iCURRENT_IN_THE_MOTOR] = mbm1.getResponseBuffer(0);
          mbm1.clearResponseBuffer();
        }

        delay(100);

        if (mbm1.readHoldingRegisters(MOTOR_TORQUE, 1) == 0) {
          registerAddress[iMOTOR_TORQUE] = mbm1.getResponseBuffer(0);
          mbm1.clearResponseBuffer();
        }

        delay(100);

        // Hiển thị tần số lên LCD
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Tan so: ");
        lcd.print(String(registerAddress[iOUTPUT_FREQUENCY] * 0.1, 1));
        lcd.print(" Hz");
        lcd.setCursor(0, 1);
        lcd.print("Dong tai: ");
        lcd.print(String(registerAddress[iCURRENT_IN_THE_MOTOR] * 0.1, 1));
        lcd.print(" A");
        delay(5);

        break;
      }

    
    case RUN_TO_SETUP:
    {
      // Cho phép ngắt ngoài
      attachInterrupt(0, ISR_readVolumeEncoder, CHANGE);

      deviceMode = SETUP;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("----SETUP--->");

      delay(3000);

      break;
    }
  }
}
//===================================================
// FUNTIONS
//===================================================

//---------------------------------------------------
// Điều khiển biến tần
void khoiDongDongCo(void) {
  mbm1.setTransmitBuffer(0, KHOI_DONG);
  mbm1.writeMultipleRegisters(CONTROL_WORD, 1);
  return;
}

void dungDongCo(void) {
  mbm1.setTransmitBuffer(0, DUNG);
  mbm1.writeMultipleRegisters(CONTROL_WORD, 1);
  return;
}

void quayThuan(void) {
  mbm1.setTransmitBuffer(0, QUAY_THUAN);
  mbm1.writeMultipleRegisters(CONTROL_WORD, 1);
  return;
}

void quayNghich(void) {
  mbm1.setTransmitBuffer(0, QUAY_NGHICH);
  mbm1.writeMultipleRegisters(CONTROL_WORD, 1);
  return;
}

void datTanSo(int l_nTanSo) {
  mbm1.setTransmitBuffer(0, 10 * l_nTanSo);
  mbm1.writeMultipleRegisters(FREQUENCY_REFERENCE, 1);
  return;
}

//---------------------------------------------------
// Đọc Rotary Encoder
void ISR_readVolumeEncoder(void) {
  volatile static bool s_bEnable = true;
  volatile static bool s_bStartStatus = false, s_bCurrentStatus = false;

  if (s_bEnable) {
    s_bStartStatus = digitalRead(PHA1_PIN);
    s_bEnable = false;
  }

  s_bCurrentStatus = digitalRead(PHA1_PIN);
  if (s_bCurrentStatus != s_bStartStatus) {
    if (digitalRead(PHA2_PIN) != s_bCurrentStatus) g_nIDDevice++;
    else g_nIDDevice--;

    if (g_nIDDevice > 247) g_nIDDevice = 247;
    if (g_nIDDevice < 1) g_nIDDevice = 1;

    s_bEnable = true;
  }
}
