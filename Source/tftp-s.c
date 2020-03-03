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
* Filename : tftp-s.c
* Version  : V2.01.00
*********************************************************************************************************
* Note(s)  : (1) This is an full implementation of the server side of the TFTP protocol, as
*                described in RFC #1350.
*
*            (2) This server is a 'single-user' one, meaning that while a transaction is in progress,
*                other transactions are held off by returning an error condition indicating that
*                the server is busy.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*********************************************************************************************************
*                                            INCLUDE FILES
*********************************************************************************************************
*********************************************************************************************************
*/

#define    MICRIUM_SOURCE
#define    TFTPs_MODULE
#include  "tftp-s.h"
#include  <Source/net_cfg_net.h>

#ifdef  NET_IPv4_MODULE_EN
#include  <IP/IPv4/net_ipv4.h>
#endif
#ifdef  NET_IPv6_MODULE_EN
#include  <IP/IPv6/net_ipv6.h>
#endif

#include  <Source/net_tmr.h>
#include  <Source/net_util.h>
#include  <Source/net_app.h>
#include  <Source/net_ascii.h>

#include  <KAL/kal.h>


/*
*********************************************************************************************************
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*********************************************************************************************************
*/

#define  TFTPs_TASK_NAME                        "TFTPs Task"            /* Task Name.               */

/*
*********************************************************************************************************
*                                      TFTPs SPECIFIC CONSTANTS
*********************************************************************************************************
*/

#define  TFTPs_FILE_OPEN_RD                                0
#define  TFTPs_FILE_OPEN_WR                                1


/*
*********************************************************************************************************
*                                        TFTP SPECIFIC DEFINES
*********************************************************************************************************
*/

#define  TFTP_PKT_OFFSET_OPCODE                            0
#define  TFTP_PKT_OFFSET_FILENAME                          2
#define  TFTP_PKT_OFFSET_BLK_NBR                           2
#define  TFTP_PKT_OFFSET_ERR_CODE                          2
#define  TFTP_PKT_OFFSET_ERR_MSG                           4
#define  TFTP_PKT_OFFSET_DATA                              4

#define  TFTP_PKT_SIZE_OPCODE                              2
#define  TFTP_PKT_SIZE_BLK_NBR                             2
#define  TFTP_PKT_SIZE_ERR_CODE                            2
#define  TFTP_PKT_SIZE_FILENAME_NUL                        1
#define  TFTP_PKT_SIZE_MODE_NUL                            1

                                                                /* ---- TFTP opcodes (see Stevens p. 466) ------------- */
#define  TFTP_OPCODE_RD_REQ                                1    /* Read                                                 */
#define  TFTP_OPCODE_WR_REQ                                2    /* Write                                                */
#define  TFTP_OPCODE_DATA                                  3    /* Data                                                 */
#define  TFTP_OPCODE_ACK                                   4    /* Acknowledge                                          */
#define  TFTP_OPCODE_ERR                                   5    /* Error                                                */


/*
*********************************************************************************************************
*                                       TFTPs SPECIFIC DEFINES
*********************************************************************************************************
*/

                                                                /* ---- TFTP Server error codes: (see Stevens p. 467) - */
#define  TFTPs_ERR_CODE_ERR_STR                            0    /* Not defined.                                         */
#define  TFTPs_ERR_CODE_FILE_NOT_FOUND                     1    /* File not found.                                      */
#define  TFTPs_ERR_CODE_ACCESS_VIOLATION                   2    /* Access violation.                                    */
#define  TFTPs_ERR_CODE_DISK_FULL                          3    /* Disk full.                                           */
#define  TFTPs_ERR_CODE_ILLEGAL_OP                         4    /* Illegal TFTP operation.                              */
#define  TFTPs_ERR_CODE_BAD_PORT_NBR                       5    /* Unknown port number.                                 */
#define  TFTPs_ERR_CODE_FILE_EXISTS                        6    /* File already exists.                                 */
#define  TFTPs_ERR_CODE_NO_SUCH_USER                       7    /* No such user.                                        */

                                                                /* ---- TFTP Server modes ----------------------------- */
#define  TFTPs_MODE_OCTET                                  1
#define  TFTPs_MODE_NETASCII                               2

                                                                /* ---- TFTP Server states ---------------------------- */
#define  TFTPs_STATE_IDLE                                  0
#define  TFTPs_STATE_DATA_RD                               1
#define  TFTPs_STATE_DATA_WR                               2

#define  TFTPs_BLOCK_SIZE                                512
#define  TFTPs_BUF_SIZE                         (TFTPs_BLOCK_SIZE + TFTP_PKT_SIZE_OPCODE + TFTP_PKT_SIZE_BLK_NBR)


/*
*********************************************************************************************************
*                                         TFTPs TRACE DEFINE
*********************************************************************************************************
*/

#define  TFTPs_TRACE_STR_SIZE                             80    /* Trace string size.                                   */


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          LOCAL DATA TYPES
*********************************************************************************************************
*********************************************************************************************************
*/

#if (TFTPs_TRACE_LEVEL >= TRACE_LEVEL_INFO)
typedef  struct {
    CPU_INT16U  Id;                                             /* Event ID.                                            */
    CPU_INT32U  TS;                                             /* Time Stamp.                                          */
    CPU_INT08U  State;                                          /* Current TFTPs state.                                 */
    CPU_CHAR    Str[TFTPs_TRACE_STR_SIZE + 1];                  /* ASCII string for comment.                            */
    CPU_INT16U  RxBlkNbr;                                       /* Current Rx Block Number.                             */
    CPU_INT16U  TxBlkNbr;                                       /* Current Tx Block Number.                             */
} TFTPs_TRACE_STRUCT;
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*********************************************************************************************************
*/

TFTPs_CFG         *TFTPs_CfgPtr;

CPU_INT16U         TFTPs_RxBlkNbr;                              /* Current block number received.                       */
CPU_INT08U         TFTPs_RxMsgBuf[TFTPs_BUF_SIZE];              /* Incoming packet buffer.                              */
CPU_INT32U         TFTPs_RxMsgCtr;                              /* Number of messages received.                         */
CPU_INT32S         TFTPs_RxMsgLen;

CPU_INT16U         TFTPs_TxBlkNbr;                              /* Current block number being sent.                     */
CPU_INT08U         TFTPs_TxMsgBuf[TFTPs_BUF_SIZE];              /* Outgoing packet buffer.                              */
CPU_INT16U         TFTPs_TxMsgCtr;
CPU_SIZE_T         TFTPs_TxMsgLen;

NET_SOCK_ADDR      TFTPs_SockAddr;
NET_SOCK_ADDR_LEN  TFTPs_SockAddrLen;
NET_SOCK_ID        TFTPs_SockID;

CPU_INT08U         TFTPs_State;                                 /* Current state of TFTPs state machine.                */

CPU_INT16U         TFTPs_OpCode;

void              *TFTPs_FileHandle;                            /* File handle of currently opened file.                */

CPU_BOOLEAN        TFTPs_ServerEn;


#if (TFTPs_TRACE_LEVEL >= TRACE_LEVEL_INFO)
CPU_CHAR           TFTPs_DispTbl[TFTPs_TRACE_HIST_SIZE + 2][TFTPs_TRACE_STR_SIZE];
#endif


#if (TFTPs_TRACE_LEVEL >= TRACE_LEVEL_INFO)
static  TFTPs_TRACE_STRUCT  TFTPs_TraceTbl[TFTPs_TRACE_HIST_SIZE];
static  CPU_INT16U          TFTPs_TraceIx;
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                          LOCAL PROTOTYPES
*********************************************************************************************************
*********************************************************************************************************
*/

static  void                TFTPs_TaskInit      (TFTPs_TASK_CFG  *p_task_cfg,
                                                 TFTPs_ERR       *p_err);

static  void                TFTPs_Task          (void            *p_data);

static  TFTPs_ERR           TFTPs_ServerSockInit(NET_SOCK_FAMILY  family);


static  TFTPs_ERR           TFTPs_StateIdle     (void);

static  TFTPs_ERR           TFTPs_StateDataRd   (void);

static  TFTPs_ERR           TFTPs_StateDataWr   (void);


static  void                TFTPs_GetRxBlkNbr   (void);

static  void                TFTPs_Terminate     (void);


static  TFTPs_ERR           TFTPs_FileOpen      (CPU_BOOLEAN      rw);

static  void               *TFTPs_FileOpenMode  (CPU_CHAR        *p_filename,
                                                 CPU_BOOLEAN      rw);


static  TFTPs_ERR           TFTPs_DataRd        (void);

static  TFTPs_ERR           TFTPs_DataWr        (void);

static  void                TFTPs_DataWrAck     (CPU_INT32U       blk_nbr);


                                                                /* --------------------- TX FNCTS --------------------- */
static  void                TFTPs_TxErr         (CPU_INT16U       err_code,
                                                 CPU_CHAR        *p_err_msg);

static  NET_SOCK_RTN_CODE   TFTPs_Tx            (CPU_INT16U       opcode,
                                                 CPU_INT16U       blk_nbr,
                                                 CPU_INT08U      *p_buf,
                                                 CPU_INT16U       len);


                                                                /* ------------------- TRACE FNCTS -------------------- */
#if (TFTPs_TRACE_LEVEL >= TRACE_LEVEL_INFO)
static  void                TFTPs_TraceInit     (void);
#endif

static  void                TFTPs_Trace         (CPU_INT16U       id,
                                                 CPU_CHAR        *p_str);


/*
*********************************************************************************************************
*                                            TFTPs_Init()
*
* Description : (1) Initialize & startup the TFTP server :
*
*                   (a) Initialize TFTP server global variables & counters
*                   (b) Initialize TFTP server global OS objects
*
*
* Argument(s) : p_cfg       Pointer to TFTPs Configuration object.
*
*               p_task_cfg  Pointer to TFTPs Task Configuration object.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               TFTPs_ERR_NONE
*                               TFTPs_ERR_CFG_INVALID_SOCK_FAMILY
*
*                               ------------ RETURNED BY TFTPs_TaskInit() ------------
*                               See TFTPs_TaskInit() for additional return error codes.
*
* Return(s)   : DEF_OK,   if NO error.
*               DEF_FAIL, otherwise.
*
* Caller(s)   : Application.
*
*               This function is a TFTP server application interface (API) function & MAY be called by
*               application function(s).
*
* Note(s)     : None.
*********************************************************************************************************
*/

CPU_BOOLEAN  TFTPs_Init (const TFTPs_CFG             *p_cfg,
                         const TFTPs_TASK_CFG        *p_task_cfg,
                               TFTPs_ERR             *p_err)
{
    CPU_BOOLEAN  result;


#if (TFTPs_CFG_ARG_CHK_EXT_EN == DEF_ENABELD)
    if (p_err == DEF_NULL) {
        CPU_SW_EXCEPTION(;);
    }

    if (p_cfg == DEF_NULL) {
        result = DEF_FAIL;
       *p_err  = TFTPs_ERR_NULL_PTR;
        goto exit;
    }

    if (p_task_cfg == DEF_NULL) {
        result = DEF_FAIL;
       *p_err  = TFTPs_ERR_NULL_PTR;
        goto exit;
    }
#endif

                                                                /* -------------- INIT TFTPs GLOBAL VARS -------------- */
    TFTPs_RxBlkNbr   = 0;
    TFTPs_RxMsgCtr   = 0;
    TFTPs_TxBlkNbr   = 0;
    TFTPs_TxMsgCtr   = 0;

    TFTPs_FileHandle = (void *)0;
    TFTPs_State      = TFTPs_STATE_IDLE;
    TFTPs_ServerEn   = DEF_ENABLED;

    switch (p_cfg->SockSel) {
        case TFTPs_SOCK_SEL_IPv4:
#ifndef   NET_IPv4_MODULE_EN
             result = DEF_FAIL;
            *p_err  = TFTPs_ERR_CFG_INVALID_SOCK_FAMILY;
             goto exit;
#else
             break;
#endif  /* NET_IPv4_MODULE_EN */


        case TFTPs_SOCK_SEL_IPv6:
#ifndef   NET_IPv6_MODULE_EN
             result = DEF_FAIL;
            *p_err  = TFTPs_ERR_CFG_INVALID_SOCK_FAMILY;
             goto exit;
#else
             break;
#endif  /* NET_IPv6_MODULE_EN */


        case TFTPs_SOCK_SEL_IPv4_IPv6:
#ifndef   NET_IPv4_MODULE_EN
             result = DEF_FAIL;
            *p_err  = TFTPs_ERR_CFG_INVALID_SOCK_FAMILY;
             goto exit;
#endif  /* NET_IPv4_MODULE_EN */

#ifndef   NET_IPv6_MODULE_EN
             result = DEF_FAIL;
            *p_err  = TFTPs_ERR_CFG_INVALID_SOCK_FAMILY;
             goto exit;
#else
             break;
#endif  /* NET_IPv6_MODULE_EN */


        default:
             result = DEF_FAIL;
            *p_err = TFTPs_ERR_CFG_INVALID_SOCK_FAMILY;
             goto  exit;
    }

    TFTPs_CfgPtr = (TFTPs_CFG *)p_cfg;

                                                                /* ------------- PERFORM TFTPs TASK INIT -------------- */
    TFTPs_TaskInit((TFTPs_TASK_CFG *)p_task_cfg,
                                     p_err);
    if (*p_err != TFTPs_ERR_NONE) {
         result = DEF_FAIL;
         goto exit;
    }

    result = DEF_OK;
   *p_err  = TFTPs_ERR_NONE;


exit:
    return (result);
}


/*
*********************************************************************************************************
*                                             TFTPs_En()
*
* Description : Enable the TFTP server.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a TFTP server application interface (API) function & MAY be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  TFTPs_En (void)
{
    TFTPs_ServerEn = DEF_ENABLED;
    TFTPs_State    = TFTPs_STATE_IDLE;
}


/*
*********************************************************************************************************
*                                             TFTPs_Dis()
*
* Description : Disable the TFTP server.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a TFTP server application interface (API) function & MAY be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

void  TFTPs_Dis (void)
{
    TFTPs_ServerEn = DEF_DISABLED;
    TFTPs_Terminate();
}


/*
*********************************************************************************************************
*                                            TFTPs_Disp()
*
* Description : Output information about the TFTP server module for displaying on an ASCII display.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a TFTP server application interface (API) function & MAY be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (TFTPs_TRACE_LEVEL >= TRACE_LEVEL_INFO)
void  TFTPs_Disp (void)
{
#ifdef  NET_IPv4_MODULE_EN
    NET_SOCK_ADDR_IPv4  *p_addrv4;
#endif
#ifdef  NET_IPv6_MODULE_EN
    NET_SOCK_ADDR_IPv6  *p_addrv6;
#endif
    CPU_CHAR             str[TFTPs_TRACE_STR_SIZE];
    CPU_INT16U           i;
    NET_ERR              err;


                                              /*           1111111111222222222233333333334444444444555555555566666666667777777777 */
                                              /* 01234567890123456789012345678901234567890123456789012345678901234567890123456789 */
    Str_Copy(&TFTPs_DispTbl[ 0][0], (CPU_CHAR *)"------------------------------------ TFTPs ------------------------------------");
    Str_Copy(&TFTPs_DispTbl[ 1][0], (CPU_CHAR *)"State      : xxxxxxxxxx                                                        ");
    Str_Copy(&TFTPs_DispTbl[ 2][0], (CPU_CHAR *)"OpCode     : xxxxxx                                                            ");
    Str_Copy(&TFTPs_DispTbl[ 3][0], (CPU_CHAR *)"                                                                               ");
    Str_Copy(&TFTPs_DispTbl[ 4][0], (CPU_CHAR *)"Rx Msg Ctr : xxxxx                                                             ");
    Str_Copy(&TFTPs_DispTbl[ 5][0], (CPU_CHAR *)"Rx Block # : xxxxx                                                             ");
    Str_Copy(&TFTPs_DispTbl[ 6][0], (CPU_CHAR *)"Rx Msg Len : xxxxx                                                             ");
    Str_Copy(&TFTPs_DispTbl[ 7][0], (CPU_CHAR *)"Rx Msg     : xx xx xx xx xx xx xx xx xx xx xx                                  ");
    Str_Copy(&TFTPs_DispTbl[ 8][0], (CPU_CHAR *)"                                                                               ");
    Str_Copy(&TFTPs_DispTbl[ 9][0], (CPU_CHAR *)"Tx Msg Ctr : xxxxx                                                             ");
    Str_Copy(&TFTPs_DispTbl[10][0], (CPU_CHAR *)"Tx Block # : xxxxx                                                             ");
    Str_Copy(&TFTPs_DispTbl[11][0], (CPU_CHAR *)"Tx Msg Len : xxxxx                                                             ");
    Str_Copy(&TFTPs_DispTbl[12][0], (CPU_CHAR *)"Tx Msg     : xx xx xx xx xx xx xx xx xx xx xx                                  ");
    Str_Copy(&TFTPs_DispTbl[13][0], (CPU_CHAR *)"                                                                               ");
    Str_Copy(&TFTPs_DispTbl[14][0], (CPU_CHAR *)"Source IP  : xx.xx.xx.xx                                                       ");
    Str_Copy(&TFTPs_DispTbl[15][0], (CPU_CHAR *)"Dest   IP  : xx.xx.xx.xx                                                       ");
                                              /*           1111111111222222222233333333334444444444555555555566666666667777777777 */
                                              /* 01234567890123456789012345678901234567890123456789012345678901234567890123456789 */

                                                                /* Display state of TFTPs state machine.                */
    switch (TFTPs_State) {
        case TFTPs_STATE_IDLE:
             Str_Copy(&TFTPs_DispTbl[1][13], (CPU_CHAR *)"IDLE      ");
             break;

        case TFTPs_STATE_DATA_RD:
             Str_Copy(&TFTPs_DispTbl[1][13], (CPU_CHAR *)"DATA READ ");
             break;

        case TFTPs_STATE_DATA_WR:
             Str_Copy(&TFTPs_DispTbl[1][13], (CPU_CHAR *)"DATA WRITE");
             break;
    };

                                                                /* Display Op-Code received.                            */
    switch (TFTPs_OpCode) {
        case 0:
             Str_Copy(&TFTPs_DispTbl[2][13], (CPU_CHAR *)"-NONE-");
             break;

        case TFTP_OPCODE_RD_REQ:
             Str_Copy(&TFTPs_DispTbl[2][13], (CPU_CHAR *)"RD REQ");
             break;

        case TFTP_OPCODE_WR_REQ:
             Str_Copy(&TFTPs_DispTbl[2][13], (CPU_CHAR *)"WR REQ");
             break;

        case TFTP_OPCODE_DATA:
             Str_Copy(&TFTPs_DispTbl[2][13], (CPU_CHAR *)"DATA  ");
             break;

        case TFTP_OPCODE_ACK:
             Str_Copy(&TFTPs_DispTbl[2][13], (CPU_CHAR *)"ACK   ");
             break;

        case TFTP_OPCODE_ERR:
             Str_Copy(&TFTPs_DispTbl[2][13], (CPU_CHAR *)"ERR   ");
             break;
    };


                                                                /* Display number of messages received.                 */
    Str_FmtPrint((char *)str, TFTPs_TRACE_STR_SIZE, "%5u", (unsigned int)TFTPs_RxMsgCtr);
    Str_Copy(&TFTPs_DispTbl[4][13], str);

                                                                /* Display current block number.                        */
    Str_FmtPrint((char *)str, TFTPs_TRACE_STR_SIZE, "%5u", (unsigned int)TFTPs_RxBlkNbr);
    Str_Copy(&TFTPs_DispTbl[5][13], str);

                                                                /* Display received message length.                     */
    Str_FmtPrint((char *)str, TFTPs_TRACE_STR_SIZE, "%5d", (  signed int)TFTPs_RxMsgLen);
    Str_Copy(&TFTPs_DispTbl[6][13], str);

    Str_FmtPrint((char *)str, TFTPs_TRACE_STR_SIZE, "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                            TFTPs_RxMsgBuf[0],
                            TFTPs_RxMsgBuf[1],
                            TFTPs_RxMsgBuf[2],
                            TFTPs_RxMsgBuf[3],
                            TFTPs_RxMsgBuf[4],
                            TFTPs_RxMsgBuf[5],
                            TFTPs_RxMsgBuf[6],
                            TFTPs_RxMsgBuf[7],
                            TFTPs_RxMsgBuf[8],
                            TFTPs_RxMsgBuf[9]);
    Str_Copy(&TFTPs_DispTbl[7][13], str);

                                                                /* Display number of messages sent.                     */
    Str_FmtPrint((char *)str, TFTPs_TRACE_STR_SIZE, "%5u", (unsigned int)TFTPs_TxMsgCtr);
    Str_Copy(&TFTPs_DispTbl[ 9][13], str);

                                                                /* Display current block number.                        */
    Str_FmtPrint((char *)str, TFTPs_TRACE_STR_SIZE, "%5u", (unsigned int)TFTPs_TxBlkNbr);
    Str_Copy(&TFTPs_DispTbl[10][13], str);

                                                                /* Display sent message length.                         */
    Str_FmtPrint((char *)str, TFTPs_TRACE_STR_SIZE, "%5u", (unsigned int)TFTPs_TxMsgLen);
    Str_Copy(&TFTPs_DispTbl[11][13], str);

    Str_FmtPrint((char *)str, TFTPs_TRACE_STR_SIZE, "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                 TFTPs_TxMsgBuf[0],
                 TFTPs_TxMsgBuf[1],
                 TFTPs_TxMsgBuf[2],
                 TFTPs_TxMsgBuf[3],
                 TFTPs_TxMsgBuf[4],
                 TFTPs_TxMsgBuf[5],
                 TFTPs_TxMsgBuf[6],
                 TFTPs_TxMsgBuf[7],
                 TFTPs_TxMsgBuf[8],
                 TFTPs_TxMsgBuf[9]);
    Str_Copy(&TFTPs_DispTbl[12][13], str);

    switch (TFTPs_SockAddr.AddrFamily) {
#ifdef  NET_IPv4_MODULE_EN
        case NET_SOCK_ADDR_FAMILY_IP_V4:
             p_addrv4 = (NET_SOCK_ADDR_IPv4 *)&TFTPs_SockAddr;
             NetASCII_IPv4_to_Str(NET_UTIL_NET_TO_HOST_32(p_addrv4->Addr), str, DEF_NO, &err);
             break;
#endif
#ifdef  NET_IPv6_MODULE_EN
        case NET_SOCK_ADDR_FAMILY_IP_V6:
             p_addrv6 = (NET_SOCK_ADDR_IPv6 *)&TFTPs_SockAddr;
             NetASCII_IPv6_to_Str(&p_addrv6->Addr, str, DEF_NO, DEF_YES, &err);
             break;
#endif

        default:
             str[0] = '\0';
             break;
    }

    Str_Copy(&TFTPs_DispTbl[14][13], str);

    for (i = 0; i < 16; i++) {
        TFTPs_TRACE("%s\r\n", TFTPs_DispTbl[i]);
    }
}
#endif


/*
*********************************************************************************************************
*                                          TFTPs_DispTrace()
*
* Description : Output the TFTP server module traces for displaying on an ASCII display.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : Application.
*
*               This function is a TFTP server application interface (API) function & MAY be called by
*               application function(s).
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (TFTPs_TRACE_LEVEL >= TRACE_LEVEL_INFO)
void  TFTPs_DispTrace (void)
{
    CPU_CHAR     str[TFTPs_TRACE_STR_SIZE];
    CPU_CHAR    *p_str;
    CPU_INT16U   i;


                                         /*           1111111111222222222233333333334444444444555555555566666666667777777777 */
                                         /* 01234567890123456789012345678901234567890123456789012345678901234567890123456789 */
    Str_Copy(TFTPs_DispTbl[0], (CPU_CHAR *)"--------------------------------- TFTPs TRACE ---------------------------------");
    Str_Copy(TFTPs_DispTbl[1], (CPU_CHAR *)" TS     ID     State  Rx#   Tx#                                                ");
                                         /*           1111111111222222222233333333334444444444555555555566666666667777777777 */
                                         /* 01234567890123456789012345678901234567890123456789012345678901234567890123456789 */

    for (i = 2; i < TFTPs_TRACE_HIST_SIZE + 2; i++) {
        p_str = &TFTPs_DispTbl[i][0];

        if (i == TFTPs_TraceIx) {                               /* Indicate current position of trace.                  */
            Str_Copy(p_str, (CPU_CHAR *)">");
        } else {
            Str_Copy(p_str, (CPU_CHAR *)" ");
        }

        Str_FmtPrint((char       *) str,
                                    TFTPs_TRACE_STR_SIZE,
                                   "%5u  %5u",
                     (unsigned int)(TFTPs_TraceTbl[i].TS & 0xFFFF),
                     (unsigned int) TFTPs_TraceTbl[i].Id);
        Str_Cat(p_str, str);

        switch (TFTPs_TraceTbl[i].State) {
            case TFTPs_STATE_IDLE:                              /* Idle state, expecting a new 'connection'.            */
                 Str_Cat(p_str, (CPU_CHAR *)"  IDLE ");
                 break;


            case TFTPs_STATE_DATA_RD:                           /* Processing a read request.                           */
                 Str_Cat(p_str, (CPU_CHAR *)"  RD   ");
                 break;


            case TFTPs_STATE_DATA_WR:                           /* Processing a write request.                          */
                 Str_Cat(p_str, (CPU_CHAR *)"  WR   ");
                 break;


            default:
                 Str_Cat(p_str, (CPU_CHAR *)"  ERROR");
                 break;
        }

        Str_FmtPrint((char       *)str,
                                   TFTPs_TRACE_STR_SIZE,
                                   "  %5u  %5u  ",
                     (unsigned int)TFTPs_TraceTbl[i].RxBlkNbr,
                     (unsigned int)TFTPs_TraceTbl[i].TxBlkNbr);
        Str_Cat(p_str, str);

        Str_Cat(p_str, TFTPs_TraceTbl[i].Str);
    }

    for (i = 0; i < TFTPs_TRACE_HIST_SIZE + 2; i++) {
        TFTPs_TRACE("%s\r\n", TFTPs_DispTbl[i]);
    }
}
#endif


/*
*********************************************************************************************************
*********************************************************************************************************
*                                           LOCAL FUNCTIONS
*********************************************************************************************************
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                           TFTPs_TaskInit()
*
* Description : Initialize the TFTP server task.
*
* Argument(s) : p_task_cfg  Pointer to task configuration object.
*
*               p_err       Pointer to variable that will receive the return error code from this function :
*
*                               TFTPs_ERR_NONE
*                               TFTPs_ERR_INIT_TASK_INVALID_ARG
*                               TFTPs_ERR_INIT_TASK_MEM_ALLOC
*                               TFTPs_ERR_INIT_TASK_CREATE
*
* Return(s)   : None.
*
* Caller(s)   : TFTPs_Init().
*
* Note(s)     : None.
*********************************************************************************************************
*/

static void  TFTPs_TaskInit (TFTPs_TASK_CFG  *p_task_cfg,
                             TFTPs_ERR       *p_err)
{
    KAL_TASK_HANDLE   task_handle;
    KAL_ERR           err_kal;


                                                                /* ------- ALLOCATE MEMORY SPACE FOR TFTPs TASK  ------ */
    task_handle = KAL_TaskAlloc((const  CPU_CHAR *)TFTPs_TASK_NAME,
                                                   p_task_cfg->StkPtr,
                                                   p_task_cfg->StkSizeBytes,
                                                   DEF_NULL,
                                                  &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
        break;

        case KAL_ERR_INVALID_ARG:
            *p_err = TFTPs_ERR_INIT_TASK_INVALID_ARG;
             goto exit;

        case KAL_ERR_MEM_ALLOC:
        default:
            *p_err = TFTPs_ERR_INIT_TASK_MEM_ALLOC;
             goto exit;
    }

                                                                /* ---------------- CREATE TFTPs TASK ----------------- */
    KAL_TaskCreate(task_handle,
                   TFTPs_Task,
                   DEF_NULL,
                   p_task_cfg->Prio,
                   DEF_NULL,
                  &err_kal);
    switch (err_kal) {
        case KAL_ERR_NONE:
             break;

        case KAL_ERR_INVALID_ARG:
        case KAL_ERR_ISR:
        case KAL_ERR_OS:
        default:
            *p_err = TFTPs_ERR_INIT_TASK_CREATE;
             goto exit;
    }

    *p_err = TFTPs_ERR_NONE;


exit:
    return;
}


/*
*********************************************************************************************************
*                                            TFTPs_Task()
*
* Description : TFTP server code loop.
*
* Argument(s) : p_data      Pointer to task initialization data (required by uC/OS-II).
*
* Return(s)   : none.
*
* Caller(s)   : TFTPs_Init().
*
* Note(s)     : (1) TID stands for "transfer identifier" as referenced by RFC #1350.
*
*               (2) #### In case of a timeout error, retransmission of the last sent packet should take
*                   place.  Terminating the transmission might NOT the correct action to take according
*                   to RFC #1350, Section 2 'Overview of the Protocol', which states that "If a packet
*                   gets lost in the network, the intended recipient will timeout and may retransmit
*                   his last packet [...], thus causing the sender of the lost packet to retransmit that
*                   lost packet".
*********************************************************************************************************
*/

static  void  TFTPs_Task (void  *p_data)
{
    TFTPs_CFG             *p_cfg;
#ifdef  NET_IPv4_MODULE_EN
    NET_SOCK_ADDR_IPv4    *p_addr_v4_remote;
    NET_SOCK_ADDR_IPv4    *p_addr_v4_server;
#endif
#ifdef  NET_IPv6_MODULE_EN
    NET_SOCK_ADDR_IPv6    *p_addr_v6_remote;
    NET_SOCK_ADDR_IPv6    *p_addr_v6_server;
#endif
    NET_SOCK_FAMILY        sock_family;
    CPU_INT16U            *p_opcode;
    CPU_BOOLEAN            same_addr;
    CPU_BOOLEAN            valid_tid;                             /* See Note #1.                                         */
    TFTPs_ERR              tftp_err;
    NET_ERR                net_err;
    NET_SOCK_ADDR          addr_ip_remote;
    NET_SOCK_ADDR          addr_ip_server;


    p_cfg    = TFTPs_CfgPtr;

    switch (p_cfg->SockSel) {
        case TFTPs_SOCK_SEL_IPv4:
             sock_family = NET_SOCK_FAMILY_IP_V4;
             break;

        case TFTPs_SOCK_SEL_IPv6:
             sock_family = NET_SOCK_FAMILY_IP_V6;
             break;

        case TFTPs_SOCK_SEL_IPv4_IPv6:
        default:
            TFTPs_Trace((CPU_INT16U)0,
                        (CPU_CHAR *)"Init error, Socket IP family");
            while (DEF_ON) {
                ;
            }
    }

                                                                /* ----------------- INIT SERVER SOCK ----------------- */
    tftp_err = TFTPs_ServerSockInit(sock_family);
    if (tftp_err != TFTPs_ERR_NONE) {                           /* If sock err, do NOT enter server loop.               */
        TFTPs_Trace((CPU_INT16U)0,
                    (CPU_CHAR *)"Init error, server NOT started");
        while (DEF_ON) {
            ;
        }
    }

                                                                /* Set sock to blocking until incoming req.             */
    NetSock_CfgTimeoutRxQ_Set((NET_SOCK_ID) TFTPs_SockID,
                              (CPU_INT32U ) NET_TMR_TIME_INFINITE,
                              (NET_ERR   *)&net_err);


                                                                /* ----------------- TFTP SERVER LOOP ----------------- */
    while (DEF_ON) {
        TFTPs_SockAddrLen = sizeof(addr_ip_remote);

                                                                /* --------------- WAIT FOR INCOMING PKT -------------- */

        TFTPs_RxMsgLen = NetSock_RxDataFrom((NET_SOCK_ID        ) TFTPs_SockID,
                                            (void              *)&TFTPs_RxMsgBuf[0],
                                            (CPU_INT16U         ) sizeof(TFTPs_RxMsgBuf),
                                            (CPU_INT16S         ) NET_SOCK_FLAG_NONE,
                                            (NET_SOCK_ADDR     *)&addr_ip_remote,
                                            (NET_SOCK_ADDR_LEN *)&TFTPs_SockAddrLen,
                                            (void              *) 0,
                                            (CPU_INT08U         ) 0,
                                            (CPU_INT08U        *) 0,
                                            (NET_ERR           *)&net_err);

        if (TFTPs_RxMsgLen == NET_SOCK_BSD_ERR_RX) {            /* If an error occurred, ...                            */
            TFTPs_Terminate();                                  /* ... terminate the current file tx (see Note #2).     */
            continue;
        }

        TFTPs_RxMsgCtr++;                                       /* Inc nbr or rx'd pkts.                                */

        if (TFTPs_ServerEn != DEF_ENABLED) {
            TFTPs_TxErr((CPU_INT16U)0,
                        (CPU_CHAR *)"Transaction denied, Server DISABLED");
            continue;
        }


                                                                /* --------------- PROCESS INCOMING PKT --------------- */
        valid_tid = DEF_YES;

        switch (addr_ip_remote.AddrFamily) {
#ifdef  NET_IPv4_MODULE_EN
            case NET_SOCK_ADDR_FAMILY_IP_V4:
                 p_addr_v4_remote = (NET_SOCK_ADDR_IPv4 *)&addr_ip_remote;
                 p_addr_v4_server = (NET_SOCK_ADDR_IPv4 *)&TFTPs_SockAddr;

                 if ((p_addr_v4_remote->Port != p_addr_v4_server->Port) ||
                     (p_addr_v4_remote->Addr != p_addr_v4_server->Addr)) {
                    valid_tid = DEF_NO;
                 }
                 (void)&same_addr;
                 break;
#endif
#ifdef  NET_IPv6_MODULE_EN
            case NET_SOCK_ADDR_FAMILY_IP_V6:
                 p_addr_v6_remote = (NET_SOCK_ADDR_IPv6 *)&addr_ip_remote;
                 p_addr_v6_server = (NET_SOCK_ADDR_IPv6 *)&TFTPs_SockAddr;

                 same_addr = Mem_Cmp(&p_addr_v6_remote->Addr.Addr,
                                     &p_addr_v6_server->Addr.Addr,
                                      NET_IPv6_ADDR_SIZE);

                 if (same_addr == DEF_NO) {
                     valid_tid = DEF_NO;
                 }
                 break;
#endif

            default:
                valid_tid = DEF_NO;
                break;
        }
#if 0
        valid_tid    = ((addr_ip_remote.Port == TFTPs_SockAddr.Port) &&
                        (addr_ip_remote.Addr == TFTPs_SockAddr.Addr)) ? DEF_YES : DEF_NO;
#endif
        p_opcode     = (CPU_INT16U *)&TFTPs_RxMsgBuf[TFTP_PKT_OFFSET_OPCODE];
        TFTPs_OpCode =  NET_UTIL_NET_TO_HOST_16(*p_opcode);
        switch (TFTPs_State) {
            case TFTPs_STATE_IDLE:                              /* Idle state, expecting a new req.                     */
                 TFTPs_SockAddr = addr_ip_remote;
                 tftp_err       = TFTPs_StateIdle();
                 break;


            case TFTPs_STATE_DATA_RD:                           /* Processing a rd req.                                 */
                 if (valid_tid == DEF_YES) {
                     tftp_err       = TFTPs_StateDataRd();
                 } else {
                     addr_ip_server = TFTPs_SockAddr;
                     TFTPs_SockAddr = addr_ip_remote;
                     TFTPs_TxErr((CPU_INT16U)0,
                                 (CPU_CHAR *)"Transaction denied, Server BUSY");
                     TFTPs_SockAddr = addr_ip_server;
                     tftp_err       = TFTPs_ERR_NONE;
                 }
                 break;


            case TFTPs_STATE_DATA_WR:                           /* Processing a wr req.                                 */
                 if (valid_tid == DEF_YES) {
                     tftp_err       = TFTPs_StateDataWr();
                 } else {
                     addr_ip_server = TFTPs_SockAddr;
                     TFTPs_SockAddr = addr_ip_remote;
                     TFTPs_TxErr((CPU_INT16U)0,
                                 (CPU_CHAR *)"Transaction denied, Server BUSY");
                     TFTPs_SockAddr = addr_ip_server;
                     tftp_err       = TFTPs_ERR_NONE;
                 }
                 break;


            default:
                 tftp_err = TFTPs_ERR_INVALID_STATE;
                 break;
        }


        if (tftp_err != TFTPs_ERR_NONE) {                       /* If err, terminate file tx.                           */
            TFTPs_Trace((CPU_INT16U)1,
                        (CPU_CHAR *)"Task, Error, session terminated");
            TFTPs_Terminate();
        }
    }
}


/*
*********************************************************************************************************
*                                       TFTPs_ServerSockInit()
*
* Description : Initialize the TFTP server socket.
*
* Argument(s) : family    IP family of the server socket.
*
* Return(s)   : TFTP_ERR_NONE,              if server successfully initialized.
*               TFTP_ERR_NO_SOCK,           if the socket could not be opened.
*               TFTP_ERR_CANT_BIND,         if the socket could not be bound.
*               TFTP_ERR_INVALID_FAMILY,    if the socket family is not supported.
*               TFTP_ERR_INVALID_ADDR,      if the socket address configuration fails.
*
* Caller(s)   : TFTPs_Task().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  TFTPs_ERR  TFTPs_ServerSockInit (NET_SOCK_FAMILY  family)
{
    TFTPs_CFG          *p_cfg;
#ifdef  NET_IPv4_MODULE_EN
    NET_IPv4_ADDR       ipv4_addr;
#endif
    CPU_INT08U         *p_addr;
    NET_IP_ADDR_LEN     addr_len;
    NET_SOCK_RTN_CODE   bind_status;
    NET_ERR             err;


    p_cfg = TFTPs_CfgPtr;

#if (TFTPs_TRACE_LEVEL >= TRACE_LEVEL_INFO)
    TFTPs_TraceInit();
#endif

                                                                /* Open a socket to listen for incoming connections.    */
    TFTPs_SockID = NetSock_Open((NET_SOCK_PROTOCOL_FAMILY)family,
                                                          NET_SOCK_TYPE_DATAGRAM,
                                                          NET_SOCK_PROTOCOL_UDP,
                                                         &err);

    if (TFTPs_SockID < 0) {                                     /* Could not open a socket.                             */
        return (TFTPs_ERR_NO_SOCK);
    }

    Mem_Set(&TFTPs_SockAddr, (CPU_CHAR)0, NET_SOCK_ADDR_SIZE);  /* Bind a local address so the client can send to us.   */

    switch (family) {
#ifdef  NET_IPv4_MODULE_EN
        case NET_SOCK_FAMILY_IP_V4:
             ipv4_addr = NET_UTIL_HOST_TO_NET_32(NET_IPv4_ADDR_NONE);
             p_addr    = (CPU_INT08U *)&ipv4_addr;
             addr_len  = NET_IPv4_ADDR_SIZE;
             break;
#endif
#ifdef  NET_IPv6_MODULE_EN
        case NET_SOCK_FAMILY_IP_V6:
             p_addr    = (CPU_INT08U *)&NET_IPv6_ADDR_ANY;
             addr_len  = NET_IPv6_ADDR_SIZE;
             break;
#endif

        default:
            return (TFTPs_ERR_INVALID_FAMILY);
    }

    NetApp_SetSockAddr(                     &TFTPs_SockAddr,
                       (NET_SOCK_ADDR_FAMILY)family,
                                             p_cfg->Port,
                                             p_addr,
                                             addr_len,
                                            &err);
    if (err != NET_APP_ERR_NONE) {
        return (TFTPs_ERR_INVALID_ADDR);
    }

    bind_status = NetSock_Bind((NET_SOCK_ID       ) TFTPs_SockID,
                               (NET_SOCK_ADDR    *)&TFTPs_SockAddr,
                               (NET_SOCK_ADDR_LEN ) NET_SOCK_ADDR_SIZE,
                               (NET_ERR          *)&err);
    if (bind_status != NET_SOCK_BSD_ERR_NONE) {                 /* Could not bind to the TFTPs port.                    */
        NetSock_Close(TFTPs_SockID, &err);
        return (TFTPs_ERR_CANT_BIND);
    }

    return (TFTPs_ERR_NONE);
}


/*
*********************************************************************************************************
*                                          TFTPs_StateIdle()
*
* Description : TFTP server idle state handler.
*
* Argument(s) : none.
*
* Return(s)   : Error code for this function.
*
* Caller(s)   : TFTPs_Task().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  TFTPs_ERR  TFTPs_StateIdle (void)
{
    TFTPs_CFG  *p_cfg;
    TFTPs_ERR   err;
    NET_ERR     err_sock;


    p_cfg = TFTPs_CfgPtr;

    TFTPs_Trace(10, (CPU_CHAR *)"Idle State");
    switch (TFTPs_OpCode) {
        case TFTP_OPCODE_RD_REQ:
                                                                /* Open the desired file for reading.                   */
             err = TFTPs_FileOpen(TFTPs_FILE_OPEN_RD);
             if (err == TFTPs_ERR_NONE) {
                 TFTPs_Trace(11, (CPU_CHAR *)"Rd Request, File Opened");
                 TFTPs_TxBlkNbr = 0;
                 TFTPs_State    = TFTPs_STATE_DATA_RD;
                 err            = TFTPs_DataRd();               /* Read the first block of data from the file and send  */
                                                                /* to client.                                           */
             }
             break;


        case TFTP_OPCODE_ACK:                                   /* NOT supposed to get ACKs in the Idle state.          */
             TFTPs_Trace(12, (CPU_CHAR *)"ACK received, not supposed to!");
             err = TFTPs_ERR_ACK;
             break;


        case TFTP_OPCODE_WR_REQ:
             TFTPs_TxBlkNbr = 0;
                                                                /* Open the desired file for writing.                   */
             err = TFTPs_FileOpen(TFTPs_FILE_OPEN_WR);
             if (err == TFTPs_ERR_NONE) {
                 TFTPs_Trace(13, (CPU_CHAR *)"Wr Request, File Opened");
                 TFTPs_State = TFTPs_STATE_DATA_WR;
                 TFTPs_DataWrAck(TFTPs_TxBlkNbr);               /* Acknowledge the client.                              */
                 err = TFTPs_ERR_NONE;
             }
             break;


        case TFTP_OPCODE_DATA:                                  /* NOT supposed to get DATA packets in the Idle state.  */
             err = TFTPs_ERR_DATA;
             break;


        case TFTP_OPCODE_ERR:                                   /* NOT supposed to get ERR packets in the Idle state.   */
             err = TFTPs_ERR_ERR;
             break;
    }

    if (err == TFTPs_ERR_NONE) {
        TFTPs_Trace(14, (CPU_CHAR *)"No error, Timeout set");
        NetSock_CfgTimeoutRxQ_Set((NET_SOCK_ID) TFTPs_SockID,
                                  (CPU_INT32U ) p_cfg->RxTimeoutMax,
                                  (NET_ERR   *)&err_sock);
    }

    return (err);
}


/*
*********************************************************************************************************
*                                         TFTPs_StateDataRd()
*
* Description : Process read action.
*
* Argument(s) : none.
*
* Return(s)   : Error code for this function.
*
* Caller(s)   : TFTPs_Task().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  TFTPs_ERR  TFTPs_StateDataRd (void)
{
    NET_SOCK_RTN_CODE  tx_size;
    TFTPs_ERR          err;


    err = TFTPs_ERR_NONE;

    switch (TFTPs_OpCode) {
        case TFTP_OPCODE_RD_REQ:                                /* NOT supposed to get RRQ pkts in the DATA Read state. */
                                                                /* Close and re-open file.                              */
             NetFS_FileClose(TFTPs_FileHandle);
             err = TFTPs_FileOpen(TFTPs_FILE_OPEN_RD);
             if (err == TFTPs_ERR_NONE) {
                 TFTPs_Trace(20, (CPU_CHAR *)"Data Rd, Rx RD_REQ.");
                 TFTPs_TxBlkNbr = 0;
                 TFTPs_State    = TFTPs_STATE_DATA_RD;
                 err            = TFTPs_DataRd();               /* Read first block of data and tx to client.           */
            }
            break;


        case TFTP_OPCODE_ACK:
             TFTPs_GetRxBlkNbr();
             if (TFTPs_RxBlkNbr == TFTPs_TxBlkNbr) {            /* If sent data ACK'd, ...                              */
                 TFTPs_Trace(21, (CPU_CHAR *)"Data Rd, ACK Rx'd");
                 err = TFTPs_DataRd();                          /* ... read next block of data and tx to client.        */

             } else {                                           /* Else re-tx prev block.                               */

                 tx_size = TFTPs_Tx((CPU_INT16U  ) TFTP_OPCODE_DATA,
                                    (CPU_INT16U  ) TFTPs_TxBlkNbr,
                                    (CPU_INT08U *)&TFTPs_TxMsgBuf[0],
                                    (CPU_INT16U  ) TFTPs_TxMsgLen);

                 if (tx_size < 0) {
                     TFTPs_TxErr(0, (CPU_CHAR *)"RRQ file read error");
                     err = TFTPs_ERR_TX;
                 }
             }
             break;


        case TFTP_OPCODE_WR_REQ:                                /* NOT supposed to get WRQ pkts in the DATA Read state. */
             TFTPs_Trace(23, (CPU_CHAR *)"Data Rd, Rx'd WR_REQ");
             TFTPs_TxErr(0,  (CPU_CHAR *)"RRQ server busy, WRQ  opcode?");
             err = TFTPs_ERR_WR_REQ;
             break;


        case TFTP_OPCODE_DATA:                                  /* NOT supposed to get DATA pkts in the DATA Read state.*/
             TFTPs_Trace(24, (CPU_CHAR *)"Data Rd, Rx'd DATA");
             TFTPs_TxErr(0,  (CPU_CHAR *)"RRQ server busy, DATA opcode?");
             err= TFTPs_ERR_DATA;
             break;


        case TFTP_OPCODE_ERR:
             TFTPs_Trace(25, (CPU_CHAR *)"Data Rd, Rx'd ERR");
             TFTPs_TxErr(0,  (CPU_CHAR *)"RRQ server busy, ERR  opcode?");
             err = TFTPs_ERR_ERR;
             break;
    }


    return (err);
}


/*
*********************************************************************************************************
*                                         TFTPs_StateDataWr()
*
* Description : Process write action.
*
* Argument(s) : none.
*
* Return(s)   : Error code for this function.
*
* Caller(s)   : TFTPs_Task().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  TFTPs_ERR  TFTPs_StateDataWr (void)
{
    TFTPs_ERR  err;


    err = TFTPs_ERR_NONE;
    switch (TFTPs_OpCode) {
        case TFTP_OPCODE_RD_REQ:
             TFTPs_Trace(30, (CPU_CHAR *)"Data Wr, WRQ server busy, RRQ  opcode?");
             TFTPs_TxErr(0,  (CPU_CHAR *)"WRQ server busy, RRQ  opcode?");
             err = TFTPs_ERR_RD_REQ;
             break;


        case TFTP_OPCODE_ACK:
             TFTPs_Trace(31, (CPU_CHAR *)"Data Wr, WRQ server busy, ACK  opcode?");
             TFTPs_TxErr(0,  (CPU_CHAR *)"WRQ server busy, ACK  opcode?");
             err = TFTPs_ERR_ACK;
             break;


        case TFTP_OPCODE_WR_REQ:
             NetFS_FileClose(TFTPs_FileHandle);
             TFTPs_TxBlkNbr  = 0;
                                                                /* Open the desired file for writing.                   */
             err = TFTPs_FileOpen(TFTPs_FILE_OPEN_WR);
             if (err == TFTPs_ERR_NONE) {
                 TFTPs_Trace(32, (CPU_CHAR *)"Data Wr, Rx'd WR_REQ again");
                 TFTPs_State = TFTPs_STATE_DATA_WR;
                 TFTPs_DataWrAck(TFTPs_TxBlkNbr);               /* Acknowledge the client.                              */
                 err = TFTPs_ERR_NONE;
             }
             break;


        case TFTP_OPCODE_DATA:
             TFTPs_Trace(33, (CPU_CHAR *)"Data Wr, Rx'd DATA --- OK");
             err = TFTPs_DataWr();                  /* Write data to file.                                  */
             break;


        case TFTP_OPCODE_ERR:
             TFTPs_Trace(34, (CPU_CHAR *)"Data Wr, WRQ server busy, ERR  opcode?");
             TFTPs_TxErr(0,  (CPU_CHAR *)"WRQ server busy, ERR  opcode?");
             err = TFTPs_ERR_ERR;
             break;
    }

    return (err);
}


/*
*********************************************************************************************************
*                                         TFTPs_GetRxBlkNbr()
*
* Description : Extract the block number from the received TFTP command packet.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : TFTPs_StateDataRd().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  TFTPs_GetRxBlkNbr (void)
{
    CPU_INT16U  *p_blk_nbr;


    p_blk_nbr      = (CPU_INT16U *)&TFTPs_RxMsgBuf[TFTP_PKT_OFFSET_BLK_NBR];
    TFTPs_RxBlkNbr =  NET_UTIL_NET_TO_HOST_16(*p_blk_nbr);
}


/*
*********************************************************************************************************
*                                          TFTPs_Terminate()
*
* Description : Terminate the current file transfer process.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : TFTPs_Dis(),
*               TFTPs_Task().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  TFTPs_Terminate (void)
{
    NET_ERR  err_sock;


    TFTPs_State = TFTPs_STATE_IDLE;                             /* Abort current file transfer.                         */
    if (TFTPs_FileHandle != (void *)0) {
        NetFS_FileClose(TFTPs_FileHandle);                      /* Close the current opened file.                       */
        TFTPs_FileHandle = (void *)0;
    }

                                                                /* Reset blocking timeout to infinite.                  */
    NetSock_CfgTimeoutRxQ_Set((NET_SOCK_ID) TFTPs_SockID,
                              (CPU_INT32U ) NET_TMR_TIME_INFINITE,
                              (NET_ERR   *)&err_sock);
}


/*
*********************************************************************************************************
*                                          TFTPs_FileOpen()
*
* Description : Get filename and file mode from the TFTP packet and attempt to open that file.
*
* Argument(s) : rw          File access :
*
*                               TFTPs_FILE_OPEN_RD      Open for reading
*                               TFTPs_FILE_OPEN_WR      Open for writing
*
* Return(s)   : TFTP_ERR_NONE,           if NO error.
*
*               TFTP_ERR_FILE_NOT_FOUND, if file not found.
*
* Caller(s)   : TFTPs_StateIdle(),
*               TFTPs_StateDataRd(),
*               TFTPs_StateDataWr().
*
* Note(s)     : (1) This function also extracts options as specified in RFC #2349 :
*
*                       "timeout"     specifies the timeout in seconds to wait in case we don't receive
*                                     data request after we initiated a read request.
*
*                       "tsize"       specifies the size of the file in bytes that the client is writing.
*
*                   Note that both these options may not be supported by the client and thus, we assume
*                   default values if they are not specified..
*********************************************************************************************************
*/

static  TFTPs_ERR  TFTPs_FileOpen (CPU_BOOLEAN  rw)
{
    CPU_CHAR  *p_filename;
    CPU_CHAR  *p_mode;
    CPU_CHAR  *p_name;
#if 0
    CPU_CHAR  *p_value;
#endif

                                                                /* ---- GET FILENAME ---------------------------------- */
    p_filename = (CPU_CHAR *)&TFTPs_RxMsgBuf[TFTP_PKT_OFFSET_FILENAME];
                                                                /* ---- GET FILE MODE --------------------------------- */
    p_mode = p_filename;                                        /* Point to the 'Mode' string.                          */
    while (*p_mode > (CPU_CHAR)0) {
        p_mode++;
    }
    p_mode++;
                                                                /* ---- GET RFC2349 "timeout" OPTION (IF AVAILABLE) --- */
    p_name = p_mode;                                            /* Skip over the 'Mode' string.                         */
    while (*p_name > (CPU_CHAR)0) {
        p_name++;
    }
    p_name++;


#if 0
                                                                /* See if the client specified a "timeout" string       */
    if (Str_Cmp(p_name, (CPU_CHAR *)"timeout") == 0) {          /* (RFC2349).                                           */

        p_value = p_name;                                       /* Yes, skip over the "timeout" string.                 */
        while (*p_value > (CPU_CHAR)0) {
            p_value++;
        }
        p_value++;
                                                                /* Get the timeout (in seconds).                        */
        TFTPs_Timeout_s = Str_ParseNbr_Int32U(p_value, (CPU_CHAR  **)0, 0);
    } else {
        TFTPs_Timeout_s = TFTPs_TIMEOUT_SEC_DFLT;               /* No,  assume the default timeout (in seconds).        */

                                                                /* ---- GET RFC2349 "tsize" OPTION (IF AVAILABLE) ----- */
                                                                /* See if the client specified a "tsize" string         */
                                                                /* (RFC2349).                                           */
        if (Str_Cmp(p_name, (CPU_CHAR *)"tsize") == 0) {
            p_value = p_name;
            while (*p_value > (CPU_CHAR)0) {                    /* Yes, skip over the "tsize" string.                   */
                p_value++;
            }
            p_value++;
                                                                /* Get the size of the file to write.                   */
            TFTPs_WrSize = Str_ParseNbr_Int32U(p_value, (CPU_CHAR  **)0, 0);
        } else {
            TFTPs_WrSize = 0;                                   /* Assume a default value of 0.                         */
        }
    }
#endif
                                                                /* ---- OPEN THE FILE --------------------------------- */
    TFTPs_FileHandle = TFTPs_FileOpenMode(p_filename, rw);

    if (TFTPs_FileHandle == (void *)0) {
        TFTPs_TxErr(0, (CPU_CHAR *)"file not found");
        return (TFTPs_ERR_FILE_NOT_FOUND);
    }

    return (TFTPs_ERR_NONE);
}


/*
*********************************************************************************************************
*                                        TFTPs_FileOpenMode()
*
* Description : Open the specified file.
*
* Argument(s) : p_filename  File name to open.
*
*               rw          File access :
*
*                               TFTPs_FILE_OPEN_RD      Open for reading
*                               TFTPs_FILE_OPEN_WR      Open for writing
*
* Return(s)   : Pointer to a file handle for the opened file, if NO error.
*
*               Pointer to NULL,                              otherwise.
*
* Caller(s)   : TFTPs_FileOpen().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  *TFTPs_FileOpenMode (CPU_CHAR     *p_filename,
                                   CPU_BOOLEAN   rw)
{
    void  *p_file;


    p_file = (void *)0;
    switch (rw) {
        case TFTPs_FILE_OPEN_RD:
             p_file = NetFS_FileOpen(p_filename,
                                     NET_FS_FILE_MODE_OPEN,
                                     NET_FS_FILE_ACCESS_RD);
             break;

        case TFTPs_FILE_OPEN_WR:
             p_file = NetFS_FileOpen(p_filename,
                                     NET_FS_FILE_MODE_CREATE,
                                     NET_FS_FILE_ACCESS_WR);
             break;


        default:
             break;
    }

    return (p_file);
}


/*
*********************************************************************************************************
*                                           TFTPs_DataRd()
*
* Description : Read data from the opened file and send it to the client.
*
* Argument(s) : none.
*
* Return(s)   : TFTP_ERR_NONE,    if NO error.
*
*               TFTP_ERR_FILE_RD, if file read error.
*
*               TFTP_ERR_TX,      if transmit  error.
*
* Caller(s)   : TFTPs_StateIdle(),
*               TFTPs_StateDataRd().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  TFTPs_ERR  TFTPs_DataRd (void)
{
    NET_SOCK_RTN_CODE  tx_size;
    CPU_BOOLEAN        ok;


                                                                /* Read data from file.                                 */
    ok = NetFS_FileRd((void       *) TFTPs_FileHandle,
                      (void       *)&TFTPs_TxMsgBuf[TFTP_PKT_OFFSET_DATA],
                      (CPU_SIZE_T  ) TFTPs_BLOCK_SIZE,
                      (CPU_SIZE_T *)&TFTPs_TxMsgLen);

    if (TFTPs_TxMsgLen < TFTPs_BLOCK_SIZE) {                    /* Close file when all data read.                       */
        NetFS_FileClose(TFTPs_FileHandle);
        TFTPs_State = TFTPs_STATE_IDLE;
    }

    if (ok == DEF_FAIL) {                                       /* If read err, ...                                     */
        TFTPs_TxErr(0, (CPU_CHAR *)"RRQ file read error");      /* ... tx  err pkt.                                     */
        return (TFTPs_ERR_FILE_RD);
    }

    TFTPs_TxMsgCtr++;
    TFTPs_TxBlkNbr++;

    TFTPs_TxMsgLen += TFTP_PKT_SIZE_OPCODE + TFTP_PKT_SIZE_BLK_NBR;

    tx_size = TFTPs_Tx((CPU_INT16U  ) TFTP_OPCODE_DATA,
                       (CPU_INT16U  ) TFTPs_TxBlkNbr,
                       (CPU_INT08U *)&TFTPs_TxMsgBuf[0],
                       (CPU_INT16U  ) TFTPs_TxMsgLen);

    if (tx_size < 0) {                                          /* If tx  err, ...                                      */
        TFTPs_TxErr(0, (CPU_CHAR *)"RRQ file read error");      /* ... tx err pkt.                                      */
        return (TFTPs_ERR_TX);
    }

    return (TFTPs_ERR_NONE);
}


/*
*********************************************************************************************************
*                                           TFTPs_DataWr()
*
* Description : Write data to the opened file.
*
* Argument(s) : none.
*
* Return(s)   : TFTP_ERR_NONE.
*
* Caller(s)   : TFTPs_StateDataWr().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  TFTPs_ERR  TFTPs_DataWr (void)
{
    CPU_INT16U   blk_nbr;
    CPU_INT16S   data_bytes;
    CPU_SIZE_T   data_bytes_wr;
    CPU_INT16U  *p_blk_nbr;


                                                                /* Get block nbr.                                       */
    p_blk_nbr = (CPU_INT16U *)&TFTPs_RxMsgBuf[TFTP_PKT_OFFSET_BLK_NBR];
    blk_nbr   =  NET_UTIL_NET_TO_HOST_16(*p_blk_nbr);


    if (blk_nbr > TFTPs_TxBlkNbr) {                             /* If block nbr > last block nbr, ...                   */
        data_bytes = TFTPs_RxMsgLen - TFTP_PKT_SIZE_OPCODE - TFTP_PKT_SIZE_BLK_NBR;

        if (data_bytes > 0) {                                   /* ... wr data to file.                                 */
            (void)NetFS_FileWr((void       *) TFTPs_FileHandle,
                               (void       *)&TFTPs_RxMsgBuf[TFTP_PKT_OFFSET_DATA],
                               (CPU_SIZE_T  ) data_bytes,
                               (CPU_SIZE_T *)&data_bytes_wr);
            (void)&data_bytes_wr;
        }

        if (data_bytes < TFTPs_BLOCK_SIZE) {                    /* If last block of transmission, ...                   */
            NetFS_FileClose(TFTPs_FileHandle);                  /* ... close file.                                      */
            TFTPs_State = TFTPs_STATE_IDLE;
        }
    }


    TFTPs_DataWrAck(blk_nbr);

    TFTPs_TxBlkNbr = blk_nbr;


    return (TFTPs_ERR_NONE);
}


/*
*********************************************************************************************************
*                                          TFTPs_DataWrAck()
*
* Description : Send an acknowledgement to the client.
*
* Argument(s) : blk_nbr     Block number to acknowledge.
*
* Return(s)   : none.
*
* Caller(s)   : TFTPs_StateIdle(),
*               TFTPs_StateDataWr(),
*               TFTPs_DataWr().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  TFTPs_DataWrAck (CPU_INT32U  blk_nbr)
{
    CPU_INT16S  tx_len;


    tx_len = TFTP_PKT_SIZE_OPCODE + TFTP_PKT_SIZE_BLK_NBR;
    TFTPs_TxMsgCtr++;

    TFTPs_Tx((CPU_INT16U  ) TFTP_OPCODE_ACK,
             (CPU_INT16U  ) blk_nbr,
             (CPU_INT08U *)&TFTPs_TxMsgBuf[0],
             (CPU_INT16U  ) tx_len);
}


/*
*********************************************************************************************************
*                                            TFTPs_TxErr()
*
* Description : Send error message to the client.
*
* Argument(s) : err_code    TFTP error code        indicating the nature of the error.
*
*               p_err_msg   NULL terminated string indicating the nature of the error.
*
* Return(s)   : none.
*
* Caller(s)   : TFTPs_Task(),
*               TFTPs_StateDataRd(),
*               TFTPs_StateDataWr(),
*               TFTPs_FileOpen(),
*               TFTPs_DataRd().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  TFTPs_TxErr (CPU_INT16U   err_code,
                           CPU_CHAR    *p_err_msg)
{
    CPU_INT16S  tx_len;


    tx_len = Str_Len(p_err_msg) + TFTP_PKT_SIZE_OPCODE + TFTP_PKT_SIZE_BLK_NBR + 1;

    Str_Copy((CPU_CHAR *)&TFTPs_TxMsgBuf[TFTP_PKT_OFFSET_DATA], p_err_msg);

    TFTPs_Tx( TFTP_OPCODE_ERR,
              err_code,
             &TFTPs_TxMsgBuf[0],
              tx_len);
}


/*
*********************************************************************************************************
*                                             TFTPs_Tx()
*
* Description : Send TFTP packet.
*
* Argument(s) : opcode      TFTP packet operation code.
*
*               blk_nbr     Block number (or error code) for packet to transmit.
*
*               p_buf       Pointer to                       packet to transmit.
*
*               tx_len      Length of the                    packet to transmit (in octets).
*
* Return(s)   : Number of positive data octets transmitted, if NO errors.
*
*               NET_SOCK_BSD_RTN_CODE_CONN_CLOSED,          if socket connection closed.
*
*               NET_SOCK_BSD_ERR_TX,                        otherwise.
*
* Caller(s)   : TFTPs_StateDataRd(),
*               TFTPs_DataRd(),
*               TFTPs_DataWrAck(),
*               TFTPs_TxErr().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  NET_SOCK_RTN_CODE  TFTPs_Tx (CPU_INT16U   opcode,
                                     CPU_INT16U   blk_nbr,
                                     CPU_INT08U  *p_buf,
                                     CPU_INT16U   tx_len)
{
    CPU_INT16U         *p_buf16;
    NET_SOCK_RTN_CODE   bytes_sent;
    NET_ERR             err;


    p_buf16 = (CPU_INT16U *)&TFTPs_TxMsgBuf[TFTP_PKT_OFFSET_OPCODE];
   *p_buf16 = NET_UTIL_NET_TO_HOST_16(opcode);

    p_buf16 = (CPU_INT16U *)&TFTPs_TxMsgBuf[TFTP_PKT_OFFSET_BLK_NBR];
   *p_buf16 = NET_UTIL_NET_TO_HOST_16(blk_nbr);


    bytes_sent = NetSock_TxDataTo((NET_SOCK_ID      ) TFTPs_SockID,
                                  (void            *) p_buf,
                                  (CPU_INT16U       ) tx_len,
                                  (CPU_INT16S       ) NET_SOCK_FLAG_NONE,
                                  (NET_SOCK_ADDR   *)&TFTPs_SockAddr,
                                  (NET_SOCK_ADDR_LEN) NET_SOCK_ADDR_SIZE,
                                  (NET_ERR         *)&err);

    return (bytes_sent);
}


/*
*********************************************************************************************************
*                                          TFTPs_TraceInit()
*
* Description : Initialize the trace debug feature.
*
* Argument(s) : none.
*
* Return(s)   : none.
*
* Caller(s)   : TFTPs_ServerSockInit().
*
* Note(s)     : none.
*********************************************************************************************************
*/

#if (TFTPs_TRACE_LEVEL >= TRACE_LEVEL_INFO)
static  void  TFTPs_TraceInit (void)
{
    CPU_INT16U  i;


    TFTPs_TraceIx = 0;
    for (i = 0; i < TFTPs_TRACE_HIST_SIZE; i++) {
        TFTPs_TraceTbl[i].Id = 0;
        TFTPs_TraceTbl[i].TS = 0;
    }
}
#endif


/*
*********************************************************************************************************
*                                            TFTPs_Trace()
*
* Description : Record execution trace.
*
* Argument(s) : id          Trace identification number.
*
*               p_str       NULL terminated string representing action to trace.
*
* Return(s)   : none.
*
* Caller(s)   : TFTPs_Task(),
*               TFTPs_StateIdle(),
*               TFTPs_StateDataRd(),
*               TFTPs_StateDataWr(),
*               TFTPs_Disp().
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  TFTPs_Trace (CPU_INT16U   id,
                           CPU_CHAR    *p_str)
{
    KAL_ERR  err_kal;


#if (TFTPs_TRACE_LEVEL >= TRACE_LEVEL_INFO)
    TFTPs_TraceTbl[TFTPs_TraceIx].Id       = id;
    TFTPs_TraceTbl[TFTPs_TraceIx].TS       = KAL_TickGet(&err_kal);
    TFTPs_TraceTbl[TFTPs_TraceIx].State    = TFTPs_State;

    Str_Copy(TFTPs_TraceTbl[TFTPs_TraceIx].Str, p_str);

    TFTPs_TraceTbl[TFTPs_TraceIx].RxBlkNbr = TFTPs_RxBlkNbr;
    TFTPs_TraceTbl[TFTPs_TraceIx].TxBlkNbr = TFTPs_TxBlkNbr;

    TFTPs_TraceIx++;
    if (TFTPs_TraceIx >= TFTPs_TRACE_HIST_SIZE) {
        TFTPs_TraceIx  = 0;
    }
#endif
}

