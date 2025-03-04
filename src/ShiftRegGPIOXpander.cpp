/**
 * @file ShiftRegGPIOXpander.cpp
 * @brief Code file for the ShiftRegGPIOXtender_ESP32 library 
 * 
 * @author Gabriel D. Goldman
 * 
 * @version 1.0.0
 * 
 * @date First release: 12/02/2025 
 *       Last update:   27/02/2025 12:40 (GMT+0200)
 * 
 * @copyright Copyright (c) 2025  GPL-3.0 license
 *******************************************************************************
  * @attention	This library was developed as part of the refactoring process for
  * an industrial machines security enforcement and productivity control
  * (hardware & firmware update). As such every class included complies **AT LEAST**
  * with the provision of the attributes and methods to make the hardware & firmware
  * replacement transparent to the controlled machines. Generic use attribute and
  * methods were added to extend the usability to other projects and application
  * environments, but no fitness nor completeness of those are given but for the
  * intended refactoring project.
  * 
  * @warning **Use of this library is under your own responsibility**
  * 
  * @warning The use of this library falls in the category describe by The Alan 
  * Parsons Project (c) 1980 Games People play:
  * Games people play, you take it or you leave it
  * Things that they say aren't alright
  * If I promised you the moon and the stars, would you believe it?
  * Games people play in the middle of the night
 *******************************************************************************
 */
#include <Arduino.h>
#include <ShiftRegGPIOXpander.h>

ShiftRegGPIOXpander::ShiftRegGPIOXpander()
{
}

ShiftRegGPIOXpander::ShiftRegGPIOXpander(uint8_t ds, uint8_t sh_cp, uint8_t st_cp, uint8_t srQty, uint8_t* initCntnt)
:_ds{ds}, _sh_cp{sh_cp}, _st_cp{st_cp}, _srQty{srQty}, _srArryBuffPtr {new uint8_t [srQty]}
{
   digitalWrite(_sh_cp, HIGH);
   digitalWrite(_ds, LOW);
   digitalWrite(_st_cp, HIGH);
   pinMode(_sh_cp, OUTPUT);
   pinMode(_ds, OUTPUT);
   pinMode(_st_cp, OUTPUT);

   _maxSrPin = (_srQty * 8) - 1;

   //-------------------->> Section that migh be replaced by a digitalWritteAllReset BEGIN
   if(initCntnt != nullptr){
      memcpy(_srArryBuffPtr, initCntnt, _srQty);   // destPtr, srcPtr, size
   }
   else{
      for(int i{0}; i < _srQty; i++){ 
         *(_srArryBuffPtr + i) = 0x00;
      }   
   }
   sendAllSRCntnt();
   //---------------------->> Section that migh be replaced by a digitalWritteAllReset END
}

ShiftRegGPIOXpander::~ShiftRegGPIOXpander(){
   if(_auxArryBuffPtr !=nullptr){
      delete [] _auxArryBuffPtr;
      _auxArryBuffPtr = nullptr;
   }
   if(_srArryBuffPtr !=nullptr){
      delete [] _srArryBuffPtr;
      _srArryBuffPtr = nullptr;
   }
}

bool ShiftRegGPIOXpander::copyMainToAux(bool overWriteIfExists){
   bool result {false};
   
   if((_auxArryBuffPtr == nullptr) || overWriteIfExists){
      if(_auxArryBuffPtr == nullptr){
         _auxArryBuffPtr = new uint8_t [_srQty];
      }
      memcpy(_auxArryBuffPtr, _srArryBuffPtr, _srQty);   // destPtr, srcPtr, size
      result = true;
   }

   return result;
}

uint8_t ShiftRegGPIOXpander::digitalReadSr(const uint8_t &srPin){
   uint8_t result{0xFF};

   if(srPin <= _maxSrPin){
      if(_auxArryBuffPtr != nullptr){
         moveAuxToMain(true);
      }   

      result = (*(_srArryBuffPtr + (srPin / 8)) >> (srPin % 8)) & 0x01;
   }

   return result;
}

void ShiftRegGPIOXpander::digitalWriteSr(const uint8_t srPin, const uint8_t value){
   if(srPin <= _maxSrPin){
      if(_auxArryBuffPtr != nullptr)
         moveAuxToMain(false);
      if(value)
         *(_srArryBuffPtr + (srPin / 8)) |= (0x01 << (srPin % 8));
      else
         *(_srArryBuffPtr + (srPin / 8)) &= ~(0x01 << (srPin % 8));
   }
   sendAllSRCntnt();

   return;
}

void ShiftRegGPIOXpander::digitalWriteSrAllReset(){
   if(_auxArryBuffPtr != nullptr)
      discardAux();
   for(uint8_t i{0}; i < _srQty; i++)
      *(_srArryBuffPtr + i) = 0x00;
   sendAllSRCntnt();

   return;
}

void ShiftRegGPIOXpander::digitalWriteSrAllSet(){
   if(_auxArryBuffPtr != nullptr)
      discardAux();
   for(uint8_t i{0}; i < _srQty; i++)
      *(_srArryBuffPtr + i) = (uint8_t)0xFF;
   sendAllSRCntnt();

   return;
}

void ShiftRegGPIOXpander::digitalWriteSrToAux(const uint8_t srPin, const uint8_t value){
   if(srPin <= _maxSrPin){
      if(_auxArryBuffPtr == nullptr){
         copyMainToAux();
      }
      if(value)
         *(_auxArryBuffPtr + (srPin / 8)) |= (0x01 << (srPin % 8));
      else
         *(_auxArryBuffPtr + (srPin / 8)) &= ~(0x01 << (srPin % 8));
   }

   return;
}

void ShiftRegGPIOXpander::discardAux(){
   if(_auxArryBuffPtr != nullptr){
      delete [] _auxArryBuffPtr;
      _auxArryBuffPtr = nullptr;
   }

   return;
}

uint8_t* ShiftRegGPIOXpander::getMainBuffPtr(){

   return _srArryBuffPtr;
}

uint8_t ShiftRegGPIOXpander::getMaxPin(){

   return _maxSrPin;
}

uint8_t ShiftRegGPIOXpander::getSrQty(){

   return _srQty;
}

bool ShiftRegGPIOXpander::moveAuxToMain(bool flushASAP){
   bool result {false};

   if(_auxArryBuffPtr != nullptr){
      memcpy( _srArryBuffPtr, _auxArryBuffPtr, _srQty);   // destPtr, srcPtr, size
      discardAux();
      if(flushASAP)
         sendAllSRCntnt();
   }

   return result;
}

bool ShiftRegGPIOXpander::sendAllSRCntnt(){
   uint8_t curSRcntnt{0};
   bool result{true};
   portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

	taskENTER_CRITICAL(&mux);
   if((_srQty > 0) && (_srArryBuffPtr != nullptr)){
      digitalWrite(_st_cp, LOW); // Start of access to the shift register internal buffer to write -> Lower the latch pin

      for(int srBuffDsplcPtr{_srQty - 1}; srBuffDsplcPtr >= 0; srBuffDsplcPtr--){
         curSRcntnt = *(_srArryBuffPtr + srBuffDsplcPtr);
         result = _sendSnglSRCntnt(curSRcntnt);
      }
      digitalWrite(_st_cp, HIGH);   // End of access to the shift register internal buffer, copy the buffer values to the output pins -> Lower the latch pin
   }
   else{
      result = false;
   }      
	taskEXIT_CRITICAL(&mux);

   return result;
}

bool ShiftRegGPIOXpander::_sendSnglSRCntnt(uint8_t data){
   bool result{true};

   for (int bitPos {7}; bitPos >= 0; bitPos--){   //Send each of the bits correspondig to one 8-bits shift register module
      digitalWrite(_sh_cp, LOW); // Start of next bit value addition to the shift register internal buffer -> Lower the clock pin         
      digitalWrite(_ds, (data & 0x80)?HIGH:LOW);   // Set the value of the next bit value
      data <<= 1;
      delayMicroseconds(10);  // Time required by the 74HCx595 to modify the SH_CP line by datasheet
      digitalWrite(_sh_cp, HIGH);   // End of next bit value addition to the shift register internal buffer -> Lower the clock pin
   }

   return result;
}
bool ShiftRegGPIOXpander::stampOverMain(uint8_t* newCntntPtr){
   bool result {false};

   if ((newCntntPtr != nullptr) && (newCntntPtr != NULL)){
      if(_auxArryBuffPtr != nullptr){
         discardAux();
      }
      memcpy(_srArryBuffPtr, newCntntPtr, _srQty);
      sendAllSRCntnt();
      result = true;
   }
   
   return result;
}

