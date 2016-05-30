#include "../include/dbmaintenancethread.h"
#include "../include/stringfunctions.h"
#include "../include/option.h"
#include "../include/threadbuilder.h"
#include "../include/dbsetup.h"
#include "../include/fmsapp.h"

#include <vector>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <Poco/Timestamp.h>
#include <Poco/Timespan.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/Thread.h>

DBMaintenanceThread::DBMaintenanceThread()
{
	// move last maintenance times back so they will all run soon
	m_last10minute=Poco::Timestamp();
	m_last30minute=Poco::Timestamp();
	m_last30minute-=Poco::Timespan(0,0,11,0,0);
	m_last1hour=Poco::Timestamp();
	m_last1hour-=Poco::Timespan(0,0,49,0,0);
	m_last6hour=Poco::Timestamp();
	m_last6hour-=Poco::Timespan(0,5,42,0,0);
	m_last1day=Poco::Timestamp();
	m_last1day-=Poco::Timespan(0,23,51,0,0);
}


void DBMaintenanceThread::Do10MinuteMaintenance()
{
	// commented out for now
	m_log->debug("PeriodicDBMaintenance::Do10MinuteMaintenance");
}

void DBMaintenanceThread::Do30MinuteMaintenance()
{
	std::vector<int> m_boardlist;
	std::vector<std::pair<long,long> > m_unthreadedmessages;
	Option option(m_db);
	std::string ll("");
	option.Get("LogLevel",ll);

	m_log->debug("PeriodicDBMaintenance::Do30MinuteMaintenance start");

	ThreadBuilder tb(m_db);
	SQLite3DB::Statement boardst=m_db->Prepare("SELECT BoardID FROM tblBoard WHERE Forum='true';");
	// select messages for a board that aren't in a thread
	// This query was causing the db to be locked and a journal file created.
	// build a list of boards and messageids and then use that instead of keeping the query in use
	
	/*SQLite3DB::Statement selectst=m_db->Prepare("SELECT tblMessage.MessageID FROM tblMessage \
												INNER JOIN tblMessageBoard ON tblMessage.MessageID=tblMessageBoard.MessageID \
												LEFT JOIN (SELECT tblThread.BoardID, tblThreadPost.MessageID FROM tblThread INNER JOIN tblThreadPost ON tblThread.ThreadID=tblThreadPost.ThreadID WHERE tblThread.BoardID=?) AS temp1 ON tblMessage.MessageID=temp1.MessageID \
												WHERE tblMessageBoard.BoardID=? AND temp1.BoardID IS NULL;");*/
	// This query is about 50x faster than the above (roughly tested)
	SQLite3DB::Statement selectst=m_db->Prepare("SELECT tblMessageBoard.MessageID FROM tblMessageBoard \
												WHERE tblMessageBoard.BoardID=? AND \
												tblMessageBoard.MessageID NOT IN (SELECT MessageID FROM tblThreadPost INNER JOIN tblThread ON tblThreadPost.ThreadID=tblThread.ThreadID WHERE tblThread.BoardID=?);");

	SQLite3DB::Statement latestmessagest=m_db->Prepare("UPDATE tblBoard SET LatestMessageID=(SELECT tblMessage.MessageID FROM tblMessage INNER JOIN tblMessageBoard ON tblMessage.MessageID=tblMessageBoard.MessageID WHERE tblMessageBoard.BoardID=tblBoard.BoardID ORDER BY tblMessage.MessageDate DESC, tblMessage.MessageTime DESC LIMIT 0,1) WHERE tblBoard.BoardID=?;");

	boardst.Step();
	while(boardst.RowReturned())
	{
		int boardid=-1;
		boardst.ResultInt(0,boardid);
		boardst.Step();

		selectst.Bind(0,boardid);
		selectst.Bind(1,boardid);
		selectst.Step();

		while(selectst.RowReturned())
		{
			int messageid=-1;

			selectst.ResultInt(0,messageid);

			m_unthreadedmessages.push_back(std::pair<long,long>(boardid,messageid));
			m_boardlist.push_back(boardid);

			selectst.Step();
		}
		selectst.Reset();
	}
	selectst.Finalize();
	boardst.Finalize();

	for(std::vector<std::pair<long,long> >::iterator i=m_unthreadedmessages.begin(); i!=m_unthreadedmessages.end(); ++i)
	{
		tb.Build((*i).second,(*i).first,true);
	}

	// get latest message ids for the updated boards
	for(std::vector<int>::iterator i=m_boardlist.begin(); i!=m_boardlist.end(); ++i)
	{
		latestmessagest.Bind(0,(*i));
		latestmessagest.Step();
		latestmessagest.Reset();
	}

	// now rebuild threads where the message has been deleted
	std::vector<std::pair<int,int> > messageboards;
	SQLite3DB::Statement st=m_db->Prepare("SELECT tblThreadPost.MessageID, tblThread.BoardID FROM tblThreadPost INNER JOIN tblThread ON tblThreadPost.ThreadID=tblThread.ThreadID LEFT JOIN tblMessage ON tblThreadPost.MessageID=tblMessage.MessageID WHERE tblMessage.MessageID IS NULL;");
	st.Step();
	while(st.RowReturned())
	{
		int messageid=-1;
		int boardid=-1;

		st.ResultInt(0,messageid);
		st.ResultInt(1,boardid);

		//tb.Build(messageid,boardid,true);
		messageboards.push_back(std::pair<int,int>(messageid,boardid));

		st.Step();
	}
	st.Reset();
	st.Finalize();
	for(std::vector<std::pair<int,int> >::const_iterator i=messageboards.begin(); i!=messageboards.end(); ++i)
	{
		tb.Build((*i).first,(*i).second);
	}

	// delete threads that have no messages
	m_db->Execute("DELETE FROM tblThread WHERE ThreadID IN (SELECT tblThread.ThreadID FROM tblThread LEFT JOIN tblThreadPost ON tblThread.ThreadID=tblThreadPost.ThreadID WHERE tblThreadPost.ThreadID IS NULL);");

	m_log->debug("PeriodicDBMaintenance::Do30MinuteMaintenance end");
}

void DBMaintenanceThread::Do1HourMaintenance()
{

	SQLite3DB::Transaction trans(m_db);
	int idcount=0;

	m_log->debug("PeriodicDBMaintenance::Do1HourMaintenance start");

	// get count of local identities
	SQLite3DB::Statement st=m_db->Prepare("SELECT COUNT(*) FROM tblLocalIdentity;");
	st.Step();
	if(st.RowReturned())
	{
		st.ResultInt(0,idcount);
	}
	st.Finalize();

	// only recalculate trust if a local identity exists
	if(idcount>0)
	{

		// recalculate all trust levels - this is CPU instensive
		// do 1 identity at a time as doing it with 1 UPDATE statement locks that database for the duration
		std::vector<trustdata> trust;

		// deferred ok - only select
		trans.Begin();
		SQLite3DB::Statement st=m_db->Prepare("SELECT TargetIdentityID,PeerMessageTrust,PeerTrustListTrust FROM vwCalculatedPeerTrust;");
		trans.Step(st);
		while(st.RowReturned())
		{
			trustdata td;

			st.ResultInt(0,td.id);
			int temp=0;
			if(st.ResultNull(1)==false)
			{
				st.ResultInt(1,temp);
				td.pmt=temp;
			}
			if(st.ResultNull(2)==false)
			{
				st.ResultInt(2,temp);
				td.ptlt=temp;
			}

			trust.push_back(td);

			trans.Step(st);
		}
		trans.Finalize(st);
		trans.Commit();

		// now update each identity - put 10 updates in each transaction
		trans.Begin(SQLite3DB::Transaction::TRANS_IMMEDIATE);
		SQLite3DB::Statement upd=m_db->Prepare("UPDATE tblIdentity SET PeerMessageTrust=?, PeerTrustListTrust=? WHERE IdentityID=?");

		int count=1;
		for(std::vector<trustdata>::const_iterator i=trust.begin(); i!=trust.end(); ++i,++count)
		{
			if((*i).pmt>-1)
			{
				upd.Bind(0,(*i).pmt);
			}
			else
			{
				upd.Bind(0);
			}
			if((*i).ptlt>-1)
			{
				upd.Bind(1,(*i).ptlt);
			}
			else
			{
				upd.Bind(1);
			}
			upd.Bind(2,(*i).id);
			trans.Step(upd);
			trans.Reset(upd);

			// start new transaction every 10 records to allow other threads to do db writes
			if(count%10==0)
			{
				trans.Commit();
				if(trans.IsSuccessful()==false)
				{
					m_log->error("DBMaintenanceThread::Do1HourMaintenance transaction failed with SQLite error:"+trans.GetLastErrorStr()+" SQL="+trans.GetErrorSQL());
				}
				trans.Begin(SQLite3DB::Transaction::TRANS_IMMEDIATE);
			}
		}
		trans.Finalize(upd);
		trans.Commit();
		if(trans.IsSuccessful()==false)
		{
			m_log->error("DBMaintenanceThread::Do1HourMaintenance transaction failed with SQLite error:"+trans.GetLastErrorStr()+" SQL="+trans.GetErrorSQL());
		}

		std::vector<int> nulllist;
		trans.Begin();
		st=m_db->Prepare("SELECT IdentityID FROM tblIdentity WHERE IdentityID NOT IN (SELECT TargetIdentityID FROM vwCalculatedPeerTrust);");
		trans.Step(st);
		while(st.RowReturned())
		{
			int id=0;
			st.ResultInt(0,id);
			nulllist.push_back(id);
			trans.Step(st);
		}
		trans.Finalize(st);
		trans.Commit();
		if(trans.IsSuccessful()==false)
		{
			m_log->error("DBMaintenanceThread::Do1HourMaintenance transaction failed with SQLite error:"+trans.GetLastErrorStr()+" SQL="+trans.GetErrorSQL());
		}

		trans.Begin(SQLite3DB::Transaction::TRANS_IMMEDIATE);
		count=1;
		// set null peer trust for identities without a calculated trust
		upd=m_db->Prepare("UPDATE tblIdentity SET PeerMessageTrust=NULL, PeerTrustListTrust=NULL WHERE IdentityID=?;");
		trans.Step(st);
		for(std::vector<int>::const_iterator i=nulllist.begin(); i!=nulllist.end(); ++i,++count)
		{
			upd.Bind(0,(*i));
			trans.Step(upd);
			trans.Reset(upd);
			if(count%10==0)
			{
				trans.Commit();
				if(trans.IsSuccessful()==false)
				{
					m_log->error("DBMaintenanceThread::Do1HourMaintenance transaction failed with SQLite error:"+trans.GetLastErrorStr()+" SQL="+trans.GetErrorSQL());
				}
				trans.Begin(SQLite3DB::Transaction::TRANS_IMMEDIATE);
			}
		}
		trans.Finalize(upd);

		trans.Commit();
		if(trans.IsSuccessful()==false)
		{
			m_log->error("DBMaintenanceThread::Do1HourMaintenance transaction failed with SQLite error:"+trans.GetLastErrorStr()+" SQL="+trans.GetErrorSQL());
		}

		trans.Begin(SQLite3DB::Transaction::TRANS_IMMEDIATE);

		// insert all identities not in trust list already
		trans.Execute("INSERT OR IGNORE INTO tblIdentityTrust(LocalIdentityID,IdentityID) SELECT LocalIdentityID,IdentityID FROM tblLocalIdentity,tblIdentity;");

		trans.Commit();
		if(trans.IsSuccessful()==false)
		{
			m_log->error("DBMaintenanceThread::Do1HourMaintenance transaction failed with SQLite error:"+trans.GetLastErrorStr()+" SQL="+trans.GetErrorSQL());
		}

	}

	m_log->debug("PeriodicDBMaintenance::Do1HourMaintenance end");
}

void DBMaintenanceThread::Do6HourMaintenance()
{
	SQLite3DB::Transaction trans(m_db);

	m_log->debug("PeriodicDBMaintenance::Do6HourMaintenance start");

	trans.Begin(SQLite3DB::Transaction::TRANS_IMMEDIATE);

	// if we remove a board and the reply boardid is still set to it, we need to replace it with a boardid that does exist
	SQLite3DB::Statement st=m_db->Prepare("SELECT MessageID FROM tblMessage WHERE ReplyBoardID NOT IN (SELECT BoardID FROM tblBoard);");
	SQLite3DB::Statement st2=m_db->Prepare("SELECT BoardID FROM tblMessageBoard WHERE MessageID=?;");
	SQLite3DB::Statement upd=m_db->Prepare("UPDATE tblMessage SET ReplyBoardID=? WHERE MessageID=?;");
	trans.Step(st);
	while(st.RowReturned())
	{
		// find a valid boardid for the message
		int messageid=0;
		int boardid=0;

		st.ResultInt(0,messageid);

		st2.Bind(0,messageid);
		trans.Step(st2);
		if(st2.RowReturned())
		{
			st2.ResultInt(0,boardid);
			upd.Bind(0,boardid);
			upd.Bind(1,messageid);
			trans.Step(upd);
			trans.Reset(upd);
		}
		trans.Reset(st2);
		
		trans.Step(st);
	}

	trans.Finalize(st);
	trans.Finalize(st2);
	trans.Finalize(upd);

	trans.Commit();
	if(trans.IsSuccessful()==false)
	{
		m_log->error("DBMaintenanceThread::Do6HourMaintenance transaction 1 failed with SQLite error:"+trans.GetLastErrorStr()+" SQL="+trans.GetErrorSQL());
	}

	if(m_db->IsProfiling())
	{
		bool warntransaction=false;
		std::string output("\nSQLite ");
		output+=std::string(SQLITE_VERSION);
		output+="\nSQL\tCount\tTotalTime\tMinTime\tMaxTime\n";
		std::map<std::string,SQLite3DB::DB::ProfileData> profiledata;
		m_db->GetProfileData(profiledata,true);
		for(std::map<std::string,SQLite3DB::DB::ProfileData>::const_iterator i=profiledata.begin(); i!=profiledata.end(); ++i)
		{
			std::ostringstream ostr;
			ostr << (*i).first << "\t" << (*i).second.m_count << "\t" << (*i).second.m_time << "\t" << (*i).second.m_min << "\t" << (*i).second.m_max << "\n";
			output+=ostr.str();
			if((*i).first=="TRANSACTION TOTAL TIME" && (*i).second.m_max>static_cast<sqlite3_uint64>(40000000000))
			{
				warntransaction=true;
			}
		}
		m_log->information(output);
		if(warntransaction==true)
		{
			m_log->warning("DBMaintenanceThread::Do6HourMaintenance Some transactions are taking more than 40 seconds to complete.  This may cause locking issues with other transactions.");
		}
	}

	m_log->debug("PeriodicDBMaintenance::Do6HourMaintenance end");
}

void DBMaintenanceThread::Do1DayMaintenance()
{
	Poco::DateTime date;
	Option option(m_db);
	bool backup=false;
	SQLite3DB::Transaction trans(m_db);

	m_log->debug("PeriodicDBMaintenance::Do1DayMaintenance start");

	option.GetBool("BackupDatabase",backup);

	trans.Begin(SQLite3DB::Transaction::TRANS_IMMEDIATE);

	// delete all puzzles 2 or more days old
	date=Poco::Timestamp();
	date-=Poco::Timespan(2,0,0,0,0);
	trans.Execute("DELETE FROM tblIntroductionPuzzleInserts WHERE Day<='"+Poco::DateTimeFormatter::format(date,"%Y-%m-%d")+"';");
	trans.Execute("DELETE FROM tblIntroductionPuzzleRequests WHERE Day<='"+Poco::DateTimeFormatter::format(date,"%Y-%m-%d")+"';");

	// delete all identities we've never seen and were added more than 20 days ago
	// number of days needs be to greater than the number of days backwards in the trust list inserter, otherwise we'd delete them and add them again
	// from another trust list
	date=Poco::Timestamp();
	date-=Poco::Timespan(20,0,0,0,0);
	trans.Execute("DELETE FROM tblIdentity WHERE LastSeen IS NULL AND WOTLastSeen IS NULL AND DateAdded<'"+Poco::DateTimeFormatter::format(date,"%Y-%m-%d")+"';");

	// delete old identity requests - we don't need them anymore
	date=Poco::Timestamp();
	date-=Poco::Timespan(2,0,0,0,0);
	trans.Execute("DELETE FROM tblIdentityRequests WHERE Day<'"+Poco::DateTimeFormatter::format(date,"%Y-%m-%d")+"';");

	// delete old board list inserts/requests - we don't need them anymore
	date=Poco::Timestamp();
	date-=Poco::Timespan(2,0,0,0,0);
	trans.Execute("DELETE FROM tblBoardListInserts WHERE Day<'"+Poco::DateTimeFormatter::format(date,"%Y-%m-%d")+"';");
	trans.Execute("DELETE FROM tblBoardListRequests WHERE Day<'"+Poco::DateTimeFormatter::format(date,"%Y-%m-%d")+"';");

	// delete old local identity inserts - we don't need them anymore
	date=Poco::Timestamp();
	date-=Poco::Timespan(2,0,0,0,0);
	trans.Execute("DELETE FROM tblLocalIdentityInserts WHERE Day<'"+Poco::DateTimeFormatter::format(date,"%Y-%m-%d")+"';");

	// delete old message list inserts/requests - we don't need them anymore
	// only delete those older than the max # of days backward we are downloading messages
	date=Poco::Timestamp();
	date-=Poco::Timespan(m_messagedownloadmaxdaysbackward,0,0,0,0);
	trans.Execute("DELETE FROM tblMessageListInserts WHERE Day<'"+Poco::DateTimeFormatter::format(date,"%Y-%m-%d")+"';");
	trans.Execute("DELETE FROM tblMessageListRequests WHERE Day<'"+Poco::DateTimeFormatter::format(date,"%Y-%m-%d")+"';");

	// delete old trust list inserts/requests - we don't need them anymore
	date=Poco::Timestamp();
	date-=Poco::Timespan(2,0,0,0,0);
	trans.Execute("DELETE FROM tblTrustListInserts WHERE Day<'"+Poco::DateTimeFormatter::format(date,"%Y-%m-%d")+"';");
	trans.Execute("DELETE FROM tblTrustListRequests WHERE Day<'"+Poco::DateTimeFormatter::format(date,"%Y-%m-%d")+"';");

	// delete trust lists from identities we aren't trusting anymore
	// trans.Execute("DELETE FROM tblPeerTrust WHERE IdentityID NOT IN (SELECT IdentityID FROM tblIdentity WHERE (LocalTrustListTrust>=(SELECT OptionValue FROM tblOption WHERE Option='MinLocalTrustListTrust')) AND (PeerTrustListTrust IS NULL OR PeerTrustListTrust>=(SELECT OptionValue FROM tblOption WHERE Option='MinPeerTrustListTrust')));");

	trans.Commit();
	if(trans.IsSuccessful()==false)
	{
		m_log->error("DBMaintenanceThread::Do1DayMaintenance transaction failed with SQLite error:"+trans.GetLastErrorStr()+" SQL="+trans.GetErrorSQL());
	}

	trans.Begin(SQLite3DB::Transaction::TRANS_IMMEDIATE);

	// set purge date for single use identities that are older than 7 days
	date=Poco::Timestamp();
	date-=Poco::Timespan(7,0,0,0,0);
	SQLite3DB::Statement st=m_db->Prepare("UPDATE tblIdentity SET PurgeDate=datetime('now'), Hidden='true' WHERE IsWOT=0 AND SingleUse='true' AND DateAdded<? AND PurgeDate IS NULL;");
	st.Bind(0,Poco::DateTimeFormatter::format(date,"%Y-%m-%d %H:%M:%S"));
	trans.Step(st);

	st=m_db->Prepare("UPDATE tblIdentity SET Hidden='true' WHERE Hidden='false' AND PurgeDate IS NOT NULL AND PurgeDate<=datetime('now');");
	trans.Step(st);

	// delete local single use identities that are older than 7 days
	date=Poco::Timestamp();
	date-=Poco::Timespan(7,0,0,0,0);
	st=m_db->Prepare("DELETE FROM tblLocalIdentity WHERE SingleUse='true' AND DateCreated<?;");
	st.Bind(0,Poco::DateTimeFormatter::format(date,"%Y-%m-%d %H:%M:%S"));
	trans.Step(st);

	// delete old messages
	if(m_deletemessagesolderthan>=0)
	{
		date=Poco::Timestamp();
		date-=Poco::Timespan(m_deletemessagesolderthan,0,0,0,0);
		m_log->trace("PeriodicDBMaintenance::Do1DayMaintenance deleting messages prior to "+Poco::DateTimeFormatter::format(date,"%Y-%m-%d"));
		st=m_db->Prepare("DELETE FROM tblMessage WHERE MessageDate<?;");
		st.Bind(0,Poco::DateTimeFormatter::format(date,"%Y-%m-%d"));
		trans.Step(st);
	}

	// delete old message requests
	date=Poco::Timestamp();
	date-=Poco::Timespan(m_messagedownloadmaxdaysbackward,0,0,0,0);
	st=m_db->Prepare("DELETE FROM tblMessageRequests WHERE Day<?;");
	st.Bind(0,Poco::DateTimeFormatter::format(date,"%Y-%m-%d"));
	trans.Step(st);

#ifdef FROST_SUPPORT
	// delete old frost message requests
	date=Poco::Timestamp();
	date-=Poco::Timespan(m_frostmaxdaysbackward,0,0,0,0);
	st=m_db->Prepare("DELETE FROM tblFrostMessageRequests WHERE Day<?;");
	st.Bind(0,Poco::DateTimeFormatter::format(date,"%Y-%m-%d"));
	trans.Step(st);
#endif

	// delete tblIdentityTrust for local identities and identities that have been deleted
	trans.Execute("DELETE FROM tblIdentityTrust WHERE LocalIdentityID NOT IN (SELECT LocalIdentityID FROM tblLocalIdentity);");
	trans.Execute("DELETE FROM tblIdentityTrust WHERE IdentityID NOT IN (SELECT IdentityID FROM tblIdentity);");

	// cap failure count
	trans.Execute("UPDATE tblIdentity SET FailureCount=(SELECT OptionValue FROM tblOption WHERE Option='MaxFailureCount') WHERE FailureCount>(SELECT OptionValue FROM tblOption WHERE Option='MaxFailureCount');");
	// reduce failure count for each identity
	trans.Execute("UPDATE tblIdentity SET FailureCount=0 WHERE FailureCount<(SELECT OptionValue FROM tblOption WHERE Option='FailureCountReduction');");
	trans.Execute("UPDATE tblIdentity SET FailureCount=FailureCount-(SELECT OptionValue FROM tblOption WHERE Option='FailureCountReduction') WHERE FailureCount>=(SELECT OptionValue FROM tblOption WHERE Option='FailureCountReduction');");

	// delete null entries from tblMessageInserts
	date=Poco::Timestamp();
	st=m_db->Prepare("DELETE FROM tblMessageInserts WHERE MessageUUID IS NULL AND MessageXML IS NULL AND SendDate IS NULL AND Inserted='true' AND Day<?;");
	st.Bind(0,Poco::DateTimeFormatter::format(date,"%Y-%m-%d"));
	trans.Step(st);

	trans.Finalize(st);

	// If at least 2 days have passed without retrieving one of our own inserted messages, reset the date of the message insert so it will be inserted again.
	trans.Execute("UPDATE tblMessageInserts SET Day=NULL, InsertIndex=NULL, Inserted='false' WHERE LENGTH(MessageXML)<=1000000 AND MessageUUID IN (SELECT tblMessageInserts.MessageUUID FROM tblMessageInserts LEFT JOIN tblMessage ON tblMessageInserts.MessageUUID=tblMessage.MessageUUID WHERE Inserted='true' AND SendDate>=(SELECT date('now','-' || (SELECT CASE WHEN CAST(OptionValue AS INTEGER)<=0 THEN 30 WHEN CAST(OptionValue AS INTEGER)>30 THEN 30 ELSE OptionValue END FROM tblOption WHERE Option='DeleteMessagesOlderThan') || ' days')) AND tblMessage.MessageUUID IS NULL AND tblMessageInserts.SendDate<date('now','-2 days'));");

	// delete messages that don't belong to any boards
	trans.Execute("DELETE FROM tblMessage WHERE MessageID NOT IN (SELECT MessageID FROM tblMessageBoard);");

	trans.Commit();
	if(trans.IsSuccessful()==false)
	{
		m_log->error("DBMaintenanceThread::Do1DayMaintenance transaction 3 failed with SQLite error:"+trans.GetLastErrorStr()+" SQL="+trans.GetErrorSQL());
	}

	// recount messages in each board
	m_db->Execute("UPDATE tblBoard SET MessageCount=(SELECT IFNULL(COUNT(*),0) FROM tblMessageBoard WHERE tblMessageBoard.BoardID=tblBoard.BoardID);");

	// remove FMS or WOT flags from identities that were never seen in 30 days
	m_db->Execute("UPDATE tblIdentity SET IsFMS=0 WHERE LastSeen IS NULL AND IsFMS=1 AND SolvedPuzzleCount=0 AND DateAdded<datetime('now','-30 days');");
	m_db->Execute("UPDATE tblIdentity SET IsWOT=0 WHERE WOTLastSeen IS NULL AND IsWOT=1 AND DateAdded<datetime('now','-30 days');");

	if(backup==true)
	{
		std::string oldbackupname(global::basepath+"fms-old-backup.db3");
		std::string backupname(global::basepath+"fms-backup.db3");
		/*
		 1. Remove any existing copy of database backup
		 2. Rename existing database backup
		 3. Open up backup database and perform backup
		 4. Verify backup database
		 5. If backup was successful and database verified, remove copy of database backup
		 6. If backup wasn't successful or database not verified, remove backup and rename backup copy
		*/
		m_log->information("PeriodicDBMaintenance::Do1DayMaintenance backing up database");

#ifdef _WIN32
		_unlink(oldbackupname.c_str());
#else
		unlink(oldbackupname.c_str());
#endif
		rename(backupname.c_str(),oldbackupname.c_str());
		bool backupok=true;

		// keep backupdb scoped
		{
			SQLite3DB::DB backupdb(backupname);
			backupdb.SetBusyTimeout(40000);
			backupdb.Execute("PRAGMA temp_store=2;");
			backupdb.Execute("PRAGMA sychronous=OFF;");

			// keep dbbackup scoped
			{
				SQLite3DB::DBBackup dbbackup(m_db,&backupdb);
				bool done=false;
				do
				{
					int pages=dbbackup.Step(10);
					if(pages==0)
					{
						done=true;
					}
					else if(pages<0)
					{
						done=true;
						backupok=false;
					}
				}while(done==false);
			}

			if(backupok==true)
			{
				if(VerifyDB(&backupdb)==true)
				{
#ifdef _WIN32
					_unlink(oldbackupname.c_str());
#else
					unlink(oldbackupname.c_str());
#endif
					m_log->information("PeriodicDBMaintenance::Do1DayMaintenance backup database created successfully");
				}
				else
				{
					backupok=false;
					m_log->error("PeriodicDBMaintenance::Do1DayMaintenance backup database failed integrity test");
				}
			}
		}
		
		if(backupok==false)
		{
#ifdef _WIN32
			_unlink(backupname.c_str());
#else
			unlink(backupname.c_str());
#endif
			rename(oldbackupname.c_str(),backupname.c_str());
			m_log->error("PeriodicDBMaintenance::Do1DayMaintenance could not backup database");
		}
	}

	m_log->debug("PeriodicDBMaintenance::Do1DayMaintenance end");

}

void DBMaintenanceThread::run()
{
	m_log->debug("DBMaintenanceThread::run thread started.");

	try
	{

		LoadDatabase(m_log);
		Option option(m_db);
		std::string tempval("");

		m_deletemessagesolderthan=180;
		tempval="180";
		option.Get("DeleteMessagesOlderThan",tempval);
		StringFunctions::Convert(tempval,m_deletemessagesolderthan);

		m_messagedownloadmaxdaysbackward=5;
		tempval="5";
		option.Get("MessageDownloadMaxDaysBackward",tempval);
		StringFunctions::Convert(tempval,m_messagedownloadmaxdaysbackward);

		m_frostmaxdaysbackward=5;
		tempval="5";
		option.Get("FrostMessageMaxDaysBackward",tempval);
		StringFunctions::Convert(tempval,m_frostmaxdaysbackward);

		Poco::DateTime now;
		int i=0;

		do
		{
			now=Poco::Timestamp();

			/*
			if((m_last10minute+Poco::Timespan(0,0,10,0,0))<=now)
			{
				Do10MinuteMaintenance();
				m_last10minute=Poco::Timestamp();
			}
			*/
			if((m_last30minute+Poco::Timespan(0,0,30,0,0))<=now)
			{
				Do30MinuteMaintenance();
				m_last30minute=Poco::Timestamp();
			}
			if((m_last1hour+Poco::Timespan(0,1,0,0,0))<=now)
			{
				Do1HourMaintenance();
				m_last1hour=Poco::Timestamp();
			}
			if((m_last6hour+Poco::Timespan(0,6,0,0,0))<=now)
			{
				Do6HourMaintenance();
				m_last6hour=Poco::Timestamp();
			}
			if((m_last1day+Poco::Timespan(1,0,0,0,0))<=now)
			{
				Do1DayMaintenance();
				m_last1day=Poco::Timestamp();
			}

			i=0;
			while(i++<5 && !IsCancelled())
			{
				Poco::Thread::sleep(1000);
			}

		}while(!IsCancelled());
	}
	catch(SQLite3DB::Exception &e)
	{
		m_log->fatal("DBMaintenanceThread caught SQLite3DB::Exception "+e.what());
		((FMSApp *)&FMSApp::instance())->Terminate();
	}

	m_log->debug("DBMaintenanceThread::run thread exiting.");
}
