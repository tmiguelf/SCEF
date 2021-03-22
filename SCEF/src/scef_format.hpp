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

#pragma once

#include <cstdint>

#include "SCEF/SCEF.hpp"

namespace scef
{
class stream_decoder;
class stream_encoder;
}

namespace scef::format
{

struct _Warning_Def
{
	Error_Context* _error_context;
	warningBehaviour (*_user_warning_callback)(const Error_Context&, void*);
	void* _user_context;
	inline warningBehaviour Notify() { return _user_warning_callback(*_error_context, _user_context);}
};

//Note:
//	1. to simplify algorithm, let's agree that the caller to this function will set the line in the Error Context
Error FinishVersionDecoding	(stream_decoder& p_decoder, uint16_t& p_version, Error_Context& p_err);
Error WriteVersion			(stream_encoder& p_encoder, uint16_t p_version);
} //namespace scef::format
