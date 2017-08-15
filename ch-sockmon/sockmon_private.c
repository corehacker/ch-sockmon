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
 * \file   sockmon_private.c
 *
 * \author sandeepprakash
 *
 * \date   28-Oct-2012
 *
 * \brief
 *
 ******************************************************************************/

/********************************** INCLUDES **********************************/
#include <ch-pal/exp_pal.h>
#include <ch-utils/exp_list.h>
#include <ch-utils/exp_msgq.h>
#include <ch-utils/exp_task.h>
#include <ch-utils/exp_sock_utils.h>
#include <ch-sockmon/exp_sockmon.h>
#include "sockmon_private.h"

/********************************* CONSTANTS **********************************/

/*********************************** MACROS ***********************************/

/******************************** ENUMERATIONS ********************************/

/************************* STRUCTURE/UNION DATA TYPES *************************/

/************************ STATIC FUNCTION PROTOTYPES **************************/
static void *sockmon_task(
   void *p_thread_args);

static LIST_RET_E sockmon_for_all_socks_set_cbk (
   LIST_NODE_DATA_X *px_curr_node_data,
   void *p_app_data);

static LIST_RET_E sockmon_for_all_socks_check_cbk (
   LIST_NODE_DATA_X *px_curr_node_data,
   void *p_app_data);

static SOCKMON_RET_E sockmon_prepare_select_sock_list(
   SOCKMON_CTXT_X *px_sockmon_ctxt);

static SOCKMON_RET_E sockmon_check_select_sock_list(
   SOCKMON_CTXT_X *px_sockmon_ctxt);

/****************************** LOCAL FUNCTIONS *******************************/
SOCKMON_RET_E sockmon_create_resources(
   SOCKMON_CTXT_X *px_sockmon_ctxt,
   SOCKMON_CREATE_PARAMS_X *px_create_params)
{
   SOCKMON_RET_E e_error = eSOCKMON_RET_FAILURE;
   PAL_RET_E e_pal_ret = ePAL_RET_FAILURE;
   TASK_RET_E e_task_ret = eTASK_RET_FAILURE;
   LIST_RET_E e_list_ret = eLIST_RET_FAILURE;
   TASK_CREATE_PARAM_X x_task_params = { { 0 } };
   LIST_INIT_PARAMS_X x_list_params = { 0 };
   PAL_MUTEX_CREATE_PARAM_X x_mutex_params = { { 0 } };
   uint16_t us_port_ho = 0;

   if (NULL == px_sockmon_ctxt)
   {
      SMON_LOG_MED("Invalid Args");
      e_error = eSOCKMON_RET_INVALID_ARGS;
      goto CLEAN_RETURN;
   }

   if ((0 == px_create_params->ui_max_monitored_socks)
      || (px_create_params->ui_max_monitored_socks
         > SOCKMON_MAX_MONITORED_SOCKETS))
   {
      SMON_LOG_MED("Invalid Args. Max Sockets Allowed: %d, Passed: %d",
         SOCKMON_MAX_MONITORED_SOCKETS,
         px_create_params->ui_max_monitored_socks);
      e_error = eSOCKMON_RET_INVALID_ARGS;
      goto CLEAN_RETURN;
   }

   e_pal_ret = pal_mutex_create (&(px_sockmon_ctxt->hl_mutex_hdl),
      &x_mutex_params);
   if ((ePAL_RET_SUCCESS != e_pal_ret)
      || (NULL == px_sockmon_ctxt->hl_mutex_hdl))
   {
      SMON_LOG_MED("pal_mutex_create failed: %d", e_pal_ret);
      e_error = eSOCKMON_RET_RESOURCE_FAILURE;
      goto CLEAN_RETURN;
   }

   x_list_params.ui_list_max_elements =
      px_create_params->ui_max_monitored_socks;
   e_list_ret = list_create (&(px_sockmon_ctxt->hl_list_hdl), &x_list_params);
   if ((eLIST_RET_SUCCESS != e_list_ret)
      || (NULL == px_sockmon_ctxt->hl_list_hdl))
   {
      SMON_LOG_MED("list_create failed: %d", e_list_ret);
      e_error = eSOCKMON_RET_RESOURCE_FAILURE;
      goto CLEAN_RETURN;
   }

   if ((0 != px_create_params->us_port_range_start)
      && (0 != px_create_params->us_port_range_end))
   {
      us_port_ho = px_create_params->us_port_range_start;
   }
   else
   {
      us_port_ho = SOCKMON_LOCAL_LISTEN_SOCK_PORT_HO;
   }

   e_pal_ret = tcp_listen_sock_create (&(px_sockmon_ctxt->hl_listen_hdl),
      us_port_ho);
   if ((ePAL_RET_SUCCESS != e_pal_ret)
      || (NULL == px_sockmon_ctxt->hl_listen_hdl))
   {
      SMON_LOG_MED("tcp_listen_sock_create failed: %d", e_pal_ret);
      e_error = eSOCKMON_RET_RESOURCE_FAILURE;
      goto CLEAN_RETURN;
   }

   x_task_params.fn_task_routine = sockmon_task;
   x_task_params.p_app_data = px_sockmon_ctxt;
   x_task_params.ui_thread_stack_size = SOCKMON_TASK_STACK_SIZE;
   e_task_ret = task_create (&(px_sockmon_ctxt->hl_task_hdl), &x_task_params);
   if ((eTASK_RET_SUCCESS != e_task_ret)
      || (NULL == px_sockmon_ctxt->hl_task_hdl))
   {
      SMON_LOG_MED("task_create failed: %d", e_task_ret);
      e_error = eSOCKMON_RET_RESOURCE_FAILURE;
   }
   else
   {
      e_task_ret = task_start (px_sockmon_ctxt->hl_task_hdl);
      if (eTASK_RET_SUCCESS != e_task_ret)
      {
         SMON_LOG_MED("task_start failed: %d", e_task_ret);
         e_task_ret = task_delete (px_sockmon_ctxt->hl_task_hdl);
         if (eTASK_RET_SUCCESS != e_task_ret)
         {
            SMON_LOG_MED("task_delete failed: %d", e_task_ret);
         }
         px_sockmon_ctxt->hl_task_hdl = NULL;
         e_error = eSOCKMON_RET_RESOURCE_FAILURE;
      }
      else
      {
         SMON_LOG_HIGH("Created Socket Monitor Resources");
         e_error = eSOCKMON_RET_SUCCESS;
      }
   }
   CLEAN_RETURN: return e_error;
}

SOCKMON_RET_E sockmon_delete_resources(
   SOCKMON_CTXT_X *px_sockmon_ctxt)
{
   SOCKMON_RET_E e_error = eSOCKMON_RET_FAILURE;
   PAL_RET_E e_pal_ret = ePAL_RET_FAILURE;
   TASK_RET_E e_task_ret = eTASK_RET_FAILURE;
   LIST_RET_E e_list_ret = eLIST_RET_FAILURE;
   LIST_NODE_DATA_X x_node_data = { NULL };
   SOCKMON_NODE_DATA_X *px_node_data = NULL;

   if (NULL == px_sockmon_ctxt)
   {
      SMON_LOG_MED("Invalid Args");
      e_error = eSOCKMON_RET_INVALID_ARGS;
      goto CLEAN_RETURN;
   }

   if (NULL != px_sockmon_ctxt->hl_task_hdl)
   {
      e_task_ret = task_delete (px_sockmon_ctxt->hl_task_hdl);
      if (eTASK_RET_SUCCESS != e_task_ret)
      {
         SMON_LOG_MED("task_delete failed: %d", e_task_ret);
      }
      px_sockmon_ctxt->hl_task_hdl = NULL;
   }

   if (NULL != px_sockmon_ctxt->hl_listen_hdl)
   {
      e_pal_ret = tcp_listen_sock_delete (px_sockmon_ctxt->hl_listen_hdl);
      if (ePAL_RET_SUCCESS != e_pal_ret)
      {
         SMON_LOG_MED("tcp_listen_sock_delete failed: %d", e_pal_ret);
      }
      px_sockmon_ctxt->hl_listen_hdl = NULL;
   }

   if (NULL != px_sockmon_ctxt->hl_list_hdl)
   {
      if (NULL != px_sockmon_ctxt->hl_mutex_hdl)
      {
         e_pal_ret = pal_mutex_lock (px_sockmon_ctxt->hl_mutex_hdl);
         if (ePAL_RET_SUCCESS != e_pal_ret)
         {
            SMON_LOG_MED("pal_mutex_lock failed: %d", e_pal_ret);
         }
      }
      do
      {
         e_list_ret = list_node_delete_at_head (px_sockmon_ctxt->hl_list_hdl,
            &x_node_data);
         if (eLIST_RET_SUCCESS == e_list_ret)
         {
            /*
             * TODO: Take action on the deleted node.
             */
            px_node_data = (SOCKMON_NODE_DATA_X *) x_node_data.p_data;
            if (NULL != px_node_data)
            {
               pal_free(px_node_data);
               px_node_data = NULL;
            }
         }
      } while (eLIST_RET_LIST_EMPTY != e_list_ret);

      e_list_ret = list_delete (px_sockmon_ctxt->hl_list_hdl);
      if (eLIST_RET_SUCCESS != e_list_ret)
      {
         SMON_LOG_MED("list_delete failed: %d", e_list_ret);
      }
      px_sockmon_ctxt->hl_list_hdl = NULL;

      if (NULL != px_sockmon_ctxt->hl_mutex_hdl)
      {
         e_pal_ret = pal_mutex_unlock (px_sockmon_ctxt->hl_mutex_hdl);
         if (ePAL_RET_SUCCESS != e_pal_ret)
         {
            SMON_LOG_MED("pal_mutex_unlock failed: %d", e_pal_ret);
         }
      }
   }

   if (NULL != px_sockmon_ctxt->hl_mutex_hdl)
   {
      e_pal_ret = pal_mutex_destroy (px_sockmon_ctxt->hl_mutex_hdl);
      if (ePAL_RET_SUCCESS != e_pal_ret)
      {
         SMON_LOG_MED("pal_mutex_destroy failed: %d", e_pal_ret);
      }
      px_sockmon_ctxt->hl_mutex_hdl = NULL;
   }
   SMON_LOG_HIGH("Deleted Socket Monitor Resources");
   e_error = eSOCKMON_RET_SUCCESS;
   CLEAN_RETURN: return e_error;
}

static void *sockmon_task(
   void *p_thread_args)
{
   TASK_RET_E e_task_ret = eTASK_RET_FAILURE;
   SOCKMON_CTXT_X *px_sockmon_ctxt = NULL;
   PAL_RET_E e_pal_ret = ePAL_RET_FAILURE;
   SOCKMON_RET_E e_sockmon_ret = eSOCKMON_RET_FAILURE;
   uint32_t ui_no_active_socks = 0;

   if (NULL == p_thread_args)
   {
      SMON_LOG_MED("Invalid Args");
      goto CLEAN_RETURN;
   }

   px_sockmon_ctxt = (SOCKMON_CTXT_X *) p_thread_args;

   while (task_is_in_loop (px_sockmon_ctxt->hl_task_hdl))
   {
      e_sockmon_ret = sockmon_prepare_select_sock_list (px_sockmon_ctxt);
      if (eSOCKMON_RET_SUCCESS != e_sockmon_ret)
      {
         SMON_LOG_MED ("sockmon_prepare_select_sock_list failed: %d",
            e_sockmon_ret);
      }

      e_pal_ret = pal_select (&(px_sockmon_ctxt->x_rd_set), NULL,
         &(px_sockmon_ctxt->x_ex_set), SOCKMON_SELECT_TIMEOUT_MS,
         &ui_no_active_socks);
      if (ePAL_RET_SUCCESS != e_pal_ret)
      {
         SMON_LOG_MED("pal_select failed: %d", e_pal_ret);
      }
      else
      {
         if (ui_no_active_socks > 0)
         {
            e_sockmon_ret = sockmon_check_select_sock_list (px_sockmon_ctxt);
            if (eSOCKMON_RET_SUCCESS != e_sockmon_ret)
            {
               SMON_LOG_MED("sockmon_check_select_sock_list failed: %d",
                  e_sockmon_ret);
            }
         }
         else
         {
            SMON_LOG_FULL("Select success...but no active sockets.");
         }
      }
      SMON_LOG_FULL("Sockmon Task Active");
   }

   e_task_ret = task_notify_exit (px_sockmon_ctxt->hl_task_hdl);
   if (eTASK_RET_SUCCESS != e_task_ret)
   {
      SMON_LOG_MED("task_notify_exit failed: %d", e_task_ret);
   }
   else
   {
      SMON_LOG_HIGH("task_notify_exit success");
   }
CLEAN_RETURN:
   return p_thread_args;
}

static LIST_RET_E sockmon_for_all_socks_set_cbk (
   LIST_NODE_DATA_X *px_curr_node_data,
   void *p_app_data)
{
   LIST_RET_E e_list_ret = eLIST_RET_FAILURE;
   SOCKMON_CTXT_X *px_sockmon_ctxt = NULL;
   SOCKMON_NODE_DATA_X *px_node_data = NULL;
   PAL_SOCK_SET_X *px_rd_set = NULL;
   PAL_SOCK_SET_X *px_ex_set = NULL;

   if ((NULL == px_curr_node_data) || (NULL == p_app_data))
   {
      SMON_LOG_MED("Invalid Args");
      e_list_ret = eLIST_RET_INVALID_ARGS;
      goto CLEAN_RETURN;
   }
   if ((NULL == px_curr_node_data->p_data)
         || (0 == px_curr_node_data->ui_data_size)
         || (sizeof(SOCKMON_NODE_DATA_X) != px_curr_node_data->ui_data_size))
   {
      SMON_LOG_MED("Invalid Args");
      e_list_ret = eLIST_RET_INVALID_ARGS;
      goto CLEAN_RETURN;
   }

   px_sockmon_ctxt = (SOCKMON_CTXT_X *) p_app_data;
   px_node_data = (SOCKMON_NODE_DATA_X *) px_curr_node_data->p_data;

   if (true == px_node_data->b_is_active)
   {
      px_rd_set = &(px_sockmon_ctxt->x_rd_set);
      px_ex_set = &(px_sockmon_ctxt->x_ex_set);

      PAL_SOCK_SET_SET(px_node_data->x_register_data.hl_sock_hdl, px_rd_set);
      PAL_SOCK_SET_SET(px_node_data->x_register_data.hl_sock_hdl, px_ex_set);
   }
   e_list_ret = eLIST_RET_SUCCESS;
CLEAN_RETURN:
   return e_list_ret;
}

static LIST_RET_E sockmon_for_all_socks_check_cbk (
   LIST_NODE_DATA_X *px_curr_node_data,
   void *p_app_data)
{
   LIST_RET_E e_list_ret = eLIST_RET_FAILURE;
   SOCKMON_RET_E e_sockmon_ret = eSOCKMON_RET_FAILURE;
   SOCKMON_CTXT_X *px_sockmon_ctxt = NULL;
   SOCKMON_NODE_DATA_X *px_node_data = NULL;
   PAL_SOCK_SET_X *px_rd_set = NULL;
   PAL_SOCK_SET_X *px_ex_set = NULL;
   bool b_is_set = false;

   if ((NULL == px_curr_node_data) || (NULL == p_app_data))
   {
      SMON_LOG_MED("Invalid Args");
      e_list_ret = eLIST_RET_INVALID_ARGS;
      goto CLEAN_RETURN;
   }
   if ((NULL == px_curr_node_data->p_data)
         || (0 == px_curr_node_data->ui_data_size)
         || (sizeof(SOCKMON_NODE_DATA_X) != px_curr_node_data->ui_data_size))
   {
      SMON_LOG_MED("Invalid Args");
      e_list_ret = eLIST_RET_INVALID_ARGS;
      goto CLEAN_RETURN;
   }

   px_sockmon_ctxt = (SOCKMON_CTXT_X *) p_app_data;
   px_node_data = (SOCKMON_NODE_DATA_X *) px_curr_node_data->p_data;

   px_rd_set = &(px_sockmon_ctxt->x_rd_set);
   px_ex_set = &(px_sockmon_ctxt->x_ex_set);
   b_is_set = PAL_SOCK_SET_ISSET(px_node_data->x_register_data.hl_sock_hdl,
      px_ex_set);
   if (true == b_is_set)
   {
      if (NULL != px_node_data->x_register_data.fn_active_sock_cbk)
      {
         e_sockmon_ret = px_node_data->x_register_data.fn_active_sock_cbk (
            e_SOCKMON_SOCK_ACTIVITY_STATUS_EXCEPTION,
            px_node_data->x_register_data.hl_sock_hdl,
            px_node_data->x_register_data.p_app_data);
         if (eSOCKMON_RET_SUCCESS != e_sockmon_ret)
         {
            SMON_LOG_MED("fn_active_sock_cbk failed for socket: %p",
               px_node_data->x_register_data.hl_sock_hdl);
         }
         e_list_ret = eLIST_RET_SUCCESS;
      }
      else
      {
         e_list_ret = eLIST_RET_SUCCESS;
      }
      goto CLEAN_RETURN;
   }


   b_is_set = PAL_SOCK_SET_ISSET(px_node_data->x_register_data.hl_sock_hdl,
      px_rd_set);
   if (true == b_is_set)
   {
      SMON_LOG_FULL ("Data available on sock: %p. Issuing callback. Disabling "
         "this sock for further select until next registration.",
         px_node_data->x_register_data.hl_sock_hdl);
      px_node_data->b_is_active = false;
      if (NULL != px_node_data->x_register_data.fn_active_sock_cbk)
      {
         e_sockmon_ret = px_node_data->x_register_data.fn_active_sock_cbk (
            e_SOCKMON_SOCK_ACTIVITY_STATUS_DATA_AVAILABLE,
            px_node_data->x_register_data.hl_sock_hdl,
            px_node_data->x_register_data.p_app_data);
         if (eSOCKMON_RET_SUCCESS != e_sockmon_ret)
         {
            SMON_LOG_MED("fn_active_sock_cbk failed for socket: %p",
               px_node_data->x_register_data.hl_sock_hdl);
         }
         e_list_ret = eLIST_RET_SUCCESS;
      }
      else
      {
         e_list_ret = eLIST_RET_SUCCESS;
      }
   }
   else
   {
      e_list_ret = eLIST_RET_SUCCESS;
   }
CLEAN_RETURN:
   return e_list_ret;
}

static SOCKMON_RET_E sockmon_prepare_select_sock_list(
   SOCKMON_CTXT_X *px_sockmon_ctxt)
{
   SOCKMON_RET_E e_error = eSOCKMON_RET_FAILURE;
   LIST_RET_E e_list_ret = eLIST_RET_FAILURE;
   PAL_RET_E e_pal_ret = ePAL_RET_FAILURE;
   PAL_SOCK_SET_X *px_rd_set = NULL;
   PAL_SOCK_SET_X *px_ex_set = NULL;

   if (NULL == px_sockmon_ctxt)
   {
      SMON_LOG_MED("Invalid Args");
      e_error = eSOCKMON_RET_INVALID_ARGS;
      goto CLEAN_RETURN;
   }

   px_rd_set = &(px_sockmon_ctxt->x_rd_set);
   px_ex_set = &(px_sockmon_ctxt->x_ex_set);

   PAL_SOCK_SET_ZERO(px_rd_set);
   PAL_SOCK_SET_ZERO(px_ex_set);

   PAL_SOCK_SET_SET(px_sockmon_ctxt->hl_listen_hdl, px_rd_set);
   PAL_SOCK_SET_SET(px_sockmon_ctxt->hl_listen_hdl, px_ex_set);

   e_pal_ret = pal_mutex_lock(px_sockmon_ctxt->hl_mutex_hdl);
   if (ePAL_RET_SUCCESS != e_pal_ret)
   {
      SMON_LOG_MED("pal_mutex_lock failed: %d", e_pal_ret);
      e_error = eSOCKMON_RET_RESOURCE_FAILURE;
      goto CLEAN_RETURN;
   }

   e_list_ret = list_for_all_nodes (px_sockmon_ctxt->hl_list_hdl,
      sockmon_for_all_socks_set_cbk, px_sockmon_ctxt);
   if (eLIST_RET_SUCCESS != e_list_ret)
   {
      if (eLIST_RET_LIST_EMPTY != e_list_ret)
      {
         SMON_LOG_MED("list_for_all_nodes failed: %d", e_list_ret);
         e_error = eSOCKMON_RET_RESOURCE_FAILURE;
      }
      else
      {
         // SMON_LOG_HIGH("No sockets registered. Nothing to select on");
         e_error = eSOCKMON_RET_SUCCESS;
      }
   }
   else
   {
      SMON_LOG_FULL("Registered %d sockets for select",
         px_sockmon_ctxt->x_rd_set.ui_count);
      e_error = eSOCKMON_RET_SUCCESS;
   }
   e_pal_ret = pal_mutex_unlock(px_sockmon_ctxt->hl_mutex_hdl);
   if (ePAL_RET_SUCCESS != e_pal_ret)
   {
      SMON_LOG_MED("pal_mutex_unlock failed: %d", e_pal_ret);
   }
CLEAN_RETURN:
   return e_error;
}

static SOCKMON_RET_E sockmon_check_select_sock_list(
   SOCKMON_CTXT_X *px_sockmon_ctxt)
{
   SOCKMON_RET_E e_error = eSOCKMON_RET_FAILURE;
   LIST_RET_E e_list_ret = eLIST_RET_FAILURE;
   bool b_is_set = false;
   PAL_RET_E e_pal_ret = ePAL_RET_FAILURE;
   PAL_SOCK_ADDR_IN_X x_in_addr = {0};
   PAL_SOCK_HDL hl_sock_hdl = NULL;
   PAL_SOCK_SET_X *px_rd_set = NULL;

   if (NULL == px_sockmon_ctxt)
   {
      SMON_LOG_MED("Invalid Args");
      e_error = eSOCKMON_RET_INVALID_ARGS;
      goto CLEAN_RETURN;
   }

   px_rd_set = &(px_sockmon_ctxt->x_rd_set);

   b_is_set = PAL_SOCK_SET_ISSET(px_sockmon_ctxt->hl_listen_hdl, px_rd_set);
   if (true == b_is_set)
   {
      SMON_LOG_HIGH ("Thread Woke Up...Accepting the Socket...And Closing It");
      e_pal_ret = pal_sock_accept(px_sockmon_ctxt->hl_listen_hdl, &x_in_addr,
         &hl_sock_hdl);
      if ((ePAL_RET_SUCCESS != e_pal_ret) || (NULL == hl_sock_hdl))
      {
         SMON_LOG_MED("pal_sock_accept failed: %d", e_pal_ret);
      }
      else
      {
         e_pal_ret = pal_sock_close(hl_sock_hdl);
         if (ePAL_RET_SUCCESS != e_pal_ret)
         {
            SMON_LOG_MED("pal_sock_close failed: %d", e_pal_ret);
         }
         hl_sock_hdl = NULL;
      }
   }

   e_pal_ret = pal_mutex_lock(px_sockmon_ctxt->hl_mutex_hdl);
   if (ePAL_RET_SUCCESS != e_pal_ret)
   {
      SMON_LOG_MED("pal_mutex_lock failed: %d", e_pal_ret);
      e_error = eSOCKMON_RET_RESOURCE_FAILURE;
      goto CLEAN_RETURN;
   }

   e_list_ret = list_for_all_nodes (px_sockmon_ctxt->hl_list_hdl,
      sockmon_for_all_socks_check_cbk, px_sockmon_ctxt);
   if (eLIST_RET_SUCCESS != e_list_ret)
   {
      SMON_LOG_MED("list_for_all_nodes failed: %d", e_list_ret);
      e_error = eSOCKMON_RET_RESOURCE_FAILURE;
   }
   else
   {
      e_error = eSOCKMON_RET_SUCCESS;
   }

   e_pal_ret = pal_mutex_unlock(px_sockmon_ctxt->hl_mutex_hdl);
   if (ePAL_RET_SUCCESS != e_pal_ret)
   {
      SMON_LOG_MED("pal_mutex_unlock failed: %d", e_pal_ret);
   }
CLEAN_RETURN:
   return e_error;
}

SOCKMON_RET_E sockmon_wakeup_sockmon_task (
   SOCKMON_CTXT_X *px_sockmon_ctxt)
{
   SOCKMON_RET_E e_error = eSOCKMON_RET_FAILURE;
   PAL_SOCK_HDL hl_temp_sock_hdl = NULL;
   PAL_RET_E e_pal_ret = ePAL_RET_FAILURE;
   PAL_SOCK_ADDR_IN_X x_local_sock_addr = {0};
   PAL_SOCK_IN_ADDR_X x_in_addr = {0};
   uint16_t us_port_ho = 0;

   if (NULL == px_sockmon_ctxt)
   {
      SMON_LOG_MED("Invalid Args");
      e_error = eSOCKMON_RET_INVALID_ARGS;
      goto CLEAN_RETURN;
   }
   e_pal_ret = pal_sock_create(&hl_temp_sock_hdl, ePAL_SOCK_DOMAIN_AF_INET,
      ePAL_SOCK_TYPE_SOCK_STREAM, ePAL_SOCK_PROTOCOL_DEFAULT);
   if ((ePAL_RET_SUCCESS != e_pal_ret) || (NULL == hl_temp_sock_hdl))
   {
      SMON_LOG_MED("pal_sock_create failed: %d", e_pal_ret);
      e_error = eSOCKMON_RET_RESOURCE_FAILURE;
      goto CLEAN_RETURN;
   }

   e_pal_ret = pal_inet_aton ((uint8_t *) SOCKMON_LOOPBACK_ADDR, &x_in_addr);
   if ((ePAL_RET_SUCCESS != e_pal_ret) || (0 == x_in_addr.ui_ip_addr_no))
   {
      SMON_LOG_MED("pal_inet_aton failed: %d", e_pal_ret);
      e_error = eSOCKMON_RET_RESOURCE_FAILURE;
      goto CLEAN_RETURN;
   }

   if ((0 != px_sockmon_ctxt->x_create_params.us_port_range_start)
      && (0 != px_sockmon_ctxt->x_create_params.us_port_range_end))
   {
      us_port_ho = px_sockmon_ctxt->x_create_params.us_port_range_start;
   }
   else
   {
      us_port_ho = SOCKMON_LOCAL_LISTEN_SOCK_PORT_HO;
   }

   x_local_sock_addr.us_sin_port_no = pal_htons (us_port_ho);
   x_local_sock_addr.x_sin_addr.ui_ip_addr_no = x_in_addr.ui_ip_addr_no;
   e_pal_ret = pal_sock_connect(hl_temp_sock_hdl, &x_local_sock_addr,
      ePAL_SOCK_CONN_MODE_BLOCKING, 0);
   if (ePAL_RET_SUCCESS != e_pal_ret)
   {
      SMON_LOG_MED("pal_sock_connect failed: %d", e_pal_ret);
      e_error = eSOCKMON_RET_RESOURCE_FAILURE;
      goto CLEAN_RETURN;
   }
   else
   {
      SMON_LOG_HIGH("Sockmon Task wakeup Success");
      e_error = eSOCKMON_RET_SUCCESS;
   }

CLEAN_RETURN:
   if (NULL != hl_temp_sock_hdl)
   {
      e_pal_ret = pal_sock_close(hl_temp_sock_hdl);
      if (ePAL_RET_SUCCESS != e_pal_ret)
      {
         SMON_LOG_MED("pal_sock_close failed: %d", e_pal_ret);
      }
      hl_temp_sock_hdl = NULL;
   }
   return e_error;
}

LIST_RET_E sockmon_search_node_cbk (
   LIST_NODE_DATA_X *px_search_node_data,
   LIST_NODE_DATA_X *px_curr_node_data,
   void *p_app_data)
{
   LIST_RET_E e_list_ret = eLIST_RET_FAILURE;
   SOCKMON_REGISTER_DATA_X *px_search_data = NULL;
   SOCKMON_NODE_DATA_X *px_node_data = NULL;

   if ((NULL == px_curr_node_data) || (NULL == p_app_data)
         || (NULL == px_search_node_data))
   {
      SMON_LOG_MED("Invalid Args");
      e_list_ret = eLIST_RET_INVALID_ARGS;
      goto CLEAN_RETURN;
   }
   if ((NULL == px_curr_node_data->p_data)
      || (0 == px_curr_node_data->ui_data_size)
      || (sizeof(SOCKMON_NODE_DATA_X) != px_curr_node_data->ui_data_size)
      || (NULL == px_search_node_data->p_data)
      || (0 == px_search_node_data->ui_data_size)
      || (sizeof(SOCKMON_REGISTER_DATA_X) != px_search_node_data->ui_data_size))
   {
      SMON_LOG_MED("Invalid Args");
      e_list_ret = eLIST_RET_INVALID_ARGS;
      goto CLEAN_RETURN;
   }

   px_node_data = (SOCKMON_NODE_DATA_X *) px_curr_node_data->p_data;
   px_search_data = (SOCKMON_REGISTER_DATA_X *) px_search_node_data->p_data;
   if (px_search_data->hl_sock_hdl == px_node_data->x_register_data.hl_sock_hdl)
   {
      e_list_ret = eLIST_RET_LIST_NODE_FOUND;
   }
   else
   {
      e_list_ret = eLIST_RET_LIST_NODE_NOT_FOUND;
   }
CLEAN_RETURN:
   return e_list_ret;
}

SOCKMON_RET_E sockmon_register_new_sock (
   SOCKMON_CTXT_X *px_sockmon_ctxt,
   SOCKMON_REGISTER_DATA_X *px_register_data)
{
   SOCKMON_RET_E e_error = eSOCKMON_RET_FAILURE;
   LIST_RET_E e_list_ret = eLIST_RET_FAILURE;
   SOCKMON_NODE_DATA_X *px_node_data = NULL;
   LIST_NODE_DATA_X x_node_data = {NULL};

   if ((NULL == px_sockmon_ctxt) || (NULL == px_register_data))
   {
      SMON_LOG_MED("Invalid Args");
      e_error = eSOCKMON_RET_INVALID_ARGS;
      goto CLEAN_RETURN;
   }

   px_node_data = pal_malloc (sizeof(SOCKMON_NODE_DATA_X), NULL );
   if (NULL == px_node_data)
   {
      SMON_LOG_MED("pal_malloc failed: %p", px_node_data);
      e_error = eSOCKMON_RET_RESOURCE_FAILURE;
      goto CLEAN_RETURN;
   }

   (void) pal_memmove (&(px_node_data->x_register_data), px_register_data,
      sizeof(px_node_data->x_register_data));
   px_node_data->b_is_active = true;
   x_node_data.p_data = px_node_data;
   x_node_data.ui_data_size = sizeof(*px_node_data);

   e_list_ret = list_node_append (px_sockmon_ctxt->hl_list_hdl,
      &x_node_data);
   if (eLIST_RET_SUCCESS != e_list_ret)
   {
      SMON_LOG_MED("list_node_append failed: %d", e_list_ret);
      e_error = eSOCKMON_RET_RESOURCE_FAILURE;
   }
   else
   {
      e_error = eSOCKMON_RET_SUCCESS;
   }
CLEAN_RETURN:
   if (eSOCKMON_RET_SUCCESS != e_error)
   {
      if (NULL != px_node_data)
      {
         pal_free(px_node_data);
         px_node_data = NULL;
      }
   }
   return e_error;
}
