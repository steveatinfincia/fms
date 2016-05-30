#ifndef _ifmsxmldocument_
#define _ifmsxmldocument_

#include <Poco/DOM/Document.h>
#include <Poco/DOM/Element.h>
#include <Poco/DOM/Text.h>
#include <Poco/DOM/CDATASection.h>
#include <Poco/DOM/DOMParser.h>
#include <Poco/DOM/DOMWriter.h>
#include <Poco/XML/XMLWriter.h>
#include <Poco/AutoPtr.h>

#include "stringfunctions.h"

#include <string>

#ifdef XMEM
	#include <xmem.h>
#endif

/**
	\brief Interface for objects that represent an XML document
*/
class IFMSXMLDocument
{
public:
	
	/**
		\brief Returns xml document represented by this object
		
		\return xml document
	*/
	virtual std::string GetXML()=0;
	
	/**
		\brief Parses an xml document into this object
		
		\return true if the document was parsed successfully, false if it was not
	*/
	virtual const bool ParseXML(const std::string &xml)=0;

protected:
	/**
		Poco doesn't like CDATA with whitespace outside the tags
		This will remove the whitespace from around CDATA tags
	*/
	static const std::string FixCDATA(const std::string &xmlstr)
	{
		std::string rstring=xmlstr;
		std::string::size_type beg1=std::string::npos;
		std::string::size_type end1=rstring.find("<![CDATA[");
		std::string::size_type beg2=std::string::npos;
		std::string::size_type end2=std::string::npos;

		while(end1!=std::string::npos)
		{
			beg1=rstring.rfind('>',end1);
			if(beg1!=end1-1)
			{
				rstring.erase(beg1+1,end1-(beg1+1));
			}

			beg2=rstring.find("]]>",end1);
			if(beg2!=std::string::npos)
			{
				end2=rstring.find('<',beg2);
				if(end2!=std::string::npos)
				{
					rstring.erase(beg2+3,end2-(beg2+3));
				}
			}

			end1=rstring.find("<![CDATA[",end1+1);
		}
		return rstring;
	}
	/**
		\brief Creates and returns an element with a boolean value
	*/
	static Poco::AutoPtr<Poco::XML::Element> XMLCreateBooleanElement(Poco::AutoPtr<Poco::XML::Document> doc, const std::string &name, const bool value)
	{
		if(doc)
		{
			Poco::AutoPtr<Poco::XML::Text> txt=doc->createTextNode(value ? "true" : "false");
			Poco::AutoPtr<Poco::XML::Element> el=doc->createElement(name);
			el->appendChild(txt);
			return el;
		}
		else
		{
			return NULL;
		}
	}

	/**
		\brief Creates and returns an element with a CDATA value
	*/
	static Poco::AutoPtr<Poco::XML::Element> XMLCreateCDATAElement(Poco::AutoPtr<Poco::XML::Document> doc, const std::string &name, const std::string &data)
	{
		if(doc)
		{
			// Poco XML won't break up CDATA sections correctly when assigned a string with the 
			// end tag is present.  However, it will parse it correctly, so we will manually break the
			// CDATA into separate parts
			Poco::AutoPtr<Poco::XML::CDATASection> sec=doc->createCDATASection(StringFunctions::Replace(data,"]]>","]]]><![CDATA[]>"));
			Poco::AutoPtr<Poco::XML::Element> el=doc->createElement(name);
			el->appendChild(sec);
			return el;
		}
		else
		{
			return NULL;
		}
	}

	/**
		\brief Creates and returns a text element
	*/
	static Poco::AutoPtr<Poco::XML::Element> XMLCreateTextElement(Poco::AutoPtr<Poco::XML::Document> doc, const std::string &name, const std::string &data)
	{
		if(doc)
		{
			Poco::AutoPtr<Poco::XML::Text> txt=doc->createTextNode(data);
			Poco::AutoPtr<Poco::XML::Element> el=doc->createElement(name);
			el->appendChild(txt);
			return el;
		}
		else
		{
			return NULL;
		}
	}

	static Poco::AutoPtr<Poco::XML::Element> XMLCreateTextElement(Poco::AutoPtr<Poco::XML::Document> doc, const std::string &name, const long data)
	{
		if(doc)
		{
			std::string datastr;
			StringFunctions::Convert(data,datastr);
			return XMLCreateTextElement(doc,name,datastr);
		}
		else
		{
			return NULL;
		}
	}

	static const bool XMLGetBooleanElement(Poco::XML::Element *parent, const std::string &name)
	{
		Poco::XML::Element *el=XMLGetFirstChild(parent,name);
		if(el && el->firstChild())
		{
			if(el->firstChild()->getNodeValue()=="true")
			{
				return true;
			}
			else
			{
				return false;
			}
		}
		return false;
	}

	static Poco::XML::Element *XMLGetFirstChild(Poco::XML::Node *parent, const std::string &name)
	{
		if(parent)
		{
			Poco::XML::Node *child=parent->firstChild();
			while(child && child->nodeName()!=name)
			{
				child=child->nextSibling();
			}
			//return child.cast<Poco::XML::Element>();
			return static_cast<Poco::XML::Element *>(child);
		}
		else
		{
			return NULL;
		}
	}

	static Poco::XML::Element *XMLGetNextSibling(Poco::XML::Node *node, const std::string &name)
	{
		if(node)
		{
			Poco::XML::Node *next=node->nextSibling();
			while(next && next->nodeName()!=name)
			{
				next=next->nextSibling();
			}
			//return next.cast<Poco::XML::Element>();
			return static_cast<Poco::XML::Element *>(next);
		}
		else
		{
			return NULL;
		}
	}

	static
	const std::string SanitizeSingleString(const std::string &text)
	{
		std::string::const_iterator i;

		for(i = text.begin(); i < text.end(); ++i)
		{
		    if (*i >= 0 && *i < 32)
		    {
			break;
		    }
		}
		if(i==text.end()) // no bugus chars - return original string
		{
			return text;
		}

		std::string returntext;
		returntext.reserve(text.size()-1);

		returntext.append(text.begin(), i);
		for(++i; i < text.end(); ++i)
		{
			// skip bogus chars from text string
			if(!(*i >= 0 && *i < 32))
			{
				returntext += *i;
			}
		}
		return returntext;
	}

	static
	const std::string SanitizeMultilineString(const std::string &text)
	{
		std::string returntext=text;
		// only 0x09, 0x0a, and 0x0d control chars are allowed in XML 1.0
		for(char i=0; i<32; i++)
		{
			if(i!=0x09 && i!=0x0a && i!=0x0d)
			{
				returntext=StringFunctions::Replace(returntext,std::string(1,i),"");
			}
		}
		return returntext;
	}

	const std::string GenerateXML(Poco::AutoPtr<Poco::XML::Document> doc, const bool prettyprint=true)
	{
		std::ostringstream str;
		if(doc)
		{
			Poco::XML::DOMWriter dr;
			if(prettyprint==true)
			{
				dr.setOptions(Poco::XML::XMLWriter::WRITE_XML_DECLARATION | Poco::XML::XMLWriter::PRETTY_PRINT);
			}
			else
			{
				dr.setOptions(Poco::XML::XMLWriter::WRITE_XML_DECLARATION);
			}
			dr.setNewLine(Poco::XML::XMLWriter::NEWLINE_CRLF);
			dr.writeNode(str,doc);
		}
		return str.str();
	}
	
};

#endif	// _ifmsxmldocument_
