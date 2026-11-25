#include "XmlAttribute.h"
#include "Xml.h"
#include "../defines.h"

namespace xml
{

	XmlAttribute::XmlAttribute(rapidxml::xml_attribute<>* pAttr) : m_pAttr(pAttr)
	{
	}

	bool XmlAttribute::IsValid() const
	{
		return IS_NOT_NULLPTR(m_pAttr);
	}

	std::string XmlAttribute::GetName() const
	{
		RETURN_EMTPTY_IFNULLPTR(m_pAttr);

		return xml::ToString(m_pAttr->name());
	}

	std::string XmlAttribute::GetValue() const
	{
		RETURN_EMTPTY_IFNULLPTR(m_pAttr);

		return xml::ToString(m_pAttr->value());
	}

	void XmlAttribute::SetValue(const std::string& strValue)
	{
		RETURN_IFNULLPTR(m_pAttr);

		std::string_view str = m_pAttr->document()->allocate_string(strValue.c_str());
		m_pAttr->value(str);
	}

} // namespace xml