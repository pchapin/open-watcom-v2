/****************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2002-2024 The Open Watcom Contributors. All Rights Reserved.
*    Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.
*
*  ========================================================================
*
*    This file contains Original Code and/or Modifications of Original
*    Code as defined in and that are subject to the Sybase Open Watcom
*    Public License version 1.0 (the 'License'). You may not use this file
*    except in compliance with the License. BY USING THIS FILE YOU AGREE TO
*    ALL TERMS AND CONDITIONS OF THE LICENSE. A copy of the License is
*    provided with the Original Code and Modifications, and is also
*    available at www.sybase.com/developer/opensource.
*
*    The Original Code and all software distributed under the License are
*    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
*    EXPRESS OR IMPLIED, AND SYBASE AND ALL CONTRIBUTORS HEREBY DISCLAIM
*    ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR
*    NON-INFRINGEMENT. Please see the License for the specific language
*    governing rights and limitations under the License.
*
*  ========================================================================
*
* Description:  Parallel link remote communications core.
*
****************************************************************************/


/*
 * The WATCOM Connection
 * ---------------------
 * The AT parallel port uses pins 1 to 17 for data and 18 to 25 for
 * ground.  To send 8 bits of data between two ATs, we use 8 lines in one
 * direction, 8 in the other direction, and 1 for handshaking (total 17).
 * The 5 data lines from port 3BD/379 and 3 data lines from port 3BE/37A
 * are combined for 8 input data bits.  We use pin 17 for handshaking
 * since it is bi-directional (i. e. , you can read/write this line).
 *
 * Here's how to hook up a WATCOM cable to make this work.
 *
 *             Port   Bit Pin -> Pin  Port     Bit
 *
 *             GROUND      18 <-> 18
 *             CtlPort2 3  17 <-> 17  CtlPort2 3 (SLCT IN)
 *
 *             DataPort 0   2 ->  1*  CtlPort2 0 (STROBE)
 *                "     1   3 -> 14*     "     1 (AUTO FD XT)
 *                "     2   4 -> 16      "     2 (-INIT)
 *                "     3   5 -> 15   CtlPort1 3 (-ERROR)
 *
 *                "     4   6 -> 13      "     4 (Select)
 *                "     5   7 -> 12      "     5 (PE)
 *                "     6   8 -> 10      "     6 (-ACK)
 *                "     7   9 -> 11*     "     7 (-BUSY)
 *
 *                          1* <- 2
 *                         14* <- 3
 *                         16  <- 4
 *                         15  <- 5
 *
 *                         13  <- 6
 *                         12  <- 7
 *                         10  <- 8
 *                         11* <- 9
 *
 *      (*) means logic inverted
 *
 * The Fujistsu Connection
 * -----------------------
 * The Fujitsu FMR-xx also supports the transmission and reception of 8
 * data bits.  There are enough data lines to do this.  Unfortunately,
 * there are no bidirectional data lines.  Therefore, we are short one pin
 * on the AT side (we would require 8 data + 8 data + 2 handshaking lines
 * on the AT since the Fujitsu has no bidirectional data lines).
 *
 * From the software point of view, we will be able to send 8 bits at a
 * time in one direction (to the FMR-xx) and 4 bits at a time in the
 * other direction (to the AT).  Given that the higher volume of traffic
 * is usually from the application machine to debugger machine, this
 * seems to be the best choice.
 *
 * Here's how to hook up a Fujitsu WATCOM cable to make this work.
 *
 *                 AT side              FM-R side
 *             --------------      -------------------
 *             Port   Bit Pin      Pin  Port     Bit
 *
 *             GROUND      18  <-> 23  GROUND
 *  (-SLCT IN) CtlPort2 3  17   -> 13* StatReg2  3 (*SLCT)
 *
 *  (+DB0)     DataPort 0   2   -> 14  StatReg2  0 (RINF1)
 *  (+DB1)        "     1   3   -> 15     "      1 (RINF2)
 *  (+DB2)        "     2   4   -> 16     "      2 (RINF3)
 *  (+DB3)        "     3   5   -> 17  StatReg1  3 (POW +5 VP)
 *
 *  (+DB4)        "     4   6   -> 22      "     4 (THSN)
 *  (+DB5)        "     5   7   -> 21*     "     5 (*FUSE)
 *  (+DB6)        "     6   8   -> 12      "     6 (PE)
 *  (+DB7)        "     7   9   -> 11      "     7 (BUSY)
 *
 *  (-STROBE)  CtlPort2 0   1* <- 2    DataReg   0 (PD1)
 *  (-AUTO FEED)  "     1  14* <- 3              1 (PD2)
 *  (-INIT PRT)   "     2  16  <- 4              2 (PD3)
 *  (-ERROR)   CtlPort1 3  15  <- 5              3 (PD4)

 *  (+SELECT)     "     4  13  <- 6              4 (PD5)
 *  (+P.END)      "     5  12  <- 7              5 (PD6)
 *  (-ACKNOWLEDGE)"     6  10  <- 8              6 (PD7)
 *  (+BUSY)       "     7  11* <- 9              7 (PD8)
 *
 *      (*) means logic inverted
 *
 * The LapLink Connection
 * ----------------------
 * Here's how to hook up a LapLink cable.
 *
 *              Port   Bit Pin -> Pin  Port      Bit
 *
 *           DataPort   0   2  ->  15  CtlPort1  3      (Error)
 *              "       1   3  ->  13     "      4      (Select)
 *              "       2   4  ->  12     "      5      (P End)
 *              "       3   5  ->  10     "      6      (Ack)
 *              "       4   6  ->  11*    "      7      (Busy)
 *
 *                          10 <-  5
 *                          11 <-  6
 *                          12 <-  4
 *                          13 <-  3
 *                          15 <-  2
 *
 *           Ground         25 <-> 25
 *
 * The Flying Dutchman Connection
 * ------------------------------
 * Here's how to hook up a Flying Dutchman cable.
 *
 *              Port   Bit Pin -> Pin  Port      Bit
 *
 *           CtlPort2   0   1  ->  11* CtlPort1  7
 *           DataPort   0   2  ->  15     "      3      (Error)
 *              "       1   3  ->  13     "      4      (Select)
 *              "       2   4  ->  12     "      5      (P End)
 *              "       3   5  ->  10     "      6      (Ack)
 *
 *                          10 <-  5
 *                          11 <-  1
 *                          12 <-  4
 *                          13 <-  3
 *                          15 <-  2
 *
 * The following is some technical information on the AT parallel port.
 * The pin numbers refer to the DB25 pins on the AT parallel port.
 *
 *          IBM AT Parallel Port
 * +-----------------------------------+
 * |Input/Output to 3BC/378            |
 * +-----------------------------------+
 * |Bit   7   6   5   4   3   2   1   0|
 * |Pin   9   8   7   6   5   4   3   2|
 * +-----------------------------------+
 *
 * +-----------------------------------+
 * |Output to 3BE/37A
 * +-----------------------------------+
 * |Bit   7   6   5   4   3   2   1   0|
 * |Pin   -   -   - IRQ  17  16  14   1|
 * |               Enable              |
 * +-----------------------------------+
 *
 * +-----------------------------------+
 * |Input from 3BD/379
 * +-----------------------------------+
 * |Bit   7   6   5   4   3   2   1   0|
 * |Pin  11  10  12  13  15   -   -   -|
 * +-----------------------------------+
 *
 * +-----------------------------------+
 * |Input from 3BE/37A
 * +-----------------------------------+
 * |Bit   7   6   5   4   3   2   1   0|
 * |Pin   -   -   -  IRQ 17  16  14   1|
 * |               Enable              |
 * +-----------------------------------+
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__NETWARE__)
#include <dos.h>
#else
#include <i86.h>
#endif
#if defined(_DBG) && !defined( SERVER )
    #include <conio.h>
#endif
#include "bool.h"
#include "trptypes.h"
#include "trperr.h"
#include "packet.h"
#include "nothing.h"
#include "parlink.h"
#include "portio.h"
#include "pardata.h"


#if !defined(_DBG)
    #define my_inp(x)       inp(hwdata.controller.port + x)
    #define my_outp(x,y)    outp(hwdata.controller.port + x,y)
    #define dbgrtn(x)
#else
    #define my_inp(x)       dbg_inp(hwdata.controller.port + x)
    #define my_outp(x,y)    dbg_outp(hwdata.controller.port + x,y)
  #ifdef SERVER
    #define dbgrtn(x)       printf( x )
  #else
    #define dbgrtn(x)       cputs( x )
  #endif
#endif

static hw_data  hwdata;

static char     InvalidPort[] = TRP_ERR_invalid_parallel_port_number;

#if defined(_DBG)
char dbg_inp( int port )
{
    char x;

    x = inp( port );
  #ifdef SERVER
    printf( "in %x=%2.2x ", port, x );
  #else
    {
        char buf[10];

        itoa( port, buf, 16 );
        cputs( " in " ); cputs( buf );
        itoa( x, buf, 16 );
        cputs( "=" ); cputs( buf );
    }
  #endif
    return( x );
}

void dbg_outp( int port, char x )
{
    outp( port, x );
  #ifdef SERVER
    printf( "out %x=%2.2x ", port, x );
  #else
    {
        char buf[10];

        itoa( port, buf, 16 );
        cputs( " out " ); cputs( buf );
        itoa( x, buf, 16 );
        cputs( "=" ); cputs( buf );
    }
  #endif
}
#endif

/*********************** CABLE TYPE DEFINITIONS ***********************/

/*
 * The undefined/uninitialized link protocol (cable type).
 */
#define NULL_VAL        0

/*
 * The WATCOM cable link.
 * 0x18 is used to ensure that the control lines stay in a high state
 * until Synch is called
 */
#define WATCOM_VAL      (0x18 | 2)
#define TWIDLE_ON       WATCOM_VAL
#define TWIDLE_OFF      (WATCOM_VAL & ~2)

/*
 * The Fujitsu FMR link looks like a busted WATCOM cable
 * in which RaiseCtl1()/LowerCtl1() cannot be performed
 */
#define FMR_VAL         0x2A

/*
 * How the LAPLINK_VAL calculation works:
 *
 * <<3            simulates the effect of being transmitted over the LAPLINK
 * |0x2           0x2  is the value returned from a read of ctlport 2
 *                (after appropriate bit operations have be done by
 *                ReadData).  This value is dependent on the value
 *                written when XX_RaiseCtl1 is called
 */
#define LAPLINK_VAL     ((TWIDLE_ON << 3) | 0x2)

/*
 * The DUTCHMAN_VAL calculation works the same as the LAPLINK_VAL
 * calculation except:
 *
 * &0x0f        acounts for the fact that pin 6 is not connected in the
 *              flying dutchman cable
 */
#define DUTCHMAN_VAL    (((TWIDLE_ON & 0x0f) << 3) | 0x02)

/*********************** WATCOM CABLE MACROS **************************/
#define PC_CTL1         0x08
#define PC_CTL2         0x08
#define Ctl1Hi()        ((my_inp( CTLPORT2 ) & PC_CTL1) != 0)
#define Ctl1Lo()        ((my_inp( CTLPORT2 ) & PC_CTL1) == 0)
#define RaiseCtl1()     (my_outp( CTLPORT2, PC_CTL1 | 0x04 ))
#define LowerCtl1()     (my_outp( CTLPORT2, 0x04 ))

#define Ctl2Hi()        ((my_inp( CTLPORT1 ) & PC_CTL2) != 0)
#define Ctl2Lo()        ((my_inp( CTLPORT1 ) & PC_CTL2) == 0)
#define RaiseCtl2()     (my_outp( DATAPORT, PC_CTL2 ))
#define LowerCtl2()     (my_outp( DATAPORT, 0x00 ))

#define ReadData()      (((my_inp( CTLPORT1 ) ^ 0x80) & 0xF8) \
                          | ((my_inp( CTLPORT2 ) ^ 0x03) & 0x07))
#define WriteData(d)    (my_outp( DATAPORT, d ))

/*********************** WATCOM FMR CABLE MACROS **********************/
#define FM_CTL1         0x40
/*
 * Can't use bit 0x08 in CtlPort2 (line disabled)
 */
#define FM_Ctl1Hi()     ((my_inp( CTLPORT1 ) & FM_CTL1) != 0)
#define FM_Ctl1Lo()     ((my_inp( CTLPORT1 ) & FM_CTL1) == 0)

/********************** LAPLINK CABLE MACROS **************************/
#define LL_Ctl1Hi()     ((my_inp( CTLPORT1 ) & 0x80) == 0)
#define LL_Ctl1Lo()     ((my_inp( CTLPORT1 ) & 0x80) != 0)

#define LL_RaiseCtl1()  (my_outp( DATAPORT, 0x10 ))
#define LL_LowerCtl1()  (my_outp( DATAPORT, 0x00 ))

#define LL_Ctl2Hi()     ((my_inp( CTLPORT1 ) & 0x40) != 0)
#define LL_Ctl2Lo()     ((my_inp( CTLPORT1 ) & 0x40) == 0)

#define LL_RaiseCtl2()  (my_outp( DATAPORT, 0x08 ))
#define LL_LowerCtl2()  (my_outp( DATAPORT, 0x00 ))

#define LL_ReadData()   ((my_inp( CTLPORT1 ) >> 3) & 0x0f)
/*
 * write the data and raise control line 1
 */
#define LL_WriteData(d) (my_outp( DATAPORT, (d | 0x10) ))

/***************** FLYING DUTCHMAN CABLE MACROS ***********************/

#define FD_Ctl1Hi()     ((my_inp( CTLPORT1 ) & 0x80) != 0)
#define FD_Ctl1Lo()     ((my_inp( CTLPORT1 ) & 0x80) == 0)

#define FD_RaiseCtl1()  (my_outp( CTLPORT2, 0x01 ))
#define FD_LowerCtl1()  (my_outp( CTLPORT2, 0x00 ))

#define FD_Ctl2Hi()     ((my_inp( CTLPORT1 ) & 0x40) != 0)
#define FD_Ctl2Lo()     ((my_inp( CTLPORT1 ) & 0x40) == 0)

#define FD_RaiseCtl2()  (my_outp( DATAPORT, 0x08 ))
#define FD_LowerCtl2()  (my_outp( DATAPORT, 0x00 ))

#define FD_ReadData()   ((my_inp( CTLPORT1 ) >> 3) & 0x0f)
#define FD_WriteData(d) (my_outp( DATAPORT, d ))

/***************** Cable Detection MACROS ****************************/

/*
 * This operation disables bits 3,2,0 in CtrlPort2 (LowerCtl1 fixes it)
 */
#define XX_RaiseCtl1()  (my_outp( CTLPORT2, 0x01 ))

/*********************************************************************/

#define TWIDDLE_THUMBS  if( wait == RELINQUISH ) { NothingToDo(); } \
                else if( wait != KEEP && wait < Ticks() ) return( TIMEOUT )

#define TWIDLE_TIME     1
#define TWIDLE_NUM      2

#define DONE_LINE_TEST  255

#define LINE_TEST_WAIT  20
#define SYNCH_WAIT      40
#define TIMEOUT         -1

/*
 * if wait is not KEEP or RELINQUISH it is the latest time that this
 * operation should take before it times out
 */
static int DataGetByte( unsigned long wait )
{
    unsigned char   data;

    dbgrtn( "\r\n-DataGet-" );
    data = 0;
    switch( hwdata.cable_type ) {
    case WATCOM_VAL:
        RaiseCtl2();            /* Hi, I'm ready to read */
        while( Ctl1Lo() ) {     /* wait till he's written the data */
            TWIDDLE_THUMBS;
        }
        data = ReadData();      /* bag the bits */
        LowerCtl2();            /* Hey you! I got the bits */
        while( Ctl1Hi() ) {     /* Wait till he heard us */
            TWIDDLE_THUMBS;
        }
        break;
    case FMR_VAL:
        /*
         * We're talking to the FMR which can't RaiseCtl1/LowerCtl1
         *
         * get the low nibble
         */
        RaiseCtl2();                    /* ready to read */
        while( FM_Ctl1Lo() ) {          /* wait for data */
            TWIDDLE_THUMBS;
        }
        data = ReadData() & 0x0f;       /* bag the bits */
        LowerCtl2();                    /* Hey you! I got the bits */
        while( FM_Ctl1Hi() ) {          /* Wait till he heard us */
            TWIDDLE_THUMBS;
        }
        /*
         * get the high nibble
         */
        RaiseCtl2();                    /* ready to read */
        while( FM_Ctl1Lo() ) {          /* wait for data */
            TWIDDLE_THUMBS;
        }
        data |= ( ReadData() << 4 );    /* bag the bits */
        LowerCtl2();                    /* Hey you! I got the bits */
        while( FM_Ctl1Hi() ) {          /* Wait till he heard us */
            TWIDDLE_THUMBS;
        }
        break;
    case LAPLINK_VAL:
        /*
         * get the low nibble
         */
        LL_RaiseCtl2();                 /* ready to read */
        while( LL_Ctl1Lo() ) {          /* wait for data */
            TWIDDLE_THUMBS;
        }
        data = LL_ReadData();           /* bag the bits */
        LL_LowerCtl2();                 /* Hey you! I got the bits */
        while( LL_Ctl1Hi() ) {          /* Wait till he heard us */
            TWIDDLE_THUMBS;
        }
        /*
         * get the high nibble
         */
        LL_RaiseCtl2();                 /* ready to read */
        while( LL_Ctl1Lo() ) {          /* wait for data */
            TWIDDLE_THUMBS;
        }
        data |= ( LL_ReadData() << 4 ); /* bag the bits */
        LL_LowerCtl2();                 /* Hey you! I got the bits */
        while( LL_Ctl1Hi() ) {          /* Wait till he heard us */
            TWIDDLE_THUMBS;
        }
        break;
    case DUTCHMAN_VAL:
        /*
         * get the low nibble
         */
        FD_RaiseCtl2();                 /* ready to read */
        while( FD_Ctl1Lo() ) {          /* wait for data */
            TWIDDLE_THUMBS;
        }
        data = FD_ReadData();           /* bag the bits */
        FD_LowerCtl2();                 /* Hey you! I got the bits */
        while( FD_Ctl1Hi() ) {          /* Wait till he heard us */
            TWIDDLE_THUMBS;
        }
        /*
         * get the high nibble
         */
        FD_RaiseCtl2();                 /* ready to read */
        while( FD_Ctl1Lo() ) {          /* wait for data */
            TWIDDLE_THUMBS;
        }
        data |= ( FD_ReadData() << 4 ); /* bag the bits */
        FD_LowerCtl2();                 /* Hey you! I got the bits */
        while( FD_Ctl1Hi() ) {          /* Wait till he heard us */
            TWIDDLE_THUMBS;
        }
        break;
    }
    return( data );
}


/*
 * if wait is not KEEP or RELINQUISH it is the latest time that this
 * operation should take before it times out
 */
static int DataPutByte( unsigned char data, unsigned long wait )
{
    dbgrtn( "\r\n-DataPut-" );
    switch( hwdata.cable_type ) {
    case WATCOM_VAL:
        while( Ctl2Lo() ) {             /* wait till he's ready to read */
            TWIDDLE_THUMBS;
        }
        WriteData( data );              /* write the data */
        RaiseCtl1();                    /* tell him the data's there */
        while( Ctl2Hi() ) {             /* wait till he got the bits */
            TWIDDLE_THUMBS;
        }
        LowerCtl1();                    /* clear control line */
        break;
    case FMR_VAL:
        /*
         * We're talking to the FMR which can RaiseCtl2/LowerCtl2
         */
        while( Ctl2Lo() ) {             /* wait till he's ready to read */
            TWIDDLE_THUMBS;
        }
        WriteData( data );              /* write the data */
        RaiseCtl1();                    /* tell him the data's there */
        while( Ctl2Hi() ) {             /* wait till he got the bits */
            TWIDDLE_THUMBS;
        }
        LowerCtl1();                    /* clear control line */
        break;
   case LAPLINK_VAL:
        /*
         * send low nibble
         */
        while( LL_Ctl2Lo() ) {          /* wait till he's ready to read */
            TWIDDLE_THUMBS;
        }
        LL_WriteData( data & 0x0f );    /* write the data */
                                        /* and tell him the data's there */
        while( LL_Ctl2Hi() ) {          /* wait till he got the bits */
            TWIDDLE_THUMBS;
        }
        LL_LowerCtl1();                 /* clear control line */
        /*
         * send high nibble
         */
        while( LL_Ctl2Lo() ) {          /* wait till he's ready to read */
            TWIDDLE_THUMBS;
        }
        LL_WriteData( data >> 4 );      /* write the data */
                                        /* and tell him the data's there */
        while( LL_Ctl2Hi() ) {          /* wait till he got the bits */
            TWIDDLE_THUMBS;
        }
        LL_LowerCtl1();                 /* clear control line */
        break;
    case DUTCHMAN_VAL:
        /*
         * send low nibble
         */
        while( FD_Ctl2Lo() ) {          /* wait till he's ready to read */
            TWIDDLE_THUMBS;
        }
        FD_WriteData( data & 0x0f );    /* write the data */
        FD_RaiseCtl1();                 /* tell him the data's there */
        while( FD_Ctl2Hi() ) {          /* wait till he got the bits */
            TWIDDLE_THUMBS;
        }
        FD_LowerCtl1();                 /* clear control line */
        /*
         * send high nibble
         */
        while( FD_Ctl2Lo() ) {          /* wait till he's ready to read */
            TWIDDLE_THUMBS;
        }
        FD_WriteData( data >> 4 );      /* write the data */
        FD_RaiseCtl1();                 /* tell him the data's there */
        while( FD_Ctl2Hi() ) {          /* wait till he got the bits */
            TWIDDLE_THUMBS;
        }
        FD_LowerCtl1();                 /* clear control line */
        break;
    }
    return( 0 );
}

trap_retval RemoteGet( void *data, trap_elen len )
{
    trap_elen   get_len;
    trap_elen   i;
    char        c;

    get_len = DataGetByte( RELINQUISH );
    if( get_len & 0x80 ) {
        get_len = ((get_len & 0x7f) << 8) | DataGetByte( KEEP );
    }
    for( i = 0; i < get_len; i++ ) {
        c = DataGetByte( KEEP );
        if( i < len ) {
            *(char *)data = c;
            data = (char *)data + 1;
        }
    }
    return( get_len );
}

trap_retval RemotePut( void *data, trap_elen len )
{
    trap_elen  count;

    if( len >= 0x80 ) {
        DataPutByte( (len >> 8) | 0x80, RELINQUISH );
    }
    DataPutByte( len, ( len >= 0x80 ) ? KEEP : RELINQUISH );
    count = len;
    while( count-- > 0 ) {
        DataPutByte( *(char *)data, KEEP );
        data = (char *)data + 1;
    }
    return( len );
}

/*
 * Synch - see if server and client are ready
 */
static bool Synch( void )
{
    dbgrtn( "\r\n-Synch-" );
    switch( hwdata.cable_type ) {
    case WATCOM_VAL:
    case FMR_VAL:
        if( Ctl2Lo() ) {
            return( true );
        }
        break;
    case LAPLINK_VAL:
        if( LL_Ctl1Lo() )
            return( true );
        break;
    case DUTCHMAN_VAL:
        if( FD_Ctl1Lo() )
            return( true );
        break;
    }
    return( false );
}




static bool CountTwidle( void )
{
    unsigned char   type;

    dbgrtn( "\r\n-CountTwidle-" );
    type = ReadData();
#if defined(_DBG)
  #ifdef SERVER
    printf( "Type %2.2x ", type );
  #else
    {
        char buf[10];

        itoa( type, buf, 16 );
        cputs( " Type " ); cputs( buf );
    }
  #endif
#endif
    if( !hwdata.twidle_on ) {
        if( type == WATCOM_VAL
          || type == FMR_VAL
          || type == LAPLINK_VAL
          || type == DUTCHMAN_VAL ) {
            hwdata.twidle_on = true;
            if( type != hwdata.cable_type ) {
                hwdata.twidle_count = 0;
                hwdata.cable_type = type;
            }
        }
    } else {
        if( type != hwdata.cable_type )  {
            hwdata.twidle_count++;
            hwdata.twidle_on = false;
            if( hwdata.twidle_count == TWIDLE_NUM ) {
                return( true );
            }
        }
    }
    return( false );
}

/*
 * Twidle - send an intermittent pulse over a line to let the person
 *          at the other end know you're there
 */
static bool Twidle( bool check ) {

    unsigned            i;
    unsigned long       time;

    dbgrtn( "\r\n-Twidle-" );
    for( i = 0; i < 20; ++i ) {
        WriteData( TWIDLE_ON );
        time = Ticks() + TWIDLE_TIME;
        while( time > Ticks() ) {
            if( check ) {
                if( CountTwidle() ) {
                    return( true );
                }
            } else {
                if( Synch() ) {
                    return( true );
                }
            }
        }
        WriteData( TWIDLE_OFF );
        time = Ticks() + TWIDLE_TIME;
        while( time > Ticks() ) {
            if( check ) {
                if( CountTwidle() ) {
                    return( true );
                }
            } else {
                if( Synch() ) {
                    return( true );
                }
            }
        }
    }
    return( false );
}

static unsigned long GetLineTestWait( void )
{
    unsigned long       time;

    time = Ticks() + LINE_TEST_WAIT;
    if( time == RELINQUISH )
        time++;
    if( time == KEEP )
        time++;
    return( time );
}

/*
 * LineTest - make sure that all lines are working
 */
static bool LineTest( void )
{
    int                 data;
    int                 ret;

    dbgrtn( "\r\n-LineTest-" );
#ifdef SERVER
    for( data = 1; data != 256; data *= 2 ) {
        ret = DataPutByte( data, GetLineTestWait() );
        if( ret == TIMEOUT )
            return( false );
        ret = DataGetByte( GetLineTestWait() );
        if( ret == TIMEOUT )
            return( false );
        if( ret != data ) {
            return( false );
        }
    }
    ret = DataPutByte( DONE_LINE_TEST, GetLineTestWait() );
    if( ret == TIMEOUT ) {
        return( false );
    }
#else
    for( ;; ) {
        data = DataGetByte( GetLineTestWait() );
        if( data == TIMEOUT )
            return( false );
        if( data == DONE_LINE_TEST )
            break;
        ret = DataPutByte( data, GetLineTestWait() );
        if( ret == TIMEOUT ) {
            return( false );
        }
    }
#endif
    return( true );
}

bool RemoteConnect( void )
{
    unsigned long       time;

#ifdef SERVER
    bool                got_twidles;

    dbgrtn( "\r\n-RemoteConnect-" );
    if( !CountTwidle() )
        return( false );
    got_twidles = Twidle( false );
#else
    dbgrtn( "\r\n-RemoteConnect-" );
    if( !Twidle( true ) )
        return( false );
#endif

    switch( hwdata.cable_type ) {
    case WATCOM_VAL:
        LowerCtl1();
        LowerCtl2();
    case FMR_VAL:
        LowerCtl1();
        LowerCtl2();
    case LAPLINK_VAL:
        LL_LowerCtl1();
        break;
    case DUTCHMAN_VAL:
        FD_LowerCtl1();
        break;
    }
#ifdef SERVER
    if( !got_twidles ) {
#endif
        time = Ticks() + SYNCH_WAIT;
        for( ;; ) {
            if( Synch() ) {
                break;
            } else if( time < Ticks() ) {
                return( false );
            }
        }
#ifdef SERVER
    }
#endif
    if( !LineTest() )
        return( false );
    return( true );
}

void RemoteDisco( void )
{
    unsigned long       time;

    dbgrtn( "\r\n-RemoteDisco-" );
    time = Ticks() + TWIDLE_TIME;
    while( time > Ticks() )
        { /* delay while other side catches up */ }
    WriteData( TWIDLE_OFF );            /* initialize control ports */
    XX_RaiseCtl1();
    hwdata.twidle_count = 0;
    hwdata.cable_type = NULL_VAL;
    hwdata.twidle_on = false;
}

#ifdef SERVER
#ifdef TRAPGUI
const char *RemoteLinkGet( char *parms, size_t len )
{
    int     num;

    /* unused parameters */ (void)len;

    num = NumPrinters();
    if( num == 0 )
        return( TRP_ERR_parallel_port_not_present );
    if( num >= 1 && PrnAddress( 0 ) == hwdata.controller.port ) {
        parms[0] = '1';
        parms[1] = '\0';
    } else if( num >= 2 && PrnAddress( 1 ) == hwdata.controller.port ) {
        parms[0] = '2';
        parms[1] = '\0';
    } else if( num >= 3 && PrnAddress( 2 ) == hwdata.controller.port ) {
        parms[0] = '3';
        parms[1] = '\0';
    } else {
        sprintf( parms, "P%X", hwdata.controller.port );
    }
    return( NULL );
}
#endif
#endif

const char *RemoteLinkSet( const char *parms )
{
    int             printer;
    unsigned short  port;
    char            ch;

    ch = *parms++;
    port = 0;
    if( ch == '\0' || ch >= '1' && ch <= '3' && *parms == '\0' ) {
        if( ch == '\0' )
            ch = '1';
        printer = ch - '1';
        if( NumPrinters() <= printer ) {
            return( TRP_ERR_parallel_port_not_present );
        }
        port = PrnAddress( printer );
    } else if( ch == 'p' || ch == 'P' ) {
        for( ;; ) {
            ch = *parms++;
            if( ch == 0 )
                break;
            if( ch == ' ' )
                break;
            if( ch == '\t' )
                break;
            if( ch >= 'A' && ch <= 'F' ) {
                ch = ch - 'A' + 0x0a;
            } else if( ch >= 'a' && ch <= 'f' ) {
                ch = ch - 'a' + 0x0a;
            } else if( ch >= '0' && ch <= '9' ) {
                ch = ch - '0';
            } else {
                return( InvalidPort );
            }
            port <<= 4;
            port += ch;
        }
    } else {
        return( InvalidPort );
    }
    hwdata.controller.port = port;
    return( NULL );
}

const char *RemoteLink( const char *parms, bool server )
{
    const char  *err;

    /* unused parameters */ (void)server;

    dbgrtn( "\r\n-RemoteLink-" );

    err = InitSys();
    if( err != NULL ) {
        return( err );
    }
    if( parms != NULL ) {
        err = RemoteLinkSet( parms );
        if( err != NULL ) {
            return( err );
        }
    }
    if( !AccessPorts( hwdata.controller.port, 3 ) ) {
        return( TRP_ERR_cannot_access_parallel_ports );
    }
    WriteData( TWIDLE_OFF );            /* initialize the control ports */
    XX_RaiseCtl1();
    hwdata.twidle_on = false;
    return( NULL );
}

void RemoteUnLink( void )
{
    FiniSys();
    FreePorts( hwdata.controller.port, 3 );
    hwdata.controller.port = 0;
}
