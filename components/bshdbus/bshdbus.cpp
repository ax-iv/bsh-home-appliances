/*

   B/S/H/ D-Bus frame parser

   (C) 2024 Hajo Noerenberg

   http://www.noerenberg.de/
   https://github.com/hn/bsh-home-appliances

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3.0 as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program. If not, see <http://www.gnu.org/licenses/gpl-3.0.txt>.

*/

#include "bshdbus.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cinttypes>

#define BSHDBUS_MIN_FRAME_LENGTH 6
#define BSHDBUS_MAX_FRAME_LENGTH 32
#define BSHDBUS_READ_TIMEOUT 50
#define BSHDBUS_BUFFER_SIZE 1024

namespace esphome {
namespace bshdbus {

static const char *const TAG = "bshdbus";

void BSHDBus::dump_config() {
  ESP_LOGCONFIG(TAG, "BSHDBus:");
  check_uart_settings(9600);
}
uint8_t BSHDBus::digitToNumber(uint8_t value){
   /* #77	0   #60	1   #1e	2   #7c	3   #69	4   
      #5d	5   #5f	6   #64	7   #7f	8   #7d	9*/
   switch(value){
      case 0x77: return 0;
      case 0x60: return 1;
      case 0x1e: return 2;
      case 0x7c: return 3;
      case 0x69: return 4;
      case 0x5d: return 5;
      case 0x5f: return 6;
      case 0x64: return 7;
      case 0x7f: return 8;
      case 0x7d: return 9;
      default: return 0;
   }
}

void BSHDBus::loop() {
  if (!available())
    return;

  const uint32_t now = millis();

  if (now - this->lastread_ > BSHDBUS_READ_TIMEOUT) {
    this->buffer_.clear();
    this->lastread_ = now;
  }

  while (available() && (this->buffer_.size() < BSHDBUS_BUFFER_SIZE)) {
    uint8_t c;
    read_byte(&c);
    this->buffer_.push_back(c);
  }

  this->lastread_ = now;
  size_t lastvalid = 0;

  for (size_t p = 0; p < this->buffer_.size(); ) {
    const uint8_t* framedata = this->buffer_.data() + p;
    const uint16_t framelen = 4 + framedata[0];

    if ((framelen < BSHDBUS_MIN_FRAME_LENGTH) ||
            (framelen > BSHDBUS_MAX_FRAME_LENGTH) ||
            (framelen > (this->buffer_.size() - p)) ||
            crc16be(framedata, framelen, 0x0, 0x1021, false, false)) {
      ESP_LOGV(TAG, "Ignoring byte at %d with value 0x%02X", p, framedata[0]);
      p++;
      continue;
    }

    p += framelen;
    if ((p < this->buffer_.size()) &&
            ((framedata[framelen] == ((framedata[1] & 0xF0) | 0x0A)) ||
            ((framedata[framelen] == 0x1A) && (framedata[1] == 0x0F)))) {
      ESP_LOGV(TAG, "Skipping ack byte at %d with value 0x%02X", p, framedata[framelen]);
      p++;
    }

    lastvalid = p;
    const uint8_t dest = framedata[1];
    const uint16_t command = (framedata[2] << 8) + framedata[3];
    std::vector<uint8_t> message(framedata + 4, framedata + framelen - 2);    
     std::vector<uint8_t> TxMes;
     
     ESP_LOGD(TAG, "Valid frame dest 0x%02X cmd 0x%04X: 0x%s", dest, command,
               format_hex(message).c_str());
     /* POWER LED STATE */
        powerLedStateLast = powerLedStateNow;
        powerLedStateNow = message[6];
        dest = 0x15;
        command = 0x1100;
        TxMes.clear();   
        if((powerLedStateNow & powerLedStateLast) == 0x00 )
           TxMes.push_back(0x0);
        if((powerLedStateNow & powerLedStateLast) == 0x10 )
           TxMes.push_back(0x1);
        if((powerLedStateNow & powerLedStateLast) == 0x20 )
           TxMes.push_back(0x2);
        for (auto &listener : this->listeners_)
          listener->on_message(dest, command, TxMes);
     /* END POWER LED STATE */
     /* remaining time */
        dest = 0x2a;
        command = 0x1600;
        TxMes.clear();   
        uint16_t time = digitToNumber(message[5]);
        time += digitToNumber(message[4])*10;
        time += digitToNumber(message[3])*60;
        TxMes.push_back(time&0xFF);
        TxMes.push_back(time&0xFF00>>8);
        for (auto &listener : this->listeners_)
          listener->on_message(dest, command, TxMes);
     /* end remaining time */
        
     
       //for (auto &listener : this->listeners_)
       //  listener->on_message(dest, command, message);
    
  }

  if (lastvalid) {
    /* Remove all valid frames and (possibly) preceeding trash from buffer */
    this->buffer_.erase(this->buffer_.begin(), this->buffer_.begin() + lastvalid);
  } else if (this->buffer_.size() > BSHDBUS_MAX_FRAME_LENGTH) {
    /* Remove a first batch of trash from buffer if no valid frames are found */
    this->buffer_.erase(this->buffer_.begin(), this->buffer_.end() - BSHDBUS_MAX_FRAME_LENGTH);
  }
}

void BSHDBusListener::on_message(uint8_t dest, uint16_t command, std::vector<uint8_t> &message) {
  if ((this->command_ != 0xffff) && (this->command_ != command))
    return;
  if ((this->dest_ != 0xff) && (this->dest_ != dest))
    return;
  this->handle_message(message);
}

}  // namespace bshdbus
}  // namespace esphome
