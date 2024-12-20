/****************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2015-2019 The Open Watcom Contributors. All Rights Reserved.
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
* Description:  RDOS/POSIX timer implementations
*
* Author: L. Ekblad
*
****************************************************************************/


#include "variety.h"
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <stddef.h>
#include <time.h>
#include <process.h>
#include "rdos.h"
#include "_rdos.h"

static void *timer_data = 0;

static void TimerThread( void *param )
{
    timer_data = param;

    for( ;; )
        if( !__wait_timer_event( ) )
            break;

    timer_data = 0;
}

void RdosCreateTimerThread( void )
{
    _begintimerthread( &TimerThread );
}

_WCRTLINK int timer_create( clockid_t __clk, struct sigevent *__sevp, timer_t *__tmr )
{
    /* unused parameters */ (void)__clk; (void)__sevp, (void)__tmr;

    return( 0 );
}


_WCRTLINK int timer_delete( timer_t __tmr )
{
    /* unused parameters */ (void)__tmr;

    return( 0 );
}

_WCRTLINK int timer_getoverrun( timer_t __tmr )
{
    /* unused parameters */ (void)__tmr;

    return( 0 );
}

_WCRTLINK int timer_gettime( timer_t __tmr, struct itimerspec *__v )
{
    /* unused parameters */ (void)__tmr, (void)__v;

    return( 0 );
}

_WCRTLINK int timer_settime( timer_t __tmr, int flags, struct itimerspec *__new, struct itimerspec *__old )
{
    /* unused parameters */ (void)__tmr, (void)flags, (void)__new, (void)__old;

    return( 0 );
}
