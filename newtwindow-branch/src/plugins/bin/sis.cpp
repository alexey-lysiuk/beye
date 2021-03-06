#include "config.h"
#include "libbeye/libbeye.h"
using namespace	usr;
/**
 * @namespace	usr_plugins_auto
 * @file        plugins/bin/sis.c
 * @brief       This file contains implementation of decoder for Sis (EPOC)
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
#include <stddef.h>

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
struct SisHeader {
    unsigned long UID1;
    unsigned long UID2;
    unsigned long UID3;
    unsigned long UID4;
    unsigned short Checksum;
    unsigned short nLanguages;
    unsigned short nFiles;
    unsigned short nRequisites;
    unsigned short iLanguages;
    unsigned short iFiles;
    unsigned short iDrive;
    unsigned short nCapabilities;
    unsigned long  InstallVer;
    unsigned short Options;
    unsigned short Type;
    unsigned short MajorVer;
    unsigned short MinorVer;
    unsigned long  Variant;
    unsigned long  LanguagePointer;
    unsigned long  FilesPointer;
    unsigned long  RequisitiesPointer;
    unsigned long  SertificatePointer;
    unsigned long  ComponentNamePointer;
};

    class Sis_Parser : public Binary_Parser {
	public:
	    Sis_Parser(BeyeContext& b,binary_stream&,CodeGuider&,udn&);
	    virtual ~Sis_Parser();

	    virtual const char*		prompt(unsigned idx) const;

	    virtual __filesize_t	show_header() const;
	    virtual int			query_platform() const;
	private:
	    __filesize_t		show_sis3_header() const;

	    BeyeContext&		bctx;
	    binary_stream&		main_handle;
	    udn&			_udn;
    };
static const char* txt[]={"","","","","","","","","",""};
const char* Sis_Parser::prompt(unsigned idx) const { return txt[idx]; }

Sis_Parser::Sis_Parser(BeyeContext& b,binary_stream& h,CodeGuider& code_guider,udn& u)
	    :Binary_Parser(b,h,code_guider,u)
	    ,bctx(b)
	    ,main_handle(h)
	    ,_udn(u)
{
    unsigned long id1,id2,id3;
    main_handle.seek(0,binary_stream::Seek_Set);
    id1=main_handle.read(type_dword);
    id2=main_handle.read(type_dword);
    id3=main_handle.read(type_dword);
    if(!((id2==0x10003A12 || id2==0x1000006D) && id3==0x10000419)) {
    /* try s60 3rd */
	if(id1!=0x10201A7A) throw bad_format_exception();
    }
}
Sis_Parser::~Sis_Parser() {}
int  Sis_Parser::query_platform() const { return DISASM_CPU_ARM; }

__filesize_t Sis_Parser::show_sis3_header() const
{
    bctx.ErrMessageBox("Not implemented yet!","Sis v3 header");
    return bctx.tell();
}

__filesize_t Sis_Parser::show_header() const
{
    unsigned keycode;
    TWindow * hwnd;
    const char *TypeName;
    struct SisHeader sis;
    __filesize_t fpos,fpos2;
    fpos2=fpos = bctx.tell();
    main_handle.seek(0,binary_stream::Seek_Set);
    binary_packet bp=main_handle.read(sizeof(sis)); memcpy(&sis,bp.data(),bp.size());
    if(sis.UID1==0x10201A7A) return show_sis3_header();
    switch(sis.Type) {
	case 0x0000: TypeName="APP"; break;
	case 0x0001: TypeName="SYSTEM"; break;
	case 0x0002: TypeName="OPTION"; break;
	case 0x0003: TypeName="CONFIG"; break;
	case 0x0004: TypeName="PATCH"; break;
	case 0x0005: TypeName="UPGRADE"; break;
	default:     TypeName="unknown"; break;
    }
    hwnd = CrtDlgWndnls(" Sis Header ",78,13);
    hwnd->goto_xy(1,1);
    hwnd->printf(
	  "Number of Lang/Files/Req   = %u/%u/%u\n"
	  "Installation Lang/Files/Drv= %u/%u/%u\n"
	  "Number of capabilities     = %u\n"
	  "Installer Version          = 0x%08X\n"
	  "Options                    = 0x%04X(%s %s %s %s)\n"
	  "Type                       = 0x%04X(%s)\n"
	  "Version                    = 0x%04X.%04X\n"
	  "Variant                    = 0x%08X\n"
	  "Language Pointer           = 0x%08X\n"
	  "Files Pointer              = 0x%08X\n"
	  "Requsites Pointer          = 0x%08X\n"
	  "Certificates Pointer       = 0x%08X\n"
	  "Component Name Pointer     = 0x%08X\n"
	  ,sis.nLanguages,sis.nFiles,sis.nRequisites
	  ,sis.iLanguages,sis.iFiles,sis.iDrive
	  ,sis.nCapabilities
	  ,sis.InstallVer
	  ,sis.Options
	  ,sis.Options&0x0001?"Unicode":""
	  ,sis.Options&0x0002?"Distrib":""
	  ,sis.Options&0x0008?"NoCompr":""
	  ,sis.Options&0x0010?"ShutDwn":""
	  ,sis.Type,TypeName
	  ,sis.MajorVer,sis.MinorVer
	  ,sis.Variant
	  ,sis.LanguagePointer
	  ,sis.FilesPointer
	  ,sis.RequisitiesPointer
	  ,sis.SertificatePointer
	  ,sis.ComponentNamePointer
	  );
    while(1) {
	keycode = GetEvent(drawEmptyPrompt,NULL,hwnd);
	if(keycode == KE_F(5) || keycode == KE_ENTER) { fpos = fpos2; break; }
	else if(keycode == KE_ESCAPE || keycode == KE_F(10)) break;
    }
    delete hwnd;
    return fpos;
}

static Binary_Parser* query_interface(BeyeContext& b,binary_stream& h,CodeGuider& _parent,udn& u) { return new(zeromem) Sis_Parser(b,h,_parent,u); }
extern const Binary_Parser_Info sis_info = {
    "Sis(EPOC) Symbian OS installable file",	/**< plugin name */
    query_interface
};
} // namespace	usr
