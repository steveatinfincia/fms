#include "../../../include/http/pages/homepage.h"
#include "../../../include/stringfunctions.h"
#include "../../../include/global.h"
#include "../../../include/fmsapp.h"
#include "../../../include/option.h"

#ifdef XMEM
	#include <xmem.h>
#endif

const std::string HomePage::GenerateContent(const std::string &method, const std::map<std::string,QueryVar> &queryvars)
{

	Option option(m_db);

	std::string messagecountstr="";
	std::string filecountstr="";
	std::string fproxyhost="127.0.0.1";
	std::string fproxyport="8888";
	std::string fproxyprotocol="http";

	option.Get("FProxyHost",fproxyhost);
	option.Get("FProxyPort",fproxyport);
	option.Get("FProxyProtocol",fproxyprotocol);

	if(queryvars.find("formaction")!=queryvars.end() && (*queryvars.find("formaction")).second=="shutdown" && ValidateFormPassword(queryvars))
	{
		m_log->trace("HomePage::GeneratePage requested shutdown");
		((FMSApp *)&FMSApp::instance())->Terminate();
		global::shutdown=true;
	}

	if(queryvars.find("formaction")!=queryvars.end() && (*queryvars.find("formaction")).second=="restart" && ValidateFormPassword(queryvars))
	{
		m_log->trace("HomePage::GeneratePage requested restart");
		((FMSApp *)&FMSApp::instance())->Terminate();
		global::shutdown=true;
		global::restart=true;
	}

	std::string content="<h2>"+m_trans->Get("web.page.home.title")+"</h2>";
	content+="<p class=\"paragraph\">";
	content+="<strong>"+m_trans->Get("web.page.home.fmsversion")+" ";
	content+=FMS_VERSION;
	content+="</strong><br>";

	bool showgenericupdate=true;
	SQLite3DB::Statement st=m_db->Prepare("SELECT Major, Minor, Release, PageKey FROM tblFMSVersion ORDER BY Major DESC, Minor DESC, Release DESC LIMIT 1;");
	st.Step();
	if(st.RowReturned())
	{
		int major=0;
		int minor=0;
		int release=0;
		int currentmajor=0;
		int currentminor=0;
		int currentrelease=0;
		std::string freesite="";
		std::string majorstr="";
		std::string minorstr="";
		std::string releasestr="";

		StringFunctions::Convert(VERSION_MAJOR,currentmajor);
		StringFunctions::Convert(VERSION_MINOR,currentminor);
		StringFunctions::Convert(VERSION_RELEASE,currentrelease);

		st.ResultInt(0,major);
		st.ResultInt(1,minor);
		st.ResultInt(2,release);
		st.ResultText(3,freesite);

		StringFunctions::Convert(major,majorstr);
		StringFunctions::Convert(minor,minorstr);
		StringFunctions::Convert(release,releasestr);

		if(currentmajor<major || (currentmajor==major && currentminor<minor) || (currentmajor==major && currentminor==minor && currentrelease<release))
		{
			content+="<strong>"+m_trans->Get("web.page.home.oldversion")+" <a href=\""+fproxyprotocol+"://"+fproxyhost+":"+fproxyport+"/"+freesite+"\">FMS "+majorstr+"."+minorstr+"."+releasestr+"</a></strong><br>";
			content+=m_trans->Get("web.page.home.releaseinfo")+" <a href=\"versioninfo.htm?Major="+majorstr+"&Minor="+minorstr+"&Release="+releasestr+"\">"+m_trans->Get("web.page.home.releaseinfohere")+"</a><br>";
			showgenericupdate=false;
		}
		else
		{
			content+="<a href=\"versioninfo.htm\">"+m_trans->Get("web.page.home.releaseinfo")+"</a><br>";
		}

	}

	if(showgenericupdate)
	{
		content+=m_trans->Get("web.page.home.checknewreleases")+" <a href=\""+fproxyprotocol+"://"+fproxyhost+":"+fproxyport+"/"+FMS_FREESITE_USK+"\">"+m_trans->Get("web.page.home.fmsfreesite")+"</a><br>";
	}

	content+=m_trans->Get("web.page.home.admininstructions");
	content+="</p>";

	st=m_db->Prepare("SELECT COUNT(*) FROM tblMessageInserts WHERE Inserted='false';");
	st.Step();
	if(st.RowReturned())
	{
		st.ResultText(0,messagecountstr);
	}
	content+=m_trans->Get("web.page.home.messageswaiting")+messagecountstr;
	if (messagecountstr!="0") //show link to message page
	{
		content+=" (<a href=\"showpendingmessage.htm\">"+m_trans->Get("web.page.home.showmessageswaiting")+"</a>)";
	}
	content+="<br>";
	st=m_db->Prepare("SELECT COUNT(*) FROM tblFileInserts WHERE Key IS NULL;");
	st.Step();
	if(st.RowReturned())
	{
		st.ResultText(0,filecountstr);
	}
	content+=m_trans->Get("web.page.home.fileswaiting")+filecountstr+"<br>";


	st=m_db->Prepare("SELECT COUNT(*) FROM tblMessageInserts WHERE Inserted='true';");
	st.Step();
	if(st.RowReturned())
	{
		st.ResultText(0,messagecountstr);
	}
	/*
	st=m_db->Prepare("SELECT COUNT(*) FROM tblMessageInserts WHERE MessageUUID IN (SELECT MessageUUID FROM tblMessage);");
	st.Step();
	if(st.RowReturned())
	{
		st.ResultText(0,filecountstr);
	}
	*/
	content+=m_trans->Get("web.page.home.msgsinserted")+" "+messagecountstr;//+" / "+filecountstr;
	if (messagecountstr!="0") //show link to message page
	{
		content+=" (<a href=\"showinsertedmessage.htm\">"+m_trans->Get("web.page.home.show")+"</a>)";
	}
	content+="<br>";

	st=m_db->Prepare("SELECT COUNT(*) FROM tblMessage;");
	st.Step();
	if(st.RowReturned())
	{
		st.ResultText(0,filecountstr);
	}
	content+=m_trans->Get("web.page.home.msgsreceived")+" "+filecountstr+" (<a href=\"showreceivedmessage.htm\">"+m_trans->Get("web.page.home.show")+"</a>)<br>";

	content+="<p class=\"paragraph\">";
	content+="<form name=\"frmshutdown\" method=\"POST\">";
	content+=CreateFormPassword();
	content+="<input type=\"hidden\" name=\"formaction\" value=\"shutdown\">";
	content+="<input type=\"submit\" value=\""+m_trans->Get("web.page.home.shutdownfms")+"\">";
	content+="</form>";
	content+="<form name=\"frmrestart\" method=\"POST\">";
	content+=CreateFormPassword();
	content+="<input type=\"hidden\" name=\"formaction\" value=\"restart\">";
	content+="<input type=\"submit\" value=\""+m_trans->Get("web.page.home.restartfms")+"\">";
	content+="</form>";
	content+="</p>";

	return content;
}

const bool HomePage::WillHandleURI(const std::string &uri)
{
	return true;
}
