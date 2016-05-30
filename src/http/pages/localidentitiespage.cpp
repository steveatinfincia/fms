#include "../../../include/http/pages/localidentitiespage.h"
#include "../../../include/stringfunctions.h"
#include "../../../include/http/identityexportxml.h"
#include "../../../include/global.h"
#include "../../../include/unicode/unicodestring.h"
#include "../../../include/freenet/freenetkeys.h"

#ifdef XMEM
	#include <xmem.h>
#endif

const std::string LocalIdentitiesPage::CreatePuzzleTypeDropDown(const std::string &name, const std::string &selected)
{
	std::string rval="";

	rval+="<select name=\""+name+"\">";
	rval+="<option value=\"image\"";
	if(selected=="image")
	{
		rval+=" SELECTED";
	}
	rval+=">"+m_trans->Get("web.page.localidentities.puzzletype.image")+"</option>";

	rval+="<option value=\"unlikeimage\"";
	if(selected=="unlikeimage")
	{
		rval+=" SELECTED";
	}
	rval+=">"+m_trans->Get("web.page.localidentities.puzzletype.unlikeimage")+"</option>";

#ifdef AUDIO_CAPTCHA
	rval+="<option value=\"audio\"";
	if(selected=="audio")
	{
		rval+=" SELECTED";
	}
	rval+=">"+m_trans->Get("web.page.localidentities.puzzletype.audio")+"</option>";
#endif

	rval+="<option value=\"random\"";
	if(selected=="random")
	{
		rval+=" SELECTED";
	}
	rval+=">"+m_trans->Get("web.page.localidentities.puzzletype.random")+"</option>";

	rval+="</select>";

	return rval;
}

void LocalIdentitiesPage::ForceInsertion(std::string field, std::string idstr)
{
	int id;
	
	if(StringFunctions::Convert(idstr, id))
	{
		SQLite3DB::Statement st = m_db->Prepare("UPDATE tblLocalIdentity SET " + field + " = datetime(42, 'unixepoch') where LocalIdentityID = ?;");
		st.Bind(0, id);
		st.Step();
	}
}

std::string LocalIdentitiesPage::GenerateComposedInsertionTR(std::string label, std::string tableName, std::string date, std::string formaction, std::string id)
{
	std::string content;
	SQLite3DB::Statement st = m_db->Prepare("SELECT Inserted FROM "+tableName+" WHERE LocalIdentityID = ? ORDER BY InsertIndex DESC LIMIT 1");
	st.Bind(0, id);
	st.Step();
	if(st.RowReturned())
	{
		std::string inserting;
		st.ResultText(0, inserting);
		
		inserting = (inserting=="true") ? "false" : "true";
		
		content=GenerateInsertionTR(label, inserting, date, formaction, id);
	}
	else
	{
		content=GenerateInsertionTR(label, "false", date, formaction, id);
	}
	return content;
}
std::string LocalIdentitiesPage::GenerateInsertionTR(std::string label, std::string inserting, std::string date, std::string formaction, std::string id)
{
	std::string content;
	content="<tr>";
	content+="<td colspan=\"3\" style=\"text-align:right;font-weight:bolder;\">"+label+"</td>";
	if(inserting=="false")
	{
		if(date=="1970-01-01 00:00:42")
		{
			content+="<td colspan=\"2\" style=\"text-align:right;font-weight:bolder;\">-</td>";
			content+="<td colspan=\"2\" style=\"text-align:right;font-weight:bolder;\">"+m_trans->Get("web.page.localidentities.imminentinserting")+"</td>";
		}
		else if(date=="")
		{
			content+="<td colspan=\"2\" style=\"text-align:right;font-weight:bolder;\">-</td>";
			content+="<td colspan=\"2\" style=\"text-align:right;font-weight:bolder;\">"+m_trans->Get("web.page.localidentities.neverinserted")+"</td>";
		}
		else
		{
			content+="<td colspan=\"2\" style=\"text-align:right;font-weight:bolder;\">"+date+" UTC</td>";
			content+="<td colspan=\"2\" style=\"text-align:right;font-weight:bolder;\"><a href=\""+m_pagename+"?formaction="+formaction+"&id="+id+"&"+CreateLinkFormPassword()+"\">"+m_trans->Get("web.page.localidentities.forceinsert")+"</a></td>";
		}
	}
	else
	{
		content+="<td colspan=\"2\" style=\"text-align:right;font-weight:bolder;\">-</td>";
		content+="<td colspan=\"2\" style=\"text-align:right;font-weight:bolder;\">"+m_trans->Get("web.page.localidentities.inserting")+"</td>";
	}
	content+="</tr>";
	
	return content;
}

const std::string LocalIdentitiesPage::GenerateContent(const std::string &method, const std::map<std::string,QueryVar> &queryvars)
{

	if(queryvars.find("formaction")!=queryvars.end() && ValidateFormPassword(queryvars))
	{
		if(queryvars.find("id")!=queryvars.end())
		{
			if((*queryvars.find("formaction")).second=="forceidentityinsertion")
			{
				ForceInsertion("LastInsertedIdentity", (*queryvars.find("id")).second.GetData());
			}
			else if((*queryvars.find("formaction")).second=="forcetrustlistinsertion")
			{
				ForceInsertion("LastInsertedTrustList", (*queryvars.find("id")).second.GetData());
			}
			else if((*queryvars.find("formaction")).second=="forceboardlistinsertion")
			{
				ForceInsertion("LastInsertedBoardList", (*queryvars.find("id")).second.GetData());
			}
			else if((*queryvars.find("formaction")).second=="forcemessagelistinsertion")
			{
				ForceInsertion("LastInsertedMessageList", (*queryvars.find("id")).second.GetData());
			}
			else if((*queryvars.find("formaction")).second=="forcefreesiteinsertion")
			{
				ForceInsertion("LastInsertedFreesite", (*queryvars.find("id")).second.GetData());
			}
		}
	}
	
	int count;
	std::string countstr;
	std::string content="";
	
	content+="<h2>"+m_trans->Get("web.page.localidentities.title")+"</h2>";

	content+="<table><tr><th>"+m_trans->Get("web.page.localidentities.exportidentities")+"</th><th>"+m_trans->Get("web.page.localidentities.importidentities")+"</th></tr>";
	content+="<tr><td>";
	content+="<form name=\"frmexport\" method=\"POST\">";
	content+=CreateFormPassword();
	content+="<input type=\"hidden\" name=\"formaction\" value=\"export\">";
	content+="<input type=\"submit\" value=\""+m_trans->Get("web.page.localidentities.exportidentities")+"\">";
	content+="</form>";
	content+="</td><td>";
	content+="<form name=\"frmimport\" method=\"POST\" enctype=\"multipart/form-data\">";
	content+=CreateFormPassword();
	content+="<input type=\"hidden\" name=\"formaction\" value=\"import\">";
	content+="<input type=\"file\" name=\"file\">";
	content+="<input type=\"submit\" value=\""+m_trans->Get("web.page.localidentities.importidentities")+"\">";
	content+="</form>";
	content+="</td></tr></table>";

	content+="<hr>";

	content+="<table class=\"small90\">";

	SQLite3DB::Statement st=m_db->Prepare("SELECT LocalIdentityID,tblLocalIdentity.Name,tblLocalIdentity.PublicKey,tbLLocalIdentity.PublishTrustList,tblLocalIdentity.SingleUse,tblLocalIdentity.PublishBoardList,tblIdentity.IdentityID,tblLocalIdentity.PublishFreesite,tblLocalIdentity.MinMessageDelay,tblLocalIdentity.MaxMessageDelay,tblLocalIdentity.Active,tblLocalIdentity.IntroductionPuzzleType,tblLocalIdentity.Signature, tblLocalIdentity.FMSAvatar,tblLocalIdentity.InsertingIdentity,tblLocalIdentity.LastInsertedIdentity,tblLocalIdentity.InsertingTrustList,tblLocalIdentity.LastInsertedTrustList,tblLocalIdentity.LastInsertedBoardList,tblLocalIdentity.LastInsertedMessageList,tblLocalIdentity.LastInsertedFreesite FROM tblLocalIdentity LEFT JOIN tblIdentity ON tblLocalIdentity.PublicKey=tblIdentity.PublicKey ORDER BY tblLocalIdentity.Name;");
	st.Step();
	SQLite3DB::Statement st2=m_db->Prepare("SELECT IdentityID FROM tblIdentity WHERE PublicKey=?;");

	//SQLite3DB::Statement trustst=m_db->Prepare("SELECT COUNT(*) FROM tblPeerTrust LEFT JOIN tblIdentity ON tblPeerTrust.TargetIdentityID=tblIdentity.IdentityID WHERE tblIdentity.PublicKey=? GROUP BY tblPeerTrust.TargetIdentityID;");
	SQLite3DB::Statement trustst=m_db->Prepare("SELECT COUNT(*) FROM tblPeerTrust WHERE TargetIdentityID=? GROUP BY TargetIdentityID;");

	count=0;
	while(st.RowReturned())
	{
		StringFunctions::Convert(count,countstr);
		std::string id="";
		std::string name="";
		std::string publickey="";
		std::string publishtrustlist="";
		std::string singleuse="";
		std::string publishboardlist="";
		std::string publishfreesite="";
		std::string minmessagedelay="0";
		std::string maxmessagedelay="0";
		int identityid=0;
		std::string identityidstr="";
		std::string active="";
		std::string introductionpuzzletype="";
		std::string signature="";
		std::string avatar="";
		std::string insertingidentity="";
		std::string lastinsertedidentity="";
		std::string insertingtrustlist="";
		std::string lastinsertedtrustlist="";
		std::string lastinsertedboardlist="";
		std::string lastinsertedmessagelist="";
		std::string lastinsertedfreesite="";

		st.ResultText(0,id);
		st.ResultText(1,name);
		st.ResultText(2,publickey);
		st.ResultText(3,publishtrustlist);
		st.ResultText(4,singleuse);
		st.ResultText(5,publishboardlist);
		st.ResultText(7,publishfreesite);
		st.ResultText(8,minmessagedelay);
		st.ResultText(9,maxmessagedelay);
		st.ResultText(10,active);
		st.ResultText(11,introductionpuzzletype);
		st.ResultText(12,signature);
		st.ResultText(13,avatar);
		st.ResultText(14,insertingidentity);
		st.ResultText(15,lastinsertedidentity);
		st.ResultText(16,insertingtrustlist);
		st.ResultText(17,lastinsertedtrustlist);
		st.ResultText(18,lastinsertedboardlist);
		st.ResultText(19,lastinsertedmessagelist);
		st.ResultText(20,lastinsertedfreesite);

		st2.Bind(0,publickey);
		st2.Step();
		if(st2.RowReturned())
		{
			st2.ResultText(0,identityidstr);
			st2.ResultInt(0,identityid);
		}
		st2.Reset();

		content+="<tr><th>"+m_trans->Get("web.page.localidentities.name")+"</th><th>"+m_trans->Get("web.page.localidentities.singleuse")+"</th><th>"+m_trans->Get("web.page.localidentities.publishtrustlist")+"</th><th>"+m_trans->Get("web.page.localidentities.publishboardlist")+"</th><th>"+m_trans->Get("web.page.localidentities.publishfreesite")+"</th><th title="+SanitizeOutput(m_trans->Get("web.page.localidentities.active.description"))+">"+m_trans->Get("web.page.localidentities.active")+"</th><th>"+m_trans->Get("web.page.localidentities.minmessagedelay")+"</th><th>"+m_trans->Get("web.page.localidentities.maxmessagedelay")+"</th><th>"+m_trans->Get("web.page.localidentities.announced")+"</th></tr>";
		content+="<tr>";
		content+="<td title=\""+publickey+"\"><form name=\"frmupdate"+countstr+"\" method=\"POST\"><input type=\"hidden\" name=\"formaction\" value=\"update\">"+CreateFormPassword()+"<input type=\"hidden\" name=\"chkidentityid["+countstr+"]\" value=\""+id+"\">";
		if(identityidstr!="")
		{
			content+="<a href=\"peerdetails.htm?identityid="+identityidstr+"\">";
		}
		content+=SanitizeOutput(CreateShortIdentityName(name,publickey));
		if(identityidstr!="")
		{
			content+="</a>";
		}
		content+="</td>";
		content+="<td>"+CreateTrueFalseDropDown("singleuse["+countstr+"]",singleuse)+"</td>";
		content+="<td>"+CreateTrueFalseDropDown("publishtrustlist["+countstr+"]",publishtrustlist)+"</td>";
		content+="<td>"+CreateTrueFalseDropDown("publishboardlist["+countstr+"]",publishboardlist)+"</td>";
		content+="<td>"+CreateTrueFalseDropDown("publishfreesite["+countstr+"]",publishfreesite)+"</td>";
		content+="<td>"+CreateTrueFalseDropDown("active["+countstr+"]",active)+"</td>";
		content+="<td><input type=\"text\" size=\"2\" name=\"mindelay["+countstr+"]\" value=\""+minmessagedelay+"\"></td>";
		content+="<td><input type=\"text\" size=\"2\" name=\"maxdelay["+countstr+"]\" value=\""+maxmessagedelay+"\"></td>";
		
		//trustst.Bind(0,publickey);
		trustst.Bind(0,identityid);
		trustst.Step();
		if(trustst.RowReturned())
		{
			std::string numlists="";
			trustst.ResultText(0,numlists);
			content+="<td>"+m_trans->Get("web.page.localidentities.yes")+" ("+numlists+")</td>";
		}
		else
		{
			content+="<td>"+m_trans->Get("web.page.localidentities.no")+"</td>";
		}
		trustst.Reset();
		content+="</tr>";

		content+="<tr>";
		content+="<td colspan=\"3\" style=\"text-align:right;font-weight:bolder;\">"+m_trans->Get("web.page.localidentity.avatar")+"</td>";
		content+="<td colspan=\"5\"><input name=\"avatar["+countstr+"]\" size=\"100\" value=\""+SanitizeOutput(avatar)+"\"></td>";
		content+="</tr>";

		content+="<tr>";
		content+="<td colspan=\"3\" style=\"text-align:right;font-weight:bolder;\">"+m_trans->Get("web.page.localidentities.signature")+"</td>";
		content+="<td colspan=\"4\"><textarea cols=\"50\" rows=\"5\" name=\"signature["+countstr+"]\">"+SanitizeTextAreaOutput(signature)+"</textarea></td>";
		content+="</tr>";

		content+="<tr>";
		content+="<td colspan=\"3\" style=\"text-align:right;font-weight:bolder;\">"+m_trans->Get("web.page.localidentities.puzzletype")+"</td>";
		content+="<td colspan=\"2\">"+CreatePuzzleTypeDropDown("puzzletype["+countstr+"]",introductionpuzzletype)+"</td>";
		content+="</tr>";

		if(active=="true")
		{
			// Identity 
			content+=GenerateInsertionTR(m_trans->Get("web.page.localidentities.lastinsertedidentity"), insertingidentity, lastinsertedidentity, "forceidentityinsertion", id);
			
			if(publishtrustlist=="true")
			{
				content+=GenerateInsertionTR(m_trans->Get("web.page.localidentities.lastinsertedtrustlist"), insertingtrustlist, lastinsertedtrustlist, "forcetrustlistinsertion", id);
			}
			
			if(publishboardlist=="true")
			{
				content+=GenerateComposedInsertionTR(m_trans->Get("web.page.localidentities.lastinsertedboardlist"), "tblBoardListInserts", lastinsertedboardlist, "forceboardlistinsertion", id);
			}
			
			// MessageList
			content+=GenerateComposedInsertionTR(m_trans->Get("web.page.localidentities.lastinsertedmessagelist"), "tblMessageListInserts", lastinsertedmessagelist, "forcemessagelistinsertion", id);
			
			if(publishfreesite=="true")
			{
				content+=GenerateInsertionTR(m_trans->Get("web.page.localidentities.lastinsertedfreesite"), "false", lastinsertedfreesite, "forcefreesiteinsertion", id);
			}
		}

		content+="<tr><td></td><td colspan=\"7\" class=\"smaller\">"+publickey+"</td></tr>";
		
		content+="<tr><td colspan=\"9\"><table width=\"100%\">";
		content+="<td style=\"text-align:right;padding-right:10px;\"><input type=\"submit\" value=\""+m_trans->Get("web.page.localidentities.update")+"\"></form></td>";
		content+="<td style=\"text-align:left;padding-left:10px;\"><form name=\"frmdel"+countstr+"\" method=\"POST\" action=\"confirm.htm\">"+CreateFormPassword()+"<input type=\"hidden\" name=\"formaction\" value=\"delete\"><input type=\"hidden\" name=\"chkidentityid["+countstr+"]\" value=\""+id+"\"><input type=\"hidden\" name=\"targetpage\" value=\"localidentities.htm\"><input type=\"hidden\" name=\"confirmdescription\" value=\""+m_trans->Get("web.page.localidentities.confirmdelete")+" "+SanitizeOutput(CreateShortIdentityName(name,publickey))+"?\"><input type=\"submit\" value=\""+m_trans->Get("web.page.localidentities.delete")+"\"></form></td>";
		content+="</table></td></tr>";

		st.Step();
		if(st.RowReturned())
		{
			content+="<tr><td style=\"height:25px;font-size:1pt;\">&nbsp;</td></tr>";
		}
		count++;
	}

	content+="</table>";
	content+="<p class=\"paragraph\">"+m_trans->Get("web.page.localidentities.announceddescription")+"</p>";
	content+="<p class=\"paragraph\">"+m_trans->Get("web.page.localidentities.singleusedescription")+"</p>";
	content+="<p class=\"paragraph\">"+m_trans->Get("web.page.localidentities.delaydescription")+"</p>";
	content+="<p class=\"paragraph\">"+m_trans->Get("web.page.localidentities.active.description")+"</p>";

	return content;
}

void LocalIdentitiesPage::HandleDelete(const std::map<std::string,QueryVar> &queryvars)
{
	int id=0;
	std::vector<std::string> ids;
	CreateArgArray(queryvars,"chkidentityid",ids);

	SQLite3DB::Statement del=m_db->Prepare("DELETE FROM tblLocalIdentity WHERE LocalIdentityID=?;");
	for(int i=0; i<ids.size(); i++)
	{
		if(ids[i]!="")
		{
			StringFunctions::Convert(ids[i],id);
			del.Bind(0,id);
			del.Step();
			del.Reset();
		}
	}
}

const std::string LocalIdentitiesPage::HandleExport()
{
	IdentityExportXML xml;
	SQLite3DB::Statement exp=m_db->Prepare("SELECT Name,PublicKey,PrivateKey,SingleUse,PublishTrustList,PublishBoardList,PublishFreesite FROM tblLocalIdentity WHERE PublicKey IS NOT NULL AND PrivateKey IS NOT NULL;");
	exp.Step();
	while(exp.RowReturned())
	{
		std::string name="";
		std::string publickey="";
		std::string privatekey="";
		std::string tempval="";
		bool singleuse=false;
		bool publishtrustlist=false;
		bool publishboardlist=false;
		bool publishfreesite=false;

		exp.ResultText(0,name);
		exp.ResultText(1,publickey);
		exp.ResultText(2,privatekey);
		exp.ResultText(3,tempval);
		if(tempval=="true")
		{
			singleuse=true;
		}
		exp.ResultText(4,tempval);
		if(tempval=="true")
		{
			publishtrustlist=true;
		}
		exp.ResultText(5,tempval);
		if(tempval=="true")
		{
			publishboardlist=true;
		}
		exp.ResultText(6,tempval);
		if(tempval=="true")
		{
			publishfreesite=true;
		}

		xml.AddIdentity(name,publickey,privatekey,singleuse,publishtrustlist,publishboardlist,publishfreesite);

		exp.Step();
	}
	return xml.GetXML();
}

void LocalIdentitiesPage::HandleImport(const std::map<std::string,QueryVar> &queryvars)
{
	if(queryvars.find("file")!=queryvars.end())
	{
		IdentityExportXML xml;
		if(xml.ParseXML((*queryvars.find("file")).second.GetData()))
		{
			SQLite3DB::Statement imp=m_db->Prepare("INSERT INTO tblLocalIdentity(Name,PublicKey,PrivateKey,SingleUse,PublishTrustList,PublishBoardList,PublishFreesite) VALUES(?,?,?,?,?,?,?);");
			for(int i=0; i<xml.GetCount(); i++)
			{
				FreenetSSKKey pubssk;
				FreenetSSKKey privssk;
				if(pubssk.TryParse(xml.GetPublicKey(i))==true && privssk.TryParse(xml.GetPrivateKey(i))==true)
				{
					std::string tempval="false";
					imp.Bind(0,xml.GetName(i));
					imp.Bind(1,pubssk.GetBaseKey());
					imp.Bind(2,privssk.GetBaseKey());
					if(xml.GetSingleUse(i))
					{
						tempval="true";
					}
					else
					{
						tempval="false";
					}
					imp.Bind(3,tempval);
					if(xml.GetPublishTrustList(i))
					{
						tempval="true";
					}
					else
					{
						tempval="false";
					}
					imp.Bind(4,tempval);
					if(xml.GetPublishBoardList(i))
					{
						tempval="true";
					}
					else
					{
						tempval="false";
					}
					imp.Bind(5,tempval);
					if(xml.GetPublishFreesite(i))
					{
						tempval="true";
					}
					else
					{
						tempval="false";
					}
					imp.Bind(6,tempval);
					imp.Step();
					imp.Reset();
				}
			}
		}
	}
}

void LocalIdentitiesPage::handleRequest(Poco::Net::HTTPServerRequest &request, Poco::Net::HTTPServerResponse &response)
{
	m_log->trace("LocalIdentitiesPages::handleRequest from "+request.clientAddress().toString());

	std::map<std::string,QueryVar> vars;

	CreateQueryVarMap(request,vars);

	std::string formaction="";
	if(vars.find("formaction")!=vars.end() && ValidateFormPassword(vars))
	{
		formaction=(*vars.find("formaction")).second.GetData();
		if(formaction=="update")
		{
			HandleUpdate(vars);
		}
		else if(formaction=="delete")
		{
			HandleDelete(vars);
		}
		else if(formaction=="export")
		{
			response.setChunkedTransferEncoding(true);
			response.setContentType("application/xml");
			response.set("Content-Disposition","attachment; filename=identities.xml");
			std::ostream &out=response.send();
			out << HandleExport();
			return;
		}
		else if(formaction=="import")
		{
			HandleImport(vars);
		}
	}

	response.setChunkedTransferEncoding(true);
	response.setContentType("text/html");

	std::ostream &ostr = response.send();
	ostr << GeneratePage(request.getMethod(),vars);
}

void LocalIdentitiesPage::HandleUpdate(const std::map<std::string,QueryVar> &queryvars)
{
	int id;
	std::vector<std::string> ids;
	std::vector<std::string> singleuse;
	std::vector<std::string> publishtrustlist;
	std::vector<std::string> publishboardlist;
	std::vector<std::string> publishfreesite;
	std::vector<std::string> mindelay;
	std::vector<std::string> maxdelay;
	std::vector<std::string> active;
	std::vector<std::string> puzzletype;
	std::vector<std::string> signature;
	std::vector<std::string> avatar;

	CreateArgArray(queryvars,"chkidentityid",ids);
	CreateArgArray(queryvars,"singleuse",singleuse);
	CreateArgArray(queryvars,"publishtrustlist",publishtrustlist);
	CreateArgArray(queryvars,"publishboardlist",publishboardlist);
	CreateArgArray(queryvars,"publishfreesite",publishfreesite);
	CreateArgArray(queryvars,"mindelay",mindelay);
	CreateArgArray(queryvars,"maxdelay",maxdelay);
	CreateArgArray(queryvars,"active",active);
	CreateArgArray(queryvars,"puzzletype",puzzletype);
	CreateArgArray(queryvars,"signature",signature);
	CreateArgArray(queryvars,"avatar",avatar);

	SQLite3DB::Statement update=m_db->Prepare("UPDATE tblLocalIdentity SET SingleUse=?, PublishTrustList=?, PublishBoardList=?, PublishFreesite=?, MinMessageDelay=?, MaxMessageDelay=?, Active=?, IntroductionPuzzleType=?, Signature=?, FMSAvatar=? WHERE LocalIdentityID=?;");
	for(int i=0; i<ids.size(); i++)
	{
		if(ids[i]!="")
		{
			UnicodeString usig(signature[i]);
			usig.Trim(MAX_SIGNATURE_LENGTH);

			int minmessagedelay=0;
			int maxmessagedelay=0;
			StringFunctions::Convert(ids[i],id);
			StringFunctions::Convert(mindelay[i],minmessagedelay);
			StringFunctions::Convert(maxdelay[i],maxmessagedelay);
			update.Bind(0,singleuse[i]);
			update.Bind(1,publishtrustlist[i]);
			update.Bind(2,publishboardlist[i]);
			update.Bind(3,publishfreesite[i]);
			update.Bind(4,minmessagedelay);
			update.Bind(5,maxmessagedelay);
			update.Bind(6,active[i]);
			update.Bind(7,puzzletype[i]);
			update.Bind(8,usig.UTF8String());
			update.Bind(9,avatar[i]);
			update.Bind(10,id);
			update.Step();
			update.Reset();
		}
	}
}

const bool LocalIdentitiesPage::WillHandleURI(const std::string &uri)
{
	if(uri.find("localidentities.")!=std::string::npos)
	{
		return true;
	}
	else
	{
		return false;
	}
}
