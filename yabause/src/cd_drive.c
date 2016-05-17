/*  Copyright 2014-2016 James Laird-Wah
    Copyright 2004-2006, 2013 Theo Berkau

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*! \file cd_drive.c
    \brief CD drive onboard microcontroller HLE
*/

#include "core.h"
#include "cd_drive.h"
#include "sh7034.h"
#include "assert.h"
#include "memory.h"
#include "debug.h"
#include "cs2.h"
#include <stdarg.h>
#include "tsunami/yab_tsunami.h"

//oe rising edge to falling edge
//26 usec
#define TIME_OE 26

//serial clock rising edge of the final bit in a transmission to falling edge of start signal
//13992 usec
#define TIME_PERIODIC 13992

//start  falling edge to rising edge
//187 usec
#define TIME_START 187

//start falling edge to rising edge (slow just after power on)
//3203 usec

//poweron stable signal to first start falling edge (reset time)
//451448 usec
#define TIME_POWER_ON 451448

//time from first start falling edge to first transmission
#define TIME_WAITING 416509

//from falling edge to rising edge of serial clock signal for 1 byte
#define TIME_BYTE 150

//"when disc is reading transactions are ~6600us apart"
#define TIME_READING 6600

struct CdDriveContext cdd_cxt;

enum CdStatusOperations
{
   ReadToc = 0x04,
   Idle = 0x46,
   Stopped = 0x12,
   Seeking = 0x22,
   LidOpen = 0x80,
   NoDisc = 0x83,
   ReadingDataSectors = 0x36,
   ReadingAudioData = 0x34,
   Unknown = 0x30,
   SeekSecurityRing1 = 0xB2,
   SeekSecurityRing2 = 0xB6
};


void make_status_data(struct CdState *state, u8* data);
void set_checksum(u8 * data);

enum CommunicationState
{
   NoTransfer,
   Reset,
   Started,
   SendingFirstByte,
   ByteFinished,
   FirstByteFinished,
   SendingByte,
   SendingByteFinished,
   Running,
   NewTransfer,
   WaitToOe,
   WaitToOeFirstByte,
   WaitToRxio
}comm_state = NoTransfer;

u8 cd_drive_get_serial_bit()
{
   u8 bit = 1 << (7 - cdd_cxt.bit_counter);
   return (cdd_cxt.state_data[cdd_cxt.byte_counter] & bit) != 0;
}

void cd_drive_set_serial_bit(u8 bit)
{
   cdd_cxt.received_data[cdd_cxt.byte_counter] |= bit << cdd_cxt.bit_counter;
   cdd_cxt.bit_counter++;

   if (cdd_cxt.bit_counter == 8)
   {
      tsunami_log_value("CMD", cdd_cxt.received_data[cdd_cxt.byte_counter], 8);

      cdd_cxt.byte_counter++;
      cdd_cxt.bit_counter = 0;

      sh1_set_output_enable_rising_edge();

      if (comm_state == SendingFirstByte)
         comm_state = WaitToOeFirstByte;
      else if (comm_state == SendingByte)
         comm_state = WaitToOe;

      if (cdd_cxt.byte_counter == 13)
         comm_state = WaitToRxio;
   }
}

void do_toc()
{
   int toc_entry;
   cdd_cxt.state_data[0] = cdd_cxt.state.current_operation = ReadToc;
   comm_state = NoTransfer;
   //fill cdd_cxt.state_data with toc info

   toc_entry = cdd_cxt.toc_entry++;
   memcpy(cdd_cxt.state_data+1, &cdd_cxt.toc[toc_entry], 10);

   set_checksum(cdd_cxt.state_data);

   if (cdd_cxt.toc_entry > cdd_cxt.num_toc_entries)
   {
      cdd_cxt.state.current_operation = Idle;
      make_status_data(&cdd_cxt.state, cdd_cxt.state_data);
   }
}

int continue_command()
{
   if (cdd_cxt.state.current_operation == ReadToc)
   {
      do_toc();
      return TIME_READING;
   }
   else
   {
      comm_state = NoTransfer;
      return TIME_PERIODIC;
   }
}

u32 get_fad_from_command(u8 * buf)
{
   u32 fad = buf[1];
   fad <<= 8;
   fad |= buf[2];
   fad <<= 8;
   fad |= buf[3];

   return fad;
}

s32 toc_10_get_track(s32 fad);
void state_set_msf_info(struct CdState *state, s32 track_fad, s32 disc_fad);

int do_command()
{
   int command = cdd_cxt.received_data[0];
   switch (command)
   {
   case 0x0:
      //nop
      return continue_command();
      break;
   case 0x2:
      //seeking ring
      cdd_cxt.state.current_operation = SeekSecurityRing2;
      break;
   case 0x3:
   {
      cdd_cxt.toc_entry = 0;
      cdd_cxt.num_toc_entries = Cs2Area->cdi->ReadTOC10(cdd_cxt.toc);
      do_toc();

      return TIME_READING;
      break;
   }
   case 0x4:
      //stop disc
      cdd_cxt.state.current_operation = Stopped;
      break;
   case 0x6:
      //read data at lba
      cdd_cxt.state.current_operation = ReadingDataSectors;//what about audio data?
      break;
   case 0x8:
      //pause
      cdd_cxt.state.current_operation = Idle;
      break;
   case 0x9:
      //seek
   {
      s32 fad = get_fad_from_command(cdd_cxt.received_data);
      s32 track_start_fad = 0;
      s32 track_fad = 0;
      cdd_cxt.disc_fad = fad;//minus 4?
      track_start_fad = toc_10_get_track(cdd_cxt.disc_fad);
      track_fad = cdd_cxt.disc_fad - track_fad;
      cdd_cxt.state.current_operation = Seeking;
      state_set_msf_info(&cdd_cxt.state, track_fad, cdd_cxt.disc_fad);
      
      break;
   }
   case 0xa:
      //scan forward
      break;
   case 0xb:
      //scan backwards
      break;
   }

   return TIME_PERIODIC;
}


extern u8 transfer_buffer[13];
int cd_command_exec()
{
   if (comm_state == Reset)
   {
      cdd_cxt.state.current_operation = Idle;
      make_status_data(&cdd_cxt.state, cdd_cxt.state_data);
      comm_state = NoTransfer;
      return TIME_POWER_ON + TIME_WAITING;
   }
   else if (
      comm_state == SendingFirstByte || 
      comm_state == SendingByte)
   {
      return TIME_BYTE;
   }
   else if (
      comm_state == NoTransfer)
   {
      cdd_cxt.bit_counter = 0;
      cdd_cxt.byte_counter = 0;
      comm_state = SendingFirstByte;

      memset(&cdd_cxt.received_data, 0, sizeof(u8) * 13);

      sh1_set_start(1);
      sh1_set_output_enable_falling_edge();

      return TIME_START;
   }
   //it is required to wait to assert output enable
   //otherwise some sort of race condition occurs
   //and breaks the transfer
   else if (comm_state == WaitToOeFirstByte)
   {
      sh1_set_output_enable_falling_edge();
      sh1_set_start(0);
      comm_state = SendingByte;
      return TIME_OE;
   }
   else if (comm_state == WaitToOe)
   {
      sh1_set_output_enable_falling_edge();
      comm_state = SendingByte;

      return TIME_OE;
   }
   else if (comm_state == WaitToRxio)
   {
      //handle the command
      return do_command();
   }

   assert(0);

   return 1;

   cdd_cxt.num_execs++;
}

void cd_drive_exec(struct CdDriveContext * drive, s32 cycles)
{
   s32 cycles_temp = drive->cycles_remainder - cycles;
   while (cycles_temp < 0)
   {
      int cycles_exec = cd_command_exec(drive);
      cycles_temp += cycles_exec;
   }
   drive->cycles_remainder = cycles_temp;
}

void set_checksum(u8 * data)
{
   u8 parity = 0;
   int i = 0;
   for (i = 0; i < 11; i++)
      parity += data[i];
   data[11] = ~parity;

   data[12] = 0;
}

static INLINE void fad2msf(s32 fad, u8 *msf) {
   msf[0] = fad / (75 * 60);
   fad -= msf[0] * (75 * 60);
   msf[1] = fad / 75;
   fad -= msf[1] * 75;
   msf[2] = fad;
}

static INLINE u8 num2bcd(u8 num) {
   return ((num / 10) << 4) | (num % 10);
}

static INLINE void fad2msf_bcd(s32 fad, u8 *msf) {
   fad2msf(fad, msf);
   msf[0] = num2bcd(msf[0]);
   msf[1] = num2bcd(msf[1]);
   msf[2] = num2bcd(msf[2]);
}

static INLINE u8 bcd2num(u8 bcd) {
   return (bcd >> 4) * 10 + (bcd & 0xf);
}

static INLINE u32 msf_bcd2fad(u8 min, u8 sec, u8 frame) {
   u32 fad = 0;
   fad += bcd2num(min);
   fad *= 60;
   fad += bcd2num(sec);
   fad *= 75;
   fad += bcd2num(frame);
   return fad;
}

s32 toc_10_get_track(s32 fad)
{
   int i = 0;
   for (i = 0; i < 99; i++)
   {
      s32 track_start = msf_bcd2fad(cdd_cxt.toc[i].min, cdd_cxt.toc[i].sec, cdd_cxt.toc[i].frame);
      s32 track_end  = msf_bcd2fad(cdd_cxt.toc[i].pmin, cdd_cxt.toc[i].psec, cdd_cxt.toc[i].pframe);

      if (fad >= track_start && fad < track_end)
         return (i + 1);
   }

   assert(0);
   return 0;
}

void state_set_msf_info(struct CdState *state, s32 track_fad, s32 disc_fad)
{
   u8 msf_buf[3] = { 0 };
   fad2msf_bcd(track_fad, msf_buf);
   state->minutes = msf_buf[0];
   state->seconds = msf_buf[1];
   state->frame = msf_buf[2];
   fad2msf_bcd(disc_fad, msf_buf);
   state->absolute_minutes = msf_buf[0];
   state->absolute_seconds = msf_buf[1];
   state->absolute_frame = msf_buf[2];
}

void make_status_data(struct CdState *state, u8* data)
{
   int i = 0;
   data[0] = state->current_operation;
   data[1] = state->q_subcode;
   data[2] = state->track_number;
   data[3] = state->index_field;
   data[4] = state->minutes;
   data[5] = state->seconds;
   data[6] = state->frame;
   data[7] = 0x04; //or zero?
   data[8] = state->absolute_minutes;
   data[9] = state->absolute_seconds;
   data[10] = state->absolute_frame;

   set_checksum(data); 
}

void cdd_reset()
{
   memset(&cdd_cxt, 0, sizeof(struct CdDriveContext));
   comm_state = Reset;
}