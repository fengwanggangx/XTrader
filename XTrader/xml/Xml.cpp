#include "Xml.h"
#include "rapidxml_print.hpp"
#include <sstream>
namespace xml
{
	std::string ToString(const rapidxml::xml_base<>::view_type& str)
	{
		return { str.data(), str.size() };
	}
} // namespace xml