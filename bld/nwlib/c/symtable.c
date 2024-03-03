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
* Description:  Librarian symbol table processing.
*
****************************************************************************/


#include "wlib.h"
#include <errno.h>
#include "ar.h"
#include "convert.h"
#include "roundmac.h"

#include "clibext.h"


#define HASH_SIZE                   256
#define WLIB_LIST_MAX_MESSAGE_LEN   511
#define WLIB_LIST_LINE_WIDTH        79
#define WLIB_LIST_OFF_COLUMN        40

static char             listMsg[WLIB_LIST_MAX_MESSAGE_LEN + 1];
static size_t           msgLength = 0;

static sym_table        FileTable;
static sym_file         *CurrFile;
static sym_entry        **HashTable;
static sym_entry        **SortedSymbols;

static char             *padding_string;
static size_t           padding_string_len;

static unsigned long    NumFiles;
static size_t           NumSymbols;
static file_offset      TotalNameLength;
static file_offset      TotalFFNameLength;
static file_offset      TotalSymbolLength;

void InitFileTab( void )
/**********************/
{
    FileTable.first = NULL;
    FileTable.add_to = &FileTable.first;
    SortedSymbols = NULL;
    HashTable = MemAllocGlobal( HASH_SIZE * sizeof( HashTable[0] ) );
    memset( HashTable, 0, HASH_SIZE * sizeof( HashTable[0] ) );
}

static void FiniSymFile( sym_file *sfile )
/****************************************/
{
    sym_entry           *sym;
    sym_entry           *next_sym;
    elf_import_sym      *temp;

    for( sym = sfile->first; sym != NULL; sym = next_sym ) {
        next_sym = sym->next;
        MemFreeGlobal( sym );
    }
    MemFreeGlobal( sfile->full_name );
    MemFreeGlobal( sfile->arch.name );
    MemFreeGlobal( sfile->arch.ffname );
    if( sfile->import != NULL ) {
        switch( sfile->import->type ) {
        case ELF:
        case ELFRENAMED:
            for( temp = sfile->import->u.elf.symlist; temp != NULL;
                         temp = sfile->import->u.elf.symlist ) {
                sfile->import->u.elf.symlist = temp->next;
                MemFreeGlobal( temp->name );
                MemFreeGlobal( temp );
            }
            MemFreeGlobal( sfile->import->DLLName );
            break;
        default:
            MemFreeGlobal( sfile->import->DLLName );
            MemFreeGlobal( sfile->import->u.sym.symName );
            MemFreeGlobal( sfile->import->u.sym.exportedName );
            break;
        }
        MemFreeGlobal( sfile->import );
    }
    MemFreeGlobal( sfile );
}


void CleanFileTab( void )
/***********************/
{
    sym_file    *last = NULL;
    sym_file    *curr;
    sym_file    *next;

    for( curr = FileTable.first; curr != NULL ; curr = next ) {
        next = curr->next;
        /*
         * If curr->first is NULL then either this file contains no
         * symbols or we are ignoring all of them.  Remove the file.
         */
        if( curr->first == NULL ) {
            if( last != NULL ) {
                last->next = curr->next;
            } else {
                FileTable.first = curr->next;
            }

            if( &(curr->next) == FileTable.add_to ) {
                if( last != NULL ) {
                    FileTable.add_to = &(last->next);
                } else {
                    FileTable.add_to = &(FileTable.first);
                }
            }

            FiniSymFile( curr );
        } else {
            last = curr;
        }
    }
}


void FiniFileTab( void )
/***********************/
{
    sym_file    *sfile;

    while( (sfile = FileTable.first) != NULL ) {
        FileTable.first = sfile->next;
        FiniSymFile( sfile );
    }
    FileTable.add_to = &FileTable.first;
    if( SortedSymbols != NULL ) {
        MemFreeGlobal( SortedSymbols );
        SortedSymbols = NULL;
    }
    MemFreeGlobal( HashTable );
    HashTable = NULL;
}


static int Hash( const char *string, unsigned *plen )
/***************************************************/
{
    unsigned long       g;
    unsigned long       h;

    h = 0;
    *plen = 0;
    while( *string != 0 ) {
        h = ( h << 4 ) + *string;
        if( (g = (h & 0xf0000000)) != 0 ) {
            h = h ^ ( g >> 24 );
            h = h ^ g;
        }
        ++string;
        ++*plen;
    }
    return( h % HASH_SIZE );
}


static void RemoveFromHashTable( sym_entry *sym )
/***********************************************/
{
    sym_entry       *hash_sym;
    sym_entry       *hash_prev;
    int             hval;
    unsigned        len;

    hash_prev = NULL;
    hval = Hash( sym->name, &len );
    for( hash_sym = HashTable[hval]; hash_sym != NULL; hash_sym = hash_sym->hash_next ) {
        if( hash_sym == sym ) {
            if( hash_prev == NULL ) {
                HashTable[hval] = hash_sym->hash_next;
            } else {
                hash_prev->hash_next = hash_sym->hash_next;
            }
            break;
        } else {
            hash_prev = hash_sym;
        }
    }
}


static sym_file *NewSymFile( arch_header *arch, file_type obj_type )
/******************************************************************/
{
    sym_file    *sfile;

    sfile = MemAllocGlobal( sizeof( sym_file ) );
    sfile->obj_type = obj_type;
    sfile->arch = *arch;
    sfile->first = NULL;
    sfile->next = NULL;
    sfile->import = NULL;
    sfile->inlib_offset = 0;
    sfile->full_name = DupStrGlobal( arch->name );
    if( Options.trim_path ) {
        sfile->arch.name = DupStrGlobal( TrimPath( arch->name ) );
    } else {
        sfile->arch.name = DupStrGlobal( arch->name );
    }
    sfile->name_length = strlen( sfile->arch.name );
    if( arch->ffname != NULL ) {
        sfile->arch.ffname = DupStrGlobal( arch->ffname );
        sfile->ffname_length = strlen( sfile->arch.ffname );
    } else {
        sfile->ffname_length = 0;
    }
    *(FileTable.add_to) = sfile;
    FileTable.add_to = &sfile->next;
    return( sfile );
}



static int CompSyms( const void *ap, const void *bp )
/***************************************************/
{
    const sym_entry *a = *(const sym_entry **)ap;
    const sym_entry *b = *(const sym_entry **)bp;
    return( strcmp( a->name, b->name ) );
}

static void WriteFileHeader( arch_header *arch )
/**********************************************/
{
    ar_header   ar;

    CreateARHeader( &ar, arch );
    WriteNew( &ar, AR_HEADER_SIZE );
}

static void WritePadding( file_offset size )
/******************************************/
{
    if( size & 1 ) {
        WriteNew( padding_string, padding_string_len );
    }
}

static void SortSymbols( void )
/*****************************/
{
    sym_file    *sfile;
    sym_entry   *sym;
    sym_entry   **sym_curr;
    size_t      i;
    size_t      name_length = 0;

    NumFiles = 0;
    NumSymbols = 0;
    TotalNameLength = 0;
    TotalFFNameLength = 0;
    TotalSymbolLength = 0;
    for( sfile = FileTable.first; sfile != NULL; sfile = sfile->next ) {
        ++NumFiles;
        switch( Options.libtype ) {
        case WL_LTYPE_AR:
            /*
             * Always using "full" filename for AR
             */
            if( sfile->arch.ffname != NULL ) {
                name_length = sfile->ffname_length;
            } else {
                sfile->ffname_length = 0;
                name_length = sfile->name_length;
            }
            if( Options.ar_libformat == AR_FMT_BSD ) {
                /*
                 * BSD doesn't use special file name table
                 */
                sfile->name_offset = -1;
                name_length = 0;
            } else if( Options.ar_libformat == AR_FMT_GNU ) {
                if( name_length < AR_NAME_LEN ) {
                    sfile->name_offset = -1;
                    name_length = 0;
                } else {
                    sfile->name_offset = TotalNameLength;
                    name_length += AR_LONG_NAME_END_STRING_LEN;   // + "/\n"
                }
            } else {
                sfile->name_offset = TotalNameLength;
                name_length += 1;     // + "\n"
            }
            break;
        case WL_LTYPE_MLIB:
            /*
             * If no full filename, assume name is full, and trim
             * it to get non-full filename.
             */
            if( sfile->arch.ffname == NULL ) {
                sfile->arch.ffname = sfile->arch.name;
                sfile->ffname_length = strlen( sfile->arch.ffname );
                sfile->arch.name = DupStrGlobal( TrimPath( sfile->arch.ffname ) );
                sfile->name_length = strlen( sfile->arch.name );
            }
            name_length = sfile->name_length;
            sfile->name_offset = TotalNameLength;
            name_length += LIB_LONG_NAME_END_STRING_LEN;  // + "/\n"
            break;
        }
        TotalNameLength += name_length;
        TotalFFNameLength += sfile->ffname_length + 1;
        for( sym = sfile->first; sym != NULL; sym = sym->next ) {
            ++NumSymbols;
            TotalSymbolLength += sym->len + 1;
        }
    }

    if( NumSymbols == 0 ) {
        SortedSymbols = NULL;
        if( !Options.quiet ) {
            Warning( ERR_NO_SYMBOLS );
        }
    } else {
        SortedSymbols = MemAllocGlobal( NumSymbols * sizeof( SortedSymbols[0] ) );
    }

    sym_curr = SortedSymbols;
    for( sfile = FileTable.first; sfile != NULL; sfile = sfile->next ) {
        for( sym = sfile->first; sym != NULL; sym = sym->next ) {
            *sym_curr = sym;
            ++sym_curr;
        }
    }

    qsort( SortedSymbols, NumSymbols, sizeof( sym_entry * ), CompSyms );
    /*
     * re-hook symbols onto files in sorted order
     */
    for( sfile = FileTable.first; sfile != NULL; sfile = sfile->next ) {
        sfile->first = NULL;
    }

    for( i = NumSymbols; i-- > 0; ) {
        sym = SortedSymbols[i];
        sym->next = sym->file->first;
        sym->file->first = sym;
    }
}

void WriteFileBody( sym_file *sfile )
/***********************************/
{
    libfile     io;

    if( sfile->import == NULL ) {
        if( sfile->inlib_offset == 0 ) {
            io = LibOpen( sfile->full_name, LIBOPEN_READ );
        } else {
            io = InLibHandle( sfile->inlib );
            LibSeek( io, sfile->inlib_offset, SEEK_SET );
        }
        if( sfile->obj_type == WL_FTYPE_OMF ) {
            OmfCopy( io, sfile );
        } else {
            Copy( io, NewLibrary, sfile->arch.size );
        }
        if( sfile->inlib_offset == 0 ) {
            LibClose( io );
        }
    } else {
        switch( sfile->obj_type ) {
        case WL_FTYPE_ELF:
            ElfWriteImport( NewLibrary, sfile );
            break;
        case WL_FTYPE_COFF:
            CoffWriteImport( NewLibrary, sfile, Options.coff_import_long );
            break;
        case WL_FTYPE_OMF:
            OmfWriteImport( sfile );
            break;
        }
    }
}

static void WriteOmfLibTrailer( void )
{
    OmfRecord   *rec;
    size_t      size;

    size = DIC_REC_SIZE - (unsigned long)LibTell( NewLibrary ) % DIC_REC_SIZE;
    rec = MemAlloc( size );
    rec->basic.type = LIB_TRAILER_REC;
    rec->basic.len = GET_LE_16( size - 3 );
    memset( rec->basic.contents, 0, size - 3 );
    WriteNew( rec, size );
    MemFree( rec );
}

static void WriteOmfLibHeader( unsigned_32 dict_offset, unsigned_16 dict_size )
/******************************************************************************
 * i didn't use omfRec because page size can be quite big
 */
{
    OmfLibHeader    lib_header;

    LibSeek( NewLibrary, 0, SEEK_SET );
    lib_header.type = LIB_HEADER_REC;
    lib_header.page_size = GET_LE_16( Options.page_size - 3 );
    lib_header.dict_offset = GET_LE_32( dict_offset );
    lib_header.dict_size = GET_LE_16( dict_size );
    if( Options.respect_case ) {
        lib_header.flags = 1;
    } else {
        lib_header.flags = 0;
    }
    WriteNew( &lib_header, sizeof( lib_header ) );
}

static unsigned_16 OptimalPageSize( void )
/****************************************/
{
    unsigned    i;
    sym_file    *sfile;
    file_offset offset;
    unsigned_16 page_size;

    page_size = 0;
    for( i = 4; i < 16; i++ ) {
        page_size = 1 << i;
        offset = page_size;
        for( sfile = FileTable.first; sfile != NULL; sfile = sfile->next ) {
            if( offset / page_size > (unsigned long)USHRT_MAX )
                break;
            offset += __ROUND_UP_SIZE( sfile->arch.size, page_size );
        }
        if( sfile == NULL ) {
            break;
        }
    }
    return( page_size );
}

static void WriteOmfFileTable( void )
/***********************************/
{
    sym_file    *sfile;
    unsigned    num_blocks;
    long        dict_offset;

    if( Options.page_size == 0 ) {
        Options.page_size = DEFAULT_PAGE_SIZE;
    } else if( Options.page_size == (unsigned_16)-1 ) {
        Options.page_size = OptimalPageSize();
    }
    PadOmf( true );

    for( sfile = FileTable.first; sfile != NULL; sfile = sfile->next ) {
        WriteOmfFile( sfile );
    }
    WriteOmfLibTrailer();
    dict_offset = LibTell( NewLibrary );
    num_blocks = WriteOmfDict( FileTable.first );
    WriteOmfLibHeader( dict_offset, num_blocks );
}

static void WriteArMlibFileTable( void )
/**************************************/
{
    arch_header     arch;
    sym_file        *sfile;
    sym_entry       *sym;
    file_offset     dict1_size = 0;
    file_offset     dict2_size = 0;
    file_offset     header_size = 0;
    size_t          i;
    time_t          currenttime = time( NULL );
    file_offset     obj_offset;
    int             index;
    bool            isBSD;


    SortSymbols();
    /*
     * figure out this dictionary sizes
     */
    switch( Options.libtype ) {
    case WL_LTYPE_AR:
        dict1_size = ( NumSymbols + 1 ) * sizeof(unsigned_32) + __ROUND_UP_SIZE_EVEN( TotalSymbolLength );

        header_size = AR_IDENT_LEN + AR_HEADER_SIZE + dict1_size;

        switch( Options.ar_libformat ) {
        case AR_FMT_BSD:
            dict2_size = 0;

            padding_string     = "\0";
            padding_string_len = 1;
            break;
        case AR_FMT_GNU:
            dict2_size = 0;

            if( TotalNameLength > 0 ) {
                header_size += AR_HEADER_SIZE + __ROUND_UP_SIZE_EVEN( TotalNameLength );
            }

            padding_string     = "\0";
            padding_string_len = 1;
            break;
        default:
            dict2_size = ( NumFiles + 1 ) * sizeof( unsigned_32 )
                        + sizeof( unsigned_32 ) + NumSymbols * sizeof( unsigned_16 )
                        + __ROUND_UP_SIZE_EVEN( TotalSymbolLength );

            header_size += AR_HEADER_SIZE + dict2_size;

            if( TotalNameLength > 0 ) {
                header_size += AR_HEADER_SIZE + __ROUND_UP_SIZE_EVEN( TotalNameLength );
            }

            padding_string     = AR_FILE_PADDING_STRING;
            padding_string_len = AR_FILE_PADDING_STRING_LEN;
            break;
        }
        break;
    case WL_LTYPE_MLIB:
        dict1_size = 0;

        dict2_size = ( NumSymbols + 1 ) * sizeof( unsigned_32 ) + NumSymbols
                    + TotalSymbolLength;

        header_size = LIBMAG_LEN + LIB_CLASS_LEN + LIB_DATA_LEN
                    + LIB_HEADER_SIZE + __ROUND_UP_SIZE_EVEN( dict2_size )
                    + LIB_HEADER_SIZE + __ROUND_UP_SIZE_EVEN( TotalNameLength )
                    + LIB_HEADER_SIZE + __ROUND_UP_SIZE_EVEN( TotalFFNameLength );

        padding_string     = LIB_FILE_PADDING_STRING;
        padding_string_len = LIB_FILE_PADDING_STRING_LEN;
        break;
    }
    /*
     * calculate the object files offsets
     */
    index = 0;
    obj_offset = 0;
    isBSD = ( ( Options.libtype == WL_LTYPE_AR ) && ( Options.ar_libformat == AR_FMT_BSD ) );
    for( sfile = FileTable.first; sfile != NULL; sfile = sfile->next ) {
        sfile->u.new_offset = obj_offset + header_size;
        sfile->index = ++index;
        if( isBSD
          && ( sfile->name_length > AR_NAME_LEN
          || strchr( sfile->arch.name, ' ' ) != NULL ) ) {
            obj_offset += __ROUND_UP_SIZE_EVEN( sfile->arch.size + sfile->name_length ) + AR_HEADER_SIZE;
        } else {
            obj_offset += __ROUND_UP_SIZE_EVEN( sfile->arch.size ) + AR_HEADER_SIZE;
        }
    }

    switch( Options.libtype ) {
    case WL_LTYPE_AR:
        WriteNew( AR_IDENT, AR_IDENT_LEN );
        break;
    case WL_LTYPE_MLIB:
        WriteNew( LIBMAG, LIBMAG_LEN );
        WriteNew( LIB_CLASS_DATA_SHOULDBE, LIB_CLASS_LEN + LIB_DATA_LEN );
        break;
    }
    /*
     * write the useless dictionary
     */
    arch.date = currenttime;
    arch.uid = 0;
    arch.gid = 0;
    arch.mode = 0;
    if( dict1_size > 0 ) {
        arch.size = dict1_size;     // word round size
        arch.name = "/";
        WriteFileHeader( &arch );

        WriteBigEndian32( NumSymbols );
        for( sfile = FileTable.first; sfile != NULL; sfile = sfile->next ) {
            for( sym = sfile->first; sym != NULL; sym = sym->next ) {
                WriteBigEndian32( sym->file->u.new_offset );
            }
        }
        for( sfile = FileTable.first; sfile != NULL; sfile = sfile->next ) {
            for( sym = sfile->first; sym != NULL; sym = sym->next ) {
                WriteNew( sym->name, sym->len + 1 );
            }
        }
        WritePadding( TotalSymbolLength );
    }
    /*
     * write the useful dictionary
     */
    if( dict2_size > 0 ) {
        arch.size = dict2_size;     // word round size
        arch.name = "/";
        WriteFileHeader( &arch );

        if( Options.libtype == WL_LTYPE_AR ) {
            WriteLittleEndian32( NumFiles );
            for( sfile = FileTable.first; sfile != NULL; sfile = sfile->next ) {
                WriteLittleEndian32( sfile->u.new_offset );
            }
        }

        WriteLittleEndian32( NumSymbols );
        switch( Options.libtype ) {
        case WL_LTYPE_AR:
            for( i = 0; i < NumSymbols; ++i ) {
                WriteLittleEndian16( SortedSymbols[i]->file->index );
            }
            break;
        case WL_LTYPE_MLIB:
            for( i = 0; i < NumSymbols; ++i ) {
                WriteLittleEndian32( SortedSymbols[i]->file->index );
            }
            for( i = 0; i < NumSymbols; ++i ) {
                WriteNew( &(SortedSymbols[i]->info), 1 );
            }
            break;
        }
        for( i = 0; i < NumSymbols; ++i ) {
            WriteNew( SortedSymbols[i]->name, SortedSymbols[i]->len + 1 );
        }
        switch( Options.libtype ) {
        case WL_LTYPE_AR:
            WritePadding( TotalSymbolLength );
            break;
        case WL_LTYPE_MLIB:
            WritePadding( dict2_size );
            break;
        }
    }
    /*
     * write the string table
     */
    if( TotalNameLength > 0 ) {
        char    *stringpad;
        size_t  stringpadlen;

        if( Options.libtype == WL_LTYPE_MLIB ) {
            stringpad    = LIB_LONG_NAME_END_STRING;
            stringpadlen = LIB_LONG_NAME_END_STRING_LEN;
        } else if( Options.ar_libformat == AR_FMT_GNU ) {
            stringpad    = AR_LONG_NAME_END_STRING;
            stringpadlen = AR_LONG_NAME_END_STRING_LEN;
        } else if( Options.ar_libformat == AR_FMT_BSD ) {
            stringpad    = NULL;
            stringpadlen = 0;
        } else {
            stringpad    = "\0";
            stringpadlen = 1;
        }
        arch.size = TotalNameLength;        // real size
        arch.name = "//";
        WriteFileHeader( &arch );
        for( sfile = FileTable.first; sfile != NULL; sfile = sfile->next ) {
            if( sfile->name_offset == -1 )
                continue;
            /*
             * Always write the "full" filename for AR
             */
            if( Options.libtype == WL_LTYPE_AR
              && sfile->arch.ffname != NULL ) {
                WriteNew( sfile->arch.ffname, sfile->ffname_length );
            } else {
                WriteNew( sfile->arch.name, sfile->name_length );
            }
            WriteNew( stringpad, stringpadlen );
        }
        WritePadding( TotalNameLength );
    }
    /*
     * write the full filename table
     */
    if( Options.libtype == WL_LTYPE_MLIB ) {
        arch.size = TotalFFNameLength;      // real size
        arch.name = "///";
        WriteFileHeader( &arch );
        for( sfile = FileTable.first; sfile != NULL; sfile = sfile->next ) {
            WriteNew( sfile->arch.ffname, sfile->ffname_length + 1 );
        }
        WritePadding( TotalFFNameLength );
    }

    for( sfile = FileTable.first; sfile != NULL; sfile = sfile->next ) {
        char        buff[AR_NAME_LEN + 1];
        bool        append_name;

        append_name = false;
        arch = sfile->arch;
        if( sfile->name_offset == -1 ) {
            if( Options.ar_libformat == AR_FMT_BSD ) {
                /*
                 * BSD append file name after header and before file image if it is longer then
                 *  max.length or it contains space
                 */
                if( sfile->name_length > AR_NAME_LEN
                  || strchr( sfile->arch.name, ' ' ) != NULL ) {
                    append_name = true;
                    arch.size += sfile->name_length;
                    sprintf( buff, AR_NAME_CONTINUED_AFTER "%lu", (unsigned long)sfile->name_length );
                    arch.name = buff;
                } else {
                    arch.name = sfile->arch.name;
                }
            } else {        // COFF, GNU
                strcpy( buff, sfile->arch.name );
                buff[sfile->name_length] = AR_NAME_END_CHAR;
                buff[sfile->name_length + 1] = '\0';
                arch.name = buff;
            }
        } else {
            sprintf( buff, "/%ld", sfile->name_offset );
            arch.name = buff;
        }
        WriteFileHeader( &arch );
        if( append_name ) {
            WriteNew( sfile->arch.name, sfile->name_length );
        }
        WriteFileBody( sfile );
        WritePadding( arch.size );
    }
}

void WriteFileTable( void )
/*************************/
{
    if( Options.libtype == WL_LTYPE_NONE
      && Options.omf_found ) {
        if( Options.coff_found ) {
            Options.libtype = WL_LTYPE_AR;
        } else {
            Options.libtype = WL_LTYPE_OMF;
        }
    }
    if( Options.coff_found
      && (Options.libtype == WL_LTYPE_NONE
      || Options.libtype == WL_LTYPE_OMF) ) {
        Options.libtype = WL_LTYPE_AR;
    }
    if( Options.elf_found
      && (Options.libtype == WL_LTYPE_NONE
      || Options.libtype == WL_LTYPE_OMF) ) {
        Options.libtype = WL_LTYPE_AR;
    }
    if( Options.libtype == WL_LTYPE_AR
      || Options.libtype == WL_LTYPE_MLIB ) {
        WriteArMlibFileTable();
    } else {
        WriteOmfFileTable();
    }
}

void AddSym( const char *name, symbol_strength strength, unsigned char info )
/***************************************************************************/
{
    sym_entry   *hash_sym;
    sym_entry   **owner;
    int         hval;
    unsigned    name_len;

    hval = Hash( name, &name_len );
    for( hash_sym = HashTable[hval]; hash_sym != NULL; hash_sym = hash_sym->hash_next ) {
        if( hash_sym->len != name_len )
            continue;
        if( SymbolNameCmp( hash_sym->name, name ) == 0 ) {
            if( strength > hash_sym->strength ) {
                owner = &hash_sym->file->first;
                while( *owner != hash_sym ) {
                    owner = &(*owner)->next;
                }
                *owner = hash_sym->next;
                owner = HashTable + hval;
                while( *owner != hash_sym ) {
                    owner = &(*owner)->hash_next;
                }
                *owner = hash_sym->hash_next;
                MemFreeGlobal( hash_sym );
                break; //db
            } else if( strength == hash_sym->strength ) {
                if( strength == SYM_STRONG ) {
                    if( !Options.quiet ) {
                        Warning( ERR_DUPLICATE_SYMBOL, FormSym( name ) );
                    }
                }
            }
            return;
        }
    }
    hash_sym = MemAllocGlobal( sizeof( sym_entry ) + name_len );
    hash_sym->len = name_len;
    hash_sym->strength = strength;
    hash_sym->info = info;
    memcpy( hash_sym->name, name, name_len + 1 );
    hash_sym->next = CurrFile->first;
    CurrFile->first = hash_sym;
    hash_sym->file = CurrFile;
    hash_sym->hash_next = HashTable[hval];
    HashTable[hval] = hash_sym;
}

#ifdef DEVBUILD
void DumpFileTable( void )
{
    sym_file    *sfile;
    sym_entry   *entry;
    sym_entry   *hash_sym;
    unsigned    len;
    int         hval;
    long        files    = 0L;
    long        symbols  = 0L;

    printf("----------------------------------------------------------\n");
    printf("File Table Dump\n");
    printf("----------------------------------------------------------\n");

    for( sfile = FileTable.first; sfile != NULL; sfile = sfile->next ) {
        ++files;
        printf( "File: \"%s\"\n", sfile->full_name );
        for( entry = sfile->first; entry != NULL; entry = entry->next ) {
            ++symbols;

            hval = Hash( entry->name, &len );
            printf( "\t\"%s\" (%d, %u, \"%s\")", entry->name, hval, len,
                    (HashTable[hval] ? HashTable[hval]->name : "(NULL)") );

            for( hash_sym = entry->hash_next; hash_sym != NULL; hash_sym = hash_sym->hash_next ) {
                printf( " -> \"%s\"", hash_sym->name );
                fflush( stdout );
            }
            printf( "\n" );
        }
    }
    printf( "----------------------------------------------------------\n" );
    printf( "Files         : %ld\n", files );
    printf( "Symbols       : %ld\n", symbols );
    printf( "----------------------------------------------------------\n" );
}


void DumpHashTable( void )
{
    sym_entry   *hash_sym;
    int         hval;
    int         length;

    printf( "----------------------------------------------------------\n" );
    printf( "Hash Table Dump\n" );
    printf( "----------------------------------------------------------\n" );
    for( hval = 0; hval < HASH_SIZE; hval++ ) {
        length = 0;
        for( hash_sym = HashTable[hval]; hash_sym != NULL; hash_sym = hash_sym->next ) {
            ++length;
        }
        printf( "Offset %6d: %d\n", hval, length );
    }
    printf( "----------------------------------------------------------\n" );
}
#endif /* DEVBUILD */


bool RemoveObjectSymbols( arch_header *arch )
/*******************************************/
{
    sym_file    *sfile;
    sym_file    *sfile_prev;
    sym_entry   *sym;

    sfile_prev = NULL;
    for( sfile = FileTable.first; sfile != NULL; sfile = sfile->next ) {
        if( SymbolNameCmp( sfile->full_name, arch->name ) == 0 ) {
            if( sfile_prev != NULL ) {    /* Not deleting from head of list */
                sfile_prev->next = sfile->next;

                if( FileTable.add_to == &sfile->next ) { /* Last node in list */
                    FileTable.add_to = &sfile_prev->next;
                }
            } else {
                if( FileTable.add_to == &sfile->next ) { /* Only node in list */
                    FileTable.add_to = &FileTable.first;
                    FileTable.first = NULL;
                } else {  /* First node in the list */
                    FileTable.first = sfile->next;
                }
            }
            /*
             * remove all module symbols
             */
            for( sym = sfile->first; sym != NULL; sym = sym->next ) {
                RemoveFromHashTable( sym );
            }

            FiniSymFile( sfile );
            Options.modified = true;
            return( true );
        }
        sfile_prev = sfile;
    }
    return( false );
}

void AddObjectSymbols( libfile io, long offset, arch_header *arch )
/*****************************************************************/
{
    obj_file    *ofile;
    file_type   obj_type;

    ofile = OpenORLLibFile( io, arch->name );
    if( ofile->orl != NULL ) {
        if( ORLFileGetFormat( ofile->orl ) == ORL_COFF ) {
            if( Options.libtype == WL_LTYPE_MLIB ) {
                FatalError( ERR_NOT_LIB, ctext_WL_FTYPE_COFF, ctext_WL_LTYPE_MLIB );
            }
            Options.coff_found = true;
            obj_type = WL_FTYPE_COFF;
        } else {
            if( Options.omf_found ) {
                FatalError( ERR_MIXED_OBJ, ctext_WL_FTYPE_ELF, ctext_WL_FTYPE_OMF );
            }
            Options.elf_found = true;
            obj_type = WL_FTYPE_ELF;
        }
    } else {
        if( Options.libtype == WL_LTYPE_MLIB ) {
            FatalError( ERR_NOT_LIB, ctext_WL_FTYPE_OMF, ctext_WL_LTYPE_MLIB );
        }
        if( Options.elf_found ) {
            FatalError( ERR_MIXED_OBJ, ctext_WL_FTYPE_ELF, ctext_WL_FTYPE_OMF );
        }
        Options.omf_found = true;
        obj_type = WL_FTYPE_OMF;
    }
    CurrFile = NewSymFile( arch, obj_type );
    CurrFile->inlib_offset = offset;
    CurrFile->inlib = FindInLib( io );
    ObjWalkSymList( ofile, CurrFile );
    CloseORLLibFile( ofile );
}

void OmfMKImport( arch_header *arch, importType type,
                  long ordinal, const char *DLLname, const char *symName,
                  const char *exportedName, processor_type processor )
{
    if( Options.elf_found ) {
        FatalError( ERR_MIXED_OBJ, ctext_WL_FTYPE_ELF, ctext_WL_FTYPE_OMF );
    }
    Options.omf_found = true;
    CurrFile = NewSymFile( arch, WL_FTYPE_OMF );
    CurrFile->import = MemAllocGlobal( sizeof( import_sym ) );
    CurrFile->import->DLLName = DupStrGlobal( DLLname );
    CurrFile->import->u.sym.ordinal = ordinal;
    CurrFile->import->u.sym.symName = DupStrGlobal( symName );
    CurrFile->import->u.sym.exportedName = DupStrGlobal( exportedName );
    CurrFile->import->type = type;
    CurrFile->import->processor = processor;
    CurrFile->arch.size = OmfImportSize( CurrFile->import );
    AddSym( symName, SYM_STRONG, 0 );
}

void CoffMKImport( arch_header *arch, importType type,
                   long ordinal, const char *DLLname, const char *symName,
                   const char *exportedName, processor_type processor )
{
    if( Options.elf_found ) {
        FatalError( ERR_MIXED_OBJ, ctext_WL_FTYPE_ELF, ctext_WL_FTYPE_COFF );
    }
    Options.coff_found = true;
    CurrFile = NewSymFile( arch, WL_FTYPE_COFF );
    CurrFile->import = MemAllocGlobal( sizeof( import_sym ) );
    CurrFile->import->type = type;
    CurrFile->import->u.sym.ordinal = ordinal;
    CurrFile->import->DLLName = DupStrGlobal( DLLname );
    CurrFile->import->u.sym.symName = DupStrGlobal( symName );
    CurrFile->import->u.sym.exportedName = DupStrGlobal( exportedName );
    CurrFile->import->processor = processor;
    CurrFile->arch.size = CoffImportSize( CurrFile->import );
    switch( type ) {
    case IMPORT_DESCRIPTOR:
    case NULL_IMPORT_DESCRIPTOR:
    case NULL_THUNK_DATA:
        AddSym( symName, SYM_WEAK, 0 );
        break;
    default:
        AddSym( symName, SYM_STRONG, 0 );
        break;
    }
}

void ElfMKImport( arch_header *arch, importType type, long export_size,
                  const char *DLLname, const char *strings, Elf32_Export *export_table,
                  Elf32_Sym *sym_table, processor_type processor )
{
    int                 i;
    elf_import_sym      **temp;
    elf_import_sym      *imp_sym;

    if( Options.coff_found ) {
        FatalError( ERR_MIXED_OBJ, ctext_WL_FTYPE_ELF, ctext_WL_FTYPE_COFF );
    }
    if( Options.omf_found ) {
        FatalError( ERR_MIXED_OBJ, ctext_WL_FTYPE_ELF, ctext_WL_FTYPE_OMF );
    }
    Options.elf_found = true;
    CurrFile = NewSymFile( arch, WL_FTYPE_ELF );
    CurrFile->import = MemAllocGlobal( sizeof( import_sym ) );
    CurrFile->import->type = type;
    CurrFile->import->DLLName = DupStrGlobal( DLLname );
    CurrFile->import->u.elf.numsyms = 0;
    temp = &(CurrFile->import->u.elf.symlist);

    for( i = 0; i < export_size; i++ ) {
        if( export_table[i].exp_symbol ) {
            imp_sym = MemAllocGlobal( sizeof( elf_import_sym ) );
            imp_sym->name = DupStrGlobal( strings + sym_table[export_table[i].exp_symbol].st_name );
            imp_sym->len = strlen( imp_sym->name );
            imp_sym->ordinal = export_table[i].exp_ordinal;
            if( type == ELF ) {
                AddSym( imp_sym->name, SYM_STRONG, ELF_IMPORT_SYM_INFO );
            }

            CurrFile->import->u.elf.numsyms ++;

            *temp = imp_sym;
            temp = &(imp_sym->next);
        }
    }
    *temp = NULL;
    CurrFile->import->processor = processor;
    CurrFile->arch.size = ElfImportSize( CurrFile->import );
}

static void printVerboseTableEntryAr( sym_file *sfile )
{
    char        member_mode[11];
    char        date[128];
    time_t      t;

    member_mode[10] = '\0';
    member_mode[9] = ' ';
    if( sfile->arch.mode & AR_S_IRUSR ) {
        member_mode[0] = 'r';
    } else {
        member_mode[0] = '-';
    }
    if( sfile->arch.mode & AR_S_IWUSR ) {
        member_mode[1] = 'w';
    } else {
        member_mode[1] = '-';
    }
    if( (sfile->arch.mode & AR_S_IXUSR) == 0
      && (sfile->arch.mode & AR_S_ISUID) ) {
        member_mode[2] = 'S';
    } else if( (sfile->arch.mode & AR_S_IXUSR)
      && (sfile->arch.mode & AR_S_ISUID) ) {
        member_mode[2] = 's';
    } else if( sfile->arch.mode & AR_S_IXUSR ) {
        member_mode[2] = 'x';
    } else {
        member_mode[2] = '-';
    }
    if( sfile->arch.mode & AR_S_IRGRP ) {
        member_mode[3] = 'r';
    } else {
        member_mode[3] = '-';
    }
    if( sfile->arch.mode & AR_S_IWGRP ) {
        member_mode[4] = 'w';
    } else {
        member_mode[4] = '-';
    }
    if( (sfile->arch.mode & AR_S_IXGRP) == 0
      && (sfile->arch.mode & AR_S_ISGID) ) {
        member_mode[5] = 'S';
    } else if( (sfile->arch.mode & AR_S_IXGRP)
      && (sfile->arch.mode & AR_S_ISGID) ) {
        member_mode[5] = 's';
    } else if( sfile->arch.mode & AR_S_IXGRP ) {
        member_mode[5] = 'x';
    } else {
        member_mode[5] = '-';
    }
    if( sfile->arch.mode & AR_S_IROTH ) {
        member_mode[6] = 'r';
    } else {
        member_mode[6] = '-';
    }
    if( sfile->arch.mode & AR_S_IWOTH ) {
        member_mode[7] = 'w';
    } else {
        member_mode[7] = '-';
    }
    if( sfile->arch.mode & AR_S_IXOTH ) {
        member_mode[8] = 'x';
    } else {
        member_mode[8] = '-';
    }
    t = (time_t) sfile->arch.date;
    strftime( date, 127, "%b %d %H:%M %Y", localtime( &t ) );
    Message( "%s %u/%u %u %s %s", member_mode, sfile->arch.uid, sfile->arch.gid, sfile->arch.size,
        date, MakeFName( sfile->arch.name ) );
}


static void ListContentsAr( void )
/********************************/
{
    sym_file    *sfile;
    lib_cmd     *cmd;

    if( CmdList != NULL ) {
        for( cmd = CmdList; cmd != NULL; cmd = cmd->next ) {
            if( cmd->ops & OP_FOUND ) {
                if( Options.verbose ) {
                    for( sfile = FileTable.first; sfile != NULL; sfile = sfile->next ) {
                        if( IsSameModuleCase( sfile->arch.name, cmd->name, ( sfile->obj_type == WL_FTYPE_OMF ) ) ) {
                            if( Options.terse_listing ) {
                                Message( sfile->arch.name );
                            } else {
                                printVerboseTableEntryAr( sfile );
                            }
                            break;
                        }
                    }
                } else {
                    Message( MakeFName( cmd->name ) );
                }
            }
        }
    } else {
        if( Options.verbose ) {
            for( sfile = FileTable.first; sfile != NULL; sfile = sfile->next ) {
                printVerboseTableEntryAr( sfile );
            }
        } else {
            for( sfile = FileTable.first; sfile != NULL; sfile = sfile->next ) {
                Message( "%s", MakeFName( sfile->arch.name ) );
            }
        }
    }
}


static void listPrintWlib( FILE *fp, const char *str, ... )
{
    va_list             arglist;

    /* unused parameters */ (void)fp;

    va_start( arglist, str );
    msgLength += vsnprintf( listMsg + msgLength, WLIB_LIST_MAX_MESSAGE_LEN - msgLength, str, arglist );
    va_end( arglist );
}


static void listNewLineWlib( FILE *fp )
{
    if( fp != NULL ) {
        fprintf( fp, "%s\n", listMsg );
    } else {
        if( !Options.quiet ) {
            Message( listMsg );
        }
    }
    msgLength = 0;
    listMsg[0] = ' ';
    listMsg[1] = '\0';
}


static void fpadchWlib( FILE *fp, char ch, size_t len )
{
    /* unused parameters */ (void)fp;

    if( len > 0 ) {
        if( len > WLIB_LIST_MAX_MESSAGE_LEN - msgLength )
            len = WLIB_LIST_MAX_MESSAGE_LEN - msgLength;
        memset( listMsg + msgLength, ch, len );
        msgLength += len;
        listMsg[msgLength] = '\0';
    }
}

static void ListContentsWlib( void )
/**********************************/
{
    sym_file    *sfile;
    sym_entry   *sym;
    int         i;
    FILE        *fp;
    char        *name;
    size_t      name_len;

    if( Options.terse_listing ) {
        SortSymbols();
        for( i = 0; i < NumSymbols; ++i ) {
            sym = SortedSymbols[i];
            name = FormSym( sym->name );
            name_len = strlen( name );
            if( !Options.quiet ) {
                Message( name );
            }
        }
        return;
    }

    if( Options.list_file == NULL ) {
        Options.list_file = DupStr( MakeListName() );
    }
    if( Options.list_file[0] != '\0' ) {
        fp = fopen( Options.list_file, "w" );
        if( fp == NULL ) {
            FatalError( ERR_CANT_OPEN, Options.list_file, strerror( errno ) );
        }
    } else {
        fp = NULL;
    }
    SortSymbols();

    for( i = 0; i < NumSymbols; ++i ) {
        sym = SortedSymbols[i];
        name = FormSym( sym->name );
        name_len = strlen( name );
        listPrintWlib( fp, "%s..", name );
        fpadchWlib( fp, '.', WLIB_LIST_LINE_WIDTH - 2 - name_len - sym->file->name_length );
        listPrintWlib( fp, "%s", sym->file->arch.name );
        listNewLineWlib( fp );
    }

    listNewLineWlib( fp );

    for( sfile = FileTable.first; sfile != NULL; sfile = sfile->next ) {
        if( sfile->arch.ffname != NULL ) {
            listPrintWlib( fp, "%s ", sfile->arch.ffname );
        } else {
            listPrintWlib( fp, "%s ", sfile->arch.name );
        }
        fpadchWlib( fp, ' ', WLIB_LIST_OFF_COLUMN - 1 - sfile->name_length - 16 );
        listPrintWlib( fp, "Offset=%8.8xH", sfile->inlib_offset );
        listNewLineWlib( fp );
        for( sym = sfile->first; sym != NULL; sym = sym->next ) {
            listPrintWlib( fp, "    %s", FormSym( sym->name ) );
            listNewLineWlib( fp );
        }
        listNewLineWlib( fp );
    }

    if( fp != NULL ) {
        fclose( fp );
    }
}


void ListContents( void )
/***********************/
{
    if( Options.ar ) {
        ListContentsAr();
    } else {
        ListContentsWlib();
    }
}
