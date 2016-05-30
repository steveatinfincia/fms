#include "../../include/http/ipagehandler.h"
#include "../../include/stringfunctions.h"
#include "../../include/http/multipartparser.h"
#include "../../include/fmsapp.h"
#include "../../include/unicode/unicodestring.h"

#include <Poco/Net/HTMLForm.h>
#include <Poco/UUIDGenerator.h>
#include <Poco/UUID.h>
#include <Poco/DateTime.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/Timespan.h>
#ifndef POCO_VERSION	// Used to be in Foundation.h, as of 1.4 it's in Version.h, but not included by default
#include <Poco/Version.h>
#endif
#include <cstring>
#include <algorithm>

#ifdef XMEM
	#include <xmem.h>
#endif

IPageHandler::IPageHandler(SQLite3DB::DB *db):IDatabase(db)
{
	m_trans=Translation.get();

	// must do & first because all other elements have & in them!
	m_htmlencode.push_back(std::pair<std::string,std::string>("&","&amp;"));
	m_htmlencode.push_back(std::pair<std::string,std::string>("<","&lt;"));
	m_htmlencode.push_back(std::pair<std::string,std::string>(">","&gt;"));
	m_htmlencode.push_back(std::pair<std::string,std::string>("\"","&quot;"));
	m_htmlencode.push_back(std::pair<std::string,std::string>(" ","&#32;"));

}

IPageHandler::IPageHandler(SQLite3DB::DB *db, const std::string &templatestr, const std::string &pagename):IDatabase(db),m_template(templatestr),m_pagename(pagename)	
{
	m_trans=Translation.get();

	// must do & first because all other elements have & in them!
	m_htmlencode.push_back(std::pair<std::string,std::string>("&","&amp;"));
	m_htmlencode.push_back(std::pair<std::string,std::string>("<","&lt;"));
	m_htmlencode.push_back(std::pair<std::string,std::string>(">","&gt;"));
	m_htmlencode.push_back(std::pair<std::string,std::string>("\"","&quot;"));
	m_htmlencode.push_back(std::pair<std::string,std::string>(" ","&#32;"));

}

void IPageHandler::CreateArgArray(const std::map<std::string,QueryVar> &vars, const std::string &basename, std::vector<std::string> &args)
{
	for(std::map<std::string,QueryVar>::const_iterator i=vars.begin(); i!=vars.end(); ++i)
	{
		if((*i).first.find(basename)==0 && (*i).first.find('[')!=std::string::npos && (*i).first.find(']')!=std::string::npos)
		{
			int index=0;
			std::string indexstr;
			std::string::size_type startpos;
			std::string::size_type endpos;
			startpos=(*i).first.find('[');
			endpos=(*i).first.find(']');

			indexstr=(*i).first.substr(startpos+1,(endpos-startpos)-1);
			StringFunctions::Convert(indexstr,index);

			while(args.size()<index+1)
			{
				args.push_back("");
			}
			args[index]=(*i).second.GetData();
		}
	}
}

const std::string IPageHandler::CreateLinkFormPassword()
{
	Poco::DateTime date;
	Poco::UUIDGenerator uuidgen;
	Poco::UUID uuid;
	try
	{
		uuid=uuidgen.createRandom();
	}
	catch(...)
	{
	}

	SQLite3DB::Statement st=m_db->Prepare("INSERT INTO tmpFormPassword(Date,Password) VALUES(?,?);");
	st.Bind(0,Poco::DateTimeFormatter::format(date,"%Y-%m-%d %H:%M:%S"));
	st.Bind(1,uuid.toString());
	st.Step();

	return "formpassword="+uuid.toString();
}

const std::string IPageHandler::CreateFormPassword()
{
	Poco::DateTime date;
	Poco::UUIDGenerator uuidgen;
	Poco::UUID uuid;
	try
	{
		uuid=uuidgen.createRandom();
	}
	catch(...)
	{
	}

	SQLite3DB::Statement st=m_db->Prepare("INSERT INTO tmpFormPassword(Date,Password) VALUES(?,?);");
	st.Bind(0,Poco::DateTimeFormatter::format(date,"%Y-%m-%d %H:%M:%S"));
	st.Bind(1,uuid.toString());
	st.Step();

	return "<input type=\"hidden\" name=\"formpassword\" value=\""+uuid.toString()+"\">";

}

const std::string IPageHandler::CreateTrueFalseDropDown(const std::string &name, const std::string &selected)
{
	std::string rval="";

	rval+="<select name=\""+name+"\">";
	rval+="<option value=\"true\"";
	if(selected=="true")
	{
		rval+=" SELECTED";
	}
	rval+=">"+m_trans->Get("web.option.true")+"</option>";
	rval+="<option value=\"false\"";
	if(selected=="false")
	{
		rval+=" SELECTED";
	}
	rval+=">"+m_trans->Get("web.option.false")+"</option>";
	rval+="</select>";

	return rval;
}

void IPageHandler::CreateQueryVarMap(Poco::Net::HTTPServerRequest &request, std::map<std::string,QueryVar> &vars)
{
	// if query var ends in [] without #, create unique # inside []
	std::string queryvar("");
	std::map<std::string,int> uniqueidx;

	for(Poco::Net::HTTPServerRequest::ConstIterator i=request.begin(); i!=request.end(); i++)
	{
		queryvar=(*i).first;
		if(queryvar.find("[]")==queryvar.size()-2)
		{
			queryvar=CreateUniqueQueryVar(queryvar,uniqueidx);
		}
		vars[queryvar]=QueryVar(queryvar,(*i).second);
	}

	// handle HTMLForm and multiparts
	MultiPartParser mpp;
	Poco::Net::HTMLForm form;
#if POCO_VERSION>=0x01040300
	form.setFieldLimit(1000);
#endif
	form.load(request,request.stream(),mpp);
	for(Poco::Net::HTMLForm::ConstIterator i=form.begin(); i!=form.end(); i++)
	{
		queryvar=(*i).first;
		if(queryvar.find("[]")==queryvar.size()-2)
		{
			queryvar=CreateUniqueQueryVar(queryvar,uniqueidx);
		}
		vars[queryvar]=QueryVar(queryvar,(*i).second);
	}

	// for a POST method, the HTMLForm won't grab vars off the query string so we
	// temporarily set the method to GET and parse with the HTMLForm again
	if(request.getMethod()=="POST")
	{
		request.setMethod("GET");
		Poco::Net::HTMLForm form1(request,request.stream(),mpp);
		for(Poco::Net::HTMLForm::ConstIterator i=form1.begin(); i!=form1.end(); i++)
		{
			// don't overwrite query vars that came directly from a post
			if(vars.find((*i).first)==vars.end())
			{
				queryvar=(*i).first;
				if(queryvar.find("[]")==queryvar.size()-2)
				{
					queryvar=CreateUniqueQueryVar(queryvar,uniqueidx);
				}
				vars[queryvar]=QueryVar(queryvar,(*i).second);
			}
		}
		request.setMethod("POST");
	}

	// get any multiparts
	std::map<std::string,QueryVar> mpvars=mpp.GetVars();
	for(std::map<std::string,QueryVar>::iterator i=mpvars.begin(); i!=mpvars.end(); ++i)
	{
		queryvar=(*i).first;
		if(queryvar.find("[]")==queryvar.size()-2)
		{
			queryvar=CreateUniqueQueryVar(queryvar,uniqueidx);
		}
		vars[queryvar]=(*i).second;
	}

}

const std::string IPageHandler::CreateUniqueQueryVar(const std::string &queryvar, std::map<std::string,int> &uniqueidx)
{
	std::string result(queryvar);
	std::string idxstr("");

	if(uniqueidx.find(queryvar)==uniqueidx.end())
	{
		uniqueidx[queryvar]=0;
		idxstr="0";
	}
	else
	{
		uniqueidx[queryvar]++;
		StringFunctions::Convert(uniqueidx[queryvar],idxstr);
	}

	result.insert(result.size()-1,idxstr);
	return result;
}

const std::string IPageHandler::GenerateNavigationLinks()
{
	std::string links="<div class=\"box\">\n";
	links+="	<div class=\"header\">"+m_trans->Get("web.navlink.links")+"</div>\n";
	links+="	<ul>\n";
	links+="		<li><a href=\"index.htm\">"+m_trans->Get("web.navlink.home")+"</a></li>\n";
	links+="		<li><a href=\"options.htm\">"+m_trans->Get("web.navlink.options")+"</a></li>\n";
	links+="		<li><a href=\"createidentity.htm\">"+m_trans->Get("web.navlink.createidentity")+"</a></li>\n";
	links+="		<li><a href=\"localidentities.htm\">"+m_trans->Get("web.navlink.localidentities")+"</a></li>\n";
	links+="		<li><a href=\"announceidentity.htm\">"+m_trans->Get("web.navlink.announceidentity")+"</a></li>\n";
	links+="		<li><a href=\"addpeer.htm\">"+m_trans->Get("web.navlink.addpeer")+"</a></li>\n";
	links+="		<li><a href=\"peermaintenance.htm\">"+m_trans->Get("web.navlink.peermaintenance")+"</a></li>\n";
	links+="		<li><a href=\"peertrust.htm\">"+m_trans->Get("web.navlink.peertrust")+"</a></li>\n";
	links+="		<li><a href=\"boards.htm\">"+m_trans->Get("web.navlink.boardmaintenance")+"</a></li>\n";
	links+="		<li><a href=\"controlboard.htm\">"+m_trans->Get("web.navlink.controlboards")+"</a></li>\n";
	links+="		<li><a href=\"insertedfiles.htm\">"+m_trans->Get("web.navlink.insertedfiles")+"</a></li>\n";
	links+="		<li><a href=\"forummain.htm\">"+m_trans->Get("web.navlink.browseforums")+"</a></li>\n";
	links+="	</ul>\n";
	links+="</div>\n";

	return links;

}

const std::string IPageHandler::GeneratePage(const std::string &method, const std::map<std::string,QueryVar> &queryvars)
{
	return StringFunctions::Replace(StringFunctions::Replace(m_template,"[NAVLINKS]",GenerateNavigationLinks()),"[CONTENT]",GenerateContent(method,queryvars));
}

void IPageHandler::handleRequest(Poco::Net::HTTPServerRequest &request, Poco::Net::HTTPServerResponse &response)
{
	m_log->trace("IPageHandler::handleRequest from "+request.clientAddress().toString());

	try
	{
		std::map<std::string,QueryVar> vars;

		CreateQueryVarMap(request,vars);

		if(request.getVersion()==Poco::Net::HTTPRequest::HTTP_1_1)
		{
			response.setChunkedTransferEncoding(true);
		}
		response.setContentType("text/html");

		std::ostream &ostr = response.send();
		ostr << GeneratePage(request.getMethod(),vars);
	}
	catch(SQLite3DB::Exception &e)
	{
		m_log->fatal("IPageHandler caught SQLite3DB::Exception "+e.what());
		((FMSApp *)&FMSApp::instance())->Terminate();
	}

}

const std::string IPageHandler::SanitizeOutput(const std::string &input, const std::vector<std::string> &skipelements) const
{
	std::string output(input);
	
	for(std::vector<std::pair<std::string,std::string> >::const_iterator i=m_htmlencode.begin(); i!=m_htmlencode.end(); ++i)
	{
		if(std::find(skipelements.begin(),skipelements.end(),(*i).first)==skipelements.end())
		{
			output=StringFunctions::Replace(output,(*i).first,(*i).second);
		}
	}

	return output;
}

const std::string IPageHandler::Elide(const std::string &input, const int maxsize) const
{
	UnicodeString output(input);  // Use UnicodeString instead of std::string; prevents any multibyte characters from being cut in two (resulting in odd symbols in html).
	if(maxsize>1 && output.Size()>maxsize)
	{
		output.Trim(maxsize-1);
		output+="\342\200\246";  // Append an ellipsis character: '...' in one on-screen character.
	}
	return output.UTF8String();
}

const std::string IPageHandler::SanitizeTextAreaOutput(const std::string &input)
{
	std::vector<std::string> skip;
	skip.push_back(" ");

	return SanitizeOutput(input,skip);
}

const bool IPageHandler::ValidateFormPassword(const std::map<std::string,QueryVar> &vars)
{
	Poco::DateTime date;
	date-=Poco::Timespan(0,1,0,0,0);

	SQLite3DB::Statement st=m_db->Prepare("DELETE FROM tmpFormPassword WHERE Date<?;");
	st.Bind(0,Poco::DateTimeFormatter::format(date,"%Y-%m-%d %H:%M:%S"));
	st.Step();

	std::map<std::string,QueryVar>::const_iterator i=vars.find("formpassword");
	if(i!=vars.end())
	{
		st=m_db->Prepare("SELECT COUNT(*) FROM tmpFormPassword WHERE Password=?;");
		st.Bind(0,(*i).second.GetData());
		st.Step();
		if(st.RowReturned())
		{
			if(st.ResultNull(0)==false)
			{
				int rval=0;
				st.ResultInt(0,rval);
				if(rval>0)
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

const bool IPageHandler::WillHandleURI(const std::string &uri)
{
	if(uri.find(m_pagename)!=std::string::npos)
	{
		return true;
	}
	else
	{
		return false;
	}
}
