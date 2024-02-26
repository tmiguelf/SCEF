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
#include <istream>
#include <ostream>
#include <filesystem>
#include <CoreLib/core_file.hpp>

namespace scef
{

enum class stream_error: uint8_t
{
	None				= 0x00,
	FileNotFound		= 0x01,
	Unable2Read			= 0x02,
	Unable2Write		= 0x03,
	BadEncoding			= 0x04,
	Control_EndOfStream	= 0xFF
};

class base_istreamer
{
protected:
	uint64_t _size = 0;

public:
	virtual ~base_istreamer();

	[[nodiscard]] virtual uintptr_t		read	(void* p_buffer, uintptr_t p_size) = 0;
	[[nodiscard]] virtual stream_error	stat	() const = 0;
	[[nodiscard]] virtual uint64_t		pos		() const = 0;

	virtual void set_pos(uint64_t pos) = 0;

	[[nodiscard]] inline uint64_t size		() const { return _size; }
	[[nodiscard]] inline uint64_t remaining	() const { return _size - pos(); }
};

class base_ostreamer
{
public:
	virtual ~base_ostreamer();

	// should return error_None if write operation is successful and otherwise
	// on failure.
	// For future compatibility, it is recommended that stream_error::FileUnable2Write
	// is returned if there is a problem with the stream
	// For practical purposes, the current implementation assumes any return value
	// different from stream_error::None as stream_error::FileUnable2Write.
	// No assumptions must be made regarding "no further" Write calls independently of
	// any return value
	virtual stream_error write(const void* p_buffer, uintptr_t p_size) = 0;
};


//======== ======== ======== std::stream ======== ======== ========

class std_istream: public base_istreamer
{
private:
	std::basic_istream<char>& _istream;

public:
	std_istream(std::basic_istream<char>& p_streamer);

	uintptr_t read(void* p_buffer, uintptr_t p_size) override;
	stream_error stat() const override;

	uint64_t pos() const override;
	void set_pos(uint64_t p_pos) override;
};

class std_ostream: public base_ostreamer
{
private:
	std::basic_ostream<char>& _ostream;
public:
	inline std_ostream(std::basic_ostream<char>& p_streamer) : _ostream{p_streamer} {}
	stream_error write(const void* p_buffer, uintptr_t p_size) override;
};


//======== ======== ======== core::file ======== ======== ========

class file_istream: public base_istreamer
{
private:
	core::file_read& m_file;

public:
	file_istream(core::file_read& p_streamer);

	uintptr_t read(void* p_buffer, uintptr_t p_size) override;
	stream_error stat() const override;

	uint64_t pos() const override;
	void set_pos(uint64_t p_pos) override;
};

class file_ostream: public base_ostreamer
{
private:
	core::file_write& m_file;

public:
	inline file_ostream(core::file_write& p_streamer): m_file{p_streamer} {}
	stream_error write(const void* p_buffer, uintptr_t p_size) override;
};


//======== ======== ======== generic ======== ======== ========
class buffer_istream: public base_istreamer
{
private:
	const void*		_first;
	const void*		_last;
	const char8_t*	_pivot;

public:
	buffer_istream(const void* p_first, const void* p_last);
	buffer_istream(const void* p_buff, uintptr_t p_size);

	uintptr_t read(void* p_buffer, uintptr_t p_size) override;
	stream_error stat() const override;

	uint64_t pos() const override;
	void set_pos(uint64_t pos) override;
};	//class buffer_istream

}	// namespace scef
