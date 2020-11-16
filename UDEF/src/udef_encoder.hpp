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

#include <cstdint>
#include <array>

#include <CoreLib/Core_Alternate.hpp>

#include "UDEF/udef_stream.hpp"

namespace udef
{

// Used to interpret character encoding
// Ex. ANSI, UTF8, UTF16, UCS4, etc...
class stream_decoder
{
public:
	using result_t	= core::alternate<char32_t, stream_error, stream_error::None, stream_error::Unable2Read>;
	using read_f	= bool (*) (char32_t, void*);

protected:
	base_istreamer& m_reader;
	[[nodiscard]] virtual result_t v_get_char() = 0;

private:
	uint64_t m_column	= 0;
	uint64_t m_line		= 1;
	char32_t m_lastChar	= 0;

	inline void nextLine() { m_column = 0; ++m_line; }

public:
	inline stream_decoder(base_istreamer& p_reader): m_reader{p_reader} {}
	virtual ~stream_decoder() = 0;

	[[nodiscard]] stream_error read_while(read_f p_user_cb, void* p_context);
	[[nodiscard]] result_t get_char();

	[[nodiscard]] inline stream_error stat() { return m_reader.stat(); }
	[[nodiscard]] inline char32_t lastChar() const { return m_lastChar; }

	[[nodiscard]] inline uint64_t line	() const { return m_line; }
	[[nodiscard]] inline uint64_t column	() const { return m_column; }

	inline void reset_context()
	{
		m_column		= 0;
		m_line			= 1;
		m_lastChar		= 0;
	}

};

// Used to translate character encoding
// Ex. ANSI, UTF8, UTF16, UCS4, etc...
class stream_encoder
{
protected:
	base_ostreamer& m_writer;

public:
	inline stream_encoder(base_ostreamer& p_writer): m_writer{p_writer}{}
	virtual ~stream_encoder() = 0;

	virtual stream_error put(char32_t p_char) = 0;
	virtual stream_error put(std::u32string_view p_string) = 0;
	virtual stream_error put(std::u8string_view p_string) = 0;
};

namespace ENCODER_P
{
constexpr std::array<char8_t, 3> BOM_UTF8		= {0xEF, 0xBB, 0xBF};
constexpr std::array<char8_t, 2> BOM_UTF16BE	= {0xFE, 0xFF};
constexpr std::array<char8_t, 2> BOM_UTF16LE	= {0xFF, 0xFE};
constexpr std::array<char8_t, 4> BOM_UCS4BE		= {0x00, 0x00, 0xFE, 0xFF};
constexpr std::array<char8_t, 4> BOM_UCS4LE		= {0xFF, 0xFE, 0x00, 0x00};

//---- Decoders ----

//---- ANSI ----
class Stream_ANSI_Decoder: public stream_decoder
{
protected:
	result_t v_get_char() override;
public:
	inline Stream_ANSI_Decoder(base_istreamer& p_reader): stream_decoder(p_reader) {}
};

//---- UTF8 ----
class Stream_UTF8_Decoder: public stream_decoder
{
public:
	result_t v_get_char() override;
public:
	inline Stream_UTF8_Decoder(base_istreamer& p_reader): stream_decoder(p_reader) {}
};

class Stream_UTF8_Decoder_s: public stream_decoder //strict version
{
public:
	result_t v_get_char() override;
public:
	inline Stream_UTF8_Decoder_s(base_istreamer& p_reader): stream_decoder(p_reader) {}
};

//---- UTF16 ----
class Stream_UTF16LE_Decoder: public stream_decoder
{
public:
	result_t v_get_char() override;
public:
	inline Stream_UTF16LE_Decoder(base_istreamer& p_reader): stream_decoder(p_reader) {}
};

class Stream_UTF16BE_Decoder: public stream_decoder
{
public:
	result_t v_get_char() override;
public:
	inline Stream_UTF16BE_Decoder(base_istreamer& p_reader): stream_decoder(p_reader) {}
};

//---- UCS4 ----
class Stream_UCS4LE_Decoder: public stream_decoder
{
public:
	result_t v_get_char() override;
public:
	inline Stream_UCS4LE_Decoder(base_istreamer& p_reader): stream_decoder(p_reader) {}
};

class Stream_UCS4LE_Decoder_s: public stream_decoder //strict version
{
public:
	result_t v_get_char() override;
public:
	inline Stream_UCS4LE_Decoder_s(base_istreamer& p_reader): stream_decoder(p_reader) {}
};

class Stream_UCS4BE_Decoder: public stream_decoder
{
public:
	result_t v_get_char() override;
public:
	inline Stream_UCS4BE_Decoder(base_istreamer& p_reader): stream_decoder(p_reader) {}
};

class Stream_UCS4BE_Decoder_s: public stream_decoder //strict version
{
public:
	result_t v_get_char() override;
public:
	inline Stream_UCS4BE_Decoder_s(base_istreamer& p_reader): stream_decoder(p_reader) {}
};

//---- Encoders ----
//---- ANSI ----
class Stream_ANSI_Encoder: public stream_encoder
{
public:
	inline Stream_ANSI_Encoder(base_ostreamer& p_writer): stream_encoder(p_writer){}
	stream_error put(char32_t p_char) override;
	stream_error put(std::u32string_view p_string) override;
	stream_error put(std::u8string_view p_string) override;
};

//---- UTF8 ----
class Stream_UTF8_Encoder: public stream_encoder
{
public:
	inline Stream_UTF8_Encoder(base_ostreamer& p_writer): stream_encoder(p_writer){}
	stream_error put(char32_t p_char) override;
	stream_error put(std::u32string_view p_string) override;
	stream_error put(std::u8string_view p_string) override;
};

class Stream_UTF8_Encoder_s: public stream_encoder //strict version
{
public:
	inline Stream_UTF8_Encoder_s(base_ostreamer& p_writer): stream_encoder(p_writer){}
	stream_error put(char32_t p_char) override;
	stream_error put(std::u32string_view p_string) override;
	stream_error put(std::u8string_view p_string) override;
};

//---- UTF16 ----
class Stream_UTF16LE_Encoder: public stream_encoder
{
public:
	inline Stream_UTF16LE_Encoder(base_ostreamer& p_writer): stream_encoder(p_writer){}
	stream_error put(char32_t p_char) override;
	stream_error put(std::u32string_view p_string) override;
	stream_error put(std::u8string_view p_string) override;
};

class Stream_UTF16BE_Encoder: public stream_encoder
{
public:
	inline Stream_UTF16BE_Encoder(base_ostreamer& p_writer): stream_encoder(p_writer){}
	stream_error put(char32_t p_char) override;
	stream_error put(std::u32string_view p_string) override;
	stream_error put(std::u8string_view p_string) override;
};

//---- UCS4 ----
class Stream_UCS4LE_Encoder: public stream_encoder
{
public:
	inline Stream_UCS4LE_Encoder(base_ostreamer& p_writer): stream_encoder(p_writer){}
	stream_error put(char32_t p_char) override;
	stream_error put(std::u32string_view p_string) override;
	stream_error put(std::u8string_view p_string) override;
};

class Stream_UCS4LE_Encoder_s: public stream_encoder //strict version
{
public:
	inline Stream_UCS4LE_Encoder_s(base_ostreamer& p_writer): stream_encoder(p_writer){}
	stream_error put(char32_t p_char) override;
	stream_error put(std::u32string_view p_string) override;
	stream_error put(std::u8string_view p_string) override;
};

class Stream_UCS4BE_Encoder: public stream_encoder
{
public:
	inline Stream_UCS4BE_Encoder(base_ostreamer& p_writer): stream_encoder(p_writer){}
	stream_error put(char32_t p_char) override;
	stream_error put(std::u32string_view p_string) override;
	stream_error put(std::u8string_view p_string) override;
};

class Stream_UCS4BE_Encoder_s: public stream_encoder //strict version
{
public:
	inline Stream_UCS4BE_Encoder_s(base_ostreamer& p_writer): stream_encoder(p_writer){}
	stream_error put(char32_t p_char) override;
	stream_error put(std::u32string_view p_string) override;
	stream_error put(std::u8string_view p_string) override;
};

}	// namespace ENCODER_P
}	// namespace udef
