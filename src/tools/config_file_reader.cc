#include "config_file_reader.h"
#include <unistd.h>

#include <fstream>
SidInfo sids[] = {
   {SID_MONITOR,"monitor"},
   { SID_CONN ,"conn"},
   { SID_DISPATCH ,"dispatch"},
   { SID_LOGIN ,"login"},
   { SID_MSG ,"msg"},
   { SID_LOADBALANCE ,"loadbalance"},
   { SID_BROADCAST ,"broadcast"},
   {SID_DBPROXY,"dbproxy"},
   { SID_BUDDY,"buddy" },
	{ SID_GROUP,"group" }
};
const char* sidName(int sid) {
	for (int i = 0; i < sizeof(sids) / sizeof(SidInfo); i++) {
		if (sids[i].sid == sid) {
			return sids[i].name;
		}
	}
	return "null";
}

int getSid(const char* name) {
	for (int i = 0; i < sizeof(sids) / sizeof(SidInfo); i++) {
		if (!strcmp(sids[i].name,name)) {
			return sids[i].sid;
		}
	}
	return -1;
}

ConfigFileReader::ConfigFileReader() {
	filename = NULL;
}

ConfigFileReader * ConfigFileReader::getInstance()
{
	static ConfigFileReader instance;
	return &instance;
}

ConfigFileReader::~ConfigFileReader() {

}

int ConfigFileReader::init(const char* file)
{
	filename = file;
	if (!doc.LoadFile(file))
	{
		fprintf(stderr, doc.ErrorDesc());
		return -1;
	}
	
	 root = doc.FirstChildElement();
	if (root == NULL)
	{
		fprintf(stderr, "Failed to load file: No root element.");
		doc.Clear();
		return -1;
	}
	return 0;
}

int ConfigFileReader::ReadInt(const char* key) {
	TiXmlElement* elem;
	if (!getNodePointerByName(root, key, elem)) {
		return 0;
	}
	TiXmlNode* e1 = elem->FirstChild();
	return atoi(e1->ToText()->Value());
}

std::string ConfigFileReader::ReadString(const char* key) {
	TiXmlElement* elem;
	if (!getNodePointerByName(root, key, elem)) {
		return "";
	}
	TiXmlNode* e1 = elem->FirstChild();
	return e1->ToText()->Value();
}


void ConfigFileReader::setConfigValue(const char* key, const char* value)
{
	TiXmlElement* element1 = new TiXmlElement(key);
	root->LinkEndChild(element1);
	TiXmlText* text = new TiXmlText(value);  ///ÎÄ±¾
	element1->LinkEndChild(text);
}

TiXmlElement * ConfigFileReader::getRoot()
{
	return root;
}

bool ConfigFileReader::getNodePointerByName(TiXmlElement* pRootEle, const char* strNodeName, TiXmlElement* &destNode)
{
	// if equal root node then return
	if (0 == strcmp(strNodeName, pRootEle->Value()))
	{
		destNode = pRootEle;
		return true;
	}

	TiXmlElement* pEle = pRootEle;
	for (pEle = pRootEle->FirstChildElement(); pEle; pEle = pEle->NextSiblingElement())
	{
		// recursive find sub node return node pointer	
		if (0 != strcmp(pEle->Value(), strNodeName))
		{
			getNodePointerByName(pEle, strNodeName, destNode);
		}
		else
		{
			destNode = pEle;
			//printf("destination node name: %s\n", pEle->Value());
			return true;
		}
	}

	return false;
}

int ConfigFileReader::save()
{
	if (doc.SaveFile("bakup.xml")) {
		unlink(filename);
		rename("bakup.xml", filename);
		return 0;
	}

	return -1;
}
