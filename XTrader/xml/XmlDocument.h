#ifndef __XMLDOCUMENT_H__
#define __XMLDOCUMENT_H__

#include <string>
#include "XmlNode.h"

namespace xml
{

	// XML文档类
	class XmlDocument
	{
	public:
		// 构造函数
		XmlDocument();

		// 禁止拷贝
		XmlDocument(const XmlDocument&) = delete;
		XmlDocument& operator=(const XmlDocument&) = delete;

		// 移动构造函数和赋值运算符
		XmlDocument(XmlDocument&& other) noexcept = default;
		XmlDocument& operator=(XmlDocument&& other) noexcept = default;

		// 解析XML字符串
		bool Parse(const std::string& xml);
		// 从文件加载XML
		bool LoadFromFile(const std::string& filename);
		// 保存XML到文件
		bool SaveToFile(const std::string& filename, bool formatted = true) const;

		// 转换为字符串
		std::string ToString(bool formatted = true) const;

		// 创建根节点
		XmlNode CreateRootNode(const std::string& name);
		// 获取根节点
		XmlNode GetRoot() const;
		// 清除文档
		void Clear();

	private:
		std::unique_ptr<rapidxml::xml_document<>> m_document;
		std::unique_ptr<char[]> m_xmlData;
	};

} // namespace xml

#endif
