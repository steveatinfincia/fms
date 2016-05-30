#include "../../include/freenet/trustlistrequester.h"
#include "../../include/option.h"
#include "../../include/stringfunctions.h"
#include "../../include/freenet/trustlistxml.h"
#include "../../include/freenet/identitypublickeycache.h"
#include "../../include/unicode/unicodestring.h"
#include "../../include/global.h"
#include "../../include/freenet/freenetkeys.h"

#include <Poco/DateTimeFormatter.h>
#include <Poco/Timespan.h>

#include <algorithm>

#ifdef XMEM
	#include <xmem.h>
#endif

TrustListRequester::TrustListRequester(SQLite3DB::DB *db):IIndexRequester<long>(db)
{
	Initialize();
}

TrustListRequester::TrustListRequester(SQLite3DB::DB *db, FCPv2::Connection *fcp):IIndexRequester<long>(db,fcp)
{
	Initialize();
}

const long TrustListRequester::GetIDFromIdentifier(const std::string &identifier)
{
	long id;
	std::vector<std::string> idparts;
	StringFunctions::Split(identifier,"|",idparts);
	StringFunctions::Convert(idparts[1],id);
	return id;
}

const bool TrustListRequester::HandleAllData(FCPv2::Message &message)
{
	Poco::DateTime now;
	SQLite3DB::Statement st;
	SQLite3DB::Statement trustst;
	std::vector<std::string> idparts;
	long datalength;
	std::vector<char> data;
	TrustListXML xml;
	long identityid;
	long index;
	int tryinsertcount=0;
	int actualinsertcount=0;
	int trydayinsertcount=0;
	int actualdayinsertcount=0;
	int previnsertcount=0;
	bool savenewidentities=false;
	SQLite3DB::Transaction trans(m_db);
	Option option(m_db);
	bool savewot=false;

	option.GetBool("WOTDownloadIdentities",savewot);

	StringFunctions::Split(message["Identifier"],"|",idparts);
	StringFunctions::Convert(message["DataLength"],datalength);
	StringFunctions::Convert(idparts[1],identityid);
	StringFunctions::Convert(idparts[2],index);

	// wait for all data to be received from connection
	m_fcp->WaitForBytes(1000,datalength);

	// if we got disconnected- return immediately
	if(m_fcp->IsConnected()==false)
	{
		return false;
	}

	// receive the file
	m_fcp->Receive(data,datalength);

	// see if we will save new identities found on the trust list
	st=m_db->Prepare("SELECT IFNULL(LocalTrustListTrust>=(SELECT OptionValue FROM tblOption WHERE Option='MinLocalTrustListTrust'),0) FROM tblIdentity WHERE IdentityID=?;");
	st.Bind(0,identityid);
	st.Step();
	if(st.RowReturned())
	{
		int result=0;
		st.ResultInt(0,result);
		if(result==1)
		{
			savenewidentities=true;
		}
	}

	// get count of identities added in last 24 hours
	st=m_db->Prepare("SELECT COUNT(*) FROM tblIdentity WHERE DateAdded>=? AND IsFMS=1;");
	now-=Poco::Timespan(1,0,0,0,0);
	st.Bind(0,Poco::DateTimeFormatter::format(now,"%Y-%m-%d %H:%M:%S"));
	st.Step();
	if(st.RowReturned())
	{
		if(st.ResultNull(0)==false)
		{
			st.ResultInt(0,trydayinsertcount);
			actualdayinsertcount=trydayinsertcount;
		}
	}
	else
	{
		m_log->error("TrustListRequester::HandleAllData couldn't get count of identities added in last 24 hours");
	}

	// get count of identities added more than 24 hours ago - if 0 then we will accept more than 100 identities now
	st=m_db->Prepare("SELECT COUNT(*) FROM tblIdentity WHERE DateAdded<? AND IsFMS=1;");
	st.Bind(0,Poco::DateTimeFormatter::format(now,"%Y-%m-%d %H:%M:%S"));
	st.Step();
	if(st.RowReturned())
	{
		if(st.ResultNull(0)==false)
		{
			st.ResultInt(0,previnsertcount);
		}
	}
	else
	{
		m_log->error("TrustListRequester::HandleAllData couldn't get count of identities added more than 24 hours ago");
	}

	now=Poco::DateTime();

	// parse file into xml and update the database
	if(data.size()>0 && xml.ParseXML(std::string(data.begin(),data.end()))==true)
	{
		// find the identity name and public key of the identity publishing the trust list
		std::string publisherid="";
		st=m_db->Prepare("SELECT Name,PublicKey FROM tblIdentity WHERE IdentityID=?;");
		st.Bind(0,identityid);
		st.Step();
		if(st.RowReturned())
		{
			std::string publishername="";
			std::string publisherpublickey="";
			st.ResultText(0,publishername);
			st.ResultText(1,publisherpublickey);
			publisherid=publishername;
			if(publisherpublickey.size()>4)
			{
				publisherid+=publisherpublickey.substr(3,44);
			}
		}
		st.Finalize();

		trans.Begin(SQLite3DB::Transaction::TRANS_IMMEDIATE);

		trans.Execute("CREATE TEMPORARY TABLE IF NOT EXISTS tmpPeerTrust(IdentityID INTEGER,TargetIdentityID INTEGER,MessageTrust INTEGER,TrustListTrust INTEGER,MessageTrustChange INTEGER,TrustListTrustChange INTEGER);");
		trans.Execute("CREATE UNIQUE INDEX IF NOT EXISTS idxPeerTrustIdentity ON tmpPeerTrust(IdentityID,TargetIdentityID);");
		st=m_db->Prepare("INSERT INTO tmpPeerTrust(IdentityID,TargetIdentityID,MessageTrust,TrustListTrust,MessageTrustChange,TrustListTrustChange) SELECT IdentityID,TargetIdentityID,MessageTrust,TrustListTrust,MessageTrustChange,TrustListTrustChange FROM tblPeerTrust WHERE IdentityID=?;");
		st.Bind(0,identityid);
		trans.Step(st);
		trans.Finalize(st);

		// drop all existing peer trust from this identity - we will rebuild it when we go through each trust in the xml file
		st=m_db->Prepare("DELETE FROM tblPeerTrust WHERE IdentityID=?;");
		st.Bind(0,identityid);
		trans.Step(st);
		trans.Finalize(st);

		st=m_db->Prepare("SELECT IdentityID, IsFMS, IsWOT FROM tblIdentity WHERE PublicKey=?;");
		trustst=m_db->Prepare("INSERT INTO tblPeerTrust(IdentityID,TargetIdentityID,MessageTrust,TrustListTrust,MessageTrustComment,TrustListTrustComment) VALUES(?,?,?,?,?,?);");
		SQLite3DB::Statement idupdst=m_db->Prepare("UPDATE tblIdentity SET IsFMS=?, IsWOT=? WHERE IdentityID=?;");
		
		SQLite3DB::Statement idinsert=m_db->Prepare("INSERT INTO tblIdentity(PublicKey,DateAdded,AddedMethod,IsFMS,IsWOT) VALUES(?,?,?,?,?);");
		
		// loop through all trust entries in xml and add to database if we don't already know them
		FreenetSSKKey ssk;
		for(long i=0; i<xml.TrustCount(); i++)
		{
			int id=-1;
			std::string identity;
			UnicodeString messagetrustcomment("");
			UnicodeString trustlisttrustcomment("");
			identity=xml.GetIdentity(i);

			st.Bind(0,identity);
			trans.Step(st);
			if(st.RowReturned()==false)
			{
				if(savenewidentities==true)
				{
					if(ssk.TryParse(identity)==true)
					{
						// allow up to 10 new identities per downloaded trust list, where total inserted in the last 24 hours may not exceed 1/10 the total number of identities we know about or 500 (whichever is smaller, minimum 10)
						// 24 hour limit is lifted if the database didn't contain any identities inserted more than 24 hours ago (new db) - 100 new identities per trust list allowed in this case
						if((actualinsertcount<100 && previnsertcount==0) || (actualinsertcount<10 && actualdayinsertcount<((std::min)(((std::max)(previnsertcount/10,10)),500))))
						{
							if((savewot==true && xml.GetIsWOT(i)==true) || xml.GetIsFMS(i)==true)
							{
								idinsert.Bind(0,ssk.GetBaseKey());
								idinsert.Bind(1,Poco::DateTimeFormatter::format(now,"%Y-%m-%d %H:%M:%S"));
								idinsert.Bind(2,"FMS trust list of "+publisherid);
								idinsert.Bind(3,xml.GetIsFMS(i)==true ? 1 : 0);
								idinsert.Bind(4,xml.GetIsWOT(i)==true ? 1 : 0);
								trans.Step(idinsert,true);
								id=idinsert.GetLastInsertRowID();
								trans.Reset(idinsert);
								m_log->trace("TrustListRequester::HandleAllData saving new identity : "+identity);
								actualinsertcount++;
								actualdayinsertcount++;
							}
							else
							{
								//m_log->trace("TrustListRequester::HandleAllData not saving new identity : "+identity);
							}
						}
						else
						{
							//m_log->trace("TrustListRequester::HandleAllData too many new identities already inserted : "+identity);
						}
						tryinsertcount++;
						trydayinsertcount++;
					}
					else
					{
						m_log->warning("TrustListRequester::HandleAllData invalid SSK : "+identity);
					}
				}
			}
			else
			{
				int oldisfms=0;
				int oldiswot=0;
				int isfms=0;
				int iswot=0;
				st.ResultInt(0,id);
				st.ResultInt(1,oldisfms);
				st.ResultInt(2,oldiswot);
				isfms=oldisfms;
				iswot=oldiswot;
				// never change 1 to 0 in IsFMS or IsWOT based on trust list because trust list inserter could maliciously change it
				// only change from 0 to 1
				if((xml.GetIsFMS(i)==true && oldisfms==0) || (xml.GetIsWOT(i)==true && oldiswot==0))
				{
					if(xml.GetIsFMS(i)==true)
					{
						isfms=1;
					}
					if(xml.GetIsWOT(i)==true)
					{
						iswot=1;
					}
					idupdst.Bind(0,isfms);
					idupdst.Bind(1,iswot);
					idupdst.Bind(2,id);
					idupdst.Step();
					idupdst.Reset();
				}
			}
			trans.Reset(st);

			//insert trust for this identity
			if(id!=-1)
			{
				trustst.Bind(0,identityid);
				trustst.Bind(1,id);
				if(xml.GetMessageTrust(i)==-1)
				{
					trustst.Bind(2);
				}
				else
				{
					trustst.Bind(2,xml.GetMessageTrust(i));
				}
				if(xml.GetTrustListTrust(i)==-1)
				{
					trustst.Bind(3);
				}
				else
				{
					trustst.Bind(3,xml.GetTrustListTrust(i));
				}
				messagetrustcomment=xml.GetMessageTrustComment(i);
				trustlisttrustcomment=xml.GetTrustListTrustComment(i);
				
				messagetrustcomment.Trim(MAX_TRUST_COMMENT_LENGTH);
				trustlisttrustcomment.Trim(MAX_TRUST_COMMENT_LENGTH);

				trustst.Bind(4,messagetrustcomment.UTF8String());
				trustst.Bind(5,trustlisttrustcomment.UTF8String());
				trans.Step(trustst);
				trans.Reset(trustst);
			}
		}

		trans.Finalize(trustst);
		trans.Finalize(st);

		if(tryinsertcount>=10)
		{
			m_log->warning("TrustListRequester::HandleAllData TrustList contained more than 10 new identities : "+message["Identifier"]);
		}
		if(trydayinsertcount>=((std::min)(((std::max)(previnsertcount/10,10)),500)) && previnsertcount>0)
		{
			m_log->warning("TrustListRequester::HandleAllData TrustList would have inserted too many new identities in the last 24 hours : "+message["Identifier"]);
		}

		st=m_db->Prepare("INSERT INTO tblTrustListRequests(IdentityID,Day,RequestIndex,Found) VALUES(?,?,?,'true');");
		st.Bind(0,identityid);
		st.Bind(1,idparts[4]);
		st.Bind(2,index);
		trans.Step(st);
		trans.Finalize(st);

		trans.Execute("CREATE TEMPORARY VIEW IF NOT EXISTS vwPeerTrustChange AS\
						SELECT tblPeerTrust.IdentityID, tblPeerTrust.TargetIdentityID,\
						CASE WHEN IFNULL(tblPeerTrust.MessageTrust,0)=IFNULL(tmpPeerTrust.MessageTrust,0) THEN IFNULL(tmpPeerTrust.MessageTrustChange,0)\
						ELSE IFNULL(tblPeerTrust.MessageTrust,0)-IFNULL(tmpPeerTrust.MessageTrust,0)\
						END AS MessageTrustChange,\
						CASE WHEN IFNULL(tblPeerTrust.TrustListTrust,0)=IFNULL(tmpPeerTrust.TrustListTrust,0) THEN IFNULL(tmpPeerTrust.TrustListTrustChange,0)\
						ELSE IFNULL(tblPeerTrust.TrustListTrust,0)-IFNULL(tmpPeerTrust.TrustListTrust,0)\
						END AS TrustListTrustChange\
						FROM tblPeerTrust\
						INNER JOIN tmpPeerTrust ON tblPeerTrust.IdentityID=tmpPeerTrust.IdentityID AND tblPeerTrust.TargetIdentityID=tmpPeerTrust.TargetIdentityID;");

		st=m_db->Prepare("UPDATE tblPeerTrust SET MessageTrustChange=IFNULL((SELECT vwPeerTrustChange.MessageTrustChange FROM vwPeerTrustChange WHERE vwPeerTrustChange.IdentityID=? AND vwPeerTrustChange.TargetIdentityID=tblPeerTrust.TargetIdentityID),0), TrustListTrustChange=IFNULL((SELECT vwPeerTrustChange.TrustListTrustChange FROM vwPeerTrustChange WHERE vwPeerTrustChange.IdentityID=? AND vwPeerTrustChange.TargetIdentityID=tblPeerTrust.TargetIdentityID),0)  WHERE IdentityID=?;");
		st.Bind(0,identityid);
		st.Bind(1,identityid);
		st.Bind(2,identityid);
		trans.Step(st);

		trans.Execute("DROP TABLE tmpPeerTrust;");

		trans.Finalize(st);
		trans.Finalize(idinsert);
		trans.Commit();

		if(trans.IsSuccessful()==false)
		{
			m_log->error("TrustListRequester::HandleAllData transaction failed with SQLite error:"+trans.GetLastErrorStr()+" SQL="+trans.GetErrorSQL());
		}

		m_log->debug("TrustListRequester::HandleAllData parsed TrustList XML file : "+message["Identifier"]);
	}
	else
	{
		// bad data - mark index
		st=m_db->Prepare("INSERT INTO tblTrustListRequests(IdentityID,Day,RequestIndex,Found) VALUES(?,?,?,'false');");
		st.Bind(0,identityid);
		st.Bind(1,idparts[4]);
		st.Bind(2,index);
		st.Step();
		st.Finalize();

		m_log->error("TrustListRequester::HandleAllData error parsing TrustList XML file : "+message["Identifier"]);
	}

	// remove this identityid from request list
	RemoveFromRequestList(identityid);

	return true;

}

const bool TrustListRequester::HandleGetFailed(FCPv2::Message &message)
{
	SQLite3DB::Statement st;
	std::vector<std::string> idparts;
	long identityid;
	long index;

	StringFunctions::Split(message["Identifier"],"|",idparts);
	StringFunctions::Convert(idparts[1],identityid);
	StringFunctions::Convert(idparts[2],index);	

	// if this is a fatal error - insert index into database so we won't try to download this index again
	if(message["Fatal"]=="true")
	{
		if(message["Code"]!="25")
		{
			st=m_db->Prepare("INSERT INTO tblTrustListRequests(IdentityID,Day,RequestIndex,Found) VALUES(?,?,?,'false');");
			st.Bind(0,identityid);
			st.Bind(1,idparts[4]);
			st.Bind(2,index);
			st.Step();
			st.Finalize();
		}

		m_log->error("TrustListRequester::HandleGetFailed fatal error code="+message["Code"]+" requesting "+message["Identifier"]);
	}

	// remove this identityid from request list
	RemoveFromRequestList(identityid);

	return true;

}

void TrustListRequester::Initialize()
{
	std::string tempval="";
	m_fcpuniquename="TrustListRequester";
	m_maxrequests=0;
	Option option(m_db);

	option.GetInt("MaxIdentityRequests",m_maxrequests);
	if(m_maxrequests<1)
	{
		m_maxrequests=1;
		m_log->error("Option MaxTrustListRequests is currently set at "+tempval+".  It must be 1 or greater.");
	}
	if(m_maxrequests>100)
	{
		m_log->warning("Option MaxTrustListRequests is currently set at "+tempval+".  This value might be incorrectly configured.");
	}
	m_tempdate=Poco::Timestamp();
	m_maxsize=1000000;
}

void TrustListRequester::PopulateIDList()
{
	Poco::DateTime date;
	int id;
	std::string sql;
	bool getnull=false;
	Option opt(m_db);
	SQLite3DB::Transaction trans(m_db);

	opt.GetBool("DownloadTrustListWhenNull",getnull);

	// select identities we want to query (we've seen them today and they are publishing trust list) - sort by their trust level (descending) with secondary sort on how long ago we saw them (ascending)
	if(getnull==false)
	{
		sql="SELECT IdentityID FROM tblIdentity ";
		sql+="WHERE Name IS NOT NULL AND Name <> '' AND PublicKey IS NOT NULL AND PublicKey <> '' AND LastSeen>='"+Poco::DateTimeFormatter::format(date,"%Y-%m-%d")+"' AND PublishTrustList='true' AND LocalTrustListTrust>=(SELECT OptionValue FROM tblOption WHERE Option='MinLocalTrustListTrust') AND ( PeerTrustListTrust IS NULL OR PeerTrustListTrust>=(SELECT OptionValue FROM tblOption WHERE Option='MinPeerTrustListTrust') ) AND tblIdentity.FailureCount<=(SELECT OptionValue FROM tblOption WHERE Option='MaxFailureCount')";
		sql+="ORDER BY LocalTrustListTrust DESC, LastSeen;";
	}
	else
	{
		sql="SELECT IdentityID FROM tblIdentity ";
		sql+="WHERE Name IS NOT NULL AND Name <> '' AND PublicKey IS NOT NULL AND PublicKey <> '' AND LastSeen>='"+Poco::DateTimeFormatter::format(date,"%Y-%m-%d")+"' AND PublishTrustList='true' AND (LocalTrustListTrust IS NULL OR LocalTrustListTrust>=(SELECT OptionValue FROM tblOption WHERE Option='MinLocalTrustListTrust')) AND ( PeerTrustListTrust IS NULL OR PeerTrustListTrust>=(SELECT OptionValue FROM tblOption WHERE Option='MinPeerTrustListTrust') ) AND tblIdentity.FailureCount<=(SELECT OptionValue FROM tblOption WHERE Option='MaxFailureCount')";
		sql+="ORDER BY LocalTrustListTrust DESC, LastSeen;";
	}

	SQLite3DB::Statement st=m_db->Prepare(sql);
	st.Step();

	m_ids.clear();

	// only selects, deferred OK
	trans.Begin();

	while(st.RowReturned())
	{
		st.ResultInt(0,id);
		m_ids[id].m_requested=false;
		trans.Step(st);
	}

	// calculate maximum request size based on number of identities that would appear in our own trust list
	// max size will be 250 bytes per identity in list + 10000 bytes for header/footer info, this is more than we need to allow some growth
	// minimum max size will always be 1000000 bytes
	st=m_db->Prepare("SELECT IFNULL(COUNT(*),0) FROM tblIdentity WHERE PublicKey IS NOT NULL AND PublicKey<>'' AND (LastSeen>=datetime('now','-15 days') OR WOTLastSeen>=datetime('now','-15 days'));");
	st.Step();
	if(st.RowReturned())
	{
		int count=0;
		st.ResultInt(0,count);
		m_maxsize=(std::max)((count*250)+10000,1000000);
	}
	else
	{
		m_maxsize=1000000;
	}

	trans.Finalize(st);
	trans.Commit();
}

void TrustListRequester::StartRequest(const long &identityid)
{
	Poco::DateTime now;
	FCPv2::Message message;
	std::string publickey;
	int index;
	std::string indexstr;
	std::string identityidstr;
	std::string maxsizestr("1000000");
	IdentityPublicKeyCache pkcache(m_db);

	StringFunctions::Convert(m_maxsize,maxsizestr);

	if(pkcache.PublicKey(identityid,publickey))
	{

		SQLite3DB::Statement st2=m_db->Prepare("SELECT MAX(RequestIndex) FROM tblTrustListRequests WHERE Day=? AND IdentityID=?;");
		st2.Bind(0,Poco::DateTimeFormatter::format(now,"%Y-%m-%d"));
		st2.Bind(1,identityid);
		st2.Step();

		index=0;
		if(st2.RowReturned())
		{
			if(st2.ResultNull(0)==false)
			{
				st2.ResultInt(0,index);
				index++;
			}
		}
		st2.Finalize();

		StringFunctions::Convert(index,indexstr);
		StringFunctions::Convert(identityid,identityidstr);

		message.SetName("ClientGet");
		message["URI"]=publickey+m_messagebase+"|"+Poco::DateTimeFormatter::format(now,"%Y-%m-%d")+"|TrustList|"+indexstr+".xml";
		message["Identifier"]=m_fcpuniquename+"|"+identityidstr+"|"+indexstr+"|"+message["URI"];
		message["PriorityClass"]=m_defaultrequestpriorityclassstr;
		message["ReturnType"]="direct";
		message["MaxSize"]=maxsizestr;			// dynamic max size based on number of identities that would appear in our own trust list

		m_fcp->Send(message);

		StartedRequest(identityid,message["Identifier"]);
	}

	m_ids[identityid].m_requested=true;

}
