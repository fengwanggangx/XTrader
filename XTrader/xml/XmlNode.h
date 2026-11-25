#ifndef __XMLNODE_H__
#define __XMLNODE_H__

#include <rapidxml.hpp>

namespace xml
{
	class XmlDocument;
	class XmlAttribute;
	class XmlNode
	{
	public:
		XmlNode(rapidxml::xml_node<>* pNode = nullptr);

		bool IsValid() const;
		std::string GetName() const;

		std::string GetValue() const;
		void SetValue(const std::string& strVal);

		XmlNode GetFirstChild(const std::string& strName = "") const;
		XmlNode GetNextSibling(const std::string& strName = "") const;

		XmlNode GetParent() const;
		std::vector<XmlNode> GetChildren(const std::string& strName = "") const;
		int GetChildrenCount() const;

		XmlNode CreateChild(const std::string& strName, const std::string& value = "");
		void RemoveChild(XmlNode& node);

		XmlAttribute GetFirstAttribute(const std::string& strName = "") const;
		std::vector<XmlAttribute> GetAttributes() const;

		void AddAttribute(const std::string& strName, const std::string& strValue);
		XmlAttribute FindAttribute(const std::string& strName) const;
		void RemoveAttribute(XmlAttribute& attr);

	private:
		rapidxml::xml_node<>* m_pNode{ nullptr };
		friend class XmlDocument;
	};

} // namespace xml

#endif
