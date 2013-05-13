#include "config.h"
#include "libbeye/libbeye.h"
using namespace	usr;
/**
 * @namespace	usr
 * @file        bin_util.c
 * @brief       This file contains common functions of plugins/bin of BEYE project.
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
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "beye.h"
#include "bin_util.h"
#include "reg_form.h"
#include "beyeutil.h"
#include "bconsole.h"
#include "tstrings.h"
#include "plugins/disasm.h"

namespace	usr {
tCompare __FASTCALL__ fmtComparePubNames(const any_t* v1,const any_t* v2)
{
  const struct PubName  *pnam1, *pnam2;
  pnam1 = (const struct PubName  *)v1;
  pnam2 = (const struct PubName  *)v2;
  return __CmpLong__(pnam1->pa,pnam2->pa);
}

__filesize_t __FASTCALL__ fmtGetPubSym(unsigned& func_class,__filesize_t pa,bool as_prev,
					linearArray *PubNames,size_t& index)
{
    __filesize_t ret_addr,cur_addr;
    size_t i,idx,nitems;
    struct PubName key;
    if(!PubNames->nItems) return 0;
    ret_addr = 0L;
    index = idx = std::numeric_limits<size_t>::max();
    key.pa = pa;
    i = (unsigned)la_FindNearest(PubNames,&key,fmtComparePubNames);
    nitems = PubNames->nItems;
    if(as_prev) idx = i;
    else {
	static unsigned long multiref_i = 0;
	get_next:
	while((cur_addr = ((struct PubName  *)PubNames->data)[i].pa) <= pa) {
	    i++;
	    if((cur_addr == pa && i > multiref_i) || (i >= nitems - 1)) break;
	}
	idx = i;
	if(idx < PubNames->nItems) ret_addr = cur_addr;
	else ret_addr = 0L;
	if(ret_addr && ret_addr == pa) {
	    if(idx <= multiref_i) { i = idx; goto get_next; }
	    else multiref_i = idx;
	}
	else multiref_i = 0;
    }
    if(idx < PubNames->nItems) {
	ret_addr = ((struct PubName  *)PubNames->data)[idx].pa;
	func_class = ((struct PubName  *)PubNames->data)[idx].attr;
	if(!idx && pa < ret_addr && as_prev) ret_addr = 0;
	else {
	    index = idx;
//	    it = &((struct PubName  *)PubNames->data)[idx];
//	    (*fmt_readpub)(fmt_cache,it,str,cb_str);
//	    str[cb_str-1] = 0;
	}
    }
    return ret_addr;
}

/* User Defined names (UDN) */

typedef struct tagUDN {
    char		name[256];
    __filesize_t	offset;
}udn;

static tCompare __FASTCALL__ udn_compare(const any_t* e1,const any_t* e2)
{
    const udn  *p1 = (const udn  *)e1;
    const udn  *p2 = (const udn  *)e2;
    return p1->offset<p2->offset?-1:p1->offset>p2->offset?1:0;
}

static linearArray *udn_list=NULL;
static bool udn_modified=false;
static std::string udn_fname;

static bool __FASTCALL__ udnAddItem() {
    __filesize_t off;
    udn item,*prev;
    char ud_name[256],prompt[256];
    off = beye_context().bm_file().tell();
    sprintf(prompt," Name for %08X offset: ",off);
    prev=NULL;
    ud_name[0]='\0';
    if(udn_list) {
	item.name[255]='\0';
	item.offset=off;
	prev = (udn*)la_Find(udn_list,&item,udn_compare);
	if(prev) strcpy(ud_name,prev->name);
    }
    if(GetStringDlg(ud_name,prompt," [ENTER] - Proceed ",NAME_MSG))
    {
	if(!udn_list) udn_list=la_Build(0,sizeof(udn),NULL);
	if(udn_list) {
	    if(prev) strcpy(prev->name,ud_name);
	    else {
		strcpy(item.name,ud_name);
		item.offset=off;
		la_AddData(udn_list,&item,NULL);
	    }
	    la_Sort(udn_list,udn_compare);
	}
	udn_modified=true;
	return true;
    }
    return false;
}

static unsigned __FASTCALL__ udnGetNumItems(binary_stream& handle) {
    UNUSED(handle);
    return udn_list->nItems;
}

static bool    __FASTCALL__ udnReadItems(binary_stream& handle,memArray * names,unsigned nnames)
{
    char stmp[256];
    unsigned i;
    UNUSED(handle);
    for(i=0;i<nnames;i++) {
	sprintf(stmp,"%-40s %08lX"
		,((udn *)udn_list->data)[i].name
		,(unsigned long)((udn *)udn_list->data)[i].offset);
	if(!ma_AddString(names,stmp,true)) break;
    }
    return true;
}

static bool __FASTCALL__ udnDeleteItem() {
    int ret=-1;
    if(!udn_list) { beye_context().ErrMessageBox("UDN list is empty!",""); return false; }
    std::string title = " User-defined Names (aka bookmarks) ";
    ssize_t nnames = udnGetNumItems(beye_context().sc_bm_file());
    int flags = LB_SELECTIVE;
    bool bval;
    memArray* obj;
    TWindow* w;
    if(!(obj = ma_Build(nnames,true))) goto exit;
    w = PleaseWaitWnd();
    bval = udnReadItems(beye_context().sc_bm_file(),obj,nnames);
    delete w;
    if(bval) {
	if(!obj->nItems) { beye_context().NotifyBox(NOT_ENTRY,title); goto exit; }
	ret = ma_Display(obj,title,flags,-1);
    }
    ma_Destroy(obj);
    exit:
    if(ret!=-1) {
	la_DeleteData(udn_list,ret);
	la_Sort(udn_list,udn_compare);
	udn_modified=true;
    }
    return ret==-1?false:true;
}

bool __FASTCALL__ udnSelectName(__filesize_t *off) {
    int ret=-1;
    if(!udn_list) { beye_context().ErrMessageBox("UDN list is empty!",""); return false; }
    std::string title = " User-defined Names (aka bookmarks) ";
    ssize_t nnames = udnGetNumItems(beye_context().sc_bm_file());
    int flags = LB_SELECTIVE;
    bool bval;
    memArray* obj;
    TWindow* w;
    if(!(obj = ma_Build(nnames,true))) goto exit;
    w = PleaseWaitWnd();
    bval = udnReadItems(beye_context().sc_bm_file(),obj,nnames);
    delete w;
    if(bval) {
	if(!obj->nItems) { beye_context().NotifyBox(NOT_ENTRY,title); goto exit; }
	ret = ma_Display(obj,title,flags,-1);
    }
    ma_Destroy(obj);
    exit:
    if(ret!=-1) *off = ((udn *)udn_list->data)[ret].offset;
    return ret==-1?false:true;
}

bool __FASTCALL__ udnFindName(__filesize_t pa,char *buff, unsigned cb_buff) {
    udn *item;
    udn key;
    if(udn_list) {
	key.name[0]='\0';
	key.offset = pa;
	item=(udn*)la_Find(udn_list,&key,udn_compare);
	if(item) {
	    strncpy(buff,item->name,cb_buff);
	    buff[cb_buff-1]='\0';
	    return true;
	}
    }
    return false;
}

bool __FASTCALL__ __udnSaveList()
{
    unsigned i;
    if(udn_list) {
	FILE *out;
	if((out = fopen(udn_fname.c_str(),"wt"))!=NULL) {
	    fprintf(out,"; This is an automatically generated list of user-defined names\n"
			"; for: %s\n"
			"; by Beye-%s\n"
			,beye_context().bm_file().filename().c_str()
			,BEYE_VERSION);
	    for(i=0;i<udn_list->nItems;i++)
		fprintf(out,"%016llX:%s\n"
		,((udn *)udn_list->data)[i].offset
		,((udn *)udn_list->data)[i].name);
	    fclose(out);
	    udn_modified=false;
	    return true;
	}
	else {
	    char stmp[256];
	    sprintf(stmp,"Can't open file: %s\n",strerror(errno));
	    beye_context().ErrMessageBox(udn_fname,stmp);
	}
    }
    return false;
}


bool __FASTCALL__ udnSaveList() {
    char tmps[4096];
    if(GetStringDlg(tmps," Please enter file name: "," [ENTER] - Proceed ",NAME_MSG))
    {
	udn_fname=tmps;
	if(udn_list)	return __udnSaveList();
	else		beye_context().ErrMessageBox("UDN list is empty!","");
    }
    return false;
}

bool __FASTCALL__  __udnLoadList() {
    unsigned i;
    udn item;
    FILE *in;
    if((in = fopen(udn_fname.c_str(),"rt"))!=NULL) {
	    char buff[4096],*brk;
	    unsigned blen;
	    i = 0;
	    while(!feof(in)) {
		buff[0]='\0';
		fgets(buff,sizeof(buff),in);
		i++;
		if(buff[0]==';'||buff[0]=='\0') continue;
		brk=strchr(buff,':');
		if(!brk) {
		    char stmp[256];
		    sprintf(stmp,"Can't recognize line: %u",i);
		    beye_context().ErrMessageBox(stmp,"");
		    return true;
		}
		*brk='\0';
		sscanf(buff,"%016llX",&item.offset);
		strncpy(item.name,brk+1,sizeof(item.name));
		item.name[sizeof(item.name)-1]='\0';
		blen = strlen(item.name);
		while(item.name[blen-1]==10||item.name[blen-1]==13) {
		    item.name[blen-1]='\0'; blen--;
		}
		if(!udn_list) udn_list = la_Build(0,sizeof(udn),NULL);
		if(udn_list)  la_AddData(udn_list,&item,NULL);
		else break;
	    }
	    fclose(in);
	    if(udn_list) la_Sort(udn_list,udn_compare);
	    return true;
	}
	else {
	    char stmp[256];
	    sprintf(stmp,"Can't open file: %s\n",strerror(errno));
	    beye_context().ErrMessageBox(udn_fname,stmp);
    }
    return false;
}

bool __FASTCALL__ udnLoadList() {
    char tmps[4096];
    if(GetStringDlg(tmps," Please enter file name: "," [ENTER] - Proceed ",NAME_MSG))
    {
	udn_fname=tmps;
	if(udn_list)	return __udnLoadList();
	else		beye_context().ErrMessageBox("UDN list is empty!","");
    }
    return false;
}

static const char *udn_operations[] =
{
    "~Add item",
    "~Delete item",
    "~Load list from file",
    "~Save list to file"
};
typedef bool (__FASTCALL__ *udnFunc)();

static udnFunc udn_funcs[] =
{
    udnAddItem,
    udnDeleteItem,
    udnLoadList,
    udnSaveList
};

bool __FASTCALL__ udnUserNames() {
  unsigned nModes;
  int i;
  nModes = sizeof(udn_operations)/sizeof(char *);
  i = 0;
  i = SelBoxA(udn_operations,nModes," Select operation: ",i);
  if(i != -1)
  {
     int ret;
     TWindow * w;
     w = PleaseWaitWnd();
     ret = (*udn_funcs[i])();
     delete w;
     return ret;
  }
  return false;
}

void __FASTCALL__ udnInit( Ini_Profile& ini ) {
  if(beye_context().is_valid_ini_args())
  {
    udn_fname=beye_context().read_profile_string(ini,"Beye","Browser","udn_list","");
    if(!udn_fname.empty()) __udnLoadList();
  }
}

void __FASTCALL__ udnTerm( Ini_Profile& ini ) {
  if(udn_list) {
    if(udn_modified) {
	beye_context().WarnMessageBox("User-defined list of names was not saved",NULL);
	udnSaveList();
    }
    la_Destroy(udn_list);
  }
  beye_context().write_profile_string(ini,"Beye","Browser","udn_list",udn_fname);
}
} // namespace	usr
