#ifndef _unicodestring_
#define _unicodestring_

#include <string>

#ifdef _WIN32
	#if (_MSC_VER >= 1700)
		#include <stdint.h>
	#else
		#include "../pstdint.h"
	#endif
#else
#include <stdint.h>
#endif

class UnicodeString
{
public:
	typedef uint32_t utf32_t;
	typedef std::basic_string<utf32_t> utf32_string;
	typedef utf32_string::value_type uvalue_type;
	typedef utf32_string::size_type usize_type;
	static const utf32_string::size_type unpos;

	UnicodeString();
	UnicodeString(const UnicodeString &unistr);
	UnicodeString(const utf32_string &utf32str);
	UnicodeString(const std::string &utf8str);

	const std::string UTF8String() const;
	const std::string &UTF8String();
	const utf32_string UTF32String() const;
	const utf32_string &UTF32String();

	// number of Unicode Code Points, not number of bytes
	const usize_type Size() const;
	const usize_type Size();
	void Trim(const usize_type charpos);
	UnicodeString SubStr(const usize_type offset, const usize_type count);
	UnicodeString &Replace(const usize_type offset, const usize_type number, const UnicodeString &right);
	UnicodeString &Erase(const usize_type offset, const usize_type count);

	static const bool IsWhitespace(const uvalue_type &ch);
	const usize_type Find(const UnicodeString &str, const usize_type offset=0) const;
	const usize_type FindLastOf(const UnicodeString &str, const usize_type offset=0) const;

	UnicodeString &operator=(const UnicodeString &rhs);
	UnicodeString &operator=(const utf32_string &rhs);
	UnicodeString &operator=(const std::string &rhs);
	UnicodeString &operator+=(const UnicodeString &rhs);
	UnicodeString &operator+=(const utf32_string &rhs);
	UnicodeString &operator+=(const std::string &rhs);
	const UnicodeString operator+(const UnicodeString &rhs) const;
	const UnicodeString operator+(const utf32_string &rhs) const;
	const UnicodeString operator+(const std::string &rhs) const;
	uvalue_type &operator[](const usize_type elem);

protected:
	void CheckAndReplaceInvalid();
	void Widen();
	void Narrowen();

	static const uvalue_type m_unicodewhitespace[];

	enum Flag
	{
		FLAG_WIDE_OK=1,
		FLAG_NARROW_OK=2
	};
	int m_flags;
	utf32_string m_widestring;
	std::string m_narrowstring;
};

#endif	// _unicodestring_
