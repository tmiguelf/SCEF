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

#include "SCEF/scef_stream.hpp"

namespace scef
{

//======== ======== class: std_istream ======== ========
std_istream::std_istream(std::basic_istream<char>& p_streamer): _istream(p_streamer)
{
	std::streampos pos = static_cast<uint64_t>(p_streamer.tellg());
	p_streamer.seekg(0, std::ios_base::end);
	_size = static_cast<uint64_t>(p_streamer.tellg());
	p_streamer.seekg(pos);
}

uintptr_t std_istream::read(void* p_buffer, uintptr_t p_size)
{
	_istream.read(reinterpret_cast<char*>(p_buffer), p_size);
	return static_cast<uintptr_t>(_istream.gcount());
}

stream_error std_istream::stat() const
{
	return _istream.good() ? stream_error::None : (_istream.eof() ? stream_error::Control_EndOfStream : stream_error::Unable2Read);
}

uint64_t std_istream::pos() const
{
	return static_cast<uint64_t>(_istream.tellg());
}

void std_istream::set_pos(uint64_t p_pos)
{
	_istream.clear();
	_istream.seekg(p_pos);
}

//======== ======== class: std_ostream ======== ========
stream_error std_ostream::write(const void* p_buffer, uintptr_t p_size)
{
	_ostream.write(reinterpret_cast<const char*>(p_buffer), p_size);
	if(_ostream.good()) return stream_error::None;

	return stream_error::Unable2Write;
}

//======== ======== class: buffer_istream ======== ========
buffer_istream::buffer_istream(const void* p_first, const void* p_last)
	: _first{p_first}
	, _last {p_last}
	, _pivot{reinterpret_cast<const char8_t*>(p_first)}
{
}

buffer_istream::buffer_istream(const void* p_buff, uintptr_t p_size)
	: _first{p_buff}
	, _last {reinterpret_cast<const char8_t*>(p_buff) + p_size}
	, _pivot{reinterpret_cast<const char8_t*>(p_buff)}
{
}

uintptr_t buffer_istream::read(void* p_buffer, uintptr_t p_size)
{
	uintptr_t rem = reinterpret_cast<uintptr_t>(_last) - reinterpret_cast<uintptr_t>(_pivot);
	if(p_size < rem) rem = p_size;
	memcpy(p_buffer, _pivot, rem);
	_pivot += rem;
	return rem;
}

stream_error buffer_istream::stat() const
{
	return _pivot < _last ? stream_error::None : stream_error::Control_EndOfStream;
}

uint64_t buffer_istream::pos() const
{
	return reinterpret_cast<uintptr_t>(_pivot) - reinterpret_cast<uintptr_t>(_first);
}

void buffer_istream::set_pos(uint64_t p_pos)
{
	if(p_pos <= _size)
	{
		_pivot = reinterpret_cast<const char8_t*>(_first) + p_pos;
	}
}

}	// namespace scef
