#include "../include/ipaddressacl.h"
#include "../include/stringfunctions.h"

#include <sstream>
#include <cmath>

IPAddressACL::IPAddressACL():m_allowbydefault(true)
{

}

const bool IPAddressACL::Add(const std::string &aclentry)
{
	bool allow=m_allowbydefault;
	int maskbits=32;
	std::string::size_type strpos=std::string::npos;
	std::string entrystr=aclentry;

	// look for +/- at beginning of string then strip off
	if(entrystr.size()>0)
	{
		if(entrystr[0]=='-')
		{
			allow=false;
			entrystr.erase(0,1);
		}
		else if(entrystr[0]=='+')
		{
			allow=true;
			entrystr.erase(0,1);
		}
	}

	// look for /mask at end of string then strip off and convert to mask
	if((strpos=entrystr.find('/'))!=std::string::npos)
	{
		if(strpos!=entrystr.size()-1)
		{
			std::string bitmaskstr=entrystr.substr(strpos+1);
			entrystr.erase(strpos);

			std::istringstream i(bitmaskstr);
			i >> maskbits;
		}
		else
		{
			entrystr.erase(strpos);
		}
	}

	// try to parse address
	Poco::Net::IPAddress ip;
	if(Poco::Net::IPAddress::tryParse(entrystr,ip))
	{
		m_entries.push_back(entry(allow,Poco::Net::IPAddress(CreateMask(maskbits)),ip));
		return true;
	}
	else
	{
		return false;
	}
}

const std::string IPAddressACL::CreateMask(const int maskbits)
{
/*
	int bitsleft=maskbits;
	int parts[4]={0,0,0,0};
	std::ostringstream ipstr;

	for(int i=0; i<4; i++)
	{
		for(int b=7; b>=0 && bitsleft>0; b--)
		{
			parts[i]+=pow((float)2,b);
			bitsleft--;
		}
	}
*/
	int bits=maskbits;
	bits>32 ? bits=32 : false;
	bits<0 ? bits=0 : false;
	int parts[4]={0,0,0,0};
	std::ostringstream ipstr;

	unsigned long maskval=(((unsigned long)pow((float)2,bits)-1) << (32-bits));

	parts[0]=((maskval >> 24) & 0xff);
	parts[1]=((maskval >> 16) & 0xff);
	parts[2]=((maskval >> 8) & 0xff);
	parts[3]=(maskval & 0xff);

	ipstr << parts[0] << "." << parts[1] << "." << parts[2] << "." << parts[3];

	return ipstr.str();
}

const bool IPAddressACL::IsAllowed(const Poco::Net::IPAddress &addr)
{
	bool found=false;
	bool rval=m_allowbydefault;

	for(std::vector<entry>::reverse_iterator i=m_entries.rbegin(); i!=m_entries.rend() && found==false; ++i)
	{
		Poco::Net::IPAddress ip1=addr;
		Poco::Net::IPAddress ip2=(*i).m_addr;

		if(ip1.family()==Poco::Net::IPAddress::IPv4 && ip2.family()==Poco::Net::IPAddress::IPv4)
		{
			ip1.mask((*i).m_mask);
			ip2.mask((*i).m_mask);
		}

		if(ip1==ip2)
		{
			found=true;
			rval=(*i).m_allow;
		}
	}

	return rval;
}

const bool IPAddressACL::IsAllowed(const std::string &addrstr)
{
	Poco::Net::IPAddress ip;
	if(Poco::Net::IPAddress::tryParse(addrstr,ip))
	{
		return IsAllowed(ip);
	}
	else
	{
		return false;
	}
}
