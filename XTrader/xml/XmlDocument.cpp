#include "XmlDocument.h"
#include "rapidxml.hpp"
#include "rapidxml_print.hpp"
#include <fstream>
#include <sstream>
#include "Xml.h"
#include "../defines.h"

namespace xml
{
	XmlDocument::XmlDocument() : m_document(std::make_unique<rapidxml::xml_document<>>())
	{
	}

	bool XmlDocument::Parse(const std::string& xml)
	{
		try
		{
			m_xmlData = std::make_unique<char[]>(xml.size() + 1);
			strcpy_s(m_xmlData.get(), xml.size() + 1, xml.c_str());
			m_document->parse<0>(m_xmlData.get());
			return true;
		}
		catch (const rapidxml::parse_error& e)
		{
			return false;
		}
	}

	bool XmlDocument::LoadFromFile(const std::string& filename)
	{
		try
		{
			std::ifstream file(filename.c_str());
			if (!file.is_open())
			{
				return false;
			}

			std::stringstream buffer;
			buffer << file.rdbuf();
			return Parse(buffer.str());
		}
		catch (const std::exception& e)
		{
			return false;
		}
	}

	bool XmlDocument::SaveToFile(const std::string& filename, bool formatted) const
	{
		try
		{
			std::ofstream file(filename);
			if (!file.is_open())
			{
				return false;
			}

			if (formatted)
			{
				// file << rapidxml::pretty_print(m_document.get());
			}
			else
			{
				file << *m_document;
			}

			return true;
		}
		catch (const std::exception&)
		{
			return false;
		}
	}

	std::string XmlDocument::ToString(bool formatted) const
	{
		std::stringstream ss;
		if (formatted)
		{
			// ss << rapidxml::pretty_print(m_document.get());
		}
		else
		{
			ss << *m_document;
		}
		return ss.str();
	}

	XmlNode XmlDocument::CreateRootNode(const std::string& name)
	{
		RETURN_EMTPTY_IFNULLPTR(m_document);

		Clear();
		std::string_view nodeName = m_document->allocate_string(name.c_str());
		rapidxml::xml_node<>* root = m_document->allocate_node(rapidxml::node_element, nodeName);
		m_document->append_node(root);
		return XmlNode(root);
	}

	XmlNode XmlDocument::GetRoot() const
	{
		RETURN_EMTPTY_IFNULLPTR(m_document);

		return XmlNode(xml::GetPtr(m_document->first_node()));
	}

	void XmlDocument::Clear()
	{
		RETURN_IFNULLPTR(m_document);

		m_document->clear();
		m_xmlData.reset();
	}

} // namespace xml