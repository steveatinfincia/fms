#include "../../include/unicode/unicodestring.h"
#include "../../include/unicode/utf8.h"

const UnicodeString::uvalue_type UnicodeString::m_unicodewhitespace[]={0x0009,0x000A,0x000B,0x000C,0x000D,
												0x0020,0x0085,0x00A0,0x1680,0x180E,
												0x2000,0x2001,0x2002,0x2003,0x2004,
												0x2005,0x2006,0x2007,0x2008,0x2009,
												0x200A,0x200B,0x2029,0x202F,0x205F,
												0x3000,0xFEFF};
const UnicodeString::utf32_string::size_type UnicodeString::unpos(UnicodeString::utf32_string::npos);

UnicodeString::UnicodeString():m_flags(FLAG_WIDE_OK | FLAG_NARROW_OK),m_widestring(),m_narrowstring("")
{

}

UnicodeString::UnicodeString(const UnicodeString &rhs):m_flags(rhs.m_flags),m_widestring(rhs.m_widestring),m_narrowstring(rhs.m_narrowstring)
{

}

UnicodeString::UnicodeString(const utf32_string &rhs):m_flags(FLAG_WIDE_OK),m_widestring(rhs),m_narrowstring("")
{

}

UnicodeString::UnicodeString(const std::string &rhs):m_flags(FLAG_NARROW_OK),m_widestring(),m_narrowstring(rhs)
{
	CheckAndReplaceInvalid();
}

void UnicodeString::CheckAndReplaceInvalid()
{
	if((m_flags & FLAG_NARROW_OK)==FLAG_NARROW_OK)
	{
		std::string::iterator i=m_narrowstring.begin();
		try
		{
			i=utf8::find_invalid(m_narrowstring.begin(),m_narrowstring.end());
		}
		catch(...)
		{
		}
		if(i!=m_narrowstring.end())
		{
			bool ok=false;
			std::string validstring("");

			do
			{
				try
				{
					validstring.clear();
					utf8::replace_invalid(m_narrowstring.begin(),m_narrowstring.end(),std::back_inserter(validstring));
					ok=true;
					m_narrowstring=validstring;
					m_flags=FLAG_NARROW_OK;
				}
				catch(utf8::not_enough_room &ex)
				{
					if(m_narrowstring.size()>0)
					{
						m_narrowstring.erase(m_narrowstring.size()-1);
					}
					ok=false;
				}
			}while(ok==false && m_narrowstring.size()>0);

		}
	}
}

UnicodeString &UnicodeString::Erase(const usize_type offset, const usize_type count)
{
	if(Size()>offset && offset>=0 && count>0)
	{
		if((m_flags & FLAG_NARROW_OK) != FLAG_NARROW_OK)
		{
			Narrowen();
		}
		std::string::iterator beginit=m_narrowstring.begin();
		std::string::iterator endit=m_narrowstring.end();
		try
		{
			utf8::advance(beginit,offset,endit);
			endit=beginit;
			usize_type realcount=count;

			size_t dist=utf8::distance(endit,m_narrowstring.end());
			if(dist<realcount)
			{
				realcount=dist;
			}

			utf8::advance(endit,realcount,m_narrowstring.end());
			m_narrowstring.erase(beginit,endit);
			m_flags=FLAG_NARROW_OK;
		}
		catch(...)
		{
		}
	}
	return *this;
}

const UnicodeString::usize_type UnicodeString::Find(const UnicodeString &str, const usize_type offset) const
{
	usize_type pos=unpos;
	std::string narrow("");
	if(Size()<offset)
	{
		return unpos;
	}
	if((m_flags & FLAG_NARROW_OK) == FLAG_NARROW_OK)
	{
		narrow=m_narrowstring;
	}
	else if((m_flags & FLAG_NARROW_OK) != FLAG_NARROW_OK && (m_flags & FLAG_WIDE_OK) == FLAG_WIDE_OK)
	{
		utf8::utf32to8(m_widestring.begin(),m_widestring.end(),std::back_inserter(narrow));
	}
	else
	{
		return unpos;
	}

	std::string::iterator beginit=narrow.begin();
	std::string::iterator endit=narrow.end();

	try
	{
		utf8::advance(beginit,offset,endit);
		std::string temp(beginit,endit);
		std::string::size_type narrowpos=temp.find(str.UTF8String());
		if(narrowpos!=std::string::npos)
		{
			pos=utf8::distance(m_narrowstring.begin(),m_narrowstring.begin()+narrowpos+(narrow.size()-temp.size()));
		}
		else
		{
			pos=unpos;
		}
	}
	catch(...)
	{
	}

	return pos;
}

const UnicodeString::usize_type UnicodeString::FindLastOf(const UnicodeString &str, const usize_type offset) const
{
	usize_type pos=unpos;
	std::string narrow("");
	if(Size()<offset)
	{
		return unpos;
	}
	if((m_flags & FLAG_NARROW_OK) == FLAG_NARROW_OK)
	{
		narrow=m_narrowstring;
	}
	else if((m_flags & FLAG_NARROW_OK) != FLAG_NARROW_OK && (m_flags & FLAG_WIDE_OK) == FLAG_WIDE_OK)
	{
		utf8::utf32to8(m_widestring.begin(),m_widestring.end(),std::back_inserter(narrow));
	}
	else
	{
		return unpos;
	}

	std::string::iterator beginit=narrow.begin();
	std::string::iterator endit=narrow.end();

	try
	{
		utf8::advance(beginit,offset,endit);
		std::string temp(beginit,endit);
		std::string::size_type narrowpos=temp.find_last_of(str.UTF8String());
		if(narrowpos!=std::string::npos)
		{
			pos=utf8::distance(m_narrowstring.begin(),m_narrowstring.begin()+narrowpos+(narrow.size()-temp.size()));
		}
		else
		{
			pos=unpos;
		}
	}
	catch(...)
	{
	}

	return pos;
}

const bool UnicodeString::IsWhitespace(const uvalue_type &ch)
{
	for(usize_type i=0; m_unicodewhitespace[i]!=0; i++)
	{
		if(m_unicodewhitespace[i]==ch)
		{
			return true;
		}
	}
	return false;
}

void UnicodeString::Narrowen()
{
	if((m_flags & FLAG_NARROW_OK) != FLAG_NARROW_OK && (m_flags & FLAG_WIDE_OK) == FLAG_WIDE_OK)
	{
		m_narrowstring.clear();
		utf8::utf32to8(m_widestring.begin(),m_widestring.end(),std::back_inserter(m_narrowstring));
		m_flags |= FLAG_NARROW_OK;
	}
}

UnicodeString &UnicodeString::operator=(const UnicodeString &rhs)
{
	if(this==&rhs)
	{
		return *this;
	}
	m_flags=rhs.m_flags;
	m_narrowstring=rhs.m_narrowstring;
	m_widestring=rhs.m_widestring;
	return *this;
}

UnicodeString &UnicodeString::operator=(const utf32_string &rhs)
{
	if(&m_widestring==&rhs)
	{
		return *this;
	}
	m_flags=FLAG_WIDE_OK;
	m_narrowstring.clear();
	m_widestring=rhs;
	return *this;
}

UnicodeString &UnicodeString::operator=(const std::string &rhs)
{
	if(&m_narrowstring==&rhs)
	{
		return *this;
	}
	m_flags=FLAG_NARROW_OK;
	m_narrowstring=rhs;
	m_widestring.clear();
	CheckAndReplaceInvalid();
	return *this;
}

UnicodeString &UnicodeString::operator+=(const UnicodeString &rhs)
{
	m_narrowstring=UTF8String()+rhs.UTF8String();
	m_flags=FLAG_NARROW_OK;
	CheckAndReplaceInvalid();
	return *this;
}

UnicodeString &UnicodeString::operator+=(const utf32_string &rhs)
{
	m_widestring=UTF32String()+rhs;
	m_flags=FLAG_WIDE_OK;
	return *this;
}

UnicodeString &UnicodeString::operator+=(const std::string &rhs)
{
	m_narrowstring=UTF8String()+rhs;
	m_flags=FLAG_NARROW_OK;
	CheckAndReplaceInvalid();
	return *this;
}

const UnicodeString UnicodeString::operator+(const UnicodeString &rhs) const
{
	return UnicodeString(*this)+=rhs;
}

const UnicodeString UnicodeString::operator+(const utf32_string &rhs) const
{
	return UnicodeString(*this)+=rhs;
}

const UnicodeString UnicodeString::operator+(const std::string &rhs) const
{
	return UnicodeString(*this)+=rhs;
}

UnicodeString::uvalue_type &UnicodeString::operator[](const usize_type elem)
{
	if((m_flags & FLAG_WIDE_OK) != FLAG_WIDE_OK)
	{
		Widen();
	}
	return m_widestring[elem];
}

UnicodeString &UnicodeString::Replace(const usize_type offset, const usize_type count, const UnicodeString &str)
{
	if(Size()>offset && offset>=0 && count>0)
	{
		if((m_flags & FLAG_NARROW_OK) != FLAG_NARROW_OK)
		{
			Narrowen();
		}
		std::string::iterator beginit=m_narrowstring.begin();
		std::string::iterator endit=m_narrowstring.end();
		try
		{
			utf8::advance(beginit,offset,endit);
			endit=beginit;
			usize_type realcount=count;

			size_t dist=utf8::distance(endit,m_narrowstring.end());
			if(dist<realcount)
			{
				realcount=dist;
			}

			utf8::advance(endit,realcount,m_narrowstring.end());
			m_narrowstring.replace(beginit,endit,str.UTF8String());
			m_flags=FLAG_NARROW_OK;
		}
		catch(...)
		{
		}
	}
	return *this;
}

const UnicodeString::usize_type UnicodeString::Size() const
{
	if((m_flags & FLAG_WIDE_OK) != FLAG_WIDE_OK)
	{
		utf32_string tempstr;
		utf8::utf8to32(m_narrowstring.begin(),m_narrowstring.end(),std::back_inserter(tempstr));
		return tempstr.size();
	}
	else
	{
		return m_widestring.size();
	}
}

const UnicodeString::usize_type UnicodeString::Size()
{
	if((m_flags & FLAG_WIDE_OK) != FLAG_WIDE_OK)
	{
		Widen();
	}
	return m_widestring.size();
}

UnicodeString UnicodeString::SubStr(const usize_type offset, const usize_type count)
{
	std::string narrow("");
	if(Size()>offset && offset>=0 && count>0)
	{
		if((m_flags & FLAG_NARROW_OK) != FLAG_NARROW_OK)
		{
			Narrowen();
		}
		std::string::iterator beginit=m_narrowstring.begin();
		std::string::iterator endit=m_narrowstring.end();

		try
		{
			utf8::advance(beginit,offset,endit);
			endit=beginit;
			usize_type realcount=count;

			size_t dist=utf8::distance(endit,m_narrowstring.end());
			if(dist<realcount)
			{
				realcount=dist;
			}

			utf8::advance(endit,realcount,m_narrowstring.end());

			size_t bpos=beginit-m_narrowstring.begin();
			size_t epos=endit-m_narrowstring.begin();

			return m_narrowstring.substr(bpos,epos-bpos);
		}
		catch(...)
		{
		}
	}
	return UnicodeString(narrow);
}

void UnicodeString::Trim(const usize_type charpos)
{
	if(Size()>charpos && charpos>0)
	{
		if((m_flags & FLAG_NARROW_OK) != FLAG_NARROW_OK)
		{
			Narrowen();
		}
		std::string::iterator it=m_narrowstring.begin();
		try
		{
			utf8::advance(it,charpos,m_narrowstring.end());
		}
		catch(...)
		{
		}

		m_narrowstring.erase(it,m_narrowstring.end());
		m_flags=FLAG_NARROW_OK;
	}
	else if(charpos==0)
	{
		m_narrowstring.clear();
		m_widestring.clear();
		m_flags=FLAG_WIDE_OK|FLAG_NARROW_OK;
	}
}

const std::string UnicodeString::UTF8String() const
{
	if((m_flags & FLAG_NARROW_OK) != FLAG_NARROW_OK && (m_flags & FLAG_WIDE_OK) == FLAG_WIDE_OK)
	{
		std::string tempstr;
		utf8::utf32to8(m_widestring.begin(),m_widestring.end(),std::back_inserter(tempstr));
		return tempstr;
	}
	else if((m_flags & FLAG_NARROW_OK) == FLAG_NARROW_OK)
	{
		return m_narrowstring;
	}
	else
	{
		return std::string("");
	}
}

const std::string &UnicodeString::UTF8String()
{
	if((m_flags & FLAG_NARROW_OK) != FLAG_NARROW_OK && (m_flags & FLAG_WIDE_OK) == FLAG_WIDE_OK)
	{
		Narrowen();
	}
	return m_narrowstring;
}

const UnicodeString::utf32_string UnicodeString::UTF32String() const
{
	if((m_flags & FLAG_WIDE_OK) != FLAG_WIDE_OK && (m_flags & FLAG_NARROW_OK) == FLAG_NARROW_OK)
	{
		utf32_string tempstr;
		utf8::utf8to32(m_narrowstring.begin(),m_narrowstring.end(),std::back_inserter(tempstr));
		return tempstr;
	}
	else if((m_flags & FLAG_WIDE_OK) == FLAG_WIDE_OK)
	{
		return m_widestring;
	}
	else
	{
		return utf32_string();
	}
}

const UnicodeString::utf32_string &UnicodeString::UTF32String()
{
	if((m_flags & FLAG_WIDE_OK) != FLAG_WIDE_OK && (m_flags & FLAG_NARROW_OK) == FLAG_NARROW_OK)
	{
		Widen();
	}
	return m_widestring;
}

void UnicodeString::Widen()
{
	if((m_flags & FLAG_WIDE_OK) != FLAG_WIDE_OK && (m_flags & FLAG_NARROW_OK) == FLAG_NARROW_OK)
	{
		m_widestring.clear();
		utf8::utf8to32(m_narrowstring.begin(),m_narrowstring.end(),std::back_inserter(m_widestring));
		m_flags |= FLAG_WIDE_OK;
	}
}
