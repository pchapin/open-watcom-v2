/****************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2024      The Open Watcom Contributors. All Rights Reserved.
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


extern infilelist       *CachedFiles;
extern infilelist       *CachedLibFiles;

extern infilelist       *AllocFileEntry( const char *, const path_entry * );
extern infilelist       *AllocUniqueFileEntry( const char *, const path_entry * );
extern bool             CleanCachedHandles( void );
extern bool             DoObjOpen( infilelist *infile );
extern void             InitTokBuff( void );
extern void             FreeTokBuffs( void );
extern void             BadObject( void );
extern void             EarlyEOF( void );
extern void             ResetObjIO( void );
extern char             *MakePath( char *fullname, const char **path_list );
extern char             *MakeFileName( infilelist *infile, char *fullname );
extern bool             MakeFileNameFromList( const char **path_list, char *name, char *fullname );
