#ifndef _option_
#define _option_

#include "idatabase.h"

#include <sstream>
#include <map>

//just a wrapper around the database for the options table
class Option:public IDatabase
{
public:
	Option(SQLite3DB::DB *db):IDatabase(db)			{}

	void ClearCache();

	const bool Get(const std::string &option, std::string &value);
	const bool GetInt(const std::string &option, int &value);
	const bool GetBool(const std::string &option, bool &value);
	template<class T>
	void Set(const std::string &option, const T &value);
private:
	static std::map<std::string,std::string> m_cache;
};

template<class T>
void Option::Set(const std::string &option, const T &value)
{
	std::ostringstream valuestr;
	valuestr << value;

	std::string tempval;
	if(Get(option,tempval)==true)
	{
		SQLite3DB::Statement st=m_db->Prepare("UPDATE tblOption SET OptionValue=? WHERE Option=?;");
		st.Bind(0,valuestr.str());
		st.Bind(1,option);
		st.Step();
	}
	else
	{
		SQLite3DB::Statement st=m_db->Prepare("INSERT INTO tblOption(Option,OptionValue) VALUES(?,?);");
		st.Bind(0,option);
		st.Bind(1,valuestr.str());
		st.Step();
	}
	m_cache[option]=valuestr.str();
}

#endif	// _option_
