#include "syncdocument.h"

SyncDocument::~SyncDocument()
{
	clearUndoStack();
	clearRedoStack();
}

size_t SyncDocument::getTrackIndexFromPos(size_t track) const
{
	assert(track < tracks.size());
	
	sync::Data::TrackContainer::const_iterator trackIter = tracks.begin();
	for (size_t currTrack = 0; currTrack < track; ++currTrack, ++trackIter);
	
	assert(tracks.end() != trackIter);
	return trackIter->second;
}


#import <msxml4.dll> named_guids

bool SyncDocument::load(const std::string &fileName)
{
	MSXML2::IXMLDOMDocumentPtr doc(MSXML2::CLSID_DOMDocument);
	try
	{
		doc->load(fileName.c_str());
		MSXML2::IXMLDOMNodeListPtr trackNodes = doc->documentElement->selectNodes("track");
		for (int i = 0; i < trackNodes->Getlength(); ++i)
		{
			MSXML2::IXMLDOMNodePtr trackNode = trackNodes->Getitem(i);
			MSXML2::IXMLDOMNamedNodeMapPtr attribs = trackNode->Getattributes();
			
			std::string name = attribs->getNamedItem("name")->Gettext();
			sync::Track &t = getTrack(name);
			
			MSXML2::IXMLDOMNodeListPtr rowNodes = trackNode->GetchildNodes();
			for (int i = 0; i < rowNodes->Getlength(); ++i)
			{
				MSXML2::IXMLDOMNodePtr keyNode = rowNodes->Getitem(i);
				std::string baseName = keyNode->GetbaseName();
				if (baseName == "key")
				{
					MSXML2::IXMLDOMNamedNodeMapPtr rowAttribs = keyNode->Getattributes();
					std::string rowString = rowAttribs->getNamedItem("row")->Gettext();
					std::string valueString = rowAttribs->getNamedItem("value")->Gettext();
					std::string interpolationString = rowAttribs->getNamedItem("interpolation")->Gettext();
					
					sync::Track::KeyFrame keyFrame(
						float(atof(valueString.c_str())),
						sync::Track::KeyFrame::InterpolationType(
							atoi(interpolationString.c_str())
						)
					);
					t.setKeyFrame(atoi(rowString.c_str()), keyFrame);
				}
			}
		}
	}
	catch(_com_error &e)
	{
		char temp[256];
		_snprintf(temp, 256, "Error loading: %s\n", (const char*)_bstr_t(e.Description()));
		MessageBox(NULL, temp, NULL, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
		return false;
	}
	return true;
}

bool SyncDocument::save(const std::string &fileName)
{
	MSXML2::IXMLDOMDocumentPtr doc(MSXML2::CLSID_DOMDocument);
	try
	{
		_variant_t varNodeType((short)MSXML2::NODE_ELEMENT);
		MSXML2::IXMLDOMNodePtr rootNode = doc->createNode(varNodeType, _T("tracks"), _T(""));
		doc->appendChild(rootNode);
		
		sync::Data::TrackContainer::iterator iter;
		for (iter = tracks.begin(); iter != tracks.end(); ++iter)
		{
			size_t index = iter->second;
			const sync::Track &track = getTrack(index);
			
			MSXML2::IXMLDOMElementPtr trackElem = doc->createElement(_T("track"));
			trackElem->setAttribute(_T("name"), iter->first.c_str());
			rootNode->appendChild(trackElem);
			
			sync::Track::KeyFrameContainer::const_iterator it;
			for (it = track.keyFrames.begin(); it != track.keyFrames.end(); ++it)
			{
				char temp[256];
				size_t row = it->first;
				float value = it->second.value;
				char interpolationType = char(it->second.interpolationType);
				
				MSXML2::IXMLDOMElementPtr keyElem = doc->createElement(_T("key"));
				
				_snprintf(temp, 256, "%d", row);
				keyElem->setAttribute(_T("row"), temp);
				
				_snprintf(temp, 256, "%f", value);
				keyElem->setAttribute(_T("value"), temp);
				
				_snprintf(temp, 256, "%d", interpolationType);
				keyElem->setAttribute(_T("interpolation"), temp);
				
				trackElem->appendChild(keyElem);
			}
		}
		
		doc->save(fileName.c_str());
	}
	catch(_com_error &e)
	{
		char temp[256];
		_snprintf(temp, 256, "Error loading: %s\n", (const char*)_bstr_t(e.Description()));
		MessageBox(NULL, temp, NULL, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
		return false;
	}
	return true;
}

