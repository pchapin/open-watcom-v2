/****************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2002-2022 The Open Watcom Contributors. All Rights Reserved.
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
* Description:  WHEN YOU FIGURE OUT WHAT THIS FILE DOES, PLEASE
*               DESCRIBE IT HERE!
*
****************************************************************************/


#define FECALL_X86_SPECIAL_RETURN           ( 0x00000001LL << _TARG_AUX_SHIFT )
#define FECALL_X86_SPECIAL_STRUCT_RETURN    ( 0x00000002LL << _TARG_AUX_SHIFT )
#define FECALL_X86_FAR_CALL                 ( 0x00000004LL << _TARG_AUX_SHIFT )
#define FECALL_X86_INTERRUPT                ( 0x00000008LL << _TARG_AUX_SHIFT )
#define FECALL_X86_LOAD_DS_ON_CALL          ( 0x00000010LL << _TARG_AUX_SHIFT )
#define FECALL_X86_LOAD_DS_ON_ENTRY         ( 0x00000020LL << _TARG_AUX_SHIFT )
#define FECALL_X86_MODIFY_EXACT             ( 0x00000040LL << _TARG_AUX_SHIFT )
#define FECALL_X86_NO_8087_RETURNS          ( 0x00000080LL << _TARG_AUX_SHIFT )
#define FECALL_X86_NO_FLOAT_REG_RETURNS     ( 0x00000100LL << _TARG_AUX_SHIFT )
#define FECALL_X86_NO_STRUCT_REG_RETURNS    ( 0x00000200LL << _TARG_AUX_SHIFT )
#define FECALL_X86_ROUTINE_RETURN           ( 0x00000400LL << _TARG_AUX_SHIFT )
#define FECALL_X86_FAT_WINDOWS_PROLOG       ( 0x00000800LL << _TARG_AUX_SHIFT )
#define FECALL_X86_GENERATE_STACK_FRAME     ( 0x00001000LL << _TARG_AUX_SHIFT )
#define FECALL_X86_EMIT_FUNCTION_NAME       ( 0x00002000LL << _TARG_AUX_SHIFT )
#define FECALL_X86_GROW_STACK               ( 0x00004000LL << _TARG_AUX_SHIFT )
#define FECALL_X86_PROLOG_HOOKS             ( 0x00008000LL << _TARG_AUX_SHIFT )
#define FECALL_X86_THUNK_PROLOG             ( 0x00010000LL << _TARG_AUX_SHIFT )
#define FECALL_X86_EPILOG_HOOKS             ( 0x00020000LL << _TARG_AUX_SHIFT )
#define FECALL_X86_FAR16_CALL               ( 0x00040000LL << _TARG_AUX_SHIFT )
#define FECALL_X86_TOUCH_STACK              ( 0x00080000LL << _TARG_AUX_SHIFT )
#define FECALL_X86_LOAD_RDOSDEV_ON_ENTRY    ( 0x00100000LL << _TARG_AUX_SHIFT )
#define FECALL_X86_FARSS                    ( 0x00200000LL << _TARG_AUX_SHIFT )
#define FECALL_X86_PARMS_STACK_RESERVE      ( 0x00400000LL << _TARG_AUX_SHIFT )
#define FECALL_X86_PARMS_PREFER_REGS        ( 0x00800000LL << _TARG_AUX_SHIFT )
#define LAST_TARG_AUX_ATTRIBUTE         ( 0x00800000LL << _TARG_AUX_SHIFT )

#if LAST_TARG_AUX_ATTRIBUTE == 0
    #error Overflowed a long long constant in x86auxa.h
#endif

typedef unsigned long long      call_class;
typedef unsigned long long      call_class_target;
