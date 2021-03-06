#include "config.h"
#include "libbeye/libbeye.h"
using namespace	usr;
/**
 * @namespace	usr_plugins_auto
 * @file        plugins/bin/sisx.c
 * @brief       This file contains implementation of decoder for SisX (EPOC)
 *              file format.
 * @version     -
 * @remark      this source file is part of Binary EYE project (BEYE).
 *              The Binary EYE (BEYE) is copyright (C) 1995 Nickols_K.
 *              All rights reserved. This software is redistributable under the
 *              licence given in the file "Licence.en" ("Licence.ru" in russian
 *              translation) distributed in the BEYE archive.
 * @note        Requires POSIX compatible development system
 *
 * @author      Nickols_K
 * @since       1995
 * @note        Development, fixes and improvements
**/
#include <sstream>
#include <iomanip>

#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "bconsole.h"
#include "beyehelp.h"
#include "colorset.h"
#include "libbeye/kbd_code.h"
#include "plugins/bin/mmio.h"
#include "plugins/disasm.h"
#include "libbeye/bstream.h"
#include "plugins/binary_parser.h"
#include "beye.h"

namespace	usr {
struct E32ImageHeader {
    unsigned long iUid1;
    unsigned long iUid2;
    unsigned long iUid3;
    unsigned long iUidCheckSum;
    unsigned char iSignature[4];
    unsigned long iHeaderCrc;
    unsigned long iModuleVersion;
    unsigned long iCompressionType;
    unsigned long iToolsVersion;
    unsigned long iTimeLo;
    unsigned long iTimeHi;
    unsigned long iFlags;
    unsigned long iCodeSize;
    unsigned long iDataSize;
    unsigned long iHeapSizeMin;
    unsigned long iHeapSizeMax;
    unsigned long iStackSize;
    unsigned long iBssSize;
    unsigned long iEntryPoint;
    unsigned long iCodeBase;
    unsigned long iDataBase;
    unsigned long iDllRefTableCount;
    unsigned long iExportDirOffset;
    unsigned long iExportDirCount;
    unsigned long iTextSize;
    unsigned long iCodeOffset;
    unsigned long iDataOffset;
    unsigned long iImportOffset;
    unsigned long iCodeRelocOffset;
    unsigned long iDataRelocOffset;
    unsigned short iProcessPriority;
    unsigned short iCpuIdentifier;
};

    class SisX_Parser : public Binary_Parser {
	public:
	    SisX_Parser(BeyeContext& b,binary_stream&,CodeGuider&,udn&);
	    virtual ~SisX_Parser();

	    virtual const char*		prompt(unsigned idx) const;

	    virtual __filesize_t	show_header() const;
	    virtual int			query_platform() const;
	private:
	    __filesize_t		show_sis3_header();

	    BeyeContext&		bctx;
	    binary_stream&		main_handle;
	    udn&			_udn;
    };
static const char* txt[]={"","","","","","","","","",""};
const char* SisX_Parser::prompt(unsigned idx) const { return txt[idx]; }

SisX_Parser::SisX_Parser(BeyeContext& b,binary_stream& h,CodeGuider& code_guider,udn& u)
	    :Binary_Parser(b,h,code_guider,u)
	    ,bctx(b)
	    ,main_handle(h)
	    ,_udn(u)
{
    unsigned char sign[4];
    unsigned long id;
    main_handle.seek(0,binary_stream::Seek_Set);
    id=main_handle.read(type_dword);
    main_handle.seek(16L,binary_stream::Seek_Set);
    binary_packet bp=main_handle.read(sizeof(sign)); memcpy(sign,bp.data(),bp.size());
    if(!((id&0x10000000UL)==0x10000000UL && memcmp(sign,"EPOC",4)==0)) throw bad_format_exception();
}
SisX_Parser::~SisX_Parser() {}
int  SisX_Parser::query_platform() const {
    unsigned id;
    struct E32ImageHeader img;
    main_handle.seek(0,binary_stream::Seek_Set);
    binary_packet bp=main_handle.read(sizeof(img)); memcpy(&img,bp.data(),bp.size());
    id=DISASM_DATA;
    if((img.iCpuIdentifier&0xF000)==0x1000) id=DISASM_CPU_IX86;
    else if((img.iCpuIdentifier&0xF000)==0x2000) id=DISASM_CPU_ARM;
    return id;
}

__filesize_t SisX_Parser::show_header() const
{
    unsigned keycode;
    TWindow* hwnd;
    const char *cpuname,*exetype;
    struct E32ImageHeader img;
    __filesize_t fpos,fpos2;
    fpos2=fpos = bctx.tell();
    main_handle.seek(0,binary_stream::Seek_Set);
    binary_packet bp=main_handle.read(sizeof(img)); memcpy(&img,bp.data(),bp.size());
    switch(img.iUid1) {
	case 0x10000079: exetype="DLL"; break;
	case 0x1000007A: exetype="EXE"; break;
	default: exetype="UNK"; break;
    }
    switch(img.iCpuIdentifier) {
	case 0x1000: cpuname="x86"; break;
	case 0x2000: cpuname="ARMv4"; break;
	case 0x2001: cpuname="ARMv5"; break;
	case 0x2002: cpuname="ARMv6"; break;
	case 0x2003: cpuname="ARMv7"; break;
	case 0x2004: cpuname="ARMv8"; break;
	case 0x2005: cpuname="ARMv9"; break;
	case 0x4000: cpuname="MCore"; break;
	default:     cpuname="unknown"; break;
    }
    std::ostringstream oss;
    oss<<" E32Image Header ("<<exetype<<")";
    hwnd = CrtDlgWndnls(oss.str(),68,15);
    hwnd->goto_xy(1,1);
    hwnd->printf(
	  "Module/Tool Version  = 0x%08X/0x%08X\n"
	  "Compression Type     = 0x%08X\n"
	  "Flags                = 0x%08X (%s %s %s %s %s)\n"
	  "Code/Data size       = 0x%08X/0x%08X\n"
	  "Min/Max heap size    = 0x%08X/0x%08X\n"
	  "Stack/BSS size       = 0x%08X/0x%08X\n"
	  "Entry point          = 0x%08X\n"
	  "Code/Data base       = 0x%08X/0x%08X\n"
	  "Code/Data offset     = 0x%08X/0x%08X\n"
	  "DLL ref offset/count = 0x%08X/%u\n"
	  "ExportDir offset/cnt = 0x%08X/%u\n"
	  "Text size            = %u\n"
	  "Code/Data Reloc off  = 0x%08X/0x%08X\n"
	  "Process priority     = 0x%04X\n"
	  "CPU identifier       = 0x%04X(%s)\n"
	  ,img.iModuleVersion,img.iToolsVersion
	  ,img.iCompressionType
	  ,img.iFlags
	  ,img.iFlags&0x10000000?"PE_Fmt":""
	  ,img.iFlags&0x02000000?"Fmt_Ver":""
	  ,img.iFlags&0x00000020?"EKA2":""
	  ,img.iFlags&0x00000008?"EABI":""
	  ,img.iFlags&0x10000002?"No_call":""
	  ,img.iCodeSize,img.iDataSize
	  ,img.iHeapSizeMin,img.iHeapSizeMax
	  ,img.iStackSize,img.iBssSize
	  ,img.iEntryPoint
	  ,img.iCodeBase,img.iDataBase
	  ,img.iCodeOffset,img.iDataOffset
	  ,img.iImportOffset,img.iDllRefTableCount
	  ,img.iExportDirOffset,img.iExportDirCount
	  ,img.iTextSize
	  ,img.iCodeRelocOffset,img.iDataRelocOffset
	  ,img.iProcessPriority
	  ,img.iCpuIdentifier,cpuname
	  );
    while(1) {
	keycode = GetEvent(drawEmptyPrompt,NULL,hwnd);
	if(keycode == KE_F(5) || keycode == KE_ENTER) { fpos = fpos2; break; }
	else if(keycode == KE_ESCAPE || keycode == KE_F(10)) break;
    }
    delete hwnd;
    return fpos;
}

static Binary_Parser* query_interface(BeyeContext& b,binary_stream& h,CodeGuider& _parent,udn& u) { return new(zeromem) SisX_Parser(b,h,_parent,u); }
extern const Binary_Parser_Info sisx_info = {
    "SisX(EPOC) Symbian OS executable file",	/**< plugin name */
    query_interface
};
} // namespace	usr
