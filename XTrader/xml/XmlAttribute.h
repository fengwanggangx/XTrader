#ifndef __XMLATTRIBUTE_H__
#define __XMLATTRIBUTE_H__

#include <rapidxml_print.hpp>

namespace xml
{
	class XmlNode;
	class XmlAttribute
	{
		friend class XmlNode;

	public:
		XmlAttribute(rapidxml::xml_attribute<>* pAttr = nullptr);

		bool IsValid() const;

		std::string GetName() const;

		std::string GetValue() const;
		void SetValue(const std::string& strValue);

	private:
		rapidxml::xml_attribute<>* m_pAttr{ nullptr };
	};
} // namespace xml

#endif
