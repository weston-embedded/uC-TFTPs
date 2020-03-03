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
* Filename : tftp-s_cfg.h
* Version  : V2.01.00
*********************************************************************************************************
*/

#include  <Source/tftp-s_type.h>


/*
*********************************************************************************************************
*                                   TFTPs ARGUMENT CHECK CONFIGURATION
*
* Note(s) : (1) Configure TFTPs_CFG_ARG_CHK_EXT_EN to enable/disable the TFTP server external argument
*               check feature :
*
*               (a) When ENABLED,  ALL arguments received from any port interface provided by the developer
*                   are checked/validated.
*
*               (b) When DISABLED, NO  arguments received from any port interface provided by the developer
*                   are checked/validated.
*********************************************************************************************************
*/

#define  TFTPs_CFG_ARG_CHK_EXT_EN                 DEF_ENABLED   /* See Note #1.                                         */


/*
*********************************************************************************************************
*                                      TFTPs TRACE CONFIGURATION
*********************************************************************************************************
*/

#define  TFTPs_TRACE_HIST_SIZE                            16    /* Trace history size.  Minimum value is 16.            */


/*
*********************************************************************************************************
*********************************************************************************************************
*                                      RUN-TIME CONFIGURATION
*********************************************************************************************************
*********************************************************************************************************
*/

extern  const  TFTPs_TASK_CFG  TFTPs_TaskCfg;
extern  const  TFTPs_CFG       TFTPs_Cfg;


/*
*********************************************************************************************************
*********************************************************************************************************
*                                                TRACING
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  TRACE_LEVEL_OFF
#define  TRACE_LEVEL_OFF                                   0
#endif

#ifndef  TRACE_LEVEL_INFO
#define  TRACE_LEVEL_INFO                                  1
#endif

#ifndef  TRACE_LEVEL_DBG
#define  TRACE_LEVEL_DBG                                   2
#endif

#define  TFTPs_TRACE_LEVEL                      TRACE_LEVEL_DBG
#define  TFTPs_TRACE                            printf
