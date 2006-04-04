/* Hey EMACS -*- linux-c -*- */
/* $Id: cmd84p.c 2077 2006-03-31 21:16:19Z roms $ */

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
  This unit handles TI84+ commands with DirectLink.
*/

// Some functions should be renamed or re-organized...

#include <string.h>

#include "ticalcs.h"
#include "logging.h"
#include "error.h"
#include "macros.h"

#include "dusb_vpkt.h"
#include "cmd84p.h"

/////////////----------------

CalcParam*	cp_new(uint16_t id, uint16_t size)
{
	CalcParam* cp = calloc(1, sizeof(CalcParam));

	cp->id = id;
	cp->size = size;
	cp->data = calloc(1, size);

	return cp;
}

void		cp_del(CalcParam* cp)
{
	free(cp->data);
	free(cp);
}

CalcParam** cp_new_array(int size)
{
	CalcParam** array = calloc(size+1, sizeof(CalcParam *));
	return array;
}

void cp_del_array(int size, CalcParam **params)
{
	int i;

	for(i = 0; i < size && params[i]; i++)
	{
		if(params[i]->ok)
			free(params[i]->data);
		free(params[i]);
	}
	free(params);
}

/////////////----------------

CalcAttr*	ca_new(uint16_t id, uint16_t size)
{
	CalcAttr* cp = calloc(1, sizeof(CalcAttr));

	cp->id = id;
	cp->size = size;
	cp->data = calloc(1, size);

	return cp;
}

void		ca_del(CalcAttr* cp)
{
	free(cp->data);
	free(cp);
}

void ca_del_array(int nattrs, CalcAttr *attrs)
{
	int i;

	for(i = 0; i < nattrs; i++)
		if(attrs[i].ok)
			free(attrs[i].data);
	free(attrs);
}

/////////////----------------

// 0x0001: set mode or ping
int ti84p_mode_set(CalcHandle *h)
{
	ModeSet mode = { 0 };
	VirtualPacket* pkt;

	mode.arg1 = 3;	// normal operation mode
	mode.arg2 = 1;
	mode.arg5 = 0x07d0;

	TRYF(dusb_buffer_size_request(h));
	TRYF(dusb_buffer_size_alloc(h));

	pkt = vtl_pkt_new(sizeof(mode), VPKT_PING);
	pkt->data[0] = MSB(mode.arg1);
	pkt->data[1] = LSB(mode.arg1);
	pkt->data[2] = MSB(mode.arg2);
	pkt->data[3] = LSB(mode.arg2);
	pkt->data[4] = MSB(mode.arg3);
	pkt->data[5] = LSB(mode.arg3);
	pkt->data[6] = MSB(mode.arg4);
	pkt->data[7] = LSB(mode.arg4);
	pkt->data[8] = MSB(mode.arg5);
	pkt->data[9] = LSB(mode.arg5);
	TRYF(dusb_send_data(h, pkt));	// set mode

	vtl_pkt_del(pkt);
	return 0;
}

// 0x0002: begin OS transfer
int ti84p_os_begin(CalcHandle *h)
{
	return 0;
}

// 0x0003: acknowledgement of OS transfer
int ti84p_os_ack(CalcHandle *h)
{
	return 0;
}

// 0x0005: OS data
int ti84p_os_data(CalcHandle *h)
{
	return 0;
}

// 0x0006: acknowledgement of EOT
int ti84p_eot_ack(CalcHandle *h)
{
	return 0;
}

// 0x0007: parameter request
int ti84p_params_request(CalcHandle *h, int nparams, uint16_t *pids)
{
	VirtualPacket* pkt;
	int i;

	pkt = vtl_pkt_new((nparams + 1) * sizeof(uint16_t), VPKT_PARM_REQ);

	pkt->data[0] = MSB(nparams);
	pkt->data[1] = LSB(nparams);

	for(i = 0; i < nparams; i++)
	{
		pkt->data[2*(i+1) + 0] = MSB(pids[i]);
		pkt->data[2*(i+1) + 1] = LSB(pids[i]);
	}

	TRYF(dusb_send_data(h, pkt));

	vtl_pkt_del(pkt);
	return 0;
}

// 0x0008: parameter data
int ti84p_params_data(CalcHandle *h, int nparams, CalcParam **params)
{
	VirtualPacket* pkt;
	int i, j;

	pkt = vtl_pkt_new(0, 0);
	TRYF(dusb_recv_data(h, pkt));	// param data

	if(pkt->type != VPKT_PARM_DATA)
		return ERR_INVALID_PACKET;

	if(((pkt->data[j=0] << 8) | pkt->data[j=1]) != nparams)
		return ERR_INVALID_PACKET;

	//*params = (CalcParam *)calloc(nparams + 1, sizeof(CalcParam));
	for(i = 0, j = 2; i < nparams; i++)
	{
		CalcParam *s = params[i] = cp_new(0, 0);
		
		s->id = pkt->data[j++] << 8; s->id |= pkt->data[j++];
		s->ok = !pkt->data[j++];
		if(s->ok)
		{
			s->size = pkt->data[j++] << 8; s->size |= pkt->data[j++];
			s->data = (uint8_t *)calloc(1, s->size);
			memcpy(s->data, &pkt->data[j], s->size);
			j += s->size;
		}
	}
	
	vtl_pkt_del(pkt);
	return 0;
}

// 0x0009: request directory listing
int ti84p_dirlist_request(CalcHandle *h, int n, uint16_t *aids)
{
	VirtualPacket* pkt;
	int i;
	int j = 0;

	pkt = vtl_pkt_new(4 + 2*n + 7, VPKT_DIR_REQ);

	pkt->data[j++] = MSB(MSW(n));
	pkt->data[j++] = LSB(MSW(n));
	pkt->data[j++] = MSB(LSW(n));
	pkt->data[j++] = LSB(LSW(n));

	for(i = 0; i < n; i++)
	{
		pkt->data[j++] = MSB(aids[i]);
		pkt->data[j++] = LSB(aids[i]);
	}

	pkt->data[j++] = 0x00; pkt->data[j++] = 0x01;
	pkt->data[j++] = 0x00; pkt->data[j++] = 0x01;
	pkt->data[j++] = 0x00; pkt->data[j++] = 0x01;
	pkt->data[j++] = 0x01;

	TRYF(dusb_send_data(h, pkt));

	vtl_pkt_del(pkt);
	return 0;
}

// 0x000A: variabel header (name is utf-8)
int ti84p_var_header(CalcHandle *h, char *name, CalcAttr **attr)
{
	VirtualPacket* pkt;
	uint16_t name_length;
	int nattr;
	int i, j;

	pkt = vtl_pkt_new(0, 0);
	TRYF(dusb_recv_data(h, pkt));	// variable header

	if(pkt->type == VPKT_EOT)
	{
		vtl_pkt_del(pkt);
		return ERR_EOT;
	}
	else if(pkt->type != VPKT_VAR_HDR)
		return ERR_INVALID_PACKET;

	name_length = (pkt->data[0] << 8) | pkt->data[1];
	memcpy(name, pkt->data + 2, name_length+1);

	nattr = (pkt->data[name_length+3] << 8) | pkt->data[name_length+4];
	*attr = (CalcAttr *)calloc(nattr + 1, sizeof(CalcAttr));

	for(i = 0, j = name_length+5; i < nattr; i++)
	{
		CalcAttr *s = *attr + i;

		s->id = pkt->data[j++] << 8; s->id |= pkt->data[j++];
		s->ok = !pkt->data[j++];
		if(s->ok)
		{
			s->size = pkt->data[j++] << 8; s->size |= pkt->data[j++];
			s->data = (uint8_t *)calloc(1, s->size);
			memcpy(s->data, &pkt->data[j], s->size);
			j += s->size;
		}
	}
	
	vtl_pkt_del(pkt);
	return 0;
}

// 0x000B: request to send
int ti84p_rts(CalcHandle *h)
{
	return 0;
}

// 0x000C: variable request
int ti84p_var_request(CalcHandle *h)
{
	return 0;
}

// 0x000D: variable contents
int ti84p_var_content(CalcHandle *h)
{
	return 0;
}

// 0x000E: parameter set
int ti84p_params_set(CalcHandle *h, const CalcParam *param)
{
	VirtualPacket* pkt;

	pkt = vtl_pkt_new((2 + 2 + param->size) * sizeof(uint16_t), VPKT_PARM_SET);

	pkt->data[0] = MSB(param->id);
	pkt->data[1] = LSB(param->id);
	pkt->data[2] = MSB(param->size);
	pkt->data[3] = LSB(param->size);
	memcpy(pkt->data + 4, param->data, param->size);

	TRYF(dusb_send_data(h, pkt));	// param set

	vtl_pkt_del(pkt);
	return 0;
}

// 0x0010: variabel delete
int ti84p_var_delete(CalcHandle *h, char *name, int n, const CalcAttr *attr)
{
	VirtualPacket* pkt;
	int i;
	int j = 0;
	int pks;

	pks = 2 + strlen(name)+1 + 2;
	for(i = 0; i < n; i++) pks += 4 + attr[i].size;
	pkt = vtl_pkt_new(pks, VPKT_DEL_VAR);

	pkt->data[j++] = MSB(strlen(name)+1);
	pkt->data[j++] = LSB(strlen(name)+1);
	memcpy(pkt->data + j, name, strlen(name)+1);
	j += strlen(name)+1;
	pkt->data[j++] = MSB(n);
	pkt->data[j++] = LSB(n);

	for(i = 0; i < n; i++)
	{
		pkt->data[j++] = MSB(attr[i].id);
		pkt->data[j++] = LSB(attr[i].id);
		pkt->data[j++] = LSB(attr[i].size);
		pkt->data[j++] = LSB(attr[i].size);
		memcpy(pkt->data + j, attr[i].data, attr[i].size);
		j += attr[i].size;
	}

	pkt->data[j++] = 0x01; pkt->data[j++] = 0x00;
	pkt->data[j++] = 0x00; pkt->data[j++] = 0x00;
	pkt->data[j++] = 0x00;

	TRYF(dusb_send_data(h, pkt));

	vtl_pkt_del(pkt);
	return 0;
}

// 0x0012: acknowledgement of mode setting
int ti84p_mode_ack(CalcHandle *h)
{
	VirtualPacket* pkt;

	pkt = vtl_pkt_new(0, 0);
	TRYF(dusb_recv_data(h, pkt));

	if(pkt->type != VPKT_MODE_SET)
		return ERR_INVALID_PACKET;

	vtl_pkt_del(pkt);
	return 0;
}

// 0xAA00: acknowledgement of data
int ti84p_data_ack(CalcHandle *h)
{
	VirtualPacket* pkt;

	pkt = vtl_pkt_new(0, 0);
	TRYF(dusb_recv_data(h, pkt));

	if(pkt->type != VPKT_DATA_ACK)
		return ERR_INVALID_PACKET;

	vtl_pkt_del(pkt);
	return 0;
}

// 0xBB00: acknowledgement of parameter request
int ti84p_params_ack(CalcHandle *h)
{
	VirtualPacket* pkt;

	pkt = vtl_pkt_new(0, 0);
	TRYF(dusb_recv_data(h, pkt));

	if(pkt->type != VPKT_PARM_ACK)
		return ERR_INVALID_PACKET;

	vtl_pkt_del(pkt);
	return 0;
}

// 0xDD00: end of transmission

// 0xEE00: error