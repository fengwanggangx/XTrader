#include "XmlNode.h"
#include "Xml.h"
#include "rapidxml_print.hpp"
#include <sstream>
#include "XmlAttribute.h"
#include "../defines.h"

namespace xml
{
	// XmlNode方法实现
	XmlAttribute XmlNode::GetFirstAttribute(const std::string& strName) const
	{
		if (!m_pNode)
		{
			return XmlAttribute();
		}
		return XmlAttribute(xml::GetPtr(m_pNode->first_attribute(strName.c_str())));
	}

	std::vector<XmlAttribute> XmlNode::GetAttributes() const
	{
		std::vector<XmlAttribute> result;
		if (!m_pNode)
		{
			return result;
		}
		rapidxml::xml_attribute<>* pAttr = xml::GetPtr(m_pNode->first_attribute());
		while (nullptr != pAttr)
		{
			result.emplace_back(pAttr);
			pAttr = xml::GetPtr(pAttr->next_attribute());
		}

		return result;
	}

	XmlNode::XmlNode(rapidxml::xml_node<>* pNode) : m_pNode(pNode)
	{
	}

	bool XmlNode::IsValid() const
	{
		return IS_NOT_NULLPTR(m_pNode);
	}

	std::string XmlNode::GetName() const
	{
		RETURN_EMTPTY_IFNULLPTR(m_pNode);
		return xml::ToString(m_pNode->name());
	}

	std::string XmlNode::GetValue() const
	{
		RETURN_EMTPTY_IFNULLPTR(m_pNode);
		return xml::ToString(m_pNode->value());
	}

	void XmlNode::SetValue(const std::string& strVal)
	{
		RETURN_IFNULLPTR(m_pNode);

		std::string_view str = m_pNode->document()->allocate_string(strVal.c_str());
		m_pNode->value(str);
	}

	XmlNode XmlNode::GetFirstChild(const std::string& strName) const
	{
		RETURN_EMTPTY_IFNULLPTR(m_pNode);

		return XmlNode(xml::GetPtr(m_pNode->first_node(strName.c_str())));
	}

	XmlNode XmlNode::GetNextSibling(const std::string& strName) const
	{
		RETURN_EMTPTY_IFNULLPTR(m_pNode);
		return XmlNode(xml::GetPtr(m_pNode->next_sibling(strName.c_str())));
	}

	XmlNode XmlNode::GetParent() const
	{
		RETURN_EMTPTY_IFNULLPTR(m_pNode);
		RETURN_EMTPTY_IFNULLPTR(m_pNode->parent());

		return XmlNode(m_pNode->parent().get());
	}

	std::vector<XmlNode> XmlNode::GetChildren(const std::string& strName) const
	{
		RETURN_EMTPTY_IFNULLPTR(m_pNode);
		std::vector<XmlNode> ret;
		rapidxml::xml_node<>* pChild = xml::GetPtr(m_pNode->first_node(strName.c_str()));
		while (nullptr != pChild)
		{
			ret.emplace_back(pChild);
			pChild = xml::GetPtr(pChild->next_sibling(strName.c_str()));
		}

		return ret;
	}

	int XmlNode::GetChildrenCount() const
	{
		RETURN_VALUE_IFNULLPTR(m_pNode, -1);
		int nCount = 0;
		rapidxml::xml_node<>* pChild = xml::GetPtr(m_pNode->first_node());
		while (nullptr != pChild)
		{
			++nCount;
			pChild = xml::GetPtr(pChild->next_sibling());
		}
		return nCount;
	}

	// 创建子节点
	XmlNode XmlNode::CreateChild(const std::string& strName, const std::string& strValue)
	{
		RETURN_EMTPTY_IFNULLPTR(m_pNode);

		std::string_view strNodeName = m_pNode->document()->allocate_string(strName.c_str());
		rapidxml::xml_node<>* pNewNode = m_pNode->document()->allocate_node(rapidxml::node_element, strNodeName);

		if (!strValue.empty())
		{
			std::string_view strNodeValue = m_pNode->document()->allocate_string(strValue.c_str());
			pNewNode->value(strNodeValue);
		}

		m_pNode->append_node(pNewNode);
		return XmlNode(pNewNode);
	}

	// 删除子节点
	void XmlNode::RemoveChild(XmlNode& node)
	{
		RETURN_IFNULLPTR(m_pNode);
		RETURN_IFNULLPTR(node.m_pNode);

		if (node.m_pNode->parent() != m_pNode)
		{
			return;
		}
		m_pNode->remove_node(node.m_pNode);
		node.m_pNode = nullptr;
	}

	inline void XmlNode::AddAttribute(const std::string& strName, const std::string& strValue)
	{
		RETURN_IFNULLPTR(m_pNode);

		std::string_view attrName = m_pNode->document()->allocate_string(strName.c_str());
		std::string_view attrValue = m_pNode->document()->allocate_string(strValue.c_str());

		rapidxml::xml_attribute<>* pAttr = m_pNode->document()->allocate_attribute(attrName, attrValue);
		m_pNode->append_attribute(pAttr);
	}

	inline XmlAttribute XmlNode::FindAttribute(const std::string& strName) const
	{
		RETURN_EMTPTY_IFNULLPTR(m_pNode);

		rapidxml::xml_attribute<>* attr = m_pNode->first_attribute(strName.c_str()).get();
		return XmlAttribute(attr);
	}

	inline void XmlNode::RemoveAttribute(XmlAttribute& attr)
	{
		if (m_pNode && attr.m_pAttr && attr.m_pAttr->parent() == m_pNode)
		{
			m_pNode->remove_attribute(attr.m_pAttr);
			attr.m_pAttr = nullptr;
		}
	}

} // namespace xml