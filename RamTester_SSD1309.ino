
#include <Arduino.h>
#include <U8g2lib.h>
#include <avr/sleep.h>
#include "FastCRC.h"

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

U8G2_SSD1309_128X64_NONAME0_F_4W_SW_SPI u8g2(U8G2_R0, /* clock=*/ 13, /* data=*/ 11, /* cs=*/ 10, /* dc=*/ 12, /* reset=*/ LED_BUILTIN);  

#define DI          15  // PC1
#define DO           8  // PB0
#define CAS          9  // PB1
#define RAS         17  // PC3
#define WE          16  // PC2

#define XA0         18  // PC4
#define XA1          2  // PD2
#define XA2         19  // PC5
#define XA3          6  // PD6
#define XA4          5  // PD5
#define XA5          4  // PD4
#define XA6          7  // PD7
#define XA7          3  // PD3
#define XA8         14  // PC0

#define BUS_SIZE     9

const uint8_t a_bus[BUS_SIZE] = { XA0, XA1, XA2, XA3, XA4, XA5, XA6, XA7, XA8 };

FastCRC16 CRC16;

const char MESSAGE00[] PROGMEM = "";
const char MESSAGE01[] PROGMEM = "DRAM 4256";
const char MESSAGE02[] PROGMEM = "DRAM 4164";
const char MESSAGE03[] PROGMEM = "DRAM 4532L";
const char MESSAGE04[] PROGMEM = "DRAM 4532H";
const char MESSAGE05[] PROGMEM = "Analyse de la m\xE9moire";
const char MESSAGE06[] PROGMEM = "en cours...";
const char MESSAGE07[] PROGMEM = "Aucune m\xE9moire";
const char MESSAGE08[] PROGMEM = "d\xE9tect\xE9\x65.";
const char MESSAGE09[] PROGMEM = "M\xE9moire de type";
const char MESSAGE10[] PROGMEM = "4256 d\xE9tect\xE9\x65.";
const char MESSAGE11[] PROGMEM = "4164 d\xE9tect\xE9\x65.";
const char MESSAGE12[] PROGMEM = "4532L d\xE9tect\xE9\x65.";
const char MESSAGE13[] PROGMEM = "4532H d\xE9tect\xE9\x65.";
const char MESSAGE14[] PROGMEM = "Erreur m\xE9moire";
const char MESSAGE15[] PROGMEM = "Appuyer sur Reset";
const char MESSAGE16[] PROGMEM = "pour lancer une";
const char MESSAGE17[] PROGMEM = "nouvelle analyse.";
const char MESSAGE18[] PROGMEM = "Erreur(s): ";
const char MESSAGE19[] PROGMEM = "Ecriture al\xE9toire";
const char MESSAGE20[] PROGMEM = "Total:";
const char MESSAGE21[] PROGMEM = "Lecture";
const char MESSAGE22[] PROGMEM = "Ecriture 0x5555..AAAA";
const char MESSAGE23[] PROGMEM = "Ecriture 0xAAAA..5555";
const char MESSAGE24[] PROGMEM = "Valide";
const char MESSAGE25[] PROGMEM = "D\xE9\x66\x65\x63tueux";
// Then set up a table to refer to your strings.
const char *const string_table[] PROGMEM = {MESSAGE00, MESSAGE01, MESSAGE02, MESSAGE03, MESSAGE04, MESSAGE05, MESSAGE06, 
           MESSAGE07, MESSAGE08, MESSAGE09, MESSAGE10, MESSAGE11, MESSAGE12, MESSAGE13, MESSAGE14, MESSAGE15, MESSAGE16,
           MESSAGE17, MESSAGE18, MESSAGE19, MESSAGE20, MESSAGE21, MESSAGE22, MESSAGE23, MESSAGE24, MESSAGE25};

void u8g2_message(uint8_t x, uint8_t y, uint8_t message) {
  char buffer[24];
  strcpy_P(buffer, (char *)pgm_read_word(&(string_table[message])));
  u8g2.drawStr(x, y, buffer);
}

void u8g2_prepare(void) {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setFontMode(1); // Police transparente
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(2);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
}

char progress_str[4] = "";

void u8g2_progressbar(float progress, uint8_t posy) {
  uint8_t posx;
  if (progress >= 0 && progress <= 100) { 
    u8g2.drawFrame(0, posy, 128, 9); // Contour de la ProgresBar
    u8g2.drawBox(1, posy+1, lround(progress*1.26), 7); // Remplissage de la ProgressBar
    //u8g2.setDrawColor(2); // XOR
    sprintf(progress_str, "%d%%", lround(progress));
    switch(strlen(progress_str)) {
      case 2: posx=58; break;
      case 3: posx=55; break;
      case 4: posx=52; break;
    }
    u8g2.drawStr(posx, posy, progress_str);  
    //u8g2.setDrawColor(1); // White
  }  
}

/* Function to get no of set bits in binary
representation of positive integer n */
unsigned short countSetBits(unsigned short n)
{
    unsigned short count = 0;
    while (n) {
        count += n & 1;
        n >>= 1;
    }
    return count;
}

void setBus(unsigned short a) {
  for (uint8_t i = 0; i < BUS_SIZE; i++) {
      digitalWrite(a_bus[i], bitRead(a, i));
  } 
}

void writePage16(unsigned short r, unsigned short c, unsigned short v) {
  // row
  setBus(r);
  digitalWrite(RAS, LOW);
  for (uint8_t i = 0; i < 16; i++) {
    digitalWrite(WE, LOW);
    // value
    digitalWrite(DI, bitRead(v, i));
    // col
    setBus(c + i);
    digitalWrite(CAS, LOW);
    digitalWrite(WE, HIGH);
    digitalWrite(CAS, HIGH);
  }
  digitalWrite(RAS, HIGH);
}

unsigned int readPage16(unsigned short r, unsigned short c) {
  unsigned short ret = 0;
  // row
  setBus(r);
  digitalWrite(RAS, LOW);
  for (uint8_t i = 0; i < 16; i++) {
    // col
    setBus(c + i);
    digitalWrite(CAS, LOW);
    // read
    bitWrite(ret, i, digitalRead(DO));
    digitalWrite(CAS, HIGH);
  }
  digitalWrite(RAS, HIGH);
  return ret;
}

uint8_t memory_detect() {
  writePage16(0x000, 0x000, 0x5555);
  writePage16(0x100, 0x000, 0xAAAA);
  switch(readPage16(0x000, 0x000)) {
    case 0x0000: return 0; break;
    case 0x5555: return 1; break;
    case 0xAAAA: return 2; break;
    default: return 9; break; 
  }
}

uint8_t memory_type;
String memory_string;

void setup() {
  Serial.begin(9600);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

  // Configuration des Pin Arduino
  for (uint8_t i = 0; i < BUS_SIZE; i++)
    pinMode(a_bus[i], OUTPUT);
  pinMode(CAS, OUTPUT);
  pinMode(RAS, OUTPUT);
  pinMode(WE, OUTPUT);
  pinMode(DI, OUTPUT);
  pinMode(DO, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(WE, HIGH);
  digitalWrite(RAS, HIGH);
  digitalWrite(CAS, HIGH);

  // Initialisation de l'écran
  u8g2.begin();
  u8g2_prepare();
  u8g2.clearBuffer();
  u8g2_message(1, 16, 5); // Analyse de la memoire
  u8g2_message(31, 25, 6); // en cours...
  u8g2.sendBuffer();

  memory_type = memory_detect();
  //Serial.print("Memory Type: ");
  //Serial.println(memory_type);

  delay(1000);
  u8g2.clearBuffer();
  switch(memory_type) {
    case 0: u8g2_message(22, 10, 7); u8g2_message(37, 19, 8); break; // Aucune mémoire détectée.
    case 1: u8g2_message(19, 10, 9); u8g2_message(22, 19, 10); break; // Mémoire de type 4256 détectée.
    case 2: u8g2_message(19, 10, 9); u8g2_message(22, 19, 11); break; // Mémoire de type 4164 détectée.
    case 3: u8g2_message(19, 10, 9); u8g2_message(19, 19, 12); break; // Mémoire de type 4532L détectée.
    case 4: u8g2_message(19, 10, 9); u8g2_message(19, 19, 13); break; // Mémoire de type 4532H détectée.
    case 9: u8g2_message(19, 10, 14); break; // Erreur mémoire
  }

  if (memory_type==0 || memory_type==9) {
    u8g2_message(13, 37, 15); // Appuyer sur Reset
    u8g2_message(19, 46, 16); // pour lancer une
    u8g2_message(13, 55, 17); // nouvelle analyse.
    u8g2.sendBuffer();
    sleep_mode();
  }

  u8g2.sendBuffer();
  delay(2000);

}

void loop() {
  unsigned short row, row_min, row_max, col, col_max, value;
  unsigned short error = 0;
  char error_str[5];
  char hexrowcol[4];
  //float row_float;

  switch(memory_type) {
    case 1: row_min = 0; row_max = 511; col_max = 31; break;
    case 2: row_min = 0; row_max = 255; col_max = 15; break;
    case 3: row_min = 0; row_max = 127; col_max = 15; break;
    case 4: row_min = 128; row_max = 255; col_max = 15; break;
  }

  // Ecriture CRC (pseudo random)
  Serial.println(F("Write Pseudo Random"));
  for (row = row_min; row <= row_max; row++) {
    if (row % 2 == 0 ) {
      // On ne met a jour la ProgressBar que une row sur deux
      u8g2.clearBuffer();
      u8g2_message(34, 10, memory_type);
      u8g2.drawFrame(32, 9, 63, 11);
      sprintf(error_str, "%d", error);
      u8g2_message(0, 23, 18); //Erreur(s):
      u8g2.drawStr(66, 23, error_str);
      u8g2_message(0, 32, 19); // Ecriture aleatoire
      u8g2_progressbar(row * 100 / row_max , 42);
      u8g2_progressbar(row * 16.667 / row_max, 55);
      u8g2_message(10, 55, 20); // Total:
      u8g2.sendBuffer();
    }
    for (col = 0; col <= col_max; col++) {
      sprintf(hexrowcol, "%02X%02X", row, col);
      writePage16(row, col << 4, CRC16.ccitt(hexrowcol, 4));
      //writePage16(row, col << 4, row << 5 + col);
    }
  }  
  
  // Lecture CRC
  Serial.println(F("Read Pseudo Random"));
  for (row = row_min; row <= row_max; row++) {
    if (row % 2 == 0 ) {
      // On ne met a jour la ProgressBar que une row sur deux
      u8g2.clearBuffer();
      u8g2_message(34, 10, memory_type);
      u8g2.drawFrame(32, 9, 63, 11);
      sprintf(error_str, "%d", error);
      u8g2_message(0, 23, 18); // Erreur(s):
      u8g2.drawStr(66, 23, error_str);
      u8g2_message(0, 32, 21); // Lecture
      u8g2_progressbar(row * 100 / row_max, 42);
      u8g2_progressbar(row * 16.667 / row_max + 16.66, 55);
      u8g2_message(10, 55, 20); // Total:
      u8g2.sendBuffer();
    }
    for (col = 0; col <= col_max; col++) {
      sprintf(hexrowcol, "%02X%02X", row, col);
      if (readPage16(row, col << 4) != CRC16.ccitt(hexrowcol, 4)) {
      //if (readPage16(row, col << 4) != row << 5 + col) {
        Serial.print(F("Error Address: "));
        Serial.print(row, HEX);
        Serial.print(F(","));
        Serial.println(col << 4, HEX);
        error++;
      }
    }
  }

  // Ecriture bits croisés
  Serial.println(F("Write 0x5555..AAAA"));
  for (row = row_min; row <= row_max; row++) {
    if (row % 2 == 0 ) {
      value = 0x5555;
      // On ne met a jour la ProgressBar que une row sur deux
      u8g2.clearBuffer();
      u8g2_message(34, 10, memory_type);
      u8g2.drawFrame(32, 9, 63, 11);
      sprintf(error_str, "%d", error);
      u8g2_message(0, 23, 18); // Erreur(s):
      u8g2.drawStr(66, 23, error_str);
      u8g2_message(0, 32, 22); // Ecriture 0x5555..AAAA
      u8g2_progressbar(row * 100 / row_max, 42);
      u8g2_progressbar(row * 16.667 / row_max + 33.33, 55);
      u8g2_message(10, 55, 20); // Total:
      u8g2.sendBuffer();      
    }
    else {
      value = 0xAAAA;
    }
    for (col = 0; col <= col_max; col++) {
      writePage16(row, col << 4, value);
    }
  }  
    
  // Lecture bits croisés
  Serial.println(F("Read 0x5555..AAAA"));
  for (row = row_min; row <= row_max; row++) {
    if (row % 2 == 0 ) {
      value = 0x5555;
      // On ne met a jour la ProgressBar que une row sur deux
      u8g2.clearBuffer();
      u8g2_message(34, 10, memory_type);
      u8g2.drawFrame(32, 9, 63, 11);
      sprintf(error_str, "%d", error);
      u8g2_message(0, 23, 18); // Erreur(s):
      u8g2.drawStr(66, 23, error_str);
      u8g2_message(0, 32, 21); // Lecture
      u8g2_progressbar(row * 100 / row_max, 42);
      u8g2_progressbar(row * 16.667 / row_max + 50, 55);
      u8g2_message(10, 55, 20); // Total:
      u8g2.sendBuffer();
    }
    else {
      value = 0xAAAA;
    }
    for (col = 0; col <= col_max; col++) {
      if (countSetBits(readPage16(row, col << 4) ^ value) != 0) {
        Serial.print(F("Error Address: "));
        Serial.print(row, HEX);
        Serial.print(F(","));
        Serial.println(col << 4, HEX);
        error += countSetBits(readPage16(row, col << 4) ^ value);
      }
    }
  }

  // Ecriture
  Serial.println(F("Write 0xAAAA..5555"));
  for (row = row_min; row <= row_max; row++) {
    if (row % 2 == 0 ) {
      value = 0xAAAA;
      // On ne met a jour la ProgressBar que une row sur deux
      u8g2.clearBuffer();
      u8g2_message(34, 10, memory_type);
      u8g2.drawFrame(32, 9, 63, 11);
      sprintf(error_str, "%d", error);
      u8g2_message(0, 23, 18); // Erreur(s):
      u8g2.drawStr(66, 23, error_str);
      u8g2_message(0, 32, 23); // Ecriture 0xAAAA..5555
      u8g2_progressbar(row * 100 / row_max, 42);
      u8g2_progressbar(row * 16.667 / row_max + 66.66, 55);
      u8g2_message(10, 55, 20); // Total:
      u8g2.sendBuffer();      
      }
    else {
      value = 0x5555;
      }
    for (col = 0; col <= col_max; col++) {
      writePage16(row, col << 4, value);
      }
    }  
    
  // Lecture
  Serial.println(F("Read 0xAAAA..5555"));
  for (row = row_min; row <= row_max; row++) {
    if (row % 2 == 0 ) {
      value = 0xAAAA;
      // On ne met a jour la ProgressBar que une row sur deux
      u8g2.clearBuffer();
      u8g2_message(34, 10, memory_type);
      u8g2.drawFrame(32, 9, 63, 11);
      sprintf(error_str, "%d", error);
      u8g2_message(0, 23, 18); // Erreur(s):
      u8g2.drawStr(66, 23, error_str);
      u8g2_message(0, 32, 21); // Lecture
      u8g2_progressbar(row * 100 / row_max, 42);
      u8g2_progressbar(row * 16.667 / row_max + 83.33, 55);
      u8g2_message(10, 55, 20); // Total:
      u8g2.sendBuffer();      
      }
    else {
      value = 0x5555;
      }
    for (col = 0; col <= col_max; col++) {
      if (countSetBits(readPage16(row, col << 4) ^ value) != 0) {
        Serial.print(F("Error Address: "));
        Serial.print(row, HEX);
        Serial.print(F(","));
        Serial.println(col << 4, HEX);
        error += countSetBits(readPage16(row, col << 4) ^ value);
        }
      }
    }

  u8g2.clearBuffer();
  u8g2_message(34, 10, memory_type);
  u8g2.drawFrame(32, 9, 63, 11);
  sprintf(error_str, "%d", error);
  u8g2_message(0, 23, 18); // Erreur(s):
  u8g2.drawStr(66, 23, error_str);
  u8g2.setFont(u8g2_font_t0_22b_tf);
  if (error == 0) {
    u8g2_message(0, 40, 24); // Valide
  }
  else {
    u8g2_message(0, 40, 25); // Defectueux
  }
  u8g2.sendBuffer();  

  Serial.print(F("Total Error: "));
  Serial.println(error);
  delay(1000);
  sleep_mode();
}
