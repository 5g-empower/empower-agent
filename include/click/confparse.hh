// -*- c-basic-offset: 4; related-file-name: "../../lib/confparse.cc" -*-
#ifndef CLICK_CONFPARSE_HH
#define CLICK_CONFPARSE_HH
#include <click/string.hh>
#include <click/vector.hh>
CLICK_DECLS
class ErrorHandler;
class StringAccum;
#ifndef CLICK_TOOL
class Element;
class Router;
class HandlerCall;
# define CP_VA_PARSE_ARGS_REST Element *, ErrorHandler *, ...
# define CP_OPT_CONTEXT , Element *context = 0
# define CP_CONTEXT , Element *context
#else
# define CP_VA_PARSE_ARGS_REST ErrorHandler *, ...
# define CP_OPT_CONTEXT
# define CP_CONTEXT
#endif

const char *cp_skip_space(const char *begin, const char *end);
const char *cp_skip_comment_space(const char *begin, const char *end);
bool cp_eat_space(String &);
inline bool cp_is_space(const String &);
bool cp_is_word(const String &);
bool cp_is_click_id(const String &);

String cp_unquote(const String &);
String cp_quote(const String &, bool allow_newlines = false);
String cp_uncomment(const String &);
const char *cp_process_backslash(const char *begin, const char *end, StringAccum &);

// argument lists <-> lists of arguments
void cp_argvec(const String &, Vector<String> &);
String cp_unargvec(const Vector<String> &);
void cp_spacevec(const String &, Vector<String> &);
String cp_unspacevec(const Vector<String> &);
String cp_pop_spacevec(String &);

enum CpErrors {
    CPE_OK = 0,
    CPE_FORMAT,
    CPE_NEGATIVE,
    CPE_OVERFLOW,
    CPE_INVALID,
    CPE_MEMORY
};
extern int cp_errno;

// strings and words
bool cp_string(const String &, String *, String *rest = 0);
bool cp_word(const String &, String *, String *rest = 0);
bool cp_keyword(const String &, String *, String *rest = 0);

// numbers
bool cp_bool(const String &, bool *);

const char *cp_integer(const char *begin, const char *end, int base, int *);
bool cp_integer(const String &, int base, int *);
inline bool cp_integer(const String &, int *);
const char *cp_unsigned(const char *begin, const char *end, int base, unsigned int *);
bool cp_unsigned(const String &, int base, unsigned int *);
inline bool cp_unsigned(const String &, unsigned int *);

#if SIZEOF_LONG == SIZEOF_INT
inline const char *cp_integer(const char *begin, const char *end, int base, long *);
inline bool cp_integer(const String &, int base, long *);
inline bool cp_integer(const String &, long *);
inline const char *cp_unsigned(const char *begin, const char *end, int base, unsigned long *);
inline bool cp_unsigned(const String &, int base, unsigned long *);
inline bool cp_unsigned(const String &, unsigned long *);
#elif SIZEOF_LONG != 8
# error "long has an odd size"
#endif

#ifdef HAVE_INT64_TYPES
# if !HAVE_64_BIT_LONG
const char *cp_integer(const char *begin, const char *end, int base, int64_t *);
bool cp_integer(const String &, int base, int64_t *);
inline bool cp_integer(const String &, int64_t *);
const char *cp_unsigned(const char *begin, const char *end, int base, uint64_t *);
bool cp_unsigned(const String &, int base, uint64_t *);
inline bool cp_unsigned(const String &, uint64_t *);
# endif
# define cp_integer64 cp_integer
# define cp_unsigned64 cp_unsigned
#endif

#ifdef CLICK_USERLEVEL
bool cp_file_offset(const String &, off_t *);
#endif

#define CP_REAL2_MAX_FRAC_BITS 28
bool cp_real2(const String &, int frac_bits, int32_t *);
bool cp_unsigned_real2(const String &, int frac_bits, uint32_t *);
bool cp_real10(const String &, int frac_digits, int32_t *);
bool cp_unsigned_real10(const String &, int frac_digits, uint32_t *);
bool cp_unsigned_real10(const String &, int frac_digits, uint32_t *, uint32_t *);
#ifdef HAVE_FLOAT_TYPES
bool cp_double(const String &, double *);
#endif

bool cp_seconds_as(int want_power, const String &, uint32_t *);
bool cp_seconds_as_milli(const String &, uint32_t *);
bool cp_seconds_as_micro(const String &, uint32_t *);
bool cp_timeval(const String &, struct timeval *);

bool cp_bandwidth(const String &, uint32_t *);

String cp_unparse_bool(bool);
#ifdef HAVE_INT64_TYPES
String cp_unparse_integer64(int64_t, int base, bool uppercase);
String cp_unparse_unsigned64(uint64_t, int base, bool uppercase);
#endif
String cp_unparse_real2(int32_t, int frac_bits);
String cp_unparse_real2(uint32_t, int frac_bits);
#ifdef HAVE_INT64_TYPES
String cp_unparse_real2(int64_t, int frac_bits);
String cp_unparse_real2(uint64_t, int frac_bits);
#endif
String cp_unparse_real10(int32_t, int frac_digits);
String cp_unparse_real10(uint32_t, int frac_digits);
String cp_unparse_milliseconds(uint32_t);
String cp_unparse_microseconds(uint32_t);
String cp_unparse_interval(const struct timeval &);
String cp_unparse_bandwidth(uint32_t);

// network addresses
class IPAddress;
class IPAddressList;
bool cp_ip_address(const String &, unsigned char *  CP_OPT_CONTEXT);
bool cp_ip_address(const String &, IPAddress *  CP_OPT_CONTEXT);
bool cp_ip_prefix(const String &, unsigned char *, unsigned char *, bool allow_bare_address  CP_OPT_CONTEXT);
bool cp_ip_prefix(const String &, IPAddress *, IPAddress *, bool allow_bare_address  CP_OPT_CONTEXT);
bool cp_ip_prefix(const String &, unsigned char *, unsigned char *  CP_OPT_CONTEXT);
bool cp_ip_prefix(const String &, IPAddress *, IPAddress *  CP_OPT_CONTEXT);
bool cp_ip_address_list(const String &, IPAddressList *  CP_OPT_CONTEXT);

#ifdef HAVE_IP6
class IP6Address;
bool cp_ip6_address(const String &, unsigned char *  CP_OPT_CONTEXT);
bool cp_ip6_address(const String &, IP6Address *  CP_OPT_CONTEXT);
bool cp_ip6_prefix(const String &, unsigned char *, int *, bool allow_bare_address  CP_OPT_CONTEXT);
bool cp_ip6_prefix(const String &, IP6Address *, int *, bool allow_bare_address  CP_OPT_CONTEXT);
bool cp_ip6_prefix(const String &, unsigned char *, unsigned char *, bool allow_bare_address  CP_OPT_CONTEXT);
bool cp_ip6_prefix(const String &, IP6Address *, IP6Address *, bool allow_bare_address  CP_OPT_CONTEXT);
#endif

class EtherAddress;
bool cp_ethernet_address(const String &, unsigned char *  CP_OPT_CONTEXT);
bool cp_ethernet_address(const String &, EtherAddress *  CP_OPT_CONTEXT);

bool cp_tcpudp_port(const String &, int ip_p, uint16_t *  CP_OPT_CONTEXT);

#ifndef CLICK_TOOL
Element *cp_element(const String &, Element *, ErrorHandler *);
Element *cp_element(const String &, Router *, ErrorHandler *);
bool cp_handler(const String &, Element *, Element **, String *, ErrorHandler *);
bool cp_handler(const String &, Element *, Element **, int *, ErrorHandler *);
bool cp_handler(const String &, Element *, bool need_r, bool need_w, Element **, int *, ErrorHandler *);
bool cp_handler_call(const String &, Element *, bool is_write, HandlerCall **, ErrorHandler *);
#endif

#ifdef HAVE_IPSEC
bool cp_des_cblock(const String &, unsigned char *);
#endif

#ifndef CLICK_LINUXMODULE
bool cp_filename(const String &, String *);
#endif

typedef const char *CpVaParseCmd;
static const CpVaParseCmd cpEnd = 0;
extern const CpVaParseCmd
    cpOptional,
    cpKeywords,
    cpConfirmKeywords,
    cpMandatoryKeywords,
    cpIgnore,
    cpIgnoreRest,
			// HELPER		RESULT
    cpArgument,		//			String *
    cpArguments,	//			Vector<String> *
    cpString,		//			String *
    cpWord,		//			String *
    cpKeyword,		//			String *
    cpBool,		//			bool *
    cpByte,		//			unsigned char *
    cpShort,		//			short *
    cpUnsignedShort,	//			unsigned short *
    cpInteger,		//			int *
    cpUnsigned,		//			unsigned *
#ifdef HAVE_INT64_TYPES
    cpInteger64,	//			int64_t *
    cpUnsigned64,	//			uint64_t *
#endif
#ifdef CLICK_USERLEVEL
    cpFileOffset,	// 			off_t *
#endif
    cpUnsignedReal2,	// int frac_bits	unsigned *
    cpReal10,		// int frac_digits	int *
    cpUnsignedReal10,	// int frac_digits	unsigned *
#ifdef HAVE_FLOAT_TYPES
    cpDouble,		//			double *
#endif
    cpSeconds,		//			uint32_t *
    cpSecondsAsMilli,	//			uint32_t *milliseconds
    cpSecondsAsMicro,	//			uint32_t *microseconds
    cpTimeval,		//			struct timeval *
    cpInterval,		//			struct timeval *
    cpBandwidth,	//			uint32_t *bandwidth (in B/s)
    cpIPAddress,	//			IPAddress *
    cpIPPrefix,		//			IPAddress *a, IPAddress *mask
    cpIPAddressOrPrefix,//			IPAddress *a, IPAddress *mask
    cpIPAddressList,	//			IPAddressList *
    cpEthernetAddress,	//			EtherAddress *
    cpTCPPort,		// 			uint16_t *
    cpUDPPort,		// 			uint16_t *
    cpElement,		//			Element **
    cpHandlerName,	//			Element **e, String *hname
    cpHandler,		//			Element **e, int *hid (INITIALIZE TIME)
    cpReadHandler,	//			Element **e, int *hid (INITIALIZE TIME)
    cpWriteHandler,	//			Element **e, int *hid (INITIALIZE TIME)
    cpReadHandlerCall,	//			HandlerCall **
    cpWriteHandlerCall,	//			HandlerCall **
    cpIP6Address,	//			IP6Address *
    cpIP6Prefix,	//			IP6Address *a, IP6Address *mask
    cpIP6AddressOrPrefix,//			IP6Address *a, IP6Address *mask
    cpDesCblock,	//			uint8_t[8]
    cpFilename,		//			String *
    // old names, here for compatibility:
    cpMilliseconds,	//			int *milliseconds
    cpUnsignedLongLong,	//			uint64_t *
    cpNonnegReal2,	// int frac_bits	unsigned *
    cpNonnegReal10,	// int frac_digits	unsigned *
    cpEtherAddress;	//			EtherAddress *

int cp_va_parse(const Vector<String> &argv, CP_VA_PARSE_ARGS_REST);
int cp_va_parse(const String &arg, CP_VA_PARSE_ARGS_REST);
int cp_va_space_parse(const String &arg, CP_VA_PARSE_ARGS_REST);
int cp_va_parse_keyword(const String &arg, CP_VA_PARSE_ARGS_REST);
int cp_va_parse_remove_keywords(Vector<String> &argv, int, CP_VA_PARSE_ARGS_REST);
// Takes: cpEnd					end of argument list
//        cpOptional, cpKeywords, cpIgnore...	manipulators
//        CpVaParseCmd type_id,			actual argument
//		const char *description,
//		[[any HELPER arguments from table; usually none]],
//		[[if cpConfirmKeywords, bool *confirm_keyword_given]],
//		[[RESULT arguments from table; usually T *]]
// Returns the number of result arguments set, or negative on error.
// Stores no values in the result arguments on error.

int cp_assign_arguments(const Vector<String> &argv, const Vector<String> &keys, Vector<String> *values = 0);

void cp_va_static_initialize();
void cp_va_static_cleanup();

// adding and removing types suitable for cp_va_parse and friends
struct cp_value;
struct cp_argtype;

typedef void (*cp_parsefunc)(cp_value *, const String &arg,
			     ErrorHandler *, const char *argdesc  CP_CONTEXT);
typedef void (*cp_storefunc)(cp_value *  CP_CONTEXT);

enum { cpArgNormal = 0, cpArgStore2 = 1, cpArgExtraInt = 2, cpArgAllowNumbers = 4 };
int cp_register_argtype(const char *name, const char *description,
			int flags, cp_parsefunc, cp_storefunc, void *user_data = 0);
void cp_unregister_argtype(const char *name);

int cp_register_stringlist_argtype(const char *name, const char *description,
				   int flags);
int cp_extend_stringlist_argtype(const char *name, ...);
// Takes: const char *name, int value, ...., const char *ender = 0

struct cp_argtype {
    const char *name;
    cp_argtype *next;
    cp_parsefunc parse;
    cp_storefunc store;
    void *user_data;
    int flags;
    const char *description;
    int internal;
    int use_count;
};

struct cp_value {
    // set by cp_va_parse:
    const cp_argtype *argtype;
    const char *keyword;
    const char *description;
    int extra;
    void *store;
    void *store2;
    bool *store_confirm;
    // set by parsefunc, used by storefunc:
    union {
	bool b;
	int32_t i;
	uint32_t u;
#ifdef HAVE_INT64_TYPES
	int64_t i64;
	uint64_t u64;
#endif
#ifdef HAVE_FLOAT_TYPES
	double d;
#endif
	unsigned char address[16];
	int is[4];
#ifndef CLICK_TOOL
	Element *element;
#endif
    } v, v2;
    String v_string;
    String v2_string;
};

#undef CP_VA_ARGS_REST
#undef CP_OPT_CONTEXT
#undef CP_CONTEXT

inline bool cp_is_space(const String &str)
{
    return cp_skip_space(str.begin(), str.end()) == str.end();
}

inline bool cp_unsigned(const String &str, unsigned int *return_value)
{
    return cp_unsigned(str, 0, return_value);
}

inline bool cp_integer(const String &str, int *return_value)
{
    return cp_integer(str, 0, return_value);
}

#ifdef HAVE_INT64_TYPES
inline bool cp_unsigned(const String &str, uint64_t *return_value)
{
    return cp_unsigned(str, 0, return_value);
}

inline bool cp_integer(const String &str, int64_t *return_value)
{
    return cp_integer(str, 0, return_value);
}
#endif

#if SIZEOF_LONG == SIZEOF_INT
inline const char *cp_integer(const char *begin, const char *end, int base, long *return_value)
{
    return cp_integer(begin, end, base, reinterpret_cast<int *>(return_value));
}

inline bool cp_integer(const String &str, int base, long *return_value)
{
    return cp_integer(str, base, reinterpret_cast<int *>(return_value));
}

inline bool cp_integer(const String &str, long *return_value)
{
    return cp_integer(str, reinterpret_cast<int *>(return_value));
}

inline const char *cp_unsigned(const char *begin, const char *end, int base, unsigned long *return_value)
{
    return cp_unsigned(begin, end, base, reinterpret_cast<unsigned int *>(return_value));
}

inline bool cp_unsigned(const String &str, int base, unsigned long *return_value)
{
    return cp_unsigned(str, base, reinterpret_cast<unsigned int *>(return_value));
}

inline bool cp_unsigned(const String &str, unsigned long *return_value)
{
    return cp_unsigned(str, reinterpret_cast<unsigned int *>(return_value));
}
#endif

CLICK_ENDDECLS
#endif
