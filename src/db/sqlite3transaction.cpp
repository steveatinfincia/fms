#include "../../include/db/sqlite3db/sqlite3transaction.h"

#include <Poco/Util/ServerApplication.h>

namespace SQLite3DB
{

Transaction::Transaction():m_transactionlevel(0),m_db(0),m_error(false),m_errorstr(""),m_transactionresult(TRANS_RESULT_NOTSTARTED),m_lasterr(SQLITE_OK)
{

}

Transaction::Transaction(DB *db):m_transactionlevel(0),m_db(db),m_error(false),m_errorstr(""),m_transactionresult(TRANS_RESULT_NOTSTARTED),m_lasterr(SQLITE_OK)
{

}

Transaction::~Transaction()
{
	if(m_db && m_transactionlevel>0)
	{
		Commit();
	}
}

const bool Transaction::Begin(const TransactionType transtype)
{
	if(m_db)
	{
		std::string trtype("");

		switch(transtype)
		{
		case TRANS_IMMEDIATE:
			trtype=" IMMEDIATE";
			break;
		case TRANS_EXCLUSIVE:
			trtype=" EXCLUSIVE";
			break;
		default:
			trtype="";
		};

		if(m_db->Execute("BEGIN"+trtype+";"))
		{
			m_transactionlevel++;
			m_error=false;
			m_errorstr="";
			m_transactionresult=TRANS_RESULT_INPROGRESS;
			m_stopwatch.restart();
			return true;
		}
		else
		{
			m_lasterr=m_db->GetLastExtendedError(m_errorstr);
			DB::HandleError(m_lasterr,m_errorstr,"Transaction::Begin");
			m_errorsql="BEGIN"+trtype+";";
		}
	}
	m_error=true;
	return false;
}

const bool Transaction::Commit(const int tries)
{
	if(m_db && m_transactionlevel>0)
	{
		if(m_error==false)
		{
			int count=0;
			bool com=m_db->Execute("COMMIT;");
			while(com==false && m_db->GetLastResult()==SQLITE_BUSY && count++<tries)
			{
				com=m_db->Execute("COMMIT;");
			}
			m_stopwatch.stop();
			m_db->ProfileCallback(m_db,"TRANSACTION TOTAL TIME",static_cast<sqlite3_uint64>(m_stopwatch.elapsed())*static_cast<sqlite3_uint64>(1000));
			if(static_cast<sqlite3_uint64>(m_stopwatch.elapsed())>static_cast<sqlite3_uint64>(30000000))
			{
				Poco::Util::ServerApplication::instance().logger().warning("Transaction::Commit Transaction took longer than 30 seconds.");
			}
			if(com==true)
			{
				m_transactionlevel--;
				m_error=false;
				m_transactionresult=TRANS_RESULT_COMMIT;
				return true;
			}
			else
			{
				std::string errmsg("");
				m_lasterr=m_db->GetLastExtendedError(errmsg);
				DB::HandleError(m_lasterr,errmsg,"Transaction::Commit");
				Rollback();
			}
		}
		else
		{
			Rollback();
		}
	}
	m_error=true;
	return false;
}

const bool Transaction::Rollback(const int tries)
{
	if(m_db && m_transactionlevel>0)
	{
		int count=0;
		bool rb=m_db->Execute("ROLLBACK;");
		// rollback can fail with SQLITE_BUSY, so retry
		while(rb==false && m_db->GetLastResult()==SQLITE_BUSY && count++<tries)
		{
			rb=m_db->Execute("ROLLBACK;");
		}
		if(rb==true)
		{
			m_transactionlevel--;
			m_transactionresult=TRANS_RESULT_ROLLBACK;
			return true;
		}
		else
		{
			std::string errmsg("");
			m_lasterr=m_db->GetLastExtendedError(errmsg);
			DB::HandleError(m_lasterr,errmsg,"Transaction::Rollback");
		}
	}
	m_transactionresult=TRANS_RESULT_ERROR;
	return false;
}

const bool Transaction::Reset(Statement &st)
{
	if(m_db && m_error==false)
	{
		if(st.Reset())
		{
			return true;
		}
		else
		{
			m_lasterr=m_db->GetLastExtendedError(m_errorstr);
			m_errorsql="RESET:"+st.GetSQL();
		}
	}
	m_error=true;
	return false;
}

const bool Transaction::Finalize(Statement &st)
{
	if(m_db && m_error==false)
	{
		if(st.Valid()==false || st.Finalize())
		{
			return true;
		}
		else
		{
			m_lasterr=m_db->GetLastExtendedError(m_errorstr);
			m_errorsql="FINALIZE:"+st.GetSQL();
		}
	}
	m_error=true;
	return false;
}

const bool Transaction::Step(Statement &st, const bool saveinsertrowid)
{
	if(m_db && m_error==false)
	{
		if(st.Step(saveinsertrowid))
		{
			return true;
		}
		else
		{
			m_lasterr=m_db->GetLastExtendedError(m_errorstr);
			m_errorsql="STEP:"+st.GetSQL();
			st.Reset();
		}
	}
	m_error=true;
	return false;
}

const bool Transaction::Execute(const std::string &sql)
{
	if(m_db && m_error==false)
	{
		if(m_db->Execute(sql))
		{
			return true;
		}
		else
		{
			m_lasterr=m_db->GetLastExtendedError(m_errorstr);
			m_errorsql="EXECUTE:"+sql;
		}
	}
	m_error=true;
	return false;
}

}	// namespace
