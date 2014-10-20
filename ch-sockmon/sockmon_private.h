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
 * \file   sockmon_private.h
 *
 * \author sandeepprakash
 *
 * \date   16-Sep-2012
 *
 * \brief
 *
 ******************************************************************************/

#ifndef __SOCKMON_PRIVATE_H__
#define __SOCKMON_PRIVATE_H__

#ifdef  __cplusplus
extern  "C"
{
#endif

/********************************* CONSTANTS **********************************/

/*********************************** MACROS ***********************************/
#define SOCKMON_LOCAL_LISTEN_SOCK_PORT_HO       (19000)

#define SOCKMON_TASK_STACK_SIZE                 (16 * 1024)

#define SOCKMON_MAX_MONITORED_SOCKETS           (64)

#define SOCKMON_LOOPBACK_ADDR                   "127.0.0.1"

#define SOCKMON_SELECT_TIMEOUT_MS               (1000)

#define SOCKMON_LOG_STR                         "SMON"

#define SMON_LOG_LOW(format,...)                                              \
do                                                                            \
{                                                                             \
   LOG_LOW (SOCKMON_LOG_STR,__FILE__,__FUNCTION__,__LINE__,format,            \
      ##__VA_ARGS__);                                                         \
} while (0)

#define SMON_LOG_MED(format,...)                                              \
do                                                                            \
{                                                                             \
   LOG_MED (SOCKMON_LOG_STR,__FILE__,__FUNCTION__,__LINE__,format,            \
      ##__VA_ARGS__);                                                         \
} while (0)

#define SMON_LOG_HIGH(format,...)                                             \
do                                                                            \
{                                                                             \
   LOG_HIGH (SOCKMON_LOG_STR,__FILE__,__FUNCTION__,__LINE__,format,           \
      ##__VA_ARGS__);                                                         \
} while (0)

#define SMON_LOG_FULL(format,...)                                             \
do                                                                            \
{                                                                             \
   LOG_FULL (SOCKMON_LOG_STR,__FILE__,__FUNCTION__,__LINE__,format,           \
      ##__VA_ARGS__);                                                         \
} while (0)

/******************************** ENUMERATIONS ********************************/

/*********************** CLASS/STRUCTURE/UNION DATA TYPES *********************/
typedef struct _SOCKMON_CTXT_X
{
   SOCKMON_CREATE_PARAMS_X x_create_params;

   PAL_MUTEX_HDL hl_mutex_hdl;

   LIST_HDL hl_list_hdl;

   TASK_HDL hl_task_hdl;

   PAL_SOCK_HDL hl_listen_hdl;

   PAL_SOCK_SET_X x_rd_set;

   PAL_SOCK_SET_X x_ex_set;
} SOCKMON_CTXT_X;

typedef struct _SOCKMON_NODE_DATA_X
{
   SOCKMON_REGISTER_DATA_X x_register_data;

   bool b_is_active;
} SOCKMON_NODE_DATA_X;

/***************************** FUNCTION PROTOTYPES ****************************/
SOCKMON_RET_E sockmon_create_resources(
   SOCKMON_CTXT_X *px_sockmon_ctxt,
   SOCKMON_CREATE_PARAMS_X *px_create_params);

SOCKMON_RET_E sockmon_delete_resources(
   SOCKMON_CTXT_X *px_sockmon_ctxt);

SOCKMON_RET_E sockmon_wakeup_sockmon_task (
   SOCKMON_CTXT_X *px_sockmon_ctxt);

LIST_RET_E sockmon_search_node_cbk (
   LIST_NODE_DATA_X *px_search_node_data,
   LIST_NODE_DATA_X *px_curr_node_data,
   void *p_app_data);

SOCKMON_RET_E sockmon_register_new_sock (
   SOCKMON_CTXT_X *px_sockmon_ctxt,
   SOCKMON_REGISTER_DATA_X *px_register_data);

#ifdef   __cplusplus
}
#endif

#endif /* __SOCKMON_PRIVATE_H__ */
