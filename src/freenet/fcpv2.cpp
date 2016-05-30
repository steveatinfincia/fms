#include "../../include/freenet/fcpv2.h"

#include <sstream>
#include <algorithm>
#include <cstdarg>
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
	#include <ws2tcpip.h>
#else
	#include <netdb.h>
	#include <netinet/in.h>
	#include <unistd.h>
#endif

namespace FCPv2
{

/*

	Message Methods

*/

Message::Message():m_name("")
{

}

Message::Message(const std::string &name):m_name(name)
{
	
}

Message::Message(const std::string &name, const int fieldcount, ...):m_name(name)
{
	const char *field=0;
	const char *val=0;
	va_list args;
	va_start(args,fieldcount);

	for(int i=0; i<fieldcount; i++)
	{
		field=va_arg(args,const char *);
		val=va_arg(args,const char *);

		if(field && val)
		{
			m_fields[field]=val;
		}
	}

	va_end(args);

}

const std::string Message::GetFCPString() const
{
	std::string rval=m_name;
	rval+="\r\n";
	for(std::map<std::string,std::string>::const_iterator i=m_fields.begin(); i!=m_fields.end(); ++i)
	{
		rval+=(*i).first;
		rval+="="+(*i).second;
		rval+="\n";
	}
	if(m_name=="AllData")
	{
		rval+="Data\n";
	}
	else
	{
		rval+="EndMessage\n";
	}
	return rval;
}





/*
	
	Connection Methods

*/

#ifdef _WIN32
	bool Connection::m_wsastartup=false;
#endif

Connection::Connection():m_socket(-1),m_tempbuffer(65535,0)
{
#ifdef _WIN32
	if(m_wsastartup==false)
	{
		WSAData wsadata;
		WSAStartup(MAKEWORD(2,2),&wsadata);
		m_wsastartup=true;
	}
#endif
#ifdef FCP_SSL
	memset(&m_sslctx,0,sizeof(mbedtls_ssl_context));
	memset(&m_sslconfig,0,sizeof(mbedtls_ssl_config));
	memset(&m_entropyctx,0,sizeof(mbedtls_entropy_context));
	memset(&m_ctr_drbgctx,0,sizeof(mbedtls_ctr_drbg_context));
#endif
}

Connection::Connection(const int sock):m_socket(sock),m_tempbuffer(65535,0)
{
#ifdef _WIN32
	if(m_wsastartup==false)
	{
		WSAData wsadata;
		WSAStartup(MAKEWORD(2,2),&wsadata);
		m_wsastartup=true;
	}
#endif
#ifdef FCP_SSL
	memset(&m_sslctx,0,sizeof(mbedtls_ssl_context));
	memset(&m_sslconfig,0,sizeof(mbedtls_ssl_config));
	memset(&m_entropyctx,0,sizeof(mbedtls_entropy_context));
	memset(&m_ctr_drbgctx,0,sizeof(mbedtls_ctr_drbg_context));
#endif
}

Connection::~Connection()
{
	Disconnect();
#ifdef _WIN32
	WSACleanup();
#endif
}

const bool Connection::Connect(const std::string &fcphost, const int fcpport, const bool ssl)
{

	m_sendbuffer.clear();
	m_receivebuffer.clear();
	m_isssl=ssl;

	if(IsConnected()==true)
	{
		Disconnect();
	}

	int rval=-1;
	std::ostringstream portstring;
	addrinfo hint,*result,*current;

	result=current=0;
	portstring << fcpport;
	std::memset(&hint,0,sizeof(hint));
	hint.ai_socktype=SOCK_STREAM;

	rval=getaddrinfo(fcphost.c_str(),portstring.str().c_str(),&hint,&result);

	if(result)
	{
		for(current=result; current!=0 && m_socket==-1; current=current->ai_next)
		{

			m_socket=socket(current->ai_family,current->ai_socktype,current->ai_protocol);

			if(m_socket!=-1)
			{
				rval=connect(m_socket,current->ai_addr,current->ai_addrlen);
				if(rval==-1)
				{
					Disconnect();
				}
				else
				{
#ifdef FCP_SSL
					if(m_isssl==true)
					{
						mbedtls_entropy_init(&m_entropyctx);
						mbedtls_ctr_drbg_init(&m_ctr_drbgctx);
						rval=mbedtls_ctr_drbg_seed(&m_ctr_drbgctx,mbedtls_entropy_func,&m_entropyctx,0,0);
						if(rval!=0)
						{
							Disconnect();
						}
						else
						{
							mbedtls_ssl_init(&m_sslctx);
							mbedtls_ssl_config_init(&m_sslconfig);
							rval=mbedtls_ssl_config_defaults(&m_sslconfig,MBEDTLS_SSL_IS_CLIENT,MBEDTLS_SSL_TRANSPORT_STREAM,MBEDTLS_SSL_PRESET_DEFAULT);

							if(rval!=0)
							{
								Disconnect();
							}
							else
							{
								mbedtls_ssl_conf_endpoint(&m_sslconfig,MBEDTLS_SSL_IS_CLIENT);
								mbedtls_ssl_set_hs_authmode(&m_sslctx,MBEDTLS_SSL_VERIFY_NONE);
								mbedtls_ssl_conf_rng(&m_sslconfig,mbedtls_ctr_drbg_random,&m_ctr_drbgctx);
								mbedtls_ssl_set_bio(&m_sslctx,&m_socket,mbedtls_net_send,0,mbedtls_net_recv_timeout);
								mbedtls_ssl_conf_read_timeout(&m_sslconfig,5);

								mbedtls_ssl_setup(&m_sslctx,&m_sslconfig);

								while(IsConnected() && (rval=mbedtls_ssl_handshake(&m_sslctx))!=0)
								{
									if(rval!=MBEDTLS_ERR_SSL_WANT_READ && rval!=MBEDTLS_ERR_SSL_WANT_WRITE)
									{
										Disconnect();
									}
								}

							}
						}
					}
#endif	// FCP_SSL
				}
			}

		}

		freeaddrinfo(result);
	}

	if(rval==0)
	{
		return true;
	}
	else
	{
		return false;
	}

}

const bool Connection::Disconnect()
{
	m_sendbuffer.clear();
	m_receivebuffer.clear();
	if(IsConnected())
	{
#ifdef FCP_SSL
		if(m_isssl==true)
		{
			mbedtls_ssl_close_notify(&m_sslctx);
		}
#endif
	#ifdef _WIN32
		closesocket(m_socket);
	#else
		close(m_socket);
	#endif
		m_socket=-1;
	}
#ifdef FCP_SSL
	if(m_isssl==true)
	{
		mbedtls_ssl_free(&m_sslctx);
		mbedtls_ssl_config_free(&m_sslconfig);
		mbedtls_ctr_drbg_free(&m_ctr_drbgctx);
		mbedtls_entropy_free(&m_entropyctx);
		memset(&m_sslctx,0,sizeof(mbedtls_ssl_context));
		memset(&m_sslconfig,0,sizeof(mbedtls_ssl_config));
		memset(&m_entropyctx,0,sizeof(mbedtls_entropy_context));
		memset(&m_ctr_drbgctx,0,sizeof(mbedtls_ctr_drbg_context));
	}
#endif
	return true;
}

void Connection::DoReceive()
{
	if(IsConnected())
	{
		if(m_isssl==false)
		{
			int len=recv(m_socket,&m_tempbuffer[0],m_tempbuffer.size(),0);
			if(len>0)
			{
				m_receivebuffer.insert(m_receivebuffer.end(),m_tempbuffer.begin(),m_tempbuffer.begin()+len);
			}
			else
			{
				Disconnect();
			}
		}
		else
		{
#ifdef FCP_SSL
			int len=mbedtls_ssl_read(&m_sslctx,(unsigned char *)&m_tempbuffer[0],m_tempbuffer.size());
			if(len>0)
			{
				m_receivebuffer.insert(m_receivebuffer.end(),m_tempbuffer.begin(),m_tempbuffer.begin()+len);
			}
			else if(len<0 && len!=MBEDTLS_ERR_SSL_WANT_READ && len!=MBEDTLS_ERR_SSL_WANT_WRITE)
			{
				Disconnect();
			}
#endif
		}
	}
}

void Connection::DoSend()
{
	if(IsConnected() && m_sendbuffer.size()>0)
	{
		if(m_isssl==false)
		{
			int len=send(m_socket,&m_sendbuffer[0],m_sendbuffer.size(),0);
			if(len>0)
			{
				m_sendbuffer.erase(m_sendbuffer.begin(),m_sendbuffer.begin()+len);
			}
			else
			{
				Disconnect();
			}
		}
		else
		{
#ifdef FCP_SSL
			int len=0;
			while((len=mbedtls_ssl_write(&m_sslctx,(unsigned char *)&m_sendbuffer[0],m_sendbuffer.size()))==MBEDTLS_ERR_SSL_WANT_WRITE)
			{
			}

			if(len>0)
			{
				m_sendbuffer.erase(m_sendbuffer.begin(),m_sendbuffer.begin()+len);
			}
			else if(len<0 && len!=MBEDTLS_ERR_SSL_WANT_READ && len!=MBEDTLS_ERR_SSL_WANT_WRITE)
			{
				Disconnect();
			}
#endif
		}
	}
}

const bool Connection::MessageReady() const
{
	std::vector<char>::const_iterator tempi;
	std::vector<char>::size_type temp;
	return MessageReady(tempi,temp);
}

const bool Connection::MessageReady(std::vector<char>::const_iterator &endpos, std::vector<char>::size_type &endlen) const
{
	static std::string alldatastring="AllData\n";
	static std::string datastring="\nData\n";	// need the \n at the beginning to differentiate from AllData\n
	static std::string endmessagestring="EndMessage\n";
	std::vector<char>::const_iterator tempendpos=m_receivebuffer.end();
	std::vector<char>::size_type tempendlen=0;

	if(m_receivebuffer.size()>0)
	{
		tempendpos=std::search(m_receivebuffer.begin(),m_receivebuffer.end(),alldatastring.begin(),alldatastring.end());
		if(tempendpos==m_receivebuffer.begin())
		{
			tempendpos=std::search(m_receivebuffer.begin(),m_receivebuffer.end(),datastring.begin(),datastring.end());
			if(tempendpos!=m_receivebuffer.end())
			{
				tempendpos+=1;
				tempendlen=datastring.size()-1;
			}
		}
		else
		{
			tempendpos=std::search(m_receivebuffer.begin(),m_receivebuffer.end(),endmessagestring.begin(),endmessagestring.end());
			tempendlen=endmessagestring.size();
		}

		if(tempendpos!=m_receivebuffer.end())
		{
			endpos=tempendpos;
			endlen=tempendlen;
			return true;
		}

	}

	return false;
}

const bool Connection::MessageReady(std::vector<char>::iterator &endpos, std::vector<char>::size_type &endlen)
{
	static std::string alldatastring="AllData\n";
	static std::string datastring="\nData\n";	// need the \n at the beginning to differentiate from AllData\n
	static std::string endmessagestring="EndMessage\n";
	std::vector<char>::iterator tempendpos=m_receivebuffer.end();
	std::vector<char>::size_type tempendlen=0;

	if(m_receivebuffer.size()>0)
	{
		tempendpos=std::search(m_receivebuffer.begin(),m_receivebuffer.end(),alldatastring.begin(),alldatastring.end());
		if(tempendpos==m_receivebuffer.begin())
		{
			tempendpos=std::search(m_receivebuffer.begin(),m_receivebuffer.end(),datastring.begin(),datastring.end());
			if(tempendpos!=m_receivebuffer.end())
			{
				tempendpos+=1;
				tempendlen=datastring.size()-1;
			}
		}
		else
		{
			tempendpos=std::search(m_receivebuffer.begin(),m_receivebuffer.end(),endmessagestring.begin(),endmessagestring.end());
			tempendlen=endmessagestring.size();
		}

		if(tempendpos!=m_receivebuffer.end())
		{
			endpos=tempendpos;
			endlen=tempendlen;
			return true;
		}

	}

	return false;
}

const bool Connection::Receive(Message &message)
{
	std::vector<char>::iterator endpos;
	std::vector<char>::size_type endlen;
	if(MessageReady(endpos,endlen)==true)
	{
		std::vector<std::string> fields;

		Split(std::string(m_receivebuffer.begin(),endpos),"\n",fields);
		m_receivebuffer.erase(m_receivebuffer.begin(),endpos+endlen);

		message.Clear();

		if(fields.size()>0)
		{
			message.SetName(fields[0]);
		}

		if(fields.size()>1)
		{
			for(std::vector<std::string>::iterator i=fields.begin()+1; i!=fields.end(); ++i)
			{

				std::string::size_type pos=(*i).find_first_of('=');

				if(pos!=std::string::npos)
				{
					message.GetFields()[(*i).substr(0,pos)]=(*i).substr(pos+1);
				}
				else
				{
					if((*i)!="")
					{
						message.GetFields()[(*i)]="";
					}
				}

			}
		}
		return true;
	}
	return false;
}

const bool Connection::Receive(std::vector<char> &data, const std::vector<char>::size_type len)
{
	if(m_receivebuffer.size()>=len && len>=0)
	{
		data.insert(data.end(),m_receivebuffer.begin(),m_receivebuffer.begin()+len);
		m_receivebuffer.erase(m_receivebuffer.begin(),m_receivebuffer.begin()+len);
		return true;
	}
	else
	{
		return false;
	}
}

const bool Connection::Receive(char *data, const size_t len)
{
	if(m_receivebuffer.size()>=len && len>=0)
	{
		std::copy(m_receivebuffer.begin(),m_receivebuffer.begin()+len,data);
		m_receivebuffer.erase(m_receivebuffer.begin(),m_receivebuffer.begin()+len);
		return true;
	}
	else
	{
		return false;
	}
}

const bool Connection::ReceiveIgnore(const size_t len)
{
	if(m_receivebuffer.size()>=len && len>=0)
	{
		m_receivebuffer.erase(m_receivebuffer.begin(),m_receivebuffer.begin()+len);
		return true;
	}
	else
	{
		return false;
	}
}

const bool Connection::Send(const Message &message)
{
	if(message.GetName()!="")
	{
		std::string fcpstring=message.GetFCPString();
		m_sendbuffer.insert(m_sendbuffer.end(),fcpstring.begin(),fcpstring.end());
		return true;
	}
	return false;
}

const bool Connection::Send(const std::vector<char> &data)
{
	m_sendbuffer.insert(m_sendbuffer.end(),data.begin(),data.end());
	return true;
}

const bool Connection::Send(const char *data, const size_t len)
{
	if(data)
	{
		m_sendbuffer.insert(m_sendbuffer.end(),data[0],data[0]+len);
		return true;	
	}
	return false;
}

void Connection::Split(const std::string &str, const std::string &separators, std::vector<std::string> &elements)
{
	std::string::size_type offset = 0;
	std::string::size_type delimIndex = 0;
    
	delimIndex = str.find_first_of(separators, offset);

    while (delimIndex != std::string::npos)
    {
        elements.push_back(str.substr(offset, delimIndex - offset));
        offset += delimIndex - offset + 1;
        delimIndex = str.find_first_of(separators, offset);
    }

    elements.push_back(str.substr(offset));
}

const bool Connection::Update(const unsigned long ms)
{
	if(IsConnected())
	{
		m_timeval.tv_sec=ms/1000;
		m_timeval.tv_usec=(ms%1000)*1000;

		FD_ZERO(&m_readfs);
		FD_ZERO(&m_writefs);

		FD_SET(m_socket,&m_readfs);

		if(m_sendbuffer.size()>0)
		{
			FD_SET(m_socket,&m_writefs);
		}

		select(m_socket+1,&m_readfs,&m_writefs,0,&m_timeval);

		if(FD_ISSET(m_socket,&m_readfs))
		{
			DoReceive();
		}
		if(IsConnected() && FD_ISSET(m_socket,&m_writefs))
		{
			DoSend();
		}

	}

	if(IsConnected())
	{
		return true;
	}
	else
	{
		return false;
	}

}

const bool Connection::WaitForBytes(const unsigned long ms, const size_t len)
{
	while(IsConnected() && m_receivebuffer.size()<len)
	{
		Update(ms);
	}
	
	if(IsConnected() && m_receivebuffer.size()>=len)
	{
		return true;	
	}
	else
	{
		return false;	
	}
}

}	// namespace
