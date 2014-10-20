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
 * \file   sockmon.c
 *
 * \author sandeepprakash
 *
 * \date   15-Sep-2012
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

/****************************** LOCAL FUNCTIONS *******************************/
SOCKMON_RET_E sockmon_create(
   SOCKMON_HDL *phl_sockmon_hdl,
   SOCKMON_CREATE_PARAMS_X *px_create_params)
{
   SOCKMON_RET_E e_error = eSOCKMON_RET_FAILURE;
   SOCKMON_RET_E e_error_pvt = eSOCKMON_RET_FAILURE;
   SOCKMON_CTXT_X *px_sockmon_ctxt = NULL;

   if ((NULL == phl_sockmon_hdl) || (NULL == px_create_params))
   {
      SMON_LOG_MED("Invalid Args");
      e_error = eSOCKMON_RET_INVALID_ARGS;
      goto CLEAN_RETURN;
   }

   px_sockmon_ctxt = pal_malloc (sizeof(SOCKMON_CTXT_X), NULL );
   if (NULL == px_sockmon_ctxt)
   {
      SMON_LOG_MED("pal_malloc failed: %p", px_sockmon_ctxt);
      e_error = eSOCKMON_RET_RESOURCE_FAILURE;
      goto CLEAN_RETURN;
   }

   (void) pal_memmove(&(px_sockmon_ctxt->x_create_params), px_create_params,
      sizeof(px_sockmon_ctxt->x_create_params));

   e_error = sockmon_create_resources (px_sockmon_ctxt, px_create_params);
   if (eSOCKMON_RET_SUCCESS != e_error)
   {
      SMON_LOG_MED("sockmon_create_resources failed: %d", e_error);
   }
   else
   {
      SMON_LOG_HIGH("Created Socket Monitor");
      *phl_sockmon_hdl = (SOCKMON_HDL) px_sockmon_ctxt;
   }
CLEAN_RETURN:
   if (eSOCKMON_RET_SUCCESS != e_error)
   {
      e_error_pvt = sockmon_delete_resources (px_sockmon_ctxt);
      if (eSOCKMON_RET_SUCCESS != e_error_pvt)
      {
         SMON_LOG_MED("sockmon_delete_resources failed: %d", e_error_pvt);
      }
      if (NULL != px_sockmon_ctxt)
      {
         pal_free (px_sockmon_ctxt);
         px_sockmon_ctxt = NULL;
      }
   }
   return e_error;
}

SOCKMON_RET_E sockmon_destroy(
   SOCKMON_HDL hl_sockmon_hdl)
{
   SOCKMON_RET_E e_error = eSOCKMON_RET_FAILURE;
   SOCKMON_CTXT_X *px_sockmon_ctxt = NULL;

   if (NULL == hl_sockmon_hdl)
   {
      SMON_LOG_MED("Invalid Args");
      e_error = eSOCKMON_RET_INVALID_ARGS;
      goto CLEAN_RETURN;
   }

   px_sockmon_ctxt = (SOCKMON_CTXT_X *) hl_sockmon_hdl;

   e_error = sockmon_delete_resources (px_sockmon_ctxt);
   if (eSOCKMON_RET_SUCCESS != e_error)
   {
      SMON_LOG_MED("sockmon_delete_resources failed: %d", e_error);
   }
   pal_free (px_sockmon_ctxt);
   px_sockmon_ctxt = NULL;
   SMON_LOG_HIGH("Deleted Socket Monitor");
   e_error = eSOCKMON_RET_SUCCESS;
   CLEAN_RETURN: return e_error;
}

SOCKMON_RET_E sockmon_register_sock (
   SOCKMON_HDL hl_sockmon_hdl,
   SOCKMON_REGISTER_DATA_X *px_register_data)
{
   SOCKMON_RET_E e_error = eSOCKMON_RET_FAILURE;
   SOCKMON_CTXT_X *px_sockmon_ctxt = NULL;
   LIST_RET_E e_list_ret = eLIST_RET_FAILURE;
   LIST_NODE_DATA_X x_node_data = {NULL};
   LIST_NODE_DATA_X x_found_node_data = {NULL};
   SOCKMON_NODE_DATA_X *px_node_data = NULL;
   PAL_RET_E e_pal_ret = ePAL_RET_FAILURE;

   if ((NULL == hl_sockmon_hdl) || (NULL == px_register_data))
   {
      SMON_LOG_MED("Invalid Args");
      e_error = eSOCKMON_RET_INVALID_ARGS;
      goto CLEAN_RETURN;
   }

   if ((NULL == px_register_data->hl_sock_hdl)
      || (NULL == px_register_data->fn_active_sock_cbk))
   {
      SMON_LOG_MED("Invalid Args");
      e_error = eSOCKMON_RET_INVALID_ARGS;
      goto CLEAN_RETURN;
   }

   px_sockmon_ctxt = (SOCKMON_CTXT_X *) hl_sockmon_hdl;

   e_pal_ret = pal_mutex_lock(px_sockmon_ctxt->hl_mutex_hdl);
   if (ePAL_RET_SUCCESS != e_pal_ret)
   {
      SMON_LOG_MED("pal_mutex_lock failed: %d", e_pal_ret);
      e_error = eSOCKMON_RET_RESOURCE_FAILURE;
      goto CLEAN_RETURN;
   }

   x_node_data.p_data = px_register_data;
   x_node_data.ui_data_size = sizeof(*px_register_data);
   e_list_ret = list_search_node (px_sockmon_ctxt->hl_list_hdl, &x_node_data,
      sockmon_search_node_cbk, px_sockmon_ctxt, &x_found_node_data);
   if (eLIST_RET_LIST_NODE_FOUND == e_list_ret)
   {
      if ((NULL == x_found_node_data.p_data)
         || (0 == x_found_node_data.ui_data_size))
      {
         e_error = sockmon_register_new_sock (px_sockmon_ctxt,
            px_register_data);
         if (eSOCKMON_RET_SUCCESS != e_error)
         {
            SMON_LOG_MED("sockmon_register_new_sock failed: %d", e_error);
         }
      }
      else
      {
         px_node_data = (SOCKMON_NODE_DATA_X *) x_found_node_data.p_data;
         if (false == px_node_data->b_is_active)
         {
            SMON_LOG_HIGH("Making the sock: %p again part of select list.",
               px_node_data->x_register_data.hl_sock_hdl);
            px_node_data->b_is_active = true;
         }
         else
         {
            SMON_LOG_MED("Node already in the list and active.");
         }
      }
   }
   else
   {
      e_error = sockmon_register_new_sock (px_sockmon_ctxt, px_register_data);
      if (eSOCKMON_RET_SUCCESS != e_error)
      {
         SMON_LOG_MED("sockmon_register_new_sock failed: %d", e_error);
      }
   }

   e_error = sockmon_wakeup_sockmon_task (px_sockmon_ctxt);
   if (eSOCKMON_RET_SUCCESS != e_error)
   {
      SMON_LOG_MED("sockmon_wakeup_sockmon_task failed: %d", e_error);
   }
   else
   {
      SMON_LOG_HIGH("sockmon_wakeup_sockmon_task success");
   }

   e_pal_ret = pal_mutex_unlock(px_sockmon_ctxt->hl_mutex_hdl);
   if (ePAL_RET_SUCCESS != e_pal_ret)
   {
      SMON_LOG_MED("pal_mutex_unlock failed: %d", e_pal_ret);
   }

CLEAN_RETURN:
   return e_error;
}

SOCKMON_RET_E sockmon_deregister_sock (
   SOCKMON_HDL hl_sockmon_hdl,
   SOCKMON_REGISTER_DATA_X *px_register_data)
{
   SOCKMON_RET_E e_error = eSOCKMON_RET_FAILURE;
   SOCKMON_CTXT_X *px_sockmon_ctxt = NULL;
   LIST_RET_E e_list_ret = eLIST_RET_FAILURE;
   LIST_NODE_DATA_X x_node_data = {NULL};
   LIST_NODE_DATA_X x_found_node_data = {NULL};
   SOCKMON_NODE_DATA_X *px_node_data = NULL;
   PAL_RET_E e_pal_ret = ePAL_RET_FAILURE;

   if ((NULL == hl_sockmon_hdl) || (NULL == px_register_data))
   {
      SMON_LOG_MED("Invalid Args");
      e_error = eSOCKMON_RET_INVALID_ARGS;
      goto CLEAN_RETURN;
   }

   if (NULL == px_register_data->hl_sock_hdl)
   {
      SMON_LOG_MED("Invalid Args");
      e_error = eSOCKMON_RET_INVALID_ARGS;
      goto CLEAN_RETURN;
   }

   px_sockmon_ctxt = (SOCKMON_CTXT_X *) hl_sockmon_hdl;

   e_pal_ret = pal_mutex_lock(px_sockmon_ctxt->hl_mutex_hdl);
   if (ePAL_RET_SUCCESS != e_pal_ret)
   {
      SMON_LOG_MED("pal_mutex_lock failed: %d", e_pal_ret);
      e_error = eSOCKMON_RET_RESOURCE_FAILURE;
      goto CLEAN_RETURN;
   }

   x_node_data.p_data = px_register_data;
   x_node_data.ui_data_size = sizeof(*px_register_data);
#if 0
   e_list_ret = list_search_node (px_sockmon_ctxt->hl_list_hdl, &x_node_data,
      sockmon_search_node_cbk, px_sockmon_ctxt, &x_found_node_data);
#else
   e_list_ret = list_node_delete_after_search (px_sockmon_ctxt->hl_list_hdl,
      &x_node_data, sockmon_search_node_cbk, px_sockmon_ctxt,
      &x_found_node_data);
#endif
   if ((eLIST_RET_SUCCESS == e_list_ret)
      && (NULL != x_found_node_data.p_data)
      && (0 != x_found_node_data.ui_data_size))
   {
      px_node_data = (SOCKMON_NODE_DATA_X *) x_found_node_data.p_data;
#if 0
      px_node_data->b_is_active = false;
#else
      pal_free(px_node_data);
      px_node_data = NULL;
#endif
      e_error = sockmon_wakeup_sockmon_task (px_sockmon_ctxt);
      if (eSOCKMON_RET_SUCCESS != e_error)
      {
         SMON_LOG_MED("sockmon_wakeup_sockmon_task failed: %d", e_error);
      }
      else
      {
         SMON_LOG_HIGH("sockmon_wakeup_sockmon_task success");
      }
   }
   else
   {
      SMON_LOG_MED("list_node_delete_after_search failed: %d, %p, %d",
         e_list_ret, x_found_node_data.p_data, x_found_node_data.ui_data_size);
      e_error = eSOCKMON_RET_RESOURCE_FAILURE;
   }

   e_pal_ret = pal_mutex_unlock(px_sockmon_ctxt->hl_mutex_hdl);
   if (ePAL_RET_SUCCESS != e_pal_ret)
   {
      SMON_LOG_MED("pal_mutex_unlock failed: %d", e_pal_ret);
   }

CLEAN_RETURN:
   return e_error;
}
