//======== ======== ======== ======== ======== ======== ======== ========
///	\file
///
///	\copyright
///		Copyright (c) 2020 Tiago Miguel Oliveira Freire
///
///		Permission is hereby granted, free of charge, to any person obtaining a copy
///		of this software and associated documentation files (the "Software"), to deal
///		in the Software without restriction, including without limitation the rights
///		to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
///		copies of the Software, and to permit persons to whom the Software is
///		furnished to do so, subject to the following conditions:
///
///		The above copyright notice and this permission notice shall be included in all
///		copies or substantial portions of the Software.
///
///		THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
///		IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
///		FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
///		AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
///		LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
///		OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
///		SOFTWARE.
//======== ======== ======== ======== ======== ======== ======== ========

#include <SCEF/scef_items.hpp>


namespace scef
{
namespace _p
{

//======== ======== class _t_list_iterator
_t_list_iterator::_t_list_iterator(const _list_iterator& p_node, const _list_iterator& p_end, ItemType p_mask)
	: _node	(p_node)
	, _end	(p_end)
	, _mask	(p_mask)
{
	while(_node != p_end && ((*_node)->type() & p_mask) == ItemType{})
	{
		++_node;
	}
}

_t_list_iterator& _t_list_iterator::operator ++ () //++i
{
	++_node;
	while(_node != _end && ((*_node)->type() & _mask) == ItemType{})
	{
		++_node;
	}
	return *this;
}

_t_list_iterator _t_list_iterator::operator ++ (int) //i++
{
	_t_list_iterator t_otr = *this;
	++_node;
	while(_node != _end && ((*_node)->type() & _mask) == ItemType{})
	{
		++_node;
	}
	return t_otr;
}


void _t_list_iterator::reSetMask(ItemType p_mask)
{
	_mask = p_mask;
	while(_node != _end && ((*_node)->type() & p_mask) == ItemType{})
	{
		++_node;
	}
}

//======== ======== class _t_list_const_iterator
_t_list_const_iterator::_t_list_const_iterator(const _t_list_iterator& p_other)
	: _node	(p_other._node)
	, _end	(p_other._end)
	, _mask	(p_other._mask)
{
}

_t_list_const_iterator& _t_list_const_iterator::operator = (const _t_list_iterator& p_other)
{
	_node = p_other._node;
	_end = p_other._end;
	_mask = p_other._mask;
	return *this;
}

_t_list_const_iterator::_t_list_const_iterator(const _list_const_iterator& p_node, const _list_const_iterator& p_end, ItemType p_mask)
	: _node	(p_node)
	, _end	(p_end)
	, _mask	(p_mask)
{
	while(_node != p_end && ((*_node)->type() & p_mask) == ItemType{})
	{
		++_node;
	}
}

_t_list_const_iterator& _t_list_const_iterator::operator ++ () //++i
{
	++_node;
	while(_node != _end && ((*_node)->type() & _mask) == ItemType{})
	{
		++_node;
	}
	return *this;
}

_t_list_const_iterator _t_list_const_iterator::operator ++ (int) //i++
{
	_t_list_const_iterator t_otr = *this;
	++_node;
	while(_node != _end && ((*_node)->type() & _mask) == ItemType{})
	{
		++_node;
	}
	return t_otr;
}

void _t_list_const_iterator::reSetMask(ItemType p_mask)
{
	_mask = p_mask;
	while(_node != _end && ((*_node)->type() & p_mask) == ItemType{})
	{
		++_node;
	}
}

//======== ======== class lineSpace
void lineSpace::set_spacing(std::u8string_view p_spacing)
{
	_space = p_spacing;
	for(char8_t& val : _space)
	{
		if(!is_space_noLF(val)) val = ' ';
	}
}

void lineSpace::append_spacing(std::u8string_view p_spacing)
{
	uintptr_t size = _space.size();
	_space.append(p_spacing);
	for(uintptr_t i = size, postSize = _space.size(); i < postSize; ++i)
	{
		if(!is_space_noLF(_space[i])) _space[i] = ' ';
	}
}

//======== ======== class multiLineSpace
void multiLineSpace::set_spacing(uint64_t p_lines, const std::u8string& p_spacing)
{
	_lines = p_lines;
	_space = p_spacing;
	for(char8_t& val : _space)
	{
		if(!is_space_noLF(val)) val = ' ';
	}
}

void multiLineSpace::append_spacing(uint64_t p_lines, const std::u8string& p_spacing)
{
	_lines = p_lines;
	uintptr_t size = _space.size();
	_space.append(p_spacing);
	for(uintptr_t i = size, postSize = _space.size(); i < postSize; ++i)
	{
		if(!is_space_noLF(_space[i])) _space[i] = ' ';
	}
}

} //namespace _p


//======== ======== class ItemList

itemProxy<group> ItemList::find_group_by_name(std::u32string_view p_name)
{
	using type = group;
	for(const itemProxy<item>& tobj: *this)
	{
		if(tobj->type() == type::static_type())
		{
			if(std::u32string_view{static_cast<type*>(tobj.get())->name()} == p_name)
			{
				return std::static_pointer_cast<type>(tobj);
			}
		}
	}
	return {};
}

itemProxy<singlet> ItemList::find_singlet_by_name(std::u32string_view p_name)
{
	using type = singlet;
	for(const itemProxy<item>& tobj: *this)
	{
		if(tobj->type() == singlet::static_type())
		{
			if(std::u32string_view{static_cast<type*>(tobj.get())->name()} == p_name)
			{
				return std::static_pointer_cast<type>(tobj);
			}
		}
	}
	return {};
}

itemProxy<keyedValue> ItemList::find_key_by_name(std::u32string_view p_name)
{
	using type = keyedValue;
	for(const itemProxy<item>& tobj: *this)
	{
		if(tobj->type() == type::static_type())
		{
			if(std::u32string_view{static_cast<type*>(tobj.get())->name()} == p_name)
			{
				return std::static_pointer_cast<type>(tobj);
			}
		}
	}
	return {};
}

itemProxy<const group> ItemList::find_group_by_name(std::u32string_view p_name) const
{
	using type = const group;
	for(const itemProxy<item>& tobj: *this)
	{
		if(tobj->type() == type::static_type())
		{
			if(std::u32string_view{static_cast<type*>(tobj.get())->name()} == p_name)
			{
				return std::static_pointer_cast<type>(tobj);
			}
		}
	}
	return {};
}

itemProxy<const singlet> ItemList::find_singlet_by_name(std::u32string_view p_name) const
{
	using type = const singlet;
	for(const itemProxy<item>& tobj: *this)
	{
		if(tobj->type() == type::static_type())
		{
			if(std::u32string_view{static_cast<type*>(tobj.get())->name()} == p_name)
			{
				return std::static_pointer_cast<type>(tobj);
			}
		}
	}
	return {};
}

itemProxy<const keyedValue> ItemList::find_key_by_name(std::u32string_view p_name) const
{
	using type = const keyedValue;
	for(const itemProxy<item>& tobj: *this)
	{
		if(tobj->type() == type::static_type())
		{
			if(std::u32string_view{static_cast<type*>(tobj.get())->name()} == p_name)
			{
				return std::static_pointer_cast<type>(tobj);
			}
		}
	}
	return {};
}



item::~item() = default;

} //namespace scef
