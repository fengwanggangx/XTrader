#ifndef __XML_H__
#define __XML_H__
#include <rapidxml.hpp>
namespace xml
{
	std::string ToString(const rapidxml::xml_base<>::view_type& str);

	template <typename T>
	T* GetPtr(const flxml::optional_ptr<T>& opt_ptr)
	{
		return opt_ptr.has_value() ? const_cast<T*>(opt_ptr.get()) : nullptr;
	}

} // namespace xml

#endif
