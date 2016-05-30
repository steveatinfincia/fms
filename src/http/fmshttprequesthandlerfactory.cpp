#include "../../include/http/fmshttprequesthandlerfactory.h"
#include "../../include/option.h"
#include "../../include/stringfunctions.h"
#include "../../include/http/pages/homepage.h"
#include "../../include/http/pages/optionspage.h"
#include "../../include/http/pages/createidentitypage.h"
#include "../../include/http/pages/localidentitiespage.h"
#include "../../include/http/pages/confirmpage.h"
#include "../../include/http/pages/showcaptchapage.h"
#include "../../include/http/pages/announceidentitypage.h"
#include "../../include/http/pages/execquerypage.h"
#include "../../include/http/pages/boardspage.h"
#include "../../include/http/pages/insertedfilespage.h"
#include "../../include/http/pages/addpeerpage.h"
#include "../../include/http/pages/peerdetailspage.h"
#include "../../include/http/pages/controlboardpage.h"
#include "../../include/http/pages/peermaintenancepage.h"
#include "../../include/http/pages/peertrustpage.h"
#include "../../include/http/pages/versioninfopage.h"
#include "../../include/http/pages/recentlyaddedpage.h"
#include "../../include/http/pages/forummainpage.h"
#include "../../include/http/pages/showfilepage.h"
#include "../../include/http/pages/forumthreadspage.h"
#include "../../include/http/pages/forumviewthreadpage.h"
#include "../../include/http/pages/forumcreatepostpage.h"
#include "../../include/http/pages/forumsearchpage.h"
//ROBERT CHANGE
#include "../../include/http/pages/showpendingmessagepage.h"
#include "../../include/http/pages/translatepage.h"
#include "../../include/http/pages/showinsertedmessagepage.h"
#include "../../include/http/pages/showreceivedmessagepage.h"
#include "../../include/http/pages/showavatarpage.h"

#include <cstdio>
#include <Poco/Path.h>

FMSHTTPRequestHandlerFactory::FMSHTTPRequestHandlerFactory(SQLite3DB::DB *db):IDatabase(db)
{
	Option option(m_db);

	Poco::Path path;
	path.append(global::basepath);

	// set template
	std::string templatestr="<html><head></head><body><a href=\"home.htm\">Home</a><br><h1>Could not open template.htm!  Place in "+path.absolute().toString(Poco::Path::PATH_NATIVE)+" and restart!</h1><br>[CONTENT]</body></html>";
	FILE *infile=fopen(std::string(global::basepath+"template.htm").c_str(),"rb");
	if(infile)
	{
		fseek(infile,0,SEEK_END);
		long len=ftell(infile);
		std::vector<char> data(len,0);
		fseek(infile,0,SEEK_SET);
		fread(&data[0],1,len,infile);
		fclose(infile);
		templatestr.assign(data.begin(),data.end());
	}
	else
	{
		m_log->error("HTTPThread::HTTPThread could not open template.htm");
	}
/*
	// load forum template
	std::string forumtemplate="<html><head></head><body><a href=\"home.htm\">Home</a><br><h1>Could not open forum-template.htm!  Place in program directory and restart!</h1><br>[CONTENT]</body></html>";
	infile=fopen("forum-template.htm","rb");
	if(infile)
	{
		fseek(infile,0,SEEK_END);
		long len=ftell(infile);
		std::vector<char> data(len,0);
		fseek(infile,0,SEEK_SET);
		fread(&data[0],1,len,infile);
		fclose(infile);
		forumtemplate.assign(data.begin(),data.end());
	}
	else
	{
		m_log->error("HTTPThread::HTTPThread could not open forum-template.htm");
	}
*/

	if(m_forumtemplatehandler.LoadTemplate(global::basepath+"forum-template.htm")==false)
	{
		m_log->error("HTTPThread::HTTPThread could not open forum-template-new.htm");
	}

	// push back page handlers
	m_pagehandlers.push_back(new OptionsPage(m_db,templatestr));
	m_pagehandlers.push_back(new CreateIdentityPage(m_db,templatestr));
	m_pagehandlers.push_back(new LocalIdentitiesPage(m_db,templatestr));
	m_pagehandlers.push_back(new ConfirmPage(m_db,templatestr));
	m_pagehandlers.push_back(new ShowCaptchaPage(m_db));
	m_pagehandlers.push_back(new AnnounceIdentityPage(m_db,templatestr));
	m_pagehandlers.push_back(new ExecQueryPage(m_db,templatestr));
	m_pagehandlers.push_back(new BoardsPage(m_db,templatestr));
	m_pagehandlers.push_back(new InsertedFilesPage(m_db,templatestr));
	m_pagehandlers.push_back(new AddPeerPage(m_db,templatestr));
	m_pagehandlers.push_back(new PeerDetailsPage(m_db,templatestr));
	m_pagehandlers.push_back(new ControlBoardPage(m_db,templatestr));
	m_pagehandlers.push_back(new PeerMaintenancePage(m_db,templatestr));
	m_pagehandlers.push_back(new PeerTrustPage(m_db,templatestr));
	m_pagehandlers.push_back(new VersionInfoPage(m_db,templatestr));
	m_pagehandlers.push_back(new RecentlyAddedPage(m_db,templatestr));
	m_pagehandlers.push_back(new ShowFilePage(m_db));
	m_pagehandlers.push_back(new ShowAvatarPage(m_db));
	m_pagehandlers.push_back(new TranslatePage(m_db,templatestr));
	//m_pagehandlers.push_back(new ForumMainPage(m_db,forumtemplate));
	//m_pagehandlers.push_back(new ForumThreadsPage(m_db,forumtemplate));
	//m_pagehandlers.push_back(new ForumViewThreadPage(m_db,forumtemplate));
	//m_pagehandlers.push_back(new ForumCreatePostPage(m_db,forumtemplate));
	m_pagehandlers.push_back(new ForumTemplateMainPage(m_db,m_forumtemplatehandler));
	m_pagehandlers.push_back(new ForumTemplateThreadsPage(m_db,m_forumtemplatehandler));
	m_pagehandlers.push_back(new ForumTemplateViewThreadPage(m_db,m_forumtemplatehandler));
	m_pagehandlers.push_back(new ForumTemplateCreatePostPage(m_db,m_forumtemplatehandler));
	m_pagehandlers.push_back(new ForumSearchPage(m_db,m_forumtemplatehandler));
	//ROBERT CHANGE
	m_pagehandlers.push_back(new ShowPendingMessagePage(m_db,templatestr));
	m_pagehandlers.push_back(new ShowInsertedMessagePage(m_db,templatestr));
	m_pagehandlers.push_back(new ShowReceivedMessagePage(m_db,templatestr));
	// homepage must be last - catch all page handler
	m_pagehandlers.push_back(new HomePage(m_db,templatestr));

	// initialize the access control list
	std::string aclstr;
	std::vector<std::string> aclparts;
	option.Get("HTTPAccessControl",aclstr);
	StringFunctions::Split(aclstr,",",aclparts);
	for(std::vector<std::string>::iterator i=aclparts.begin(); i!=aclparts.end(); ++i)
	{
		m_acl.Add((*i));
	}
}

FMSHTTPRequestHandlerFactory::~FMSHTTPRequestHandlerFactory()
{

	for(std::vector<IPageHandler *>::iterator i=m_pagehandlers.begin(); i!=m_pagehandlers.end(); ++i)
	{
		delete (*i);
	}

}

class FMSRequestHandler403:public Poco::Net::HTTPRequestHandler
{
public:
	FMSRequestHandler403()	{}
	~FMSRequestHandler403()	{}
	void handleRequest(Poco::Net::HTTPServerRequest &request, Poco::Net::HTTPServerResponse &response)
	{
		response.setStatus(Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
		response.setReason("IP address not allowed access");
		response.send();
	}
};

Poco::Net::HTTPRequestHandler *FMSHTTPRequestHandlerFactory::createRequestHandler(const Poco::Net::HTTPServerRequest &request)
{
	if(m_acl.IsAllowed(request.clientAddress().host()))
	{
		for(std::vector<IPageHandler *>::iterator i=m_pagehandlers.begin(); i!=m_pagehandlers.end(); ++i)
		{
			if((*i)->WillHandleURI(request.getURI()))
			{
				// we need to return a new object because the HTTPServer will destory it when it's done.
				return (*i)->New();
			}
		}
	}
	else
	{
		m_log->debug("FMSHTTPRequestHandlerFactory::createRequestHandler host denied access "+request.clientAddress().host().toString());
		return new FMSRequestHandler403();
	}
	return 0;
}
