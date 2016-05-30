#include "../include/option.h"
#include "../include/db/sqlite3db.h"

#ifdef XMEM
	#include <xmem.h>
#endif

std::map<std::string,std::string> Option::m_cache;

void Option::ClearCache()
{
	m_cache.clear();
}

const bool Option::Get(const std::string &option, std::string &value)
{
	if(m_cache.find(option)!=m_cache.end())
	{
		value=m_cache[option];
		return true;
	}
	else
	{
		SQLite3DB::Statement st=m_db->Prepare("SELECT OptionValue FROM tblOption WHERE Option=?;");
		st.Bind(0,option);
		st.Step();
		if(st.RowReturned())
		{
			st.ResultText(0,value);
			m_cache[option]=value;
			return true;
		}
		else
		{
			return false;
		}
	}
}

const bool Option::GetBool(const std::string &option, bool &value)
{
	std::string valstr="";
	if(Get(option,valstr) && valstr=="true" || valstr=="false" || valstr=="1" || valstr=="0")
	{
		if(valstr=="true" || valstr=="1")
		{
			value=true;
		}
		else
		{
			value=false;
		}
		return true;
	}
	else
	{
		return false;
	}
}

const bool Option::GetInt(const std::string &option, int &value)
{
	std::string valstr="";
	if(Get(option,valstr))
	{
		std::istringstream istr(valstr);
		if(istr >> value)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}
