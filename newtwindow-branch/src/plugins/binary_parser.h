#ifndef BINARY_PARSER_HPP_INCLUDED
#define BINARY_PARSER_HPP_INCLUDED 1

namespace	usr {
    class binary_stream;
    class CodeGuider;
    class udn;
    struct Symbol_Info;
    struct Object_Info;
    class Bin_Format;

    class Binary_Parser : public Opaque {
	public:
	    Binary_Parser(BeyeContext&,binary_stream&,CodeGuider&,udn&) {}
	    virtual ~Binary_Parser() {}

	    virtual const char*		prompt(unsigned idx) const = 0;   /**< on ALT-Fx selection */

	    virtual __filesize_t	action_F1() { return Plugin::Bad_Address; }
	    virtual __filesize_t	action_F2() { return Plugin::Bad_Address; }
	    virtual __filesize_t	action_F3() { return Plugin::Bad_Address; }
	    virtual __filesize_t	action_F4() { return Plugin::Bad_Address; }
	    virtual __filesize_t	action_F5() { return Plugin::Bad_Address; }
	    virtual __filesize_t	action_F6() { return Plugin::Bad_Address; }
	    virtual __filesize_t	action_F7() { return Plugin::Bad_Address; }
	    virtual __filesize_t	action_F8() { return Plugin::Bad_Address; }
	    virtual __filesize_t	action_F9() { return Plugin::Bad_Address; }
	    virtual __filesize_t	action_F10(){ return Plugin::Bad_Address; }

	    virtual __filesize_t	show_header() const { return Plugin::Bad_Address; }  /**< if not an MZ style format */
			/**
			  Binss disassembler reference to string.
			   * @param str          string buffer for append to
			   * @param shift        physical address of field, that required of binding
			   * @param flags        see above
			   * @param codelen      length of field, that required binding
			   * @param r_shift      used only if APPREF_TRY_LABEL mode is set, contains real value of field, that required binding
			   * @return             true if reference was appended
			*/
	    virtual std::string		bind(const DisMode& _parent,__filesize_t shift,Bin_Format::bind_type flg,int codelen,__filesize_t r_shift) { UNUSED(_parent); UNUSED(shift); UNUSED(flg); UNUSED(codelen); UNUSED(r_shift); return ""; }

			 /** Returns CPU platform, that required by format.
			   * @note           Full list of platform please see in
			   *                 plugins/disasm.h file. If this
			   *                 function return -1 then platform is
			   *                 undefined.
			  **/
	    virtual int			query_platform() const = 0;
	    virtual Bin_Format::bitness	query_bitness(__filesize_t) const { return Bin_Format::Use16; }
	    virtual Bin_Format::endian	query_endian(__filesize_t) const { return Bin_Format::Little; }

			 /** For displaying offset within struct in left address column.
			   * @return         empty string on fail.
			  **/
	    virtual std::string		address_resolving(__filesize_t) { return ""; }

			 /** Converts virtual address to physical (means file offset).
			   * @param va       indicates virtual address to be converted
			   * @return         0 if operation meaningless
			  **/
	    virtual __filesize_t	va2pa(__filesize_t) const { return Plugin::Bad_Address; }

			 /** Converts physical address to virtual.
			   * @param pa       indicates physical address to be converted
			   * @note           seg pointer can be NULL
			  **/
	    virtual __filesize_t	pa2va(__filesize_t) const { return Plugin::Bad_Address; }

			 /** Fills the string with public symbol
			   * @param pa        indicates physical offset within file
			   * @param as_prev   indicates direction of symbol searching from given physical offset
			  **/
	    virtual Symbol_Info		get_public_symbol(__filesize_t pa,bool as_prev) { Symbol_Info rc; rc.pa=Plugin::Bad_Address; UNUSED(pa); UNUSED(as_prev); return rc; }

			 /** Determines attributes of object at given physical file address.
			   * @param pa        indicates physical file offset of object
			   * @remark          For example: if exe-format - new
			   *                  exe i.e. contains MZ and NEW
			   *                  header and given file offset
			   *                  points to old exe stub then start
			   *                  = 0, end = begin of first data or
			   *                  code object).
			  **/
	    virtual Object_Info		get_object_attribute(__filesize_t pa) { Object_Info rc; rc.number=0; UNUSED(pa);  return rc; }
    };

    struct Binary_Parser_Info {
	const char*	name;
	Binary_Parser* (*query_interface)(BeyeContext&,binary_stream&,CodeGuider&,udn&);
    };


    struct symbolic_information {
	__filesize_t pa;
	__filesize_t nameoff;
	__filesize_t addinfo;
	__filesize_t attr;

	bool operator<(const symbolic_information& rhs) const { return pa < rhs.pa; }
    };

    template<class T,class K>
    inline typename T::const_iterator find_nearest(const T& obj, const K& key) {
	typename T::const_iterator it;
	it=obj.find(key);
	if(it!=obj.end()) return it;
	if(key.pa<(*obj.begin()).pa) return obj.begin();
	typename T::const_iterator prev;
	it=obj.begin();
	prev=it;
	it++;
	for(;it!=obj.end();it++) {
	    if((*prev).pa>=key.pa && key.pa < (*it).pa) return prev;
	}
	return obj.end();
    }

    template<class T,class K>
    inline Symbol_Info find_symbolic_information(const T& names,K& key,bool as_prev,typename T::const_iterator& index)
{
    Symbol_Info rc;
    __filesize_t cur_addr;
    typename T::const_iterator i,idx;
    rc.pa=Plugin::Bad_Address;
    if(names.empty()) return rc;
    index = idx = names.end();
    i = find_nearest(names,key);
    if(as_prev) idx = i;
    else {
	static typename T::const_iterator multiref_i = names.begin();
	i = names.begin();
	get_next:
	while((cur_addr=(*i).pa)<=key.pa) {
	    i++;
	    if((cur_addr == key.pa && (*i).pa > (*multiref_i).pa) || (i == names.end())) break;
	}
	idx = i;
	if(idx!=names.end()) rc.pa = cur_addr;
	else rc.pa = Plugin::Bad_Address;
	if(rc.pa!=Plugin::Bad_Address && rc.pa == key.pa) {
	    if((*idx).pa <= (*multiref_i).pa) { i = idx; goto get_next; }
	    else multiref_i = idx;
	}
	else multiref_i = names.begin();
    }
    if(idx!=names.end()) {
	rc.pa = (*idx).pa;
	rc._class = Symbol_Info::symbol_class((*idx).attr);
	if(idx==names.begin() && key.pa < rc.pa && as_prev) rc.pa = Plugin::Bad_Address;
	else index = idx;
    }
    return rc;
}

} // namespace	usr
#endif
