//======== ======== ======== ======== ======== ======== ======== ========
///	\file
///
///	\author Tiago Freire
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

#pragma once

#include <string>
#include <UDEF/udef_items.hpp>
#include <UDEF/UDEF.hpp>

namespace udef::_p
{

class Danger_Action
{
public:
	static inline void move_spacing(lineSpace& p_spacing, std::u8string& p_string)
	{
		p_spacing._space = std::move(p_string);
	}

	static inline void move_spacing(multiLineSpace& p_spacing, std::u8string& p_string)
	{
		p_spacing._space = std::move(p_string);
	}

	static inline void setlineCount(multiLineSpace& p_spacing, uint64_t p_count)
	{
		p_spacing._lines = p_count;
	}

	static inline _Error_Context& publicError(Error_Context& p_obj) { return p_obj; }

};

} //namespace udef::_p
