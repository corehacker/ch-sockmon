/*******************************************************************************
 *  Repository for C modules.
 *  Copyright (C) 2012 Sandeep Prakash
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 ******************************************************************************/

/*******************************************************************************
 * Copyright (c) 2012, Sandeep Prakash <123sandy@gmail.com>
 *
 * \file   exp_sockmon.h
 *
 * \author sandeepprakash
 *
 * \date   14-Sep-2012
 *
 * \brief
 *
 ******************************************************************************/

#ifndef __EXP_SOCKMON_H__
#define __EXP_SOCKMON_H__

#ifdef  __cplusplus
extern  "C"
{
#endif

/********************************* CONSTANTS **********************************/

/*********************************** MACROS ***********************************/

typedef enum _SOCKMON_RET_E
{
   eSOCKMON_RET_SUCCESS                = 0x00000000,

   eSOCKMON_RET_FAILURE                = 0x00000001,

   eSOCKMON_RET_INVALID_ARGS           = 0x00000002,

   eSOCKMON_RET_INVALID_HANDLE         = 0x00000003,

   eSOCKMON_RET_RESOURCE_FAILURE       = 0x00000004,

   eSOCKMON_RET_MAX
} SOCKMON_RET_E;

/******************************** ENUMERATIONS ********************************/
typedef enum _SOCKMON_SOCK_ACTIVITY_STATUS_E
{
   e_SOCKMON_SOCK_ACTIVITY_STATUS_INVALID             = 0x00000000,

   e_SOCKMON_SOCK_ACTIVITY_STATUS_EXCEPTION,

   e_SOCKMON_SOCK_ACTIVITY_STATUS_DATA_AVAILABLE,

   e_SOCKMON_SOCK_ACTIVITY_STATUS_MAX,
} SOCKMON_SOCK_ACTIVITY_STATUS_E;

/*********************** CLASS/STRUCTURE/UNION DATA TYPES *********************/
typedef struct SOCKMON_CTXT_X       *SOCKMON_HDL;

typedef struct _SOCKMON_CREATE_PARAMS_X
{
   uint16_t us_port_range_start;

   uint16_t us_port_range_end;

   uint32_t ui_max_monitored_socks;
} SOCKMON_CREATE_PARAMS_X;

typedef SOCKMON_RET_E (*pfn_sockmon_active_sock_cbk) (
   SOCKMON_SOCK_ACTIVITY_STATUS_E e_status,
   PAL_SOCK_HDL hl_sock_hdl,
   void *p_app_data);

typedef struct _SOCKMON_REGISTER_DATA_X
{
   PAL_SOCK_HDL hl_sock_hdl;

   pfn_sockmon_active_sock_cbk fn_active_sock_cbk;

   void *p_app_data;
} SOCKMON_REGISTER_DATA_X;

/***************************** FUNCTION PROTOTYPES ****************************/
SOCKMON_RET_E sockmon_create (
   SOCKMON_HDL *phl_sockmon_hdl,
   SOCKMON_CREATE_PARAMS_X *px_create_params);

SOCKMON_RET_E sockmon_destroy (
   SOCKMON_HDL hl_sockmon_hdl);

SOCKMON_RET_E sockmon_register_sock (
   SOCKMON_HDL hl_sockmon_hdl,
   SOCKMON_REGISTER_DATA_X *px_register_data);

SOCKMON_RET_E sockmon_deregister_sock (
   SOCKMON_HDL hl_sockmon_hdl,
   SOCKMON_REGISTER_DATA_X *px_register_data);

#ifdef   __cplusplus
}
#endif

#endif /* __EXP_SOCKMON_H__ */
