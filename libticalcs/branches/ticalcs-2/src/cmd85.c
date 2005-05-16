/* Hey EMACS -*- linux-c -*- */
/* $Id$ */

/*  libticalcs - Ti Calculator library, a part of the TiLP project
 *  Copyright (C) 1999-2005  Romain Li�vin
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
  This unit handles TI85 & TI86 commands.
  This unit shares its indentation with cmd82.c
*/

#include "ticalcs.h"
#include "packets.h"
#include "error.h"
#include "logging.h"
#include "macros.h"

#ifdef __WIN32__
#pragma warning( disable : 4761 )
#endif

// Shares some commands between TI85 & 86
#define PC_TI8586 ((handle->model == CALC_TI85) ? PC_TI85 : PC_TI86)
#define TI8586_BKUP ((handle->model == CALC_TI85) ? 0x1D : 0x1D)

/* Variable (std var header: NUL padded, variable length) */
int ti85_send_VAR(CalcHandle* handle, uint16_t varsize, uint8_t vartype, char *varname)
{
  uint8_t buffer[16];
  uint8_t trans[9];

  tifiles_transcode_detokenize(handle->model, trans, varname, vartype);
  ticalcs_info(" PC->TI: VAR (size=0x%04X=%i, id=%02X, name=<%s>)",
	  varsize, varsize, vartype, trans);

  buffer[0] = LSB(varsize);
  buffer[1] = MSB(varsize);
  buffer[2] = vartype;


  if (vartype != TI8586_BKUP) {	// backup: special header
    buffer[3] = strlen(varname);
    memcpy(buffer + 4, varname, 8);
    TRYF(send_packet(handle, PC_TI8586, CMD_VAR, 4 + strlen(varname), buffer));
  } else {
    memcpy(buffer + 3, varname, 6);
    TRYF(send_packet(handle, PC_TI8586, CMD_VAR, 9, buffer));
  }

  return 0;
}

int ti85_send_CTS(CalcHandle* handle)
{
  ticalcs_info(" PC->TI: CTS");
  TRYF(send_packet(handle, PC_TI8586, CMD_CTS, 0, NULL));

  return 0;
}

int ti85_send_XDP(CalcHandle* handle, int length, uint8_t * data)
{
  ticalcs_info(" PC->TI: XDP (0x%04X = %i bytes)", length, length);
  TRYF(send_packet(handle, PC_TI8586, CMD_XDP, length, data));

  return 0;
}

/*
  Skip variable
  - rej_code [in]: a rejection code
  - int [out]: an error code
 */
int ti85_send_SKIP(CalcHandle* handle, uint8_t rej_code)
{
  ticalcs_info(" PC->TI: SKP");
  TRYF(send_packet(handle, PC_TI8586, CMD_SKP, 1, &rej_code));
  ticalcs_info(" (rejection code = %i)", rej_code);

  return 0;
}

int ti85_send_ACK(CalcHandle* handle)
{
  ticalcs_info(" PC->TI: ACK");
  TRYF(send_packet(handle, PC_TI8586, CMD_ACK, 2, NULL));

  return 0;
}

int ti85_send_ERR(CalcHandle* handle)
{
  ticalcs_info(" PC->TI: ERR");
  TRYF(send_packet(handle, PC_TI8586, CMD_ERR, 2, NULL));

  return 0;
}

int ti85_send_SCR(CalcHandle* handle)
{
  ticalcs_info(" PC->TI: SCR");
  TRYF(send_packet(handle, PC_TI8586, CMD_SCR, 2, NULL));

  return 0;
}

int ti85_send_KEY(CalcHandle* handle, uint16_t scancode)
{
  ticalcs_info(" PC->TI: KEY");
  TRYF(ticables_cable_put(handle->cable, PC_TI8586));
  TRYF(ticables_cable_put(handle->cable, CMD_KEY));
  TRYF(ticables_cable_put(handle->cable, LSB(scancode)));
  TRYF(ticables_cable_put(handle->cable, MSB(scancode)));

  return 0;
}

int ti85_send_EOT(CalcHandle* handle)
{
  ticalcs_info(" PC->TI: EOT");
  TRYF(send_packet(handle, PC_TI8586, CMD_EOT, 2, NULL));

  return 0;
}

/* Variable request (var header: NUL padded, fixed length) */
int ti85_send_REQ(CalcHandle* handle, uint16_t varsize, uint8_t vartype, char *varname)
{
  uint8_t buffer[16] = { 0 };
  uint8_t trans[9];

  tifiles_transcode_detokenize(handle->model, trans, varname, vartype);
  ticalcs_info(" PC->TI: REQ (size=0x%04X=%i, id=%02X, name=<%s>)",
	  varsize, varsize, vartype, trans);

  buffer[0] = LSB(varsize);
  buffer[1] = MSB(varsize);
  buffer[2] = vartype;
  buffer[3] = strlen(varname);
  memcpy(buffer + 4, varname, 8);
  if ((handle->model == CALC_TI86) &&
      (vartype >= 0x15/*TI86_DIR*/) && (vartype <= 0x1B/*TI86_ZRCL*/)) {
    memset(buffer, 0, 9);
    buffer[2] = vartype;
    TRYF(send_packet(handle, PC_TI86, CMD_REQ, 5, buffer));
  } else {
    pad_buffer(buffer + 4, '\0');
    TRYF(send_packet(handle, PC_TI8586, CMD_REQ, 4 + strlen(varname), buffer));
  }

  return 0;
}

/* Request to send (var header: SPC padded, fixed length) */
int ti85_send_RTS(CalcHandle* handle, uint16_t varsize, uint8_t vartype, char *varname)
{
  uint8_t buffer[16];
  uint8_t trans[9];

  tifiles_transcode_detokenize(handle->model, trans, varname, vartype);
  ticalcs_info(" PC->TI: RTS (size=0x%04X=%i, id=%02X, name=<%s>)",
	  varsize, varsize, vartype, trans);

  buffer[0] = LSB(varsize);
  buffer[1] = MSB(varsize);
  buffer[2] = vartype;
  buffer[3] = strlen(varname);
  memcpy(buffer + 4, varname, 8);
  pad_buffer(buffer + 4, ' ');
  TRYF(send_packet(handle, PC_TI8586, CMD_RTS, 12, buffer));

  return 0;
}

/* Variable (std var header: NUL padded, variable length) */
int ti85_recv_VAR(CalcHandle* handle, uint16_t * varsize, uint8_t * vartype, char *varname)
{
  uint8_t host, cmd;
  uint8_t buffer[16] = { 0 };
  uint16_t length;
  uint8_t trans[9];
  uint8_t strl;

  ticalcs_info(" TI->PC: VAR");
  TRYF(recv_packet(handle, &host, &cmd, &length, buffer));
  if (cmd == CMD_EOT)
    return ERR_EOT;		// not really an error
  if (cmd == CMD_SKP)
    return ERR_VAR_REJECTED;
  if (cmd != CMD_VAR)
    return ERR_INVALID_CMD;
  //if((length != (4+strlen(varname))) && (length != 9)) return ERR_INVALID_PACKET; 

  *varsize = buffer[0] | (buffer[1] << 8);
  *vartype = buffer[2];
  if (*vartype != TI8586_BKUP) {
    strl = buffer[3];
    memcpy(varname, buffer + 4, strl);
    varname[strl] = '\0';
  } else {
    memcpy(varname, buffer + 3, 8);
  }

  tifiles_transcode_detokenize(handle->model, trans, varname, *vartype);
  ticalcs_info(" (size=0x%04X=%i, id=%02X, name=<%s>)",
	  *varsize, *varsize, *vartype, trans);
  ticalcs_info(".");

  return 0;
}

int ti85_recv_CTS(CalcHandle* handle)
{
  uint8_t host, cmd;
  uint16_t length;

  ticalcs_info(" TI->PC: CTS");
  TRYF(recv_packet(handle, &host, &cmd, &length, NULL));
  if (cmd == CMD_SKP)
    return ERR_VAR_REJECTED;
  else if (cmd != CMD_CTS)
    return ERR_INVALID_CMD;
  if (length != 0x0000)
    return ERR_CTS_ERROR;
  ticalcs_info(".");

  return 0;
}

int ti85_recv_SKIP(CalcHandle* handle, uint8_t * rej_code)
{
  uint8_t host, cmd;
  uint16_t length;
  *rej_code = 0;

  ticalcs_info(" TI->PC: SKP");
  TRYF(recv_packet(handle, &host, &cmd, &length, rej_code));
  if (cmd == CMD_CTS) {
    ticalcs_info("->CTS.");
    return 0;
  }
  if (cmd != CMD_SKP)
    return ERR_INVALID_CMD;
  ticalcs_info(" (rejection code = %i)", *rej_code);
  ticalcs_info(".");

  return 0;
}

int ti85_recv_XDP(CalcHandle* handle, uint16_t * length, uint8_t * data)
{
  uint8_t host, cmd;

  ticalcs_info(" TI->PC: XDP");
  TRYF(recv_packet(handle, &host, &cmd, length, data));
  if (cmd != CMD_XDP)
    return ERR_INVALID_CMD;
  ticalcs_info(" (%04X=%i bytes)", *length, *length);
  ticalcs_info(".");

  return 0;
}

/*
  Receive acknowledge
  - status [in/out]: if NULL is passed, the function checks that 00 00 has
  been received. Otherwise, it put in status the received value.
  - int [out]: an error code
*/
int ti85_recv_ACK(CalcHandle* handle, uint16_t * status)
{
  uint8_t host, cmd;
  uint16_t sts;


  ticalcs_info(" TI->PC: ACK");
  TRYF(recv_packet(handle, &host, &cmd, &sts, NULL));
  if (status != NULL)
    *status = sts;
  else if (sts != 0x0000)
    return ERR_NACK;
  if (cmd != CMD_ACK)
    return ERR_INVALID_CMD;


  ticalcs_info(".");

  return 0;
}

int ti85_recv_RTS(CalcHandle* handle, uint16_t * varsize, uint8_t * vartype, char *varname)
{
  uint8_t host, cmd;
  uint8_t buffer[16];
  uint8_t trans[9];
  uint8_t strl;

  ticalcs_info(" TI->PC: RTS");
  TRYF(recv_packet(handle, &host, &cmd, varsize, buffer));
  if (cmd != CMD_RTS)
    return ERR_INVALID_CMD;
  *varsize = buffer[0] | (buffer[1] << 8);
  *vartype = buffer[2];
  strl = buffer[3];
  memcpy(varname, buffer + 4, strl);
  varname[strl] = '\0';

  tifiles_transcode_detokenize(handle->model, trans, varname, *vartype);
  ticalcs_info(" (size=0x%04X=%i, id=%02X, name=<%s>)",
	  *varsize, *varsize, *vartype, trans);
  ticalcs_info(".");

  return 0;
}
