/*
 [TridentTD] : MANABU's Esp8266 IoT Library
 www.facebook.com/miniNodeMCU
 
 TridentTD_7Segs74HC595.cpp - LED Display 7 segments (74hc595) for ESP8266 & Arduino

 Version 1.0  25/05/2560 Buddism Era  (2017)
 
 Copyright (C) 2017   Ven.Phaisarn Techajaruwong


    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see http://www.gnu.org/licenses.
*/

#include <Arduino.h>
#include "TridentTD_7Segs74HC595.h"

TridentTD_7Segs74HC595::TridentTD_7Segs74HC595(int SCLK, int RCLK, int DIO){
  TridentTD_7Segs74HC595( SCLK, RCLK, DIO, 4);
}

TridentTD_7Segs74HC595::TridentTD_7Segs74HC595(int SCLK, int RCLK, int DIO, int max_digits) {
  _7segments.sclk       = SCLK;
  _7segments.rclk       = RCLK;
  _7segments.dio        = DIO;
  _7segments.max_digits = (max_digits==4 || max_digits ==8)? max_digits : 4;

  pinMode(_7segments.sclk,OUTPUT);
  pinMode(_7segments.rclk,OUTPUT);
  pinMode(_7segments.dio, OUTPUT);
}

void TridentTD_7Segs74HC595::init(){
  digitalWrite(_7segments.rclk, HIGH);

  #ifdef ESP_H
    timer1_isr_init();
    timer1_attachInterrupt([](){
  #else
    _isr_init(2000);
    _isr_add([]() {
  #endif
      digitalWrite(_7segments.rclk, LOW);
      shiftOut(_7segments.dio, _7segments.sclk, LSBFIRST, _7segments.columns[_7segments.col_index]);
      shiftOut(_7segments.dio, _7segments.sclk, LSBFIRST, 0x80>> _7segments.col_index);  //pow(2, 7 - col_index ));
      digitalWrite(_7segments.rclk, HIGH);
      _7segments.col_index = (_7segments.col_index >= _7segments.max_digits-1 ? 0 : _7segments.col_index+1);
    });
  #ifdef ESP_H
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);  //5MHz (5 ticks/us - 1677721.4 us max)
    timer1_write((clockCyclesPerMicrosecond() / 16) * 200);
  #endif

}

#ifndef ESP_H
void    (*isrCallback)();

ISR(TIMER1_OVF_vect)          // interrupt service routine that wraps a user defined function supplied by attachInterrupt
{
  isrCallback();
}

void TridentTD_7Segs74HC595::_isr_init(long microseconds)
{
  TCCR1A = 0;                 // clear control register A 
  TCCR1B = _BV(WGM13);        // set mode 8: phase and frequency correct pwm, stop the timer
  _setPR(microseconds);
}

void TridentTD_7Segs74HC595::_setPR(long microseconds)   // AR modified for atomic access
{
  
  long cycles = (F_CPU / 2000000) * microseconds;                                // the counter runs backwards after TOP, interrupt is at BOTTOM so divide microseconds by 2
  if(cycles < RESOLUTION)              clockSelectBits = _BV(CS10);              // no prescale, full xtal
  else if((cycles >>= 3) < RESOLUTION) clockSelectBits = _BV(CS11);              // prescale by /8
  else if((cycles >>= 3) < RESOLUTION) clockSelectBits = _BV(CS11) | _BV(CS10);  // prescale by /64
  else if((cycles >>= 2) < RESOLUTION) clockSelectBits = _BV(CS12);              // prescale by /256
  else if((cycles >>= 2) < RESOLUTION) clockSelectBits = _BV(CS12) | _BV(CS10);  // prescale by /1024
  else        cycles = RESOLUTION - 1, clockSelectBits = _BV(CS12) | _BV(CS10);  // request was out of bounds, set as maximum
  
  char oldSREG;
  oldSREG = SREG;       
  cli();              // Disable interrupts for 16 bit register access
  ICR1 = cycles;                                          // ICR1 is TOP in p & f correct pwm mode
  SREG = oldSREG;
  
  TCCR1B &= ~(_BV(CS10) | _BV(CS11) | _BV(CS12));
  TCCR1B |= clockSelectBits;                                          // reset clock select register, and starts the clock
}

void TridentTD_7Segs74HC595::_isr_add(void (*isr)(), long microseconds)
{
  if(microseconds > 0) _setPR(microseconds);
  isrCallback = isr;                                       // register the user's callback with the real ISR
  TIMSK1 = _BV(TOIE1);                                     // sets the timer overflow interrupt enable bit
  // might be running with interrupts disabled (eg inside an ISR), so don't touch the global state
//  sei();
  TCCR1B |= clockSelectBits;//resume();                       
}
#endif

void TridentTD_7Segs74HC595::_setColumn(int col, int character, boolean addDot) {
  int idx;
  if((character >= 0)&&(character <= 9)){ idx = character; } 
  else if((character >= 32)&&(character <= 47)){ idx = (character ==45)? 62 : 63; }
  else if(character == 63) { idx = 64; }
  else if((character >= '0')&&(character <= '9')) { idx = character - 48; } 
  else if((character >= 'A')&&(character <= 'Z')) { idx = character - 55; } 
  else if((character >= 'a')&&(character <= 'z')) { idx = character - 61; }
  _7segments.columns[col] = _7segments.charmap[idx] & ((addDot)? 254: 255);
}


void TridentTD_7Segs74HC595::setNumber(float f_number, int decimal){
  decimal = (decimal > _7segments.max_digits - 1)? _7segments.max_digits-1 : decimal;
  bool bZero = true;
  int number =  ((int)((f_number+0.0001) * pow(10,decimal))) % (int)pow(10,_7segments.max_digits);
  
  int value;
  for(int col= _7segments.max_digits-1; col >= 0 ; col--){
    value = ((int)(number / pow(10, col )))%10;
    if(value != 0) { bZero = false; }
    _setColumn(col, (col>decimal&& bZero)? ' ':value, (col!=decimal || decimal ==0)? false:true );
    delay(1);
  }
}

void TridentTD_7Segs74HC595::setText(String text){
  int len = text.length();
  const char *text_c = text.c_str();


  int chr_index=0;
  for(int col=_7segments.max_digits -1; col >=0  ; col--){
    int character = (chr_index < len)? text_c[chr_index]:' ';

    boolean addDot= false;
    if(text[chr_index+1] == '.'){
      addDot = true; chr_index++;
    }
    _setColumn( col, character, addDot);
    chr_index++;
    delay(1);
  }
}

void TridentTD_7Segs74HC595::setTextScroll(String text, int scrolltime ,int nLoop ){
  nLoop = (nLoop<=0)? 1 : nLoop;
  
  for(int j = 0; j < nLoop; j++){
    String text2 = String("    ")+ text;
    int len      = text2.length();

    for(int i = 0; i < len; i++) {
      String subtext = text2.substring(0,8);
      setText(subtext);
      delay(scrolltime);

      if(text2.substring(1,2) != "."){
        text2 = text2.substring(1)+text2.substring(0,1);
      }else{
        text2 = text2.substring(2)+text2.substring(0,2);
      }
    }
    
    setText("    ");
    delay(scrolltime);
  }
}

String TridentTD_7Segs74HC595::getVersion(){
  return (String)("[TridentTD_7Segs74HC595] Version ") + String(_version);
}

