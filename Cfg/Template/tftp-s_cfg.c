/*
*********************************************************************************************************
*                                              uC/TFTPs
*                               Trivial File Transfer Protocol (server)
*
*                    Copyright 2004-2020 Silicon Laboratories Inc. www.silabs.com
*
*                                 SPDX-License-Identifier: APACHE-2.0
*
*               This software is subject to an open source license and is distributed by
*                Silicon Laboratories Inc. pursuant to the terms of the Apache License,
*                    Version 2.0 available at www.apache.org/licenses/LICENSE-2.0.
*
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                                   TFTP SERVER CONFIGURATION FILE
*
*                                              TEMPLATE
*
* Filename : tftp-s_cfg.c
* Version  : V2.01.00
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    TFTPs_CFG_MODULE

/*
*********************************************************************************************************
*********************************************************************************************************
*                                             INCLUDE FILES
*
* Note(s) : (1) All values that are used in this file and are defined in other header files should be
*               included in this file. Some values could be located in the same file such as task priority
*               and stack size. This template file assume that the following values are defined in app_cfg.h:
*
*                   TFTPs_OS_CFG_TASK_PRIO
*                   TFTPs_OS_CFG_TASK_STK_SIZE
*
*********************************************************************************************************
*********************************************************************************************************
*/

#include  <app_cfg.h>                                           /* See Note #1.                                         */
#include  <lib_def.h>

#include  "tftp-s_cfg.h"


/*
*********************************************************************************************************
*********************************************************************************************************
*                              TFTP SERVER SUITE CONFIGURATION OBJECT
*********************************************************************************************************
*********************************************************************************************************
*/

const  TFTPs_CFG  TFTPs_Cfg = {
/*
*--------------------------------------------------------------------------------------------------------
*                                    TFTP SERVER CONFIGURATION
*--------------------------------------------------------------------------------------------------------
*/

                                                                /* Configure socket type:                               */
        TFTPs_SOCK_SEL_IPv4,
                                                                /* TFTPs_SOCK_SEL_IPv4       Accept Only IPv4.          */
                                                                /* TFTPs_SOCK_SEL_IPv6       Accept Only IPv6.          */

                                                                /* TFTP server IP port.  Default is 69.                 */
        69,

/*
*--------------------------------------------------------------------------------------------------------
*                                     TIMEOUT CONFIGURATION
*--------------------------------------------------------------------------------------------------------
*/
                                                                /* Maximum inactivity time (ms) on RX.                  */
        5000,

                                                                /* Maximum inactivity time (ms) on TX.                  */
        5000,
};


/*
*********************************************************************************************************
*********************************************************************************************************
*                               TFTP SERVER TASK CONFIGURATION OBJECT
*
* Note(s): (1) We recommend to configure the Network Protocol Stack task priorities & TFTP server task
*              priority as follows:
*
*                   NET_OS_CFG_IF_TX_DEALLOC_TASK_PRIO (Highest)
*
*                   TFTPs_OS_CFG_TASK_PRIO             (  ...  )
*
*                   NET_OS_CFG_TMR_TASK_PRIO           (  ...  )
*
*                   NET_OS_CFG_IF_RX_TASK_PRIO         (Lowest )
*
*              We recommend that the uC/TCP-IP Timer task and network interface Receive task be lower
*              priority than almost all other application tasks; but we recommend that the network
*              interface Transmit De-allocation task be higher priority than all application tasks that
*              use uC/TCP-IP network services.
*
*              However better performance can be observed when the TFTP server is set with the lowest
*              priority. So some experimentation could be required to identify the better task priority
*              configuration.
*
*          (2) TODO note on the TFTP Server stack's task size.
*
*          (3) When the Stack pointer is defined as null pointer (DEF_NULL), the task's stack should be
*              automatically allowed on the heap of uC/LIB.
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  TFTPs_OS_CFG_TASK_PRIO
#define  TFTPs_OS_CFG_TASK_PRIO                  20
#endif

#ifndef  TFTPs_OS_CFG_TASK_STK_SIZE
#define  TFTPs_OS_CFG_TASK_STK_SIZE             512
#endif

const  TFTPs_TASK_CFG  TFTPs_TaskCfg = {
        TFTPs_OS_CFG_TASK_PRIO,                                 /* TFTPs task priority              (See Note #1).      */
        TFTPs_OS_CFG_TASK_STK_SIZE,                             /* TFTPs task stack size in bytes   (See Note #2).      */
        DEF_NULL,                                               /* TFTPs task stack pointer         (See Note #3).      */
};

