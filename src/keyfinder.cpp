#include "../include/keyfinder.h"
#include "../include/stringfunctions.h"

KeyFinderItemText::KeyFinderItemText():KeyFinderItem(TYPE_TEXT),m_text("")
{

}

KeyFinderItemText::KeyFinderItemText(const std::string &text):KeyFinderItem(TYPE_TEXT),m_text(text)
{

}

void KeyFinderItemText::Accept(KeyFinderVisitor &visitor)
{
	visitor.Visit(*this);
}

KeyFinderItemKey::KeyFinderItemKey():KeyFinderItem(TYPE_KEY),m_keypart(""),m_filepart("")
{

}

KeyFinderItemKey::KeyFinderItemKey(const std::string &keypart, const std::string &filepart):KeyFinderItem(TYPE_KEY),m_keypart(keypart),m_filepart(filepart)
{

}

void KeyFinderItemKey::Accept(KeyFinderVisitor &visitor)
{
	visitor.Visit(*this);
}

void KeyFinderHTMLRenderVisitor::Visit(KeyFinderItem &item)
{
	if(item.GetItemType()==KeyFinderItem::TYPE_TEXT)
	{
		std::string output("");
		const KeyFinderItemText *textitem=dynamic_cast<const KeyFinderItemText *>(&item);
		output=textitem->GetText();

		output=StringFunctions::Replace(output,"\r\n","\n");
		output=StringFunctions::Replace(output,"&","&amp;");
		output=StringFunctions::Replace(output,"<","&lt;");
		output=StringFunctions::Replace(output,">","&gt;");
		output=StringFunctions::Replace(output,"[","&#91;");
		output=StringFunctions::Replace(output,"]","&#93;");

		if(m_showsmilies==true)
		{
			output=m_emot->Replace(output);
		}

		output=StringFunctions::Replace(output,"\n","<br />");

		m_rendered+=output;
	}
	else if(item.GetItemType()==KeyFinderItem::TYPE_KEY)
	{
		const KeyFinderItemKey *keyitem=dynamic_cast<const KeyFinderItemKey *>(&item);
		m_rendered+="<a href=\""+m_fproxyprotocol+"://"+m_fproxyhost+":"+m_fproxyport+"/"+keyitem->GetKeyPart()+StringFunctions::UriEncode(keyitem->GetFilePart())+"\">";
		if(keyitem->GetFilePart()=="")
		{
			m_rendered+=keyitem->GetKeyPart();
		}
		m_rendered+=keyitem->GetFilePart();
		m_rendered+="</a>";
	}
}

KeyFinderParser::KeyFinderParser():
m_keyre("(http(s)?://[\\S]+?/)?(freenet:)?(((CHK@|SSK@){1}([0-9A-Za-z-~]{43},){2}([0-9A-Za-z-~]){7})|(USK@([0-9A-Za-z-~]{43},){2}([0-9A-Za-z-~]){7}/[\\S]+?/-?\\d+/?))"),
m_protocolre("(http(s)?://[\\S]+?/)?"),
m_filenamere("/([\\S ]*?\\.[\\S]{3})|/([\\S]*)")
{

}

std::vector<KeyFinderItem *> KeyFinderParser::ParseMessage(const std::string &message)
{
	std::vector<std::string::size_type> replacedchars;
	std::string workmessage(message);
	std::vector<KeyFinderItem *> items;
	bool prevnewline=false;
	bool prevblock=false;
	Poco::RegularExpression::Match keymatch;
	Poco::RegularExpression::Match protocolmatch;
	Poco::RegularExpression::Match filematch;
	std::vector<Poco::RegularExpression::Match> keymatches;
	std::string::size_type replacedoffset=0;
	bool insidekey=false;
	std::string::size_type currentpos=0;

	// find all characters we are going to ignore - (newlines and the start of block quotes)
	for(std::string::size_type pos=0; pos<message.size(); pos++)
	{
		// single /r or /r/n or /n is ok, but we will skip double /r/r or /n/r or /n/n
		if(message[pos]=='\r' && (pos==0 || (message[pos-1]!='\r' && message[pos-1]!='\n')))
		{
			replacedchars.push_back(pos);
			prevnewline=true;
			prevblock=false;
		}
		else if(message[pos]=='\n' && (pos==0 || (message[pos-1]!='\n')))
		{
			replacedchars.push_back(pos);
			prevnewline=true;
			prevblock=false;
		}
		else if((prevnewline==true || prevblock==true) && ((pos+3<message.size()&& message.substr(pos,4)=="&gt;") || message[pos]=='>'))
		{
			replacedchars.push_back(pos);
			prevnewline=false;
			prevblock=true;
		}
		else if(prevblock==true && message[pos]==' ')
		{
			replacedchars.push_back(pos);
			prevnewline=false;
			prevblock=false;
		}
		else
		{
			prevnewline=false;
			prevblock=false;
		}
	}

	// erase all the ignored chars from the message we will be searching
	for(std::vector<std::string::size_type>::reverse_iterator i=replacedchars.rbegin(); i!=replacedchars.rend(); ++i)
	{
		workmessage.erase((*i),1);
	}

	// find every key that matches
	m_keyre.match(workmessage,keymatch);
	while(keymatch.offset!=std::string::npos)
	{
		m_filenamere.match(workmessage,keymatch.offset+keymatch.length,filematch);
		if(filematch.offset!=std::string::npos && filematch.offset==keymatch.offset+keymatch.length)
		{
			// find the next new line position after the file name
			// we will adjust the length of the filename to continue until the newline if it does not already
			std::string::size_type nextnewlinepos=workmessage.size();
			for(std::string::size_type i=0; i<replacedchars.size(); i++)
			{
				if((replacedchars[i]-i)>filematch.offset)
				{
					nextnewlinepos=replacedchars[i]-i;
					i=replacedchars.size();
				}
			}
			// adjust length of filename to continue until the next newline
			if(nextnewlinepos!=std::string::npos && nextnewlinepos>(filematch.offset+filematch.length))
			{
				filematch.length=nextnewlinepos-filematch.offset;
			}

			// trim off trailing \r\n or \t
			while(filematch.length>0 && (workmessage[keymatch.offset+keymatch.length+filematch.length-1]=='\r' || workmessage[keymatch.offset+keymatch.length+filematch.length-1]=='\n' || workmessage[keymatch.offset+keymatch.length+filematch.length-1]=='\t'))
			{
				filematch.length--;
			}

			keymatch.length+=filematch.length;
		}
		keymatches.push_back(keymatch);
		m_keyre.match(workmessage,keymatch.offset+keymatch.length,keymatch);
	}

	// put back replaced chars, but not if it is inside a key
	// also adjust positions of keys based on the inserted characters
	for(std::vector<std::string::size_type>::iterator i=replacedchars.begin(); i!=replacedchars.end(); ++i)
	{
		insidekey=false;
		for(std::vector<Poco::RegularExpression::Match>::iterator mi=keymatches.begin(); mi!=keymatches.end(); ++mi)
		{
			if(((*i)-replacedoffset)<=(*mi).offset && insidekey==false)
			{
				// we're going to insert 1 char - so move the key offset 1 position
				(*mi).offset++;
			}
			else if(((*i)-replacedoffset)>(*mi).offset && ((*i)-replacedoffset)<(*mi).offset+(*mi).length)
			{
				insidekey=true;
				replacedoffset++;
			}
		}
		if(insidekey==false)
		{
			workmessage.insert((*i)-replacedoffset,message.substr((*i),1));
		}
	}

	// add all text and key items to items vector while trying to find filename attached to keys
	currentpos=0;
	for(std::vector<Poco::RegularExpression::Match>::iterator mi=keymatches.begin(); mi!=keymatches.end(); ++mi)
	{
		if(currentpos<(*mi).offset)
		{
			items.push_back(new KeyFinderItemText(workmessage.substr(currentpos,(*mi).offset-currentpos)));
		}

		currentpos=(*mi).offset+(*mi).length;

		std::string keypart("");
		std::string filepart("");
		std::string wholekey(workmessage.substr((*mi).offset,(*mi).length));

		m_protocolre.match(wholekey,protocolmatch);
		if(protocolmatch.offset!=std::string::npos)
		{
			wholekey.erase(0,protocolmatch.length);
			(*mi).offset+=protocolmatch.length;
			(*mi).length-=protocolmatch.length;
		}

		std::string::size_type slashpos=wholekey.find('/');
		if(slashpos!=std::string::npos)
		{
			keypart=workmessage.substr((*mi).offset,slashpos+1);
			filepart=workmessage.substr((*mi).offset+slashpos+1,((*mi).length-slashpos)-1);
		}
		else
		{
			keypart=workmessage.substr((*mi).offset,(*mi).length);
		}

		items.push_back(new KeyFinderItemKey(keypart,filepart));

	}

	// push last text part
	if(currentpos<workmessage.size())
	{
		items.push_back(new KeyFinderItemText(workmessage.substr(currentpos)));
	}

	return items;
}

void KeyFinderParser::Cleanup(std::vector<KeyFinderItem *> &items)
{
	for(std::vector<KeyFinderItem *>::const_iterator i=items.begin(); i!=items.end(); ++i)
	{
		delete (*i);
	}
	items.clear();
}

std::string KeyFinderHTMLRenderer::Render(const std::string &message, const std::string &fproxyprotocol, const std::string &fproxyhost, const std::string &fproxyport, const bool showsmilies, EmoticonReplacer *emot)
{
	KeyFinderHTMLRenderVisitor rv;
	std::vector<KeyFinderItem *> items=m_parser.ParseMessage(message);
	rv.SetFProxyHost(fproxyhost);
	rv.SetFProxyPort(fproxyport);
	rv.SetFProxyProtocol(fproxyprotocol);
	rv.SetShowSmilies(showsmilies);
	rv.SetEmoticonReplacer(emot);

	for(std::vector<KeyFinderItem *>::const_iterator i=items.begin(); i!=items.end(); ++i)
	{
		rv.Visit(*(*i));
	}

	m_parser.Cleanup(items);
	return rv.Rendered();
}
