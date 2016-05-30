#include "../include/fmsapp.h"
#include "../include/global.h"
#include "../include/dbsetup.h"
#include "../include/optionssetup.h"
#include "../include/option.h"
#include "../include/translationsetup.h"
#include "../include/stringfunctions.h"
#include "../include/http/httpthread.h"
#include "../include/nntp/nntplistener.h"
#include "../include/dbmaintenancethread.h"
#include "../include/freenet/freenetmasterthread.h"
#include "../include/threadwrapper/threadedexecutor.h"
#include "../include/db/sqlite3db.h"

#include <Poco/Util/HelpFormatter.h>
#include <Poco/FileChannel.h>
#include <Poco/ConsoleChannel.h>
#include <Poco/FormattingChannel.h>
#include <Poco/PatternFormatter.h>
#include <Poco/RegularExpression.h>
#include <iostream>
#include <string>
#include <cstring>

#ifdef _WIN32
	#include <Poco/Util/WinService.h>
	#include <direct.h>
#else
	#include <unistd.h>
#endif

#if defined(FROST_SUPPORT) || defined(FCP_SSL)
	#include <mbedtls/version.h>
#endif

FMSApp::FMSApp():m_displayhelp(false),m_showoptions(false),m_setoption(false),m_dontstartup(false),m_logtype("file"),m_workingdirectory(""),m_lockfile(global::basepath+"fms.lck")
{
	// get current working dir so we can go to it later
	char wd[1024];
	char *wdptr=NULL;
	memset(wd,0,1024);
	wdptr=getcwd(wd,1023);
	if(wdptr)
	{
		m_workingdirectory=wdptr;
	}
}

void FMSApp::defineOptions(Poco::Util::OptionSet &options)
{
	ServerApplication::defineOptions(options);

	// add command line options here
	options.addOption(Poco::Util::Option("help","?","Display help for command line arguments.",false).repeatable(false).callback(Poco::Util::OptionCallback<FMSApp>(this,&FMSApp::handleHelp)));
	options.addOption(Poco::Util::Option("log","l","Select type of log output (file|stdout|stderr).",false,"type",true).repeatable(false).callback(Poco::Util::OptionCallback<FMSApp>(this,&FMSApp::handleLogOption)));
	options.addOption(Poco::Util::Option("showoptions","","Show all options that can be set and their current values.",false).repeatable(false).callback(Poco::Util::OptionCallback<FMSApp>(this,&FMSApp::handleShowOptions)));
	options.addOption(Poco::Util::Option("setoption","","Set an option.  Values are not validated, so be sure to set them correctly.",false,"option=value",true).repeatable(true).callback(Poco::Util::OptionCallback<FMSApp>(this,&FMSApp::handleSetOption)));
#ifdef _WIN32	
	options.addOption(Poco::Util::Option("servicestart","ss","Sets service startup to manual or automatic",false,"type",true).repeatable(false).callback(Poco::Util::OptionCallback<FMSApp>(this,&FMSApp::handleServiceStart)));
#endif
	options.addOption(Poco::Util::Option("version","v","Displays FMS version",false).repeatable(false).callback(Poco::Util::OptionCallback<FMSApp>(this,&FMSApp::handleVersion)));
}

void FMSApp::displayHelp()
{
	Poco::Util::HelpFormatter helpFormatter(options());
	helpFormatter.setCommand(commandName());
	helpFormatter.setUsage("OPTIONS");
	helpFormatter.setHeader("The Freenet Message System.");
	helpFormatter.format(std::cout);
}

void FMSApp::displayVersion()
{
	std::cout << FMS_VERSION;
}

void FMSApp::handleHelp(const std::string &name, const std::string &value)
{
	m_displayhelp=true;
	displayHelp();
	stopOptionsProcessing();
}

void FMSApp::handleLogOption(const std::string &name, const std::string &value)
{
	if(value=="file" || value=="stdout" || value=="stderr")
	{
		m_logtype=value;
	}
}

#ifdef _WIN32
void FMSApp::handleServiceStart(const std::string &name, const std::string &value)
{
	m_dontstartup=true;
	std::string servicename = config().getString("application.baseName");
	try
	{
		Poco::Util::WinService service(servicename);
		if(service.isRegistered())
		{
			if(value=="manual")
			{
				service.setStartup(Poco::Util::WinService::SVC_MANUAL_START);
			}
			else if(value=="automatic")
			{
				service.setStartup(Poco::Util::WinService::SVC_AUTO_START);
			}
			else
			{
				std::cout << "Unknown start type " << value << "." << std::endl;
				std::cout << "Service start type must be manual or automatic." << std::endl;
			}
		}
		else
		{
			std::cout << "The service is not registered.  You must register the service first." << std::endl;
		}
	}
	catch(Poco::Exception &e)
	{
		std::cout << "Caught exception " << e.what() << std::endl;
	}
	catch(...)
	{
		std::cout << "Caught unknown exception" << std::endl;
	}
}
#endif

void FMSApp::handleSetOption(const std::string &name, const std::string &value)
{
	std::vector<std::string> valueparts;
	StringFunctions::Split(value,"=",valueparts);

	if(valueparts.size()==2)
	{
		m_setoptions[valueparts[0]]=valueparts[1];
	}
	else
	{
		std::cout << "Expected option=value but found " << value << std::endl;
	}

	m_setoption=true;
}

void FMSApp::handleShowOptions(const std::string &name, const std::string &value)
{
	m_showoptions=true;
}

void FMSApp::handleVersion(const std::string &name, const std::string &value)
{
	m_dontstartup=true;
	displayVersion();
	stopOptionsProcessing();
}

void FMSApp::initialize(Poco::Util::Application &self)
{
	ServerApplication::initialize(self);

	// set working directory - fall back on application.dir if working directory isn't set
	// if we are runing as a service, then working directory needs to be set to the application directory
	if(m_workingdirectory=="" || config().getBool("application.runAsService",false)==true)
	{
		m_workingdirectory=config().getString("application.dir");
	}
	int rval=chdir(m_workingdirectory.c_str());

#ifdef QUERY_LOG
	{
		Poco::AutoPtr<Poco::FormattingChannel> formatter=new Poco::FormattingChannel(new Poco::PatternFormatter("%Y-%m-%d %H:%M:%S | %t"));
		Poco::AutoPtr<Poco::FileChannel> fc=new Poco::FileChannel(global::basepath+"query.log");
		fc->setProperty("rotation","daily");
		fc->setProperty("times","utc");
		fc->setProperty("archive","timestamp");
		fc->setProperty("purgeCount","5");
		fc->setProperty("compress","true");
		formatter->setChannel(fc);
		Poco::Logger::create("querylog",formatter,Poco::Message::PRIO_INFORMATION);
	}
#endif

	LoadDatabase(0);
	SetupDB(m_db);
	SetupDefaultOptions(m_db);
	initializeLogger();
	config().setString("application.logger","logfile");
}

void FMSApp::initializeLogger()
{
	int initiallevel=Poco::Message::PRIO_TRACE;

	std::string tempval="";
	Option option(m_db);

	if(option.Get("LogLevel",tempval))
	{
		StringFunctions::Convert(tempval,initiallevel);
	}

	Poco::AutoPtr<Poco::PatternFormatter> pformatter=new Poco::PatternFormatter("%Y-%m-%d %H:%M:%S | %p | %t");
	Poco::AutoPtr<Poco::FormattingChannel> formatter=new Poco::FormattingChannel(pformatter);
	
	if(m_logtype=="file")
	{
		try
		{
			Poco::AutoPtr<Poco::FileChannel> fc=new Poco::FileChannel(global::basepath+"fms.log");
			fc->setProperty("rotation","daily");	// rotate log file daily
			fc->setProperty("times","utc");			// utc date/times for log entries
			fc->setProperty("archive","timestamp");	// add timestamp to old logs
			fc->setProperty("purgeCount","30");		// purge old logs after 30 logs have accumulated
			fc->setProperty("compress","true");		// gz compress old log files
			formatter->setChannel(fc);
		}
		catch(...)
		{
		}
	}
	else
	{
		if(m_logtype=="stdout")
		{
			Poco::AutoPtr<Poco::ConsoleChannel> cc=new Poco::ConsoleChannel(std::cout);
			formatter->setChannel(cc);
		}
		else
		{
			Poco::AutoPtr<Poco::ConsoleChannel> cc=new Poco::ConsoleChannel(std::cerr);
			formatter->setChannel(cc);
		}
	}
	
	try
	{
		setLogger(Poco::Logger::create("logfile",formatter,Poco::Message::PRIO_INFORMATION));
		Poco::Logger::get("logfile").information("LogLevel set to "+tempval);
		Poco::Logger::get("logfile").setLevel(initiallevel);
	}
	catch(...)
	{
	}

}

int FMSApp::main(const std::vector<std::string> &args)
{

	// running as a daemon would reset the working directory to / before calling main
	// so we need to set the working directory again
	int rval=chdir(m_workingdirectory.c_str());

	if(m_lockfile.TryLock()==false)
	{
		std::cout << "Another instance of FMS appears to be running in this directory.  If you are sure FMS is not already running here, remove the fms.lck file and start FMS again." << std::endl;
		return FMSApp::EXIT_OK;
	}

	if(VerifyDB(m_db)==false)
	{
		std::cout << "The FMS database failed verification.  It is most likely corrupt!" << std::endl;
		logger().fatal("The FMS database failed verification.  It is most likely corrupt!");
	}
	else if(m_displayhelp)
	{
	}
	else if(m_showoptions)
	{
		showOptions();
	}
	else if(m_setoption)
	{
		setOptions();
	}
	else if(m_dontstartup)
	{
	}
	else
	{
		do
		{
			global::shutdown=false;
			global::restart=false;

			std::string tempval("");
			Option option(m_db);
			option.ClearCache();

			option.Get("LogLevel",tempval);
			if(tempval!="")
			{
				int loglevel=0;
				StringFunctions::Convert(tempval,loglevel);
				logger().setLevel(loglevel);
			}

			logger().information("FMS startup v" FMS_VERSION);
			logger().information("Linked with SQLite " SQLITE_VERSION);
	#ifdef FROST_SUPPORT
			logger().information("Compiled with Frost support");
	#endif
	#ifdef FCP_SSL
			logger().information("Compiled with FCP SSL support");
	#endif
	#if defined(FROST_SUPPORT) || defined(FCP_SSL)
			logger().information("Linked with " MBEDTLS_VERSION_STRING_FULL);
	#endif

			if(testRegEx()==false)
			{
				logger().fatal("The pcre library linked with Poco isn't working correctly.  Downgrade to pcre 8.13 or recompile Poco using its own bundled pcre.");
				if(isInteractive())
				{
					std::cout << "The pcre library linked with Poco isn't working correctly.  Downgrade to pcre 8.13 or recompile Poco using its own bundled pcre." << std::endl;
				}
				return FMSApp::EXIT_SOFTWARE;
			}

			option.Get("VacuumOnStartup",tempval);
			if(tempval=="true")
			{
				logger().information("VACUUMing database");
				m_db->Execute("VACUUM;");
			}

			bool profile=false;
			option.GetBool("ProfileDBQueries",profile);
			if(profile)
			{
				logger().information("Profiling DB Queries");
				m_db->StartProfiling();
			}

			option.Get("Language",tempval);
			SetupTranslation(tempval);

			StartThreads();

			if(isInteractive()==true)
			{
				std::cout << "FMS has been started." << std::endl << std::endl;
			}
			logger().information("Startup complete");

	#ifndef JNI_SUPPORT
			waitForTerminationRequest();
	#else
			do
			{
				Poco::Thread::sleep(1000);
			}while(global::shutdown==false);
	#endif

			if(isInteractive()==true)
			{
				std::cout << "Please wait while FMS shuts down." << std::endl << std::endl;
			}

			logger().trace("FMSApp::main cancelling threads");
			m_threads.Cancel();
			logger().trace("FMSApp::main joining threads");
			m_threads.Join();

			logger().information("FMS shutdown");
		}while(global::restart==true);
	}

	return FMSApp::EXIT_OK;
}

void FMSApp::setOptions()
{
	for(std::map<std::string,std::string>::iterator i=m_setoptions.begin(); i!=m_setoptions.end(); ++i)
	{
		std::string tempval("");
		Option option(m_db);
		if(option.Get((*i).first,tempval))
		{
			option.Set((*i).first,(*i).second);
			std::cout << "Option " << (*i).first << " set to " << (*i).second << std::endl;
		}
		else
		{
			std::cout << "Unknown option " << (*i).first << std::endl;
		}
	}
}

void FMSApp::showOptions()
{
	SQLite3DB::Statement st=m_db->Prepare("SELECT Option, OptionValue FROM tblOption;");
	st.Step();
	while(st.RowReturned())
	{
		std::string option="";
		std::string optionvalue="";
		st.ResultText(0,option);
		st.ResultText(1,optionvalue);

		std::cout << option << " = " << optionvalue << std::endl;

		st.Step();
	}
}

void FMSApp::StartThreads()
{
	int threadid=0;
	std::string tempval("");
	Option option(m_db);

	// always start the DB maintenance thread
	logger().trace("FMSApp::StartThreads starting DBMaintenanceThread");
	threadid=m_threads.Start(new DBMaintenanceThread(),"DBMaintenance");
	if(isInteractive())
	{
		std::cout << "Started DB Maintenance Thread : " << threadid << std::endl;
	}

	option.Get("StartHTTP",tempval);
	if(tempval=="true")
	{
		logger().trace("FMSApp::StartThreads starting HTTPThread");
		threadid=m_threads.Start(new HTTPThread(),"HTTP");
		if(isInteractive())
		{
			std::cout << "Started HTTP Thread : " << threadid << std::endl;
		}
	}
	else
	{
		logger().trace("FMSApp::StartThreads not starting HTTPThread");
	}

	tempval="";
	option.Get("StartNNTP",tempval);
	if(tempval=="true")
	{
		logger().trace("FMSApp::StartThreads starting NNTPListener");
		threadid=m_threads.Start(new NNTPListener(),"NNTPListener");
		if(isInteractive())
		{
			std::cout << "Started NNTP Thread : " << threadid << std::endl;
		}
	}
	else
	{
		logger().trace("FMSApp::StartThreads not starting NNTPListener");
	}

	tempval="";
	option.Get("StartFreenetUpdater",tempval);
	if(tempval=="true")
	{
		logger().trace("FMSApp::StartThreads starting FreenetMasterThread");
		threadid=m_threads.Start(new FreenetMasterThread(),"Freenet");
		if(isInteractive())
		{
			std::cout << "Started Freenet FCP Thread : " << threadid << std::endl;
		}
	}
	else
	{
		logger().trace("FMSApp::StartThreads not starting FreenetMasterThread");
	}

}

const bool FMSApp::testRegEx()
{
	// taken from Poco test suite for RegEx
	
	{
		Poco::RegularExpression re("[0-9]+");
		Poco::RegularExpression::Match match;
		if(re.match("", 0, match) != 0) return false;
		if(re.match("123", 3, match) != 0) return false;
	}

	{
		Poco::RegularExpression re("[0-9]+");
		if(!re.match("123")) return false;
		if(re.match("123cd")) return false;
		if(re.match("abcde")) return false;
		if(!re.match("ab123", 2)) return false;
	}

	{
		Poco::RegularExpression re("[0-9]+");
		Poco::RegularExpression::Match match;
		if(re.match("123", 0, match) != 1) return false;
		if(match.offset != 0) return false;
		if(match.length != 3) return false;

		if(re.match("abc123def", 0, match) != 1) return false;
		if(match.offset != 3) return false;
		if(match.length != 3) return false;

		if(re.match("abcdef", 0, match) != 0) return false;
		if(match.offset != std::string::npos) return false;
		if(match.length != 0) return false;

		if(re.match("abc123def", 3, match) != 1) return false;
		if(match.offset != 3) return false;
		if(match.length != 3) return false;
	}

	{
		Poco::RegularExpression re("[0-9]+");
		Poco::RegularExpression::MatchVec match;
		if(re.match("123", 0, match) != 1) return false;
		if(match.size() != 1) return false;
		if(match[0].offset != 0) return false;
		if(match[0].length != 3) return false;

		if(re.match("abc123def", 0, match) != 1) return false;
		if(match.size() != 1) return false;
		if(match[0].offset != 3) return false;
		if(match[0].length != 3) return false;

		if(re.match("abcdef", 0, match) != 0) return false;
		if(match.size() != 0) return false;

		if(re.match("abc123def", 3, match) != 1) return false;
		if(match.size() != 1) return false;
		if(match[0].offset != 3) return false;
		if(match[0].length != 3) return false;
	}

	{
		Poco::RegularExpression re("([0-9]+) ([0-9]+)");
		Poco::RegularExpression::MatchVec matches;
		if(re.match("123 456", 0, matches) != 3) return false;
		if(matches.size() != 3) return false;
		if(matches[0].offset != 0) return false;
		if(matches[0].length != 7) return false;
		if(matches[1].offset != 0) return false;
		if(matches[1].length != 3) return false;
		if(matches[2].offset != 4) return false;
		if(matches[2].length != 3) return false;

		if(re.match("abc123 456def", 0, matches) != 3) return false;
		if(matches.size() != 3) return false;
		if(matches[0].offset != 3) return false;
		if(matches[0].length != 7) return false;
		if(matches[1].offset != 3) return false;
		if(matches[1].length != 3) return false;
		if(matches[2].offset != 7) return false;
		if(matches[2].length != 3) return false;
	}

	{
		std::string digits = "0123";
		if(!Poco::RegularExpression::match(digits, "[0-9]+")) return false;
		std::string alphas = "abcd";
		if(Poco::RegularExpression::match(alphas, "[0-9]+")) return false;
	}

	{
		Poco::RegularExpression expr("^([a-z]*)?$");
		if(!expr.match("", 0, 0)) return false;
		if(!expr.match("abcde", 0, 0)) return false;
		if(expr.match("123", 0, 0)) return false;
	}

	return true;
}
