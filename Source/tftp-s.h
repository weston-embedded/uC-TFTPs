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
*                                             TFTP SERVER
*
* Filename : tftp-s.h
* Version  : V2.01.00
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                               MODULE
*
* Note(s) : (1) This header file is protected from multiple pre-processor inclusion through use of the
*               TFTPs present pre-processor macro definition.
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  TFTPs_MODULE_PRESENT                                   /* See Note #1.                                         */
#define  TFTPs_MODULE_PRESENT


/*
*********************************************************************************************************
*********************************************************************************************************
*                                        TFTPs VERSION NUMBER
*
* Note(s) : (1) (a) The TFTPs module software version is denoted as follows :
*
*                       Vx.yy.zz
*
*                           where
*                                   V               denotes 'Version' label
*                                   x               denotes     major software version revision number
*                                   yy              denotes     minor software version revision number
*                                   zz              denotes sub-minor software version revision number
*
*               (b) The TFTPs software version label #define is formatted as follows :
*
*                       ver = x.yyzz * 100 * 100
*
*                           where
*                                   ver             denotes software version number scaled as an integer value
*                                   x.yyzz          denotes software version number, where the unscaled integer
*                                                       portion denotes the major version number & the unscaled
*                                                       fractional portion denotes the (concatenated) minor
*                                                       version numbers
*********************************************************************************************************
*********************************************************************************************************
*/

#define  TFTPs_VERSION                                 20100u   /* See Note #1.                                         */


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               EXTERNS
*********************************************************************************************************
*********************************************************************************************************
*/

#ifdef   TFTPs_MODULE
#define  TFTPs_EXT
#else
#define  TFTPs_EXT  extern
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*
* Note(s) : (1) The TFTPs module files are located in the following directories :
*
*               (a) \<Your Product Application>\tftp-s_cfg.h
*
*               (b) \<Network Protocol Suite>\Source\net_*.*
*
*               (c) (1) \<TFTPs>\Source\tftp-s.h
*                                      \tftp-s.c
*
*           (2) CPU-configuration software files are located in the following directories :
*
*               (a) \<CPU-Compiler Directory>\cpu_*.*
*               (b) \<CPU-Compiler Directory>\<cpu>\<compiler>\cpu*.*
*
*                       where
*                               <CPU-Compiler Directory>        directory path for common CPU-compiler software
*                               <cpu>                           directory name for specific processor (CPU)
*                               <compiler>                      directory name for specific compiler
*
*           (3) NO compiler-supplied standard library functions SHOULD be used.
*
*               (a) Standard library functions are implemented in the custom library module(s) :
*
*                       \<Custom Library Directory>\lib_*.*
*
*                           where
*                                   <Custom Library Directory>      directory path for custom library software
*
*               (b) #### The reference to standard library header files SHOULD be removed once all custom
*                   library functions are implemented WITHOUT reference to ANY standard library function(s).
*
*           (4) Compiler MUST be configured to include as additional include path directories :
*
*               (a) '\<Your Product Application>\' directory                            See Note #1a
*
*               (b) '\<Network Protocol Suite>\'   directory                            See Note #1b
*
*               (c) '\<TFTPs>\' directories                                             See Note #1c
*
*               (d) (1) '\<CPU-Compiler Directory>\'                  directory         See Note #2a
*                   (2) '\<CPU-Compiler Directory>\<cpu>\<compiler>\' directory         See Note #2b
*
*               (e) '\<Custom Library Directory>\' directory                            See Note #3a
*********************************************************************************************************
*********************************************************************************************************
*/

#include  <cpu.h>                                               /* CPU Configuration              (see Note #2b)        */
#include  <cpu_core.h>                                          /* CPU Core Library               (see Note #2a)        */

#include  <lib_def.h>                                           /* Standard        Defines        (see Note #3a)        */
#include  <lib_str.h>                                           /* Standard String Library        (see Note #3a)        */

#include  <tftp-s_cfg.h>                                        /* TFTP Server Configuration File (see Note #1a)        */
#include  <FS/net_fs.h>                                         /* File System Interface          (see Note #1b)        */

#include  <Source/net_sock.h>

#include  "tftp-s_type.h"

#if 1                                                           /* See Note #3b.                                        */
#include  <stdio.h>
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                         ERROR CODE DATA TYPE
*********************************************************************************************************
*/

typedef  enum tftps_err {
    TFTPs_ERR_NONE,
    TFTPs_ERR_NULL_PTR,
    TFTPs_ERR_CFG_INVALID_SOCK_FAMILY,
    TFTPs_ERR_INIT_TASK_INVALID_ARG,
    TFTPs_ERR_INIT_TASK_MEM_ALLOC,
    TFTPs_ERR_INIT_TASK_CREATE,
    TFTPs_ERR_RD_REQ,
    TFTPs_ERR_WR_REQ,
    TFTPs_ERR_DATA,
    TFTPs_ERR_ACK,
    TFTPs_ERR_ERR,
    TFTPs_ERR_INVALID_STATE,
    TFTPs_ERR_FILE_NOT_FOUND,
    TFTPs_ERR_TX,
    TFTPs_ERR_FILE_RD,
    TFTPs_ERR_TIMED_OUT,
    TFTPs_ERR_NO_SOCK,                                          /* No socket available.                                 */
    TFTPs_ERR_CANT_BIND,                                        /* Could not bind to the TFTPs port.                    */
    TFTPs_ERR_INVALID_FAMILY,                                   /* Invalid Socket Family.                               */
    TFTPs_ERR_INVALID_ADDR                                      /* Invalid Socket Address.                              */
} TFTPs_ERR;


/*
*********************************************************************************************************
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

CPU_BOOLEAN  TFTPs_Init       (const TFTPs_CFG             *p_cfg,
                               const TFTPs_TASK_CFG        *p_task_cfg,
                                     TFTPs_ERR             *p_err);

void         TFTPs_En         (void);

void         TFTPs_Dis        (void);

#if (TFTPs_TRACE_LEVEL >= TRACE_LEVEL_INFO)
void         TFTPs_Disp       (void);

void         TFTPs_DispTrace  (void);
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                               TRACING
*********************************************************************************************************
*********************************************************************************************************
*/

                                                                /* Trace level, default to TRACE_LEVEL_OFF              */
#ifndef  TRACE_LEVEL_OFF
#define  TRACE_LEVEL_OFF                                 0
#endif

#ifndef  TRACE_LEVEL_INFO
#define  TRACE_LEVEL_INFO                                1
#endif

#ifndef  TRACE_LEVEL_DBG
#define  TRACE_LEVEL_DBG                                 2
#endif

#ifndef  TFTPs_TRACE_LEVEL
#define  TFTPs_TRACE_LEVEL                      TRACE_LEVEL_OFF
#endif

#ifndef  TFTPs_TRACE
#define  TFTPs_TRACE                            printf
#endif

#if    ((defined(TFTPs_TRACE))       && \
        (defined(TFTPs_TRACE_LEVEL)) && \
        (TFTPs_TRACE_LEVEL >= TRACE_LEVEL_INFO) )

    #if  (TFTPs_TRACE_LEVEL >= TRACE_LEVEL_DBG)
        #define  TFTPs_TRACE_DBG(msg)     TFTPs_TRACE  msg
    #else
        #define  TFTPs_TRACE_DBG(msg)
    #endif

    #define  TFTPs_TRACE_INFO(msg)        TFTPs_TRACE  msg

#else
    #define  TFTPs_TRACE_DBG(msg)
    #define  TFTPs_TRACE_INFO(msg)
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                        CONFIGURATION ERRORS
*********************************************************************************************************
*********************************************************************************************************
*/

#ifndef  TFTPs_CFG_ARG_CHK_EXT_EN
    #error  "TFTPs_CFG_ARG_CHK_EXT_EN                 not #define'd in 'tftp-s_cfg.h'"
    #error  "                             [MUST be  DEF_DISABLED]                    "
    #error  "                             [     ||  DEF_ENABLED ]                    "
#elif  ((TFTPs_CFG_ARG_CHK_EXT_EN != DEF_ENABLED ) && \
        (TFTPs_CFG_ARG_CHK_EXT_EN != DEF_DISABLED))
    #error  "TFTPs_CFG_ARG_CHK_EXT_EN           illegally #define'd in 'tftp-s_cfg.h'"
    #error  "                             [MUST be  DEF_DISABLED]                    "
    #error  "                             [     ||  DEF_ENABLED ]                    "
#endif


#if     (TFTPs_TRACE_LEVEL >= TRACE_LEVEL_INFO)

#ifndef  TFTPs_TRACE_HIST_SIZE
#error  "TFTPs_TRACE_HIST_SIZE              not #define'd in 'tftp-s_cfg.h'"
#error  "                             see template file in package         "
#error  "                             named 'tftp-s_cfg.h'                 "

#elif   (TFTPs_TRACE_HIST_SIZE < 16)
#error  "TFTPs_TRACE_HIST_SIZE        illegally #define'd in 'tftp-s_cfg.h'"
#error  "                             [MUST be  >= 16]                     "
#endif

#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                             MODULE END
*********************************************************************************************************
*********************************************************************************************************
*/

#endif  /* TFTPs_MODULE_PRESENT  */
