#include "config.h"
#include "libbeye/libbeye.h"
using namespace	usr;
/**
 * @namespace	usr_plugins_auto
 * @file        plugins/bin/ne.c
 * @brief       This file contains implementation of NE (New Executable) file
 *              format decoder.
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
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <set>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include "colorset.h"
#include "plugins/disasm.h"
#include "plugins/bin/ne.h"
#include "udn.h"
#include "beyehelp.h"
#include "tstrings.h"
#include "bconsole.h"
#include "listbox.h"
#include "codeguid.h"
#include "libbeye/libbeye.h"
#include "libbeye/kbd_code.h"
#include "ne.h"
#include "beye.h"

namespace	usr {
static const char* txt[]={ "NEHelp", "ModRef", "ResNam", "NRsNam", "", "Entry ", "ResTbl", "NE Hdr", "", "SegDef" };
const char* NE_Parser::prompt(unsigned idx) const { return txt[idx]; }

const char* NE_Parser::__nedata[] =
{
  "NOAUTODATA",
  "SINGLEDATA",
  "MULTIPLEDATA",
  "(SINGLE & MULTIPLE DATA)"
};

const char * __neExeType[] =
{
   "Unknown (Any)",
   "OS/2",
   "Windows",
   "DOS4",
   "Windows Dev386"
};

std::string NE_Parser::__getNEType(unsigned type) const
{
    if(type > 4) type = 0;
    return __neExeType[type];
}

const char * PMWinAPI[4] =
{
  "Text or Windowing API not declared.",
  "(NOTWINDOWCOMPAT). Full screen character-mode application.",
  "(WINDOWCOMPAT). Text window character mode application.",
  "(WINDOWAPI). Windowing application."
};

std::string NE_Parser::GetPMWinAPI(unsigned flag)
{
    flag >>= 8;
    flag &= 0x0003;
    return PMWinAPI[flag];
}

void NE_Parser::PaintNewHeaderNE_1(TWindow& w) const
{
  w.printf(
	   "Signature                      = '%c%c'\n"
	   "Linker Version.Revision        = %hd.%hd\n"
	   "Offset of Entry Table          = %XH\n"
	   "Length of Entry Table          = %hu  [ bytes ]\n"
	   "< 32-bit Checksum  >           = %08lXH\n"
	   "NE Flags :                     = [%04hXH]\n"
	   "   Contest DATA in EXE: %s\n"
	   "   [%c]  < Per-process library initialization (INITINSTANCE) >\n"
	   "   [%c]  Runs in protected mode only (PROTMODE)\n"
	   "   [%c]  Win: [LIM 3.2 used fow Windows] OS/2: [8086 instruction]\n"
	   "   [%c]  Win: [Multi instance EMS memory] OS/2: [80286 instructions]\n"
	   "   [%c]  Win: [DLL Global memory above EMS] OS/2: [80386 instructions]\n"
	   "   [%c]  OS/2: Floating point instructrions\n"
	   "   %s\n"
	   "   [%c]  First segment contains code that loads API (Bound Family/API)\n"
	   "   [%c]  Linker detects errors at link time, but still creates module\n"
	   "   [%c]  Win: [Module must located in EMS] OS/2: [SMP mode disabled]\n"
	   "   [%c]  Module is library (DLL)\n"
	   "DS (DGROUP)                    = %hu\n"
	   "HEAPSIZE                       = %hu\n"
	   "STACKSIZE                      = %hu\n"
	   "CS : IP                        = %04hXH:%04hXH"
	   ,ne.neSignature[0],ne.neSignature[1]
	   ,(int)ne.neLinkerVersion,(int)ne.neLinkerRevision
	   ,ne.neOffsetEntryTable
	   ,ne.neLengthEntryTable
	   ,ne.neChecksum
	   ,ne.neContestEXE
	   ,__nedata[ne.neContestEXE & 0x03]
	   ,Gebool( ne.neContestEXE & 0x0004 )
	   ,Gebool( ne.neContestEXE & 0x0008 )
	   ,Gebool( ne.neContestEXE & 0x0010 )
	   ,Gebool( ne.neContestEXE & 0x0020 )
	   ,Gebool( ne.neContestEXE & 0x0040 )
	   ,Gebool( ne.neContestEXE & 0x0040 )
	   ,GetPMWinAPI(ne.neContestEXE).c_str()
	   ,Gebool( ne.neContestEXE & 0x0800 )
	   ,Gebool( ne.neContestEXE & 0x2000 )
	   ,Gebool( ne.neContestEXE & 0x4000 )
	   ,Gebool( ne.neContestEXE & 0x8000 )
	   ,ne.neAutoDataSegmentCount
	   ,ne.neHeapSize
	   ,ne.neStackSize
	   ,(unsigned)(ne.neCSIPvalue >> 16),(unsigned)(ne.neCSIPvalue & 0xFFFF));
}

void NE_Parser::PaintNewHeaderNE_2(TWindow& w) const
{
  w.printf(
	   "SS : SP                        = %04hXH:%04hXH\n"
	   "Segment Table Count            = %hu\n"
	   "Module Reference Table Count   = %hu\n"
	   "Length Non Resident Name Table = %hu\n"
	   "Segment Table Offset           = %hXH\n"
	   "Resource Table Offset          = %hXH\n"
	   "Resident Name Table Offset     = %hXH\n"
	   "Module Reference Table Offset  = %hXH\n"
	   "Import Table Offset            = %hXH\n"
	   "Non Resident Name Table Offset = %08lXH\n"
	   "Moveable Entry Point Count     = %hu\n"
	   "Logical Sector Shift Count     = %hu\n"
	   "Resource Segment Count         = %hu\n"
	   "Operating System               = %s\n"
	   "[%c] - Support for long file names\n"
	   "[%c] - Windows 2.x API runs in prot. mode\n"
	   "[%c] - Windows 2.x API getting prot. font\n"
	   "[%c] - WLO application on OS/2\n"
	   ,(unsigned short)(ne.neSSSPvalue >> 16),(unsigned short)(ne.neSSSPvalue & 0xFFFF)
	   ,(short)ne.neSegmentTableCount
	   ,ne.neModuleReferenceTableCount
	   ,ne.neLengthNonResidentNameTable
	   ,ne.neOffsetSegmentTable
	   ,ne.neOffsetResourceTable
	   ,ne.neOffsetResidentNameTable
	   ,ne.neOffsetModuleReferenceTable
	   ,ne.neOffsetImportTable
	   ,ne.neOffsetNonResidentNameTable
	   ,ne.neMoveableEntryPointCount
	   ,ne.neLogicalSectorShiftCount
	   ,ne.neResourceSegmentCount
	   ,__getNEType(ne.neOperatingSystem).c_str()
	   ,Gebool(ne.neFlagsOther & 0x01)
	   ,Gebool(ne.neFlagsOther & 0x02)
	   ,Gebool(ne.neFlagsOther & 0x02)
	   ,Gebool(ne.neFlagsOther & 0x80));
  if(ne.neOperatingSystem == 2)
  {
	/* Correction by Olivier Mengu\u00e9*/
    unsigned char high,low;
    high = ne.neWindowsVersion >> 8;
    low  = ne.neWindowsVersion & 0xFF;
	/* End of correction */
    w.set_color(dialog_cset.addinfo);
    w.printf("Offset of Fast Load Area       = %04hXH"
	     ,ne.neOffsetFastLoadArea); w.clreol();
    w.printf("\nLength of Fast Load Area       = %hu"
	     ,ne.neLengthFastLoadArea); w.clreol();
    w.printf("\nWindows version                = %02hu.%02hu"
	     ,(unsigned int)high,(unsigned int)low); w.clreol();
    w.printf("\n");
  }
  w.set_color(dialog_cset.entry);
  w.printf(">Entry Point   %s = %08XH",( ne.neContestEXE & 32768L ) ? "[ LibEntry ]   " : "[ EXEEntry ] ",entryNE);
  w.clreol();
  w.set_color(dialog_cset.main);
}

void (NE_Parser::*NE_Parser::nephead[])(TWindow& w) const =
{
    &NE_Parser::PaintNewHeaderNE_1,
    &NE_Parser::PaintNewHeaderNE_2
};

void NE_Parser::PaintNewHeaderNE(TWindow& win,const std::vector<std::string>& ptr,unsigned tpage) const
{
    std::ostringstream oss;
    win.freeze();
    win.clear();
    oss<<" New Executable Header ["<<tpage<<"/"<<ptr.size()<<"] ";
    win.set_title(oss.str(),TWindow::TMode_Center,dialog_cset.title);
    win.set_footer(PAGEBOX_SUB,TWindow::TMode_Right,dialog_cset.selfooter);
    if(tpage < 2) {
	win.goto_xy(1,1);
	(this->*nephead[tpage])(win);
    }
    win.refresh_full();
}

__filesize_t NE_Parser::action_F8()
{
    __fileoff_t pos;
    unsigned CS,IP;
    CS = (unsigned)((ne.neCSIPvalue) >> 16);  /** segment number */
    IP = (unsigned)(ne.neCSIPvalue & 0xFFFF); /** offset within segment */
    entryNE = CalcEntryPointNE(CS,IP);
    pos = bctx().tell();
    std::vector<std::string> v;
    v.push_back("");
    v.push_back("");
    if(PageBox(70,22,v,*this,&NE_Parser::PaintNewHeaderNE) != -1 && entryNE) pos = entryNE;
    return pos;
}

void NE_Parser::entpaintNE(TWindow& w,const ENTRY& nam,unsigned flags) const
{
    w.goto_xy(1,1);
    w.printf(
	   "Entry Point for %s segment\n"
	   "Entry point is %s EXPORTED\n"
	   "The Entry %s uses SHARED data segment\n"
	   "Numbers of word that compose the stack %u\n"
	   "Segment offset = %XH bytes\n"
	   "Segment number = %u"
	   ,(unsigned char)(nam.eFixed) == 0xFF ? "MOVEABLE" : "FIXED"
	   ,flags & 0x0001 ? "  " : "NO"
	   ,flags & 0x0002 ? "  " : "NO"
	   ,(flags & 0xFFF4 >> 2)
	   ,nam.eSegOff
	   ,((unsigned)((unsigned char)(nam.eSegNum))));
}

void NE_Parser::paintdummyentryNE(TWindow& w) const
{
    w.goto_xy(1,3);
    w.printf("   Entry point not present ( Dummy bungle )");
}

void NE_Parser::SegPaintNE(TWindow& win,const std::vector<SEGDEF>& names,unsigned start) const
{
    std::ostringstream oss;
    unsigned flags = names[start].sdFlags;
    win.freeze();
    win.clear();
    oss<<" Segment Table [ "<<(start + 1)<<" / "<<names.size()<<" ] ";
    win.set_title(oss.str(),TWindow::TMode_Center,dialog_cset.title);
    win.set_footer(PAGEBOX_SUB,TWindow::TMode_Right,dialog_cset.selfooter);
    win.goto_xy(1,1);
    if(names[start].sdOffset)
	win.printf("Relative offset from begining in sectors     = %04hXH\n",names[start].sdOffset);
    else
	win.printf("No data of segment in the file\n");
    win.printf(
	  "Length of segments                           = %hu bytes\n"
	  "Minimum allocated memory for segment         = %hu bytes\n"
	  "Segment is :                                   %s\n"
	  " [%c] - Loader has allocated memory for segment\n"
	  " [%c] - Iterated segment\n"
	  "Segment is :                                   %s\n"
	  "Segment is :                                   %s\n"
	  "Segment is :                                   %s\n"
	  "Segment is :                                   %s\n"
	  " [%c] - Segment contains relocation data\n"
	  "Segment is :                                   %s\n"
	  "IOPL :                                         %hu\n"
	  "Segment is :                                   %s\n"
	  "Segment bitness :                              %d\n"
	  " [%c] - Huge memory segment (sizes is sector units)\n"
	  " [%c] - GDT allocation requested"
	  ,names[start].sdLength ? names[start].sdLength : (unsigned short)0xFFFF
	  ,names[start].sdMinMemory ? names[start].sdMinMemory : (unsigned short)0xFFFF
	  ,(flags & 0x0001) ? "DATA" : "CODE"
	  ,Gebool((flags & 0x0002) == 0x0002)
	  ,Gebool((flags & 0x0004) == 0x0004)
	  ,(flags & 0x0010) ? "MOVEABLE" : "FIXED"
	  ,(flags & 0x0020) ? "PURE" : "IMPURE"
	  ,(flags & 0x0040) ? "PRELOAD" : "LOADONCALL"
	  ,(flags & 0x0080) ? (flags & 0x0001) ? "READONLY" : "EXECUTEONLY" : (flags & 0x0001) ? "READWRITE" : "DEBUGABLE"
	  ,Gebool((flags & 0x0100) == 0x0100)
	  ,(flags & 0x0200) ? (flags & 0x0001) ? "EXPAND DOWN" : "CONFORMING" : (flags & 0x0001) ? "EXPAND UP" : "NOCONFORMING"
	  ,(unsigned)(((flags & 0x0C00) >> 10) & 0x03)
	  ,(flags & 0x1000) ? "DISCARDABLE" : "NONDISCARDABLE"
	  ,(flags & 0x2000) ? 32 : 16
	  ,Gebool((flags & 0x4000) == 0x0400)
	  ,Gebool((flags & 0x8000) == 0x0800));
    win.refresh_full();
}

void NE_Parser::EntPaintNE(TWindow& win,const std::vector<ENTRY>& names,unsigned start) const
{
    std::ostringstream oss;
    unsigned flags = names[start].eFlags;
    win.freeze();
    win.clear();
    oss<<" Entry Point [ "<<(start + 1)<<" / "<<names.size()<<" ] ";
    win.set_title(oss.str(),TWindow::TMode_Center,dialog_cset.title);
    win.set_footer(PAGEBOX_SUB,TWindow::TMode_Right,dialog_cset.selfooter);
    if(names[start].eFixed) entpaintNE(win,names[start],flags);
    else paintdummyentryNE(win);
    win.refresh_full();
}

std::vector<std::string> NE_Parser::__ReadModRefNamesNE(binary_stream& handle) const
{
    std::vector<std::string> rc;
    unsigned i;
    uint_fast16_t offTable;
    handle.seek(ne.neOffsetModuleReferenceTable + headshift(),binary_stream::Seek_Set);
    for(i = 0;i < ne.neModuleReferenceTableCount;i++) {
	__filesize_t NameOff;
	unsigned char length;
	__filesize_t fp;
	char stmp[256];
	offTable = handle.read(type_word);
	fp = handle.tell();
	NameOff = (__fileoff_t)headshift() + offTable + ne.neOffsetImportTable;
	handle.seek(NameOff,binary_stream::Seek_Set);
	length = handle.read(type_byte);
	if(IsKbdTerminate() || handle.eof()) break;
	binary_packet bp=handle.read(length); memcpy(stmp,bp.data(),bp.size());
	stmp[length] = 0;
	handle.seek(fp,binary_stream::Seek_Set);
	rc.push_back(stmp);
    }
    return rc;
}

__filesize_t NE_Parser::action_F2()
{
    binary_stream& handle = *ne_cache;
    int ret;
    unsigned nnames;
    __filesize_t fret;
    TWindow * w;
    fret = bctx().tell();
    handle.seek(0L,binary_stream::Seek_Set);
    if(!(nnames = ne.neModuleReferenceTableCount)) { bctx().NotifyBox(NOT_ENTRY,MOD_REFER); return fret; }
    w = PleaseWaitWnd();
    std::vector<std::string> objs = __ReadModRefNamesNE(handle);
    delete w;
    ListBox lb(bctx());
    while(1) {
	ret = lb.run(objs,MOD_REFER,ListBox::Selective | ListBox::Sortable,-1);
	if(ret != -1) ShowProcListNE(ret);
	else break;
   }
    return fret;
}

bool NE_Parser::isPresent(const std::vector<std::string>& objs,const std::string& _tmpl) const
{
    unsigned i;
    bool ret = false;
    size_t nentry = objs.size();
    for(i = 0;i < nentry;i++) {
	if(_tmpl==objs[i]) { ret = true; break; }
    }
    return ret;
}

std::vector<std::string> NE_Parser::__ReadProcListNE(binary_stream& handle,int modno) const
{
    std::vector<std::string> rc;
    unsigned i;
    std::string buff;
    SEGDEF tsd;
    modno++;
    std::ostringstream oss;
    binary_packet bp(1);

    handle.seek(headshift()+ne.neOffsetSegmentTable,binary_stream::Seek_Set);
    for(i = 0;i < ne.neSegmentTableCount;i++) {
	bp=handle.read(sizeof(SEGDEF)); memcpy(&tsd,bp.data(),bp.size());
	if(tsd.sdLength && tsd.sdOffset && tsd.sdFlags & 0x0100) {
	    __filesize_t spos;
	    uint_fast16_t j,nrelocs;
	    RELOC_NE rne;
	    spos = handle.tell();
	    handle.seek(((__fileoff_t)(tsd.sdOffset) << ne.neLogicalSectorShiftCount) + tsd.sdLength,binary_stream::Seek_Set);
	    nrelocs = handle.read(type_word);
	    for(j = 0;j < nrelocs;j++) {
		bp=handle.read(sizeof(RELOC_NE)); memcpy(&rne,bp.data(),bp.size());
		if((rne.Type & 3) && rne.idx == modno) {
		    if((rne.Type & 3) == 1) {
			oss<<"< By ordinal >   @"<<std::hex<<std::setfill('0')<<std::setw(4)<<rne.ordinal;
			buff=oss.str();
		    } else buff=rd_ImpName(rne.ordinal,true);
		    if(!isPresent(rc,buff)) {
			if(IsKbdTerminate()) goto exit;
			rc.push_back(buff);
		    }
		}
	    }
	    handle.seek(spos,binary_stream::Seek_Set);
	}
    }
exit:
    return rc;
}

void NE_Parser::ShowProcListNE( int modno ) const
{
    binary_stream& handle = *ne_cache;
    std::string name;
    TWindow *w;
    std::ostringstream oss;
    handle.seek(0L,binary_stream::Seek_Set);
    w = PleaseWaitWnd();
    std::vector<std::string> objs = __ReadProcListNE(handle,modno);
    delete w;
    if(objs.empty()) { bctx().NotifyBox(NOT_ENTRY,MOD_REFER); return; }
    name=rd_ImpName(modno+1,false);
    oss<<IMPPROC_TABLE<<name<<" ";
    ListBox lb(bctx());
    lb.run(objs,oss.str(),ListBox::Sortable,-1);
}

std::vector<std::string> NE_Parser::RNamesReadItems(binary_stream& handle,size_t nnames,__filesize_t offset)
{
    std::vector<std::string> rc;
    unsigned char length;
    unsigned Ordinal;
    unsigned i;
    char stmp[300];
    std::ostringstream oss; /* max255 + @ordinal */
    handle.seek(offset,binary_stream::Seek_Set);
    for(i = 0;i < nnames;i++) {
	length = handle.read(type_byte);
	if(IsKbdTerminate() || handle.eof()) break;
	binary_packet bp=handle.read(length); memcpy(stmp,bp.data(),bp.size());
	Ordinal = handle.read(type_word);
	oss<<(char)ListBox::Ord_Delimiter<<std::left<<std::setw(5)<<Ordinal;
	rc.push_back(std::string(stmp)+oss.str());
    }
    return rc;
}

std::vector<std::string> NE_Parser::NERNamesReadItems(binary_stream& handle,size_t nnames) const
{
    return RNamesReadItems(handle,nnames,ne.neOffsetResidentNameTable + headshift());
}

std::vector<std::string> NE_Parser::NENRNamesReadItems(binary_stream& handle,size_t nnames) const
{
    return RNamesReadItems(handle,nnames,ne.neOffsetNonResidentNameTable);
}

std::vector<SEGDEF> NE_Parser::__ReadSegTableNE(binary_stream& handle,size_t nnames) const
{
    std::vector<SEGDEF> rc;
    unsigned i;
    for(i = 0;i < nnames;i++) {
	SEGDEF sd;
	if(IsKbdTerminate() || handle.eof()) break;
	binary_packet bp=handle.read(sizeof(SEGDEF)); memcpy(&sd,bp.data(),bp.size());
	rc.push_back(sd);
    }
    return rc;
}

unsigned NE_Parser::GetNamCountNE(binary_stream& handle,__filesize_t offset )
{
 unsigned i;
 i = 0;
 if(!offset) return 0;
 handle.seek(offset,binary_stream::Seek_Set);
 while(1)
 {
   unsigned char l;
   l = handle.read(type_byte);
   if(l == 0 || handle.eof()) break;
   handle.seek(l + 2,binary_stream::Seek_Cur);
   i++;
   if(i > 0xFFFD) break;
 }
 return i;
}

unsigned NE_Parser::NERNamesNumItems(binary_stream& handle) const
{
   return GetNamCountNE(handle,headshift() + ne.neOffsetResidentNameTable);
}

unsigned NE_Parser::NENRNamesNumItems(binary_stream& handle) const
{
   return GetNamCountNE(handle,ne.neOffsetNonResidentNameTable);
}

void NE_Parser::ReadEntryItemNE(binary_stream& handle,ENTRY * obj,unsigned char etype) const
{
 obj->eFixed = etype;
 if(etype)
 {
  if(etype == 0xFF)
  {
      obj->eFlags = handle.read(type_byte);
      handle.seek(2,binary_stream::Seek_Cur); /** int 3F */
      obj->eSegNum = handle.read(type_byte);
      obj->eSegOff = handle.read(type_word);
  }
  else
  {
     obj->eFlags = handle.read(type_byte);
     obj->eSegOff = handle.read(type_word);
  }
 }
 if(etype != 0xFE && etype != 0xFF) obj->eSegNum = etype;
}

void NE_Parser::SkipEntryItemNE(binary_stream& handle,unsigned char etype) const
{
 if(etype)
 {
  if(etype == 0xFF) handle.seek(6,binary_stream::Seek_Cur); /** moveable */
  else
   handle.seek(3,binary_stream::Seek_Cur); /** fixed */
 }
}

bool NE_Parser::ReadEntryNE(ENTRY * obj,unsigned entnum) const
{
  binary_stream& handle = *ne_cache1;
  unsigned i,j;
  unsigned char nentry,etype;
  handle.seek((__fileoff_t)headshift() + ne.neOffsetEntryTable,binary_stream::Seek_Set);
  i = 0;
  while(1)
  {
     nentry = handle.read(type_byte);
     if(nentry == 0 || handle.eof()) break;
     etype = handle.read(type_byte);
     for(j = 0;j < nentry;j++,i++)
     {
       if(i == entnum - 1)
       {
	 ReadEntryItemNE(handle,(ENTRY *)obj,etype);
	 return true;
       }
       else SkipEntryItemNE(handle,etype);
     }
  }
  return false;
}

bool NE_Parser::ReadSegDefNE(SEGDEF * obj,unsigned segnum) const
{
    binary_stream* handle;
    handle = ne_cache3;
    if(segnum > ne.neSegmentTableCount || !segnum) return false;
    handle->seek((__fileoff_t)headshift() + ne.neOffsetSegmentTable + (segnum - 1)*sizeof(SEGDEF),binary_stream::Seek_Set);
    binary_packet bp=handle->read(sizeof(SEGDEF)); memcpy(obj,bp.data(),bp.size());
    return true;
}

__filesize_t  NE_Parser::CalcEntryPointNE( unsigned segnum, unsigned offset ) const
{
  SEGDEF seg;
  __filesize_t shift;
  if(!ReadSegDefNE(&seg,segnum)) return 0;
  shift = seg.sdOffset ? (((unsigned long)seg.sdOffset)<<ne.neLogicalSectorShiftCount) + offset : 0L;
  return shift;
}

__filesize_t  NE_Parser::CalcEntryNE(unsigned ord,bool dispmsg) const
{
    ENTRY entr;
    SEGDEF segd;
    int segnum;
    if(!ReadEntryNE(&entr,ord)) { if(dispmsg) bctx().ErrMessageBox(NOT_ENTRY,""); return 0L; }
    if(entr.eFixed == 0xFE) {
	std::ostringstream oss;
	if(dispmsg) {
	    oss<<"This entry is constant : "<<std::hex<<std::setfill('0')<<std::setw(4)<<entr.eSegOff<<"H";
	    bctx().TMessageBox(oss.str(),"Virtual entry");
	}
	return 0L;
    } else segnum = entr.eSegNum;
    if(ReadSegDefNE(&segd,segnum)) return segd.sdOffset ? (((__filesize_t)segd.sdOffset)<<ne.neLogicalSectorShiftCount) + entr.eSegOff : 0L;
    else if(dispmsg) bctx().ErrMessageBox(NO_ENTRY,BAD_ENTRY);
    return 0L;
}

__filesize_t NE_Parser::action_F10()
{
    binary_stream& handle = *ne_cache;
    unsigned nnames;
    __filesize_t fpos;
    nnames = ne.neSegmentTableCount;
    fpos = bctx().tell();
    if(!nnames) { bctx().NotifyBox(NOT_ENTRY," Segment Definition "); return fpos; }
    handle.seek((__fileoff_t)headshift() + ne.neOffsetSegmentTable,binary_stream::Seek_Set);
    std::vector<SEGDEF> objs = __ReadSegTableNE(handle,nnames);
    if(!objs.empty()) {
	int i = PageBox(65,17,objs,*this,&NE_Parser::SegPaintNE) + 1;
	if(i > 0) fpos = (__filesize_t)objs[i-1].sdOffset<<ne.neLogicalSectorShiftCount;
    }
    return fpos;
}

std::vector<ENTRY> NE_Parser::__ReadEntryTableNE(binary_stream& handle) const
{
    std::vector<ENTRY> rc;
    unsigned i;
    unsigned char j,nentry;
    i = 0;
    while(1) {
	unsigned char etype;
	nentry = handle.read(type_byte);
	if(nentry == 0 || handle.eof()) break;
	etype = handle.read(type_byte);
	for(j = 0;j < nentry;j++,i++) {
	    ENTRY ent;
	    if(IsKbdTerminate()) break;
	    ReadEntryItemNE(handle,&ent,etype);
	    rc.push_back(ent);
	}
    }
    return rc;
}

unsigned NE_Parser::GetEntryCountNE() const
{
 binary_stream& handle = *ne_cache;
 unsigned i,j;
 unsigned char nentry;
 handle.seek((__fileoff_t)headshift() + ne.neOffsetEntryTable,binary_stream::Seek_Set);
 i = 0;
 while(1)
 {
   unsigned char etype;
   nentry = handle.read(type_byte);
   if(nentry == 0 || handle.eof()) break; /** end of table */
   else
   {
     etype = handle.read(type_byte);
     for(j = 0;j < nentry;j++,i++) { SkipEntryItemNE(handle,etype); if(i > 0xFFFD || IsKbdTerminate()) goto exit; }
   }
 }
 exit:
 return i;
}

__filesize_t NE_Parser::action_F6()
{
    binary_stream& handle = *ne_cache;
    unsigned nnames;
    __filesize_t fpos;
    nnames = GetEntryCountNE();
    fpos = bctx().tell();
    if(!nnames) { bctx().NotifyBox(NOT_ENTRY," Entries "); return fpos; }
    handle.seek((__fileoff_t)headshift() + ne.neOffsetEntryTable,binary_stream::Seek_Set);
    std::vector<ENTRY> objs = __ReadEntryTableNE(handle);
    if(!objs.empty()) {
	int i = PageBox(50,6,objs,*this,&NE_Parser::EntPaintNE) + 1;
	if(i > 0)  fpos = CalcEntryNE(i,true);
    }
    return fpos;
}

const char * ResourceGrNames[] =
{
  "RESERVED 0",
  "CURSOR",
  "BITMAP",
  "ICON",
  "MENU",
  "DIALOG",
  "STRINGTABLE",
  "FONTDIR",
  "FONT",
  "ACCELERATOR",
  "RCDATA",
  "RESERVED 11",
  "GROUP CURSOR",
  "RESERVED 13",
  "GROUP ICON",
  "NAME TABLE",
  "VERSIONINFO"
};

std::string NE_Parser::GetResourceIDNE(binary_stream& handle,unsigned rid,__filesize_t BegResTab) const
{
    std::ostringstream oss;
    char buff[81];
    unsigned char nByte;
    binary_packet bp(1);
    if(rid & 0x8000) { oss<<std::hex<<(rid & 0x7FFF); return oss.str(); }
    else {
	__filesize_t pos;
	pos = handle.tell();
	handle.seek(BegResTab + rid,binary_stream::Seek_Set);
	nByte = handle.read(type_byte);
	if(nByte > 26) {
	    bp=handle.read(26); memcpy(buff,bp.data(),bp.size());
	    strcat(buff,"...");
	    nByte = 29;
	} else if(nByte) { bp=handle.read(nByte); memcpy(buff,bp.data(),bp.size()); }
	buff[nByte] = 0;
	handle.seek(pos,binary_stream::Seek_Set);
    }
    return buff;
}

std::vector<std::string> NE_Parser::__ReadResourceGroupNE(binary_stream& handle,size_t nitems,long * addr) const
{
    std::vector<std::string> rc;
    unsigned i,j;
    uint_fast16_t rcAlign,rTypeID,rcount;
    unsigned long BegResTab;
    BegResTab = handle.tell();
    rcAlign = handle.read(type_word);
    std::ostringstream oss;
    for(i = 0;i < nitems;i++) {
	addr[i++] = handle.tell();
	rTypeID = handle.read(type_word);
	rcount = handle.read(type_word);
	handle.seek(4,binary_stream::Seek_Cur);
	if(IsKbdTerminate() || handle.eof()) break;
	if(rTypeID & 0x8000) {
	    rTypeID &= 0x7FFF;
	    if(rTypeID < 17) oss<<ResourceGrNames[rTypeID];
	    else             oss<<"< Ordinal type: "<<std::hex<<std::setfill('0')<<std::setw(8)<<rTypeID<<"H >";
	}
	else  oss<<'"'<<GetResourceIDNE(handle,rTypeID,BegResTab)<<'"';
	rc.push_back(oss.str());
	for(j = 0;j < rcount;j++)  {
	    NAMEINFO nam;
	    oss.str("");
	    if(IsKbdTerminate() || handle.eof()) break;
	    binary_packet bp=handle.read(sizeof(NAMEINFO)); memcpy(&nam,bp.data(),bp.size());
	    addr[i++] = ((unsigned long)nam.rnOffset)<<rcAlign;
	    oss<<" "<<GetResourceIDNE(handle,nam.rnID,BegResTab)
		<<" <length: "<<std::hex<<std::setfill('0')<<std::setw(4)<<((unsigned)((unsigned long)nam.rnLength)<<rcAlign)
		<<" "<<((nam.rnFlags & 0x0010) ? "MOVEABLE" : "FIXED")
		<<" "<<((nam.rnFlags & 0x0020) ? "PURE"     : "IMPURE")
		<<" "<<((nam.rnFlags & 0x0040) ? "PRELOAD"  : "LOADONCALL");
	    rc.push_back(oss.str());
	}
	i--;
    }
    return rc;
}

unsigned int NE_Parser::GetResourceGroupCountNE(binary_stream& handle) const
{
 uint_fast16_t rcount, rTypeID;
 int count = 0;
 __filesize_t pos;
 if(ne.neOffsetResourceTable == ne.neOffsetResidentNameTable) return 0;
 pos = handle.tell();
 handle.seek(2L,binary_stream::Seek_Cur); /** rcAlign */
 while(1)
 {
   rTypeID = handle.read(type_word);
   if(rTypeID)
   {
     rcount = handle.read(type_word);
     handle.seek(rcount*sizeof(NAMEINFO) + 4,binary_stream::Seek_Cur);
     count += rcount + 1;
     if(count > 0xF000 || IsKbdTerminate() || handle.eof()) break;
   }
   else break;
 }
 handle.seek(pos,binary_stream::Seek_Set);
 return count;
}

__filesize_t NE_Parser::action_F7()
{
    __filesize_t fpos;
    binary_stream& handle = *ne_cache;
    long * raddr;
    unsigned nrgroup;
    fpos = bctx().tell();
    handle.seek((__fileoff_t)headshift() + ne.neOffsetResourceTable,binary_stream::Seek_Set);
    if(!(nrgroup = GetResourceGroupCountNE(handle))) { bctx().NotifyBox(NOT_ENTRY," Resources "); return fpos; }
    if(!(raddr  = new long [nrgroup])) return fpos;
    std::vector<std::string> objs = __ReadResourceGroupNE(handle,nrgroup,raddr);
    ListBox lb(bctx());
    if(!objs.empty()) {
	int i = lb.run(objs," Resource groups : ",ListBox::Selective,-1);
	if(i != -1) fpos = raddr[i];
    }
    delete raddr;
    return fpos;
}

__filesize_t NE_Parser::action_F3()
{
    __filesize_t fpos = bctx().tell();
    int ret;
    unsigned ordinal;
    std::string title = RES_NAMES;
    ssize_t nnames = NERNamesNumItems(main_handle());
    ListBox::flags flags = ListBox::Selective | ListBox::Sortable;
    TWindow* w;
    ret = -1;
    w = PleaseWaitWnd();
    std::vector<std::string> objs = NERNamesReadItems(main_handle(),nnames);
    delete w;
    ListBox lb(bctx());
    if(objs.empty()) { bctx().NotifyBox(NOT_ENTRY,title); goto exit; }
    ret = lb.run(objs,title,flags,-1);
    if(ret != -1) {
	const char* cptr;
	cptr = strrchr(objs[ret].c_str(),ListBox::Ord_Delimiter);
	cptr++;
	std::istringstream iss(cptr);
	iss>>ordinal;
    }
exit:
    if(ret != -1) fpos = CalcEntryNE(ordinal,true);
    return fpos;
}

__filesize_t NE_Parser::action_F4()
{
    __filesize_t fpos = bctx().tell();
    int ret;
    unsigned ordinal;
    std::string title = NORES_NAMES;
    ssize_t nnames = NENRNamesNumItems(main_handle());
    ListBox::flags flags = ListBox::Selective | ListBox::Sortable;
    TWindow* w;
    ret = -1;
    w = PleaseWaitWnd();
    std::vector<std::string> objs = NENRNamesReadItems(main_handle(),nnames);
    delete w;
    ListBox lb(bctx());
    if(objs.empty()) { bctx().NotifyBox(NOT_ENTRY,title); goto exit; }
    ret = lb.run(objs,title,flags,-1);
    if(ret != -1) {
	const char* cptr;
	cptr = strrchr(objs[ret].c_str(),ListBox::Ord_Delimiter);
	cptr++;
	std::istringstream iss(cptr);
	iss>>ordinal;
    }
exit:
    if(ret != -1) fpos = CalcEntryNE(ordinal,true);
    return fpos;
}

/***************************************************************************/
/************************ RELOCATION FOR NE  *******************************/
/***************************************************************************/
void NE_Parser::BuildNERefChain(__filesize_t segoff,__filesize_t slength)
{
  unsigned nchains,i;
  __filesize_t reloc_off;
  TWindow * w;
  w = CrtDlgWndnls(SYSTEM_BUSY,49,1);
  w->goto_xy(1,1);
  w->printf(" Building reference chains for segment #%u",CurrChainSegment);
  if(PubNames.empty()) ne_ReadPubNameList(main_handle());
  reloc_off = segoff + slength;

  main_handle().seek(reloc_off,binary_stream::Seek_Set);
  nchains =main_handle().read(type_word);
  reloc_off += 2;
  for(i = 0;i < nchains;i++)
  {
     __filesize_t this_reloc_off;
     RELOC_NE rne;
     NERefChain nrc;
     this_reloc_off = reloc_off + i*sizeof(RELOC_NE);
     main_handle().seek(this_reloc_off,binary_stream::Seek_Set);
     binary_packet bp=main_handle().read(sizeof(RELOC_NE)); memcpy(&rne,bp.data(),bp.size());
     if(IsKbdTerminate() || main_handle().eof()) break;
     nrc.offset = rne.RefOff;
     nrc.number = i;
     CurrNEChain.insert(nrc);
     if(!(rne.Type & 0x04) && rne.AddrType) {/** if not additive fixup and not byte fixup */
       while(1) {
	 unsigned next_off;
	 std::set<NERefChain>::const_reverse_iterator it = CurrNEChain.rbegin();
	 main_handle().seek(segoff + (__filesize_t)(*it).offset,binary_stream::Seek_Set);
	 next_off =main_handle().read(type_word);
	 if(main_handle().eof()) break;
	 if(next_off > slength || next_off == 0xFFFF || next_off == (*it).offset) break;
	 nrc.offset = next_off;
	 nrc.number = i;
	 CurrNEChain.insert(nrc);
       }
     }
  }
//  la_Sort(CurrNEChain,compare_chains);
  delete w;
}

RELOC_NE NE_Parser::__found_RNE(__filesize_t segoff,__filesize_t slength,unsigned segnum,unsigned keyoff,char codelen)
{
    NERefChain key;
    std::set<NERefChain>::const_iterator it;
    RELOC_NE rne;
    if(segnum != CurrChainSegment || CurrNEChain.empty()) {
	CurrNEChain.clear();
	CurrChainSegment = segnum;
	BuildNERefChain(segoff,slength);
    }
    key.offset = keyoff;
    __codelen = codelen;
    it = CurrNEChain.find(key);
    if(it!=CurrNEChain.end()) {
	main_handle().seek(segoff + slength + 2 + sizeof(RELOC_NE)*(*it).number,binary_stream::Seek_Set);
	binary_packet bp=main_handle().read(sizeof(rne)); memcpy(&rne,bp.data(),bp.size());
    }
    else rne.Type=0xFF;
    return rne;
}

RELOC_NE NE_Parser::__found_RNE_spec(__filesize_t segoff,__filesize_t slength,unsigned segnum,unsigned keyoff,char codelen,int type)
{
    NERefChain key;
    std::set<NERefChain>::const_iterator it;
    RELOC_NE rne;
    if(segnum != CurrChainSegment || CurrNEChain.empty()) {
	CurrNEChain.clear();
	CurrChainSegment = segnum;
	BuildNERefChain(segoff,slength);
    }
    key.offset = keyoff;
    __codelen = codelen;
    __type = type;
    it = CurrNEChain.find(key);
    if(it!=CurrNEChain.end()) {
	binary_stream* b_cache;
	b_cache = ne_cache;
	b_cache->seek(segoff + slength + 2 + sizeof(RELOC_NE)*(*it).number,binary_stream::Seek_Set);
	binary_packet bp=b_cache->read(sizeof(rne)); memcpy(&rne,bp.data(),bp.size());
    }
    else rne.Type=0xFF;
    return rne;
}

unsigned NE_Parser::__findSpecType(__filesize_t sstart,__filesize_t ssize,unsigned segnum,__filesize_t target,char codelen,char type,unsigned defval)
{
    unsigned ret;
    RELOC_NE rne = __found_RNE_spec(sstart,ssize,segnum,(unsigned)(target - sstart),codelen,type);
    if(rne.Type!=0xFF)
	ret = rne.ordinal;
    else
	ret = defval;
    return ret;
}

std::string NE_Parser::rdImpNameNELX(unsigned idx,bool useasoff,__filesize_t OffTable) const
{
  unsigned char len;
  __filesize_t name_off;
  binary_stream* b_cache;
  b_cache = ne_cache2;
  name_off = OffTable;
  if(!useasoff)
  {
    __filesize_t ref_off;
    ref_off = (__filesize_t)headshift()
	      + ne.neOffsetModuleReferenceTable
	      + (idx - 1)*2;
	main_handle().seek(ref_off,binary_stream::Seek_Set);
	name_off += main_handle().read(type_word);
  }
  else name_off += idx;
  b_cache->seek(name_off,binary_stream::Seek_Set);
  len = b_cache->read(type_byte);
  char buff[len+1];
  binary_packet bp=b_cache->read(len); memcpy(buff,bp.data(),bp.size());
  buff[len] = 0;
  return buff;
}

std::string NE_Parser::rd_ImpName(unsigned idx,bool useasoff) const
{
  return rdImpNameNELX(idx,useasoff,headshift() + ne.neOffsetImportTable);
}

std::string NE_Parser::BuildReferStrNE(const DisMode& parent,const RELOC_NE& rne,int flags,__filesize_t ulShift)
{
    std::string buff,str;
    const char *pref;
    char reflen;
    bool need_virt;
    reflen = 0;
    pref = "";
    need_virt = (flags & Bin_Format::Save_Virt);
    std::ostringstream oss;
    if(PubNames.empty()) ne_ReadPubNameList(*ne_cache2);
    switch(rne.AddrType) {
	case 0: reflen = 1; pref = "(b) "; break;
	case 2: reflen = 2; pref = "seg "; break;
	case 3: reflen = 4; pref = "seg:off16 "; break;
	case 5: reflen = 2; pref = "off16 "; break;
	case 11: reflen = 6; pref = "seg:off32 "; break;
	case 13: reflen = 4; pref = "off32 "; break;
	default: break;
    }
    if(flags & Bin_Format::Use_Type) str+=pref;
    if((rne.Type & 3) == 1 || (rne.Type & 3) == 2) { /** imported type */
	buff=rd_ImpName(rne.idx,0);
	oss<<"<"<<buff<<">.";
	str+=oss.str();
	oss.str("");
	if((rne.Type & 3) == 1) { oss<<"@"<<std::hex<<std::setfill('0')<<std::setw(4)<<rne.ordinal; str+=oss.str(); }
	else str+=rd_ImpName(rne.ordinal,true);
    } else if((rne.Type & 3) == 0) {
	if(rne.idx == 0x00FF && rne.AddrType != 2) {
	    __filesize_t ea;
	    ea = CalcEntryNE(rne.ordinal,false);
	    Symbol_Info rc = FindPubName(ea);
	    if(rc.pa!=Plugin::Bad_Address) str+=rc.name;
	    else {
		oss.str("");
		oss<<"(*this).@"<<std::hex<<std::setfill('0')<<std::setw(4)<<rne.ordinal;
		str+=oss.str();
	    }
	    if(!DumpMode && !EditMode && !(flags & Bin_Format::Use_Type)) code_guider().add_go_address(parent,str,ea);
	} else {
	    __filesize_t ep;
	    ep = CalcEntryPointNE(rne.idx,rne.ordinal);
	    Symbol_Info rc = FindPubName(ep);
	    if(rc.pa!=Plugin::Bad_Address) str+=rc.name;
	    else {
		oss.str("");
		if(need_virt) oss<<"."<<std::hex<<std::setfill('0')<<std::setw(8)<<(unsigned long)pa2va(ep);
		else oss<<"(*this).seg<#"<<std::hex<<std::setfill('0')<<std::setw(4)<<rne.idx<<">:"<<std::hex<<std::setfill('0')<<std::setw(4)<<rne.ordinal;
		str+=oss.str();
	    }
	    if(!DumpMode && !EditMode && !(flags & Bin_Format::Use_Type)) code_guider().add_go_address(parent,str,ep);
	}
    } else str+="?OSFIXUP?";
    if((rne.Type & 4) == 4) {
	__filesize_t data;
	if(reflen && reflen <= 4) {
	    str+="+";
	    main_handle().seek(ulShift,binary_stream::Seek_Set);
	    data = main_handle().read(type_dword);
	    oss.str("");
	    oss<<std::hex<<std::setfill('0')<<std::setw((reflen == 1)?2:(reflen == 2)?4:8)<<data;
	    str+=oss.str();
	}
	else str+=",<add>";
    }
    return str;
}

std::string NE_Parser::bind(const DisMode& parent,__filesize_t ulShift,Bin_Format::bind_type flags,int codelen,__filesize_t r_sh)
{
    unsigned i;
    __filesize_t segpos,slength;
    std::string str;
    if(flags & Bin_Format::Try_Pic) return "";
    if(ulShift >= CurrSegmentStart && ulShift <= CurrSegmentStart + CurrSegmentLength) {
	i = CurrChainSegment - 1;
	if(CurrSegmentHasReloc) goto Direct;
	else                    goto TryLabel;
    }
    for(i = 0;i < ne.neSegmentTableCount;i++) {
	SEGDEF sd;
	ReadSegDefNE(&sd,i + 1);
	segpos = ((__filesize_t)sd.sdOffset) << ne.neLogicalSectorShiftCount;
	if(!segpos) continue;
	slength = sd.sdLength;
	if((sd.sdFlags & 0x4000) == 0x4000) slength <<= ne.neLogicalSectorShiftCount;
	if(ulShift >= segpos && ulShift <= segpos + slength) { /** we insize segment */
	    RELOC_NE rne;
	    CurrSegmentStart = segpos;
	    CurrSegmentLength = slength;
	    CurrSegmentHasReloc = (sd.sdFlags >> 8) & 1;
	    if(!CurrSegmentHasReloc) return "";
Direct:
	    rne = __found_RNE(CurrSegmentStart,CurrSegmentLength,i + 1,(unsigned)(ulShift - CurrSegmentStart),codelen);
	    if(rne.Type!=0xFF) {
		if(rne.AddrType == 2) {
		    main_handle().seek(ulShift,binary_stream::Seek_Set);
		    rne.ordinal = main_handle().read(type_word);
		    rne.ordinal = __findSpecType(CurrSegmentStart,CurrSegmentLength,i + 1,ulShift,codelen,5,rne.ordinal);
		}
		if(rne.AddrType == 5) {
		    rne.idx    = __findSpecType(CurrSegmentStart,CurrSegmentLength,i + 1,ulShift,codelen,2,rne.idx);
		}
		str=BuildReferStrNE(parent,rne,flags,ulShift);
		return str;
	    } else {
TryLabel:
		if(flags & Bin_Format::Try_Label) {
		    if(PubNames.empty()) ne_ReadPubNameList(*ne_cache2);
		    Symbol_Info rc = FindPubName(r_sh);
		    if(rc.pa!=Plugin::Bad_Address) {
			str=rc.name;
			if(!DumpMode && !EditMode) code_guider().add_go_address(parent,str,r_sh);
			return str;
		    }
		}
	    }
	    return "";
	}
    }
    return "";
}

/** return false if unsuccess true otherwise */
bool NE_Parser::ReadPubNames(binary_stream& handle,__filesize_t offset)
{
 symbolic_information pnam;
 ENTRY ent;
 unsigned ord,i = 0;
 __filesize_t noff;
 bool ret;
 if(!offset) return false;
 ret = true;
 handle.seek(offset,binary_stream::Seek_Set);
 while(1)
 {
   unsigned char l;
   noff = handle.tell();
   l = handle.read(type_byte);
   if(l == 0 || handle.eof()) { ret = true; break; }
   handle.seek(l,binary_stream::Seek_Cur);
   ord = handle.read(type_word);
   if(ord)
   {
     if(ReadEntryNE(&ent,ord))
     {
       pnam.pa = CalcEntryNE(ord,false);
       pnam.nameoff = noff;
       pnam.attr = Symbol_Info::Global;
     }
     else
     {
       ret = false;
       break;
     }
     if(pnam.pa) PubNames.insert(pnam);
   }
   i++;
   if(i > 0xFFFD || handle.eof()) { ret = false; break; }
 }
 return ret;
}

std::string NE_Parser::ne_ReadPubName(binary_stream& b_cache,const symbolic_information& it) const
{
    unsigned char rlen;
    b_cache.seek(it.nameoff,binary_stream::Seek_Set);
    rlen = b_cache.read(type_byte);
    char stmp[rlen+1];
    binary_packet bp=b_cache.read(rlen); memcpy(stmp,bp.data(),bp.size());
    stmp[rlen] = 0;
    return stmp;
}

Symbol_Info NE_Parser::FindPubName(__filesize_t pa) const
{
    Symbol_Info rc;
    symbolic_information key;
    std::set<symbolic_information>::const_iterator it;
    key.pa = pa;
    it = PubNames.find(key);
    if(it!=PubNames.end()) {
	rc.pa=pa;
	rc.name=ne_ReadPubName(*ne_cache2,*it);
	return rc;
    }
    return _udn().find(pa);
}

void NE_Parser::ne_ReadPubNameList(binary_stream& handle)
{
    ReadPubNames(handle,headshift() + ne.neOffsetResidentNameTable);
    ReadPubNames(handle,ne.neOffsetNonResidentNameTable);
}

NE_Parser::NE_Parser(BeyeContext& b,binary_stream& h,CodeGuider& __code_guider,udn& u)
	:MZ_Parser(b,h,__code_guider,u)
	,CurrChainSegment(0xFFFF)
	,CurrSegmentHasReloc(-1)
{
    char id[2];
    binary_packet bp(1);
    if(headshift()) {
	h.seek(headshift(),binary_stream::Seek_Set);
	bp=h.read(sizeof(id)); memcpy(id,bp.data(),bp.size());
	if(!(id[0] == 'N' && id[1] == 'E')) throw bad_format_exception();
    }
    else throw bad_format_exception();

    h.seek(headshift(),binary_stream::Seek_Set);
    bp=h.read(sizeof(NEHEADER)); memcpy(&ne,bp.data(),bp.size());
    ne_cache3 = h.dup();
    ne_cache1 = h.dup();
    ne_cache = h.dup();
    ne_cache2 = h.dup();
}

NE_Parser::~NE_Parser()
{
  binary_stream& h = main_handle();
  CurrNEChain.clear();
  PubNames.clear();
  if(ne_cache != &h) delete ne_cache;
  if(ne_cache2 != &h) delete ne_cache2;
  if(ne_cache3 != &h) delete ne_cache3;
  if(ne_cache1 != &h) delete ne_cache1;
}

__filesize_t NE_Parser::action_F1()
{
    Beye_Help bhelp(bctx());
    if(bhelp.open(true)) {
	bhelp.run(10006);
	bhelp.close();
    }
    return bctx().tell();
}

__filesize_t NE_Parser::va2pa(__filesize_t va) const
{
  SEGDEF nesd;
  uint_fast16_t seg,off;
  seg = (va >> 16) & 0xFFFFU;
  off = va & 0xFFFFU;
  if(!ReadSegDefNE(&nesd,seg)) return 0L;
  return nesd.sdOffset ? (((__filesize_t)nesd.sdOffset)<<ne.neLogicalSectorShiftCount) + off : 0;
}

__filesize_t NE_Parser::pa2va(__filesize_t pa) const
{
  unsigned i,segcount = ne.neSegmentTableCount;
  __filesize_t currseg_st,nextseg_st;
  for(i = 0;i < segcount-1;i++)
  {
    SEGDEF nesd_c/*,nesd_n*/;
    if(!ReadSegDefNE(&nesd_c,i+1)) return 0L;
    currseg_st = (((__filesize_t)nesd_c.sdOffset)<<ne.neLogicalSectorShiftCount);
    if(!currseg_st) continue;
    nextseg_st = currseg_st + nesd_c.sdLength;
    if(pa >= currseg_st && pa < nextseg_st) {
/*      it_seg:*/
      return ((__filesize_t)(i+1) << 16) + (unsigned)(pa - currseg_st);
    }
  }
  return 0L;
}

Symbol_Info NE_Parser::get_public_symbol(__filesize_t pa,bool as_prev)
{
    Symbol_Info rc;
    if(PubNames.empty()) ne_ReadPubNameList(*ne_cache);
    std::set<symbolic_information>::const_iterator idx;
    symbolic_information key;
    key.pa=pa;
    rc=find_symbolic_information(PubNames,key,as_prev,idx);
    if(idx!=PubNames.end()) {
	rc.name=ne_ReadPubName(*ne_cache,*idx);
    }
    return rc;
}

Object_Info NE_Parser::__get_object_attribute(__filesize_t pa) const
{
    __filesize_t currseg_st;
    unsigned i,segcount = ne.neSegmentTableCount;
    bool found;
    Object_Info rc;
    rc.start = 0;
    rc.end = main_handle().flength();
    rc._class = Object_Info::NoObject;
    rc.bitness = Bin_Format::Use16;
    rc.name.clear();
    rc.number = 0;
    main_handle().seek((__fileoff_t)headshift() + ne.neOffsetSegmentTable,binary_stream::Seek_Set);
    found = false;
    for(i = 0;i < segcount;i++) {
	SEGDEF nesd_c;
	if(!ReadSegDefNE(&nesd_c,i+1)) { rc.number=0; return rc; }
	currseg_st = (((__filesize_t)nesd_c.sdOffset)<<ne.neLogicalSectorShiftCount);
	if(!currseg_st) { rc.start = rc.end; continue; } /** BSS segment */
	if(pa < currseg_st) {
	    rc.start = rc.end;
	    rc.end = currseg_st;
	    found = true;
	    rc.number = i;
	    break;
	}
	if(pa >= currseg_st && pa < currseg_st + nesd_c.sdLength) {
	    rc.start = currseg_st;
	    rc.end = rc.start + nesd_c.sdLength;
	    rc._class = nesd_c.sdFlags & 0x01 ? Object_Info::Data : Object_Info::Code;
	    rc.bitness = nesd_c.sdFlags & 0x2000 ? Bin_Format::Use32 : Bin_Format::Use16;
	    rc.number = i+1;
	    found = true;
	    break;
	}
	rc.start = currseg_st;
	rc.end = currseg_st + nesd_c.sdLength;
    }
    if(!found) {
	rc.start = rc.end;
	rc.end = main_handle().flength();
    }
    return rc;
}

Object_Info NE_Parser::get_object_attribute(__filesize_t pa) {
    return __get_object_attribute(pa);
}

Bin_Format::bitness NE_Parser::query_bitness(__filesize_t pa) const
{
    static __filesize_t st = 0,end = 0;
    static Bin_Format::bitness bitness;
    if(!(pa >= st && pa < end)) {
	Object_Info rc = __get_object_attribute(pa);
	bitness = rc.bitness;
	st=rc.start;
	end=rc.end;
    }
    return bitness;
}

std::string NE_Parser::address_resolving(__filesize_t cfpos)
{
 /* Since this function is used in references resolving of disassembler
    it must be seriously optimized for speed. */
    uint32_t res;
    std::ostringstream oss;
    if(cfpos >= headshift() && cfpos < headshift() + sizeof(NEHEADER))
	oss<<"NEH :"<<std::hex<<std::setfill('0')<<std::setw(4)<<(cfpos - headshift());
    else if(cfpos >= headshift() + ne.neOffsetSegmentTable &&
	cfpos <  headshift() + ne.neOffsetSegmentTable + ne.neSegmentTableCount*sizeof(SEGDEF))
	oss<<"NESD:"<<std::hex<<std::setfill('0')<<std::setw(4)<<(cfpos - headshift() - ne.neOffsetSegmentTable);
    else if(cfpos >= headshift() + ne.neOffsetEntryTable &&
	cfpos <  headshift() + ne.neOffsetEntryTable + ne.neLengthEntryTable)
	oss<<"NEET:"<<std::hex<<std::setfill('0')<<std::setw(4)<<(cfpos - headshift() - ne.neOffsetEntryTable);
    else if((res=pa2va(cfpos))!=0)
	oss<<"."<<std::hex<<std::setfill('0')<<std::setw(8)<<res;
    return oss.str();
}

int NE_Parser::query_platform() const { return DISASM_CPU_IX86; }

static Binary_Parser* query_interface(BeyeContext& b,binary_stream& h,CodeGuider& _parent,udn& u) { return new(zeromem) NE_Parser(b,h,_parent,u); }
extern const Binary_Parser_Info ne_info = {
    "NE (New Exe)",	/**< plugin name */
    query_interface
};
} // namespace	usr
