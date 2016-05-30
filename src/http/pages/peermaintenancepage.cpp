#include "../../../include/http/pages/peermaintenancepage.h"
#include "../../../include/stringfunctions.h"
#include "../../../include/option.h"

#include <Poco/DateTime.h>
#include <Poco/Timestamp.h>
#include <Poco/Timespan.h>
#include <Poco/DateTimeFormatter.h>

#ifdef XMEM
	#include <xmem.h>
#endif

const std::string PeerMaintenancePage::GenerateContent(const std::string &method, const std::map<std::string,QueryVar> &queryvars)
{
	std::string content("");
	std::string sql("");
	SQLite3DB::Statement st;
	std::string tempval;
	Poco::DateTime date;
	bool m_localtrustoverrides=false;
	Option opt(m_db);

	opt.GetBool("LocalTrustOverridesPeerTrust",m_localtrustoverrides);

	if(queryvars.find("formaction")!=queryvars.end() && ValidateFormPassword(queryvars))
	{
		if((*queryvars.find("formaction")).second=="removenotseen")
		{
			m_db->Execute("DELETE FROM tblIdentity WHERE LastSeen IS NULL AND WOTLastSeen IS NULL;");
		}
		else if((*queryvars.find("formaction")).second=="removelastseen20")
		{
			date=Poco::Timestamp();
			date-=Poco::Timespan(20,0,0,0,0);
			st=m_db->Prepare("DELETE FROM tblIdentity WHERE IFNULL(LastSeen<?,1) AND IFNULL(WOTLastSeen<?,1);");
			st.Bind(0,Poco::DateTimeFormatter::format(date,"%Y-%m-%d %H:%M:%S"));
			st.Bind(1,Poco::DateTimeFormatter::format(date,"%Y-%m-%d %H:%M:%S"));
			st.Step();
		}
		else if((*queryvars.find("formaction")).second=="removeneversent")
		{
			m_db->Execute("DELETE FROM tblIdentity WHERE IdentityID NOT IN (SELECT IdentityID FROM tblMessage WHERE IdentityID IS NOT NULL GROUP BY IdentityID);");
		}
		else if((*queryvars.find("formaction")).second=="removelastseenneversent20")
		{
			date=Poco::Timestamp();
			date-=Poco::Timespan(20,0,0,0,0);
			st=m_db->Prepare("DELETE FROM tblIdentity WHERE IdentityID NOT IN (SELECT IdentityID FROM tblMessage WHERE IdentityID IS NOT NULL GROUP BY IdentityID) AND IFNULL(LastSeen<?,1) AND IFNULL(WOTLastSeen<?,1);");
			st.Bind(0,Poco::DateTimeFormatter::format(date,"%Y-%m-%d %H:%M:%S"));
			st.Bind(1,Poco::DateTimeFormatter::format(date,"%Y-%m-%d %H:%M:%S"));
			st.Step();
		}
		else if((*queryvars.find("formaction")).second=="removedaysago" && queryvars.find("daysago")!=queryvars.end() && (*queryvars.find("daysago")).second!="")
		{
			int tempint=10000;
			StringFunctions::Convert((*queryvars.find("daysago")).second.GetData(),tempint);
			date=Poco::Timestamp();
			date-=Poco::Timespan(tempint,0,0,0,0);
			st=m_db->Prepare("DELETE FROM tblIdentity WHERE IFNULL(LastSeen<?,1) AND IFNULL(WOTLastSeen<?,1);");
			st.Bind(0,Poco::DateTimeFormatter::format(date,"%Y-%m-%d %H:%M:%S"));
			st.Bind(1,Poco::DateTimeFormatter::format(date,"%Y-%m-%d %H:%M:%S"));
			st.Step();
		}
		else if((*queryvars.find("formaction")).second=="removenulldaysago" && queryvars.find("daysago")!=queryvars.end() && (*queryvars.find("daysago")).second!="")
		{
			int tempint=10000;
			StringFunctions::Convert((*queryvars.find("daysago")).second.GetData(),tempint);
			date=Poco::Timestamp();
			date-=Poco::Timespan(tempint,0,0,0,0);
			st=m_db->Prepare("DELETE FROM tblIdentity WHERE IFNULL(LastSeen<?,1) AND IFNULL(WOTLastSeen<?,1) AND LocalMessageTrust IS NULL AND LocalTrustListTrust IS NULL;");
			st.Bind(0,Poco::DateTimeFormatter::format(date,"%Y-%m-%d %H:%M:%S"));
			st.Bind(1,Poco::DateTimeFormatter::format(date,"%Y-%m-%d %H:%M:%S"));
			st.Step();
		}
		else if((*queryvars.find("formaction")).second=="removeposted30daysago")
		{
			date=Poco::Timestamp();
			date-=Poco::Timespan(30,0,0,0,0);
			st=m_db->Prepare("DELETE FROM tblIdentity WHERE IdentityID IN (SELECT tblIdentity.IdentityID FROM tblIdentity INNER JOIN tblMessage ON tblIdentity.IdentityID=tblMessage.IdentityID WHERE (SELECT MAX(MessageDate) FROM tblMessage WHERE tblMessage.IdentityID=tblIdentity.IdentityID)<=? GROUP BY tblIdentity.IdentityID);");
			st.Bind(0,Poco::DateTimeFormatter::format(date,"%Y-%m-%d"));
			st.Step();
		}
		else if((*queryvars.find("formaction")).second=="removeadded20daysneversent")
		{
			date=Poco::Timestamp();
			date-=Poco::Timespan(20,0,0,0,0);
			st=m_db->Prepare("DELETE FROM tblIdentity WHERE IdentityID IN (SELECT tblIdentity.IdentityID FROM tblIdentity LEFT JOIN tblMessage ON tblIdentity.IdentityID=tblMessage.IdentityID WHERE tblMessage.IdentityID IS NULL AND tblIdentity.DateAdded<?);");
			st.Bind(0,Poco::DateTimeFormatter::format(date,"%Y-%m-%d %H:%M:%S"));
			st.Step();
		}
		else if((*queryvars.find("formaction")).second=="removepurged")
		{
			st=m_db->Prepare("DELETE FROM tblIdentity WHERE PurgeDate IS NOT NULL AND PurgeDate<=datetime('now','-90 days');");
			st.Step();
		}
	}

	content+="<h2>"+m_trans->Get("web.page.peermaintenance.title")+"</h2>";
	content+="<p class=\"paragraph\">"+m_trans->Get("web.page.peermaintenance.instructions")+"</p>";
	content+="<p>";
	content+="<a href=\"recentlyadded.htm\">"+m_trans->Get("web.page.peermaintenance.recentlyadded")+"</a>";
	content+="</p>";
	content+="<table>";
	content+="<tr><th colspan=\"3\">"+m_trans->Get("web.page.peermaintenance.stats")+"</th></tr>";

	content+="<tr>";
	st=m_db->Prepare("SELECT COUNT(*) FROM tblIdentity;");
	st.Step();
	st.ResultText(0,tempval);
	content+="<td>"+tempval+"</td>";
	content+="<td>"+m_trans->Get("web.page.peermaintenance.knownpeers")+"</td>";
	content+="</tr>";

	content+="<tr>";
	st=m_db->Prepare("SELECT COUNT(*) FROM tblIdentity WHERE IsFMS=1;");
	st.Step();
	st.ResultText(0,tempval);
	content+="<td>"+tempval+"</td>";
	content+="<td>"+m_trans->Get("web.page.peermaintenance.fmspeers")+"</td>";
	content+="</tr>";

	content+="<tr>";
	st=m_db->Prepare("SELECT COUNT(*) FROM tblIdentity WHERE IsWOT=1;");
	st.Step();
	st.ResultText(0,tempval);
	content+="<td>"+tempval+"</td>";
	content+="<td>"+m_trans->Get("web.page.peermaintenance.wotpeers")+"</td>";
	content+="</tr>";

	content+="<tr>";
	sql="SELECT COUNT(*) FROM tblIdentity WHERE ";
	if(m_localtrustoverrides==true)
	{
		sql+="(tblIdentity.LocalMessageTrust>=(SELECT OptionValue FROM tblOption WHERE Option='MinLocalMessageTrust') OR (tblIdentity.LocalMessageTrust IS NULL AND (tblIdentity.PeerMessageTrust IS NULL OR tblIdentity.PeerMessageTrust>=(SELECT OptionValue FROM tblOption WHERE Option='MinPeerMessageTrust'))))";
	}
	else
	{
		sql+="(tblIdentity.LocalMessageTrust IS NULL OR tblIdentity.LocalMessageTrust>=(SELECT OptionValue FROM tblOption WHERE Option='MinLocalMessageTrust'))";
		sql+="AND (tblIdentity.PeerMessageTrust IS NULL OR tblIdentity.PeerMessageTrust>=(SELECT OptionValue FROM tblOption WHERE Option='MinPeerMessageTrust'))";
	}
	st=m_db->Prepare(sql);
	st.Step();
	st.ResultText(0,tempval);
	content+="<td>"+tempval+"</td>";
	content+="<td>"+m_trans->Get("web.page.peermaintenance.trustedcount")+"</td>";
	content+="</tr>";

	content+="<tr>";
	st=m_db->Prepare("SELECT COUNT(*) FROM tblIdentity WHERE LastSeen IS NULL AND WOTLastSeen IS NULL;");
	st.Step();
	st.ResultText(0,tempval);
	content+="<td>"+tempval+"</td>";
	content+="<td>"+m_trans->Get("web.page.peermaintenance.neverseen")+"</td>";
	content+="<td>";
	content+="<form name=\"frmremove\" method=\"POST\">";
	content+=CreateFormPassword();
	content+="<input type=\"hidden\" name=\"formaction\" value=\"removenotseen\">";
	content+="<input type=\"submit\" value=\""+m_trans->Get("web.page.peermaintenance.remove")+"\">";
	content+="</form>";
	content+="</td>";
	content+="</tr>";

	content+="<tr>";
	st=m_db->Prepare("SELECT COUNT(*) FROM tblIdentity WHERE PurgeDate IS NOT NULL AND PurgeDate<=datetime('now','-90 days');");
	st.Step();
	st.ResultText(0,tempval);
	content+="<td>"+tempval+"</td>";
	content+="<td>"+m_trans->Get("web.page.peermaintenance.readytopurge")+"</td>";
	content+="<td>";
	content+="<form name=\"frmremove\" method=\"POST\">";
	content+=CreateFormPassword();
	content+="<input type=\"hidden\" name=\"formaction\" value=\"removepurged\">";
	content+="<input type=\"submit\" value=\""+m_trans->Get("web.page.peermaintenance.remove")+"\">";
	content+="</form>";
	content+="</td>";
	content+="</tr>";

	date=Poco::Timestamp();
	date-=Poco::Timespan(20,0,0,0,0);
	st=m_db->Prepare("SELECT COUNT(*) FROM tblIdentity WHERE IFNULL(LastSeen<?,1) AND IFNULL(WOTLastSeen<?,1);");
	st.Bind(0,Poco::DateTimeFormatter::format(date,"%Y-%m-%d %H:%M:%S"));
	st.Step();
	st.ResultText(0,tempval);
	content+="<tr>";
	content+="<td>"+tempval+"</td>";
	content+="<td>"+m_trans->Get("web.page.peermaintenance.lastseen20days")+"</td>";
	content+="<td>";
	content+="<form name=\"frmremove\" method=\"POST\">";
	content+=CreateFormPassword();
	content+="<input type=\"hidden\" name=\"formaction\" value=\"removelastseen20\">";
	content+="<input type=\"submit\" value=\""+m_trans->Get("web.page.peermaintenance.remove")+"\">";
	content+="</form>";
	content+="</td>";
	content+="</tr>";

	date=Poco::Timestamp();
	date-=Poco::Timespan(30,0,0,0,0);
	st=m_db->Prepare("SELECT COUNT(*) FROM (SELECT tblIdentity.IdentityID FROM tblIdentity INNER JOIN tblMessage ON tblIdentity.IdentityID=tblMessage.IdentityID WHERE (SELECT MAX(MessageDate) FROM tblMessage WHERE tblMessage.IdentityID=tblIdentity.IdentityID)<=? GROUP BY tblIdentity.IdentityID);");
	st.Bind(0,Poco::DateTimeFormatter::format(date,"%Y-%m-%d"));
	st.Step();
	st.ResultText(0,tempval);
	content+="<tr>";
	content+="<td>"+tempval+"</td>";
	content+="<td>"+m_trans->Get("web.page.peermaintenance.lastsent30days")+"</td>";
	content+="<td>";
	content+="<form name=\"frmremove\" method=\"POST\">";
	content+=CreateFormPassword();
	content+="<input type=\"hidden\" name=\"formaction\" value=\"removeposted30daysago\">";
	content+="<input type=\"submit\" value=\""+m_trans->Get("web.page.peermaintenance.remove")+"\">";
	content+="</form>";
	content+="</td>";
	content+="</tr>";

	st=m_db->Prepare("SELECT COUNT(*) FROM tblIdentity LEFT JOIN tblMessage ON tblIdentity.IdentityID=tblMessage.IdentityID WHERE tblMessage.IdentityID IS NULL;");
	st.Step();
	st.ResultText(0,tempval);
	content+="<tr>";
	content+="<td>"+tempval+"</td>";
	content+="<td>"+m_trans->Get("web.page.peermaintenance.neversent")+"</td>";
	content+="<td>";
	content+="<form name=\"frmremove\" method=\"POST\">";
	content+=CreateFormPassword();
	content+="<input type=\"hidden\" name=\"formaction\" value=\"removeneversent\">";
	content+="<input type=\"submit\" value=\""+m_trans->Get("web.page.peermaintenance.remove")+"\">";
	content+="</form>";
	content+="</td>";
	content+="</tr>";

	date=Poco::Timestamp();
	date-=Poco::Timespan(20,0,0,0,0);
	st=m_db->Prepare("SELECT COUNT(*) FROM tblIdentity LEFT JOIN tblMessage ON tblIdentity.IdentityID=tblMessage.IdentityID WHERE tblMessage.IdentityID IS NULL AND tblIdentity.DateAdded<?;");
	st.Bind(0,Poco::DateTimeFormatter::format(date,"%Y-%m-%d %H:%M:%S"));
	st.Step();
	st.ResultText(0,tempval);
	content+="<tr>";
	content+="<td>"+tempval+"</td>";
	content+="<td>"+m_trans->Get("web.page.peermaintenance.added20daysneversent")+"</td>";
	content+="<td>";
	content+="<form name=\"frmremove\" method=\"POST\">";
	content+=CreateFormPassword();
	content+="<input type=\"hidden\" name=\"formaction\" value=\"removeadded20daysneversent\">";
	content+="<input type=\"submit\" value=\""+m_trans->Get("web.page.peermaintenance.remove")+"\">";
	content+="</form>";
	content+="</td>";
	content+="</tr>";

	date=Poco::Timestamp();
	date-=Poco::Timespan(20,0,0,0,0);
	st=m_db->Prepare("SELECT COUNT(*) FROM tblIdentity LEFT JOIN tblMessage ON tblIdentity.IdentityID=tblMessage.IdentityID WHERE tblMessage.IdentityID IS NULL AND IFNULL(tblIdentity.LastSeen<?,1) AND IFNULL(tblIdentity.WOTLastSeen<?,1);");
	st.Bind(0,Poco::DateTimeFormatter::format(date,"%Y-%m-%d %H:%M:%S"));
	st.Bind(1,Poco::DateTimeFormatter::format(date,"%Y-%m-%d %H:%M:%S"));
	st.Step();
	st.ResultText(0,tempval);
	content+="<tr>";
	content+="<td>"+tempval+"</td>";
	content+="<td>"+m_trans->Get("web.page.peermaintenance.lastseen20daysneversent")+"</td>";
	content+="<td>";
	content+="<form name=\"frmremove\" method=\"POST\">";
	content+=CreateFormPassword();
	content+="<input type=\"hidden\" name=\"formaction\" value=\"removelastseenneversent20\">";
	content+="<input type=\"submit\" value=\""+m_trans->Get("web.page.peermaintenance.remove")+"\">";
	content+="</form>";
	content+="</td>";
	content+="</tr>";

	content+="<tr>";
	content+="<td><form name=\"frmdelete\" method=\"POST\">";
	content+=CreateFormPassword();
	content+="<input type=\"hidden\" name=\"formaction\" value=\"removedaysago\"></td>";
	content+="<td>"+m_trans->Get("web.page.peermaintenance.lastseen")+" <input type=\"text\" name=\"daysago\" size=\"2\"> "+m_trans->Get("web.page.peermaintenance.daysago")+"</td>";
	content+="<td><input type=\"submit\" value=\""+m_trans->Get("web.page.peermaintenance.remove")+"\"></form></td>";
	content+="</tr>";

	content+="<tr>";
	content+="<td><form name=\"frmdelete\" method=\"POST\">";
	content+=CreateFormPassword();
	content+="<input type=\"hidden\" name=\"formaction\" value=\"removenulldaysago\"></td>";
	content+="<td>"+m_trans->Get("web.page.peermaintenance.lastseen")+" <input type=\"text\" name=\"daysago\" size=\"2\"> "+m_trans->Get("web.page.peermaintenance.daysagonulltrust")+"</td>";
	content+="<td><input type=\"submit\" value=\""+m_trans->Get("web.page.peermaintenance.remove")+"\"></form></td>";
	content+="</tr>";

	content+="</table>";

	return content;
}

const bool PeerMaintenancePage::WillHandleURI(const std::string &uri)
{
	if(uri.find("peermaintenance.")!=std::string::npos)
	{
		return true;
	}
	else
	{
		return false;
	}
}
