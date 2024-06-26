//======== ======== ======== ======== ======== ======== ======== ========
///	\file
///
///	\copyright
///		Copyright (c) Tiago Miguel Oliveira Freire
///
///		Permission is hereby granted, free of charge, to any person obtaining a copy
///		of this software and associated documentation files (the "Software"),
///		to copy, modify, publish, and/or distribute copies of the Software,
///		and to permit persons to whom the Software is furnished to do so,
///		subject to the following conditions:
///
///		The copyright notice and this permission notice shall be included in all
///		copies or substantial portions of the Software.
///		The copyrighted work, or derived works, shall not be used to train
///		Artificial Intelligence models of any sort; or otherwise be used in a
///		transformative way that could obfuscate the source of the copyright.
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

#include <CoreLib/core_alternate.hpp>

#include <SCEF/scef_stream.hpp>

namespace scef
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
	virtual ~stream_decoder();

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
	virtual ~stream_encoder();

	virtual stream_error put_control(char8_t p_char) = 0;
	virtual stream_error put_sequence(std::u32string_view p_string) = 0;
	virtual stream_error put_flat(std::u8string_view p_string) = 0;

	virtual bool requires_escape(std::u32string_view p_string) const = 0;
	virtual bool requires_escape(char32_t p_char) const = 0;
};

namespace ENCODER_P
{
[[maybe_unused]] constexpr std::array BOM_UTF8		= {char8_t{0xEF}, char8_t{0xBB}, char8_t{0xBF}};
[[maybe_unused]] constexpr std::array BOM_UTF16BE	= {char8_t{0xFE}, char8_t{0xFF}};
[[maybe_unused]] constexpr std::array BOM_UTF16LE	= {char8_t{0xFF}, char8_t{0xFE}};
[[maybe_unused]] constexpr std::array BOM_UCS4BE	= {char8_t{0x00}, char8_t{0x00}, char8_t{0xFE}, char8_t{0xFF}};
[[maybe_unused]] constexpr std::array BOM_UCS4LE	= {char8_t{0xFF}, char8_t{0xFE}, char8_t{0x00}, char8_t{0x00}};

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
	stream_error put_control(char8_t p_char) final;
	stream_error put_sequence(std::u32string_view p_string) final;
	stream_error put_flat(std::u8string_view p_string) final;

	bool requires_escape(std::u32string_view p_string) const final;
	bool requires_escape(char32_t p_char) const final;
};

//---- UTF8 ----
class Stream_UTF8_Encoder: public stream_encoder
{
public:
	inline Stream_UTF8_Encoder(base_ostreamer& p_writer): stream_encoder(p_writer){}
	stream_error put_control(char8_t p_char) final;
	stream_error put_sequence(std::u32string_view p_string) final;
	stream_error put_flat(std::u8string_view p_string) final;

	bool requires_escape(std::u32string_view p_string) const final;
	bool requires_escape(char32_t p_char) const final;
};

class Stream_UTF8_Encoder_s: public stream_encoder //strict version
{
public:
	inline Stream_UTF8_Encoder_s(base_ostreamer& p_writer): stream_encoder(p_writer){}
	stream_error put_control(char8_t p_char) final;
	stream_error put_sequence(std::u32string_view p_string) final;
	stream_error put_flat(std::u8string_view p_string) final;

	bool requires_escape(std::u32string_view p_string) const final;
	bool requires_escape(char32_t p_char) const final;
};

//---- UTF16 ----
class Stream_UTF16LE_Encoder: public stream_encoder
{
public:
	inline Stream_UTF16LE_Encoder(base_ostreamer& p_writer): stream_encoder(p_writer){}
	stream_error put_control(char8_t p_char) final;
	stream_error put_sequence(std::u32string_view p_string) final;
	stream_error put_flat(std::u8string_view p_string) final;

	bool requires_escape(std::u32string_view p_string) const final;
	bool requires_escape(char32_t p_char) const final;
};

class Stream_UTF16BE_Encoder: public stream_encoder
{
public:
	inline Stream_UTF16BE_Encoder(base_ostreamer& p_writer): stream_encoder(p_writer){}
	stream_error put_control(char8_t p_char) final;
	stream_error put_sequence(std::u32string_view p_string) final;
	stream_error put_flat(std::u8string_view p_string) final;

	bool requires_escape(std::u32string_view p_string) const final;
	bool requires_escape(char32_t p_char) const final;
};

//---- UCS4 ----
class Stream_UCS4LE_Encoder: public stream_encoder
{
public:
	inline Stream_UCS4LE_Encoder(base_ostreamer& p_writer): stream_encoder(p_writer){}
	stream_error put_control(char8_t p_char) final;
	stream_error put_sequence(std::u32string_view p_string) final;
	stream_error put_flat(std::u8string_view p_string) final;

	bool requires_escape(std::u32string_view p_string) const final;
	bool requires_escape(char32_t p_char) const final;
};

class Stream_UCS4LE_Encoder_s: public stream_encoder //strict version
{
public:
	inline Stream_UCS4LE_Encoder_s(base_ostreamer& p_writer): stream_encoder(p_writer){}
	stream_error put_control(char8_t p_char) final;
	stream_error put_sequence(std::u32string_view p_string) final;
	stream_error put_flat(std::u8string_view p_string) final;

	bool requires_escape(std::u32string_view p_string) const final;
	bool requires_escape(char32_t p_char) const final;
};

class Stream_UCS4BE_Encoder: public stream_encoder
{
public:
	inline Stream_UCS4BE_Encoder(base_ostreamer& p_writer): stream_encoder(p_writer){}
	stream_error put_control(char8_t p_char) final;
	stream_error put_sequence(std::u32string_view p_string) final;
	stream_error put_flat(std::u8string_view p_string) final;

	bool requires_escape(std::u32string_view p_string) const final;
	bool requires_escape(char32_t p_char) const final;
};

class Stream_UCS4BE_Encoder_s: public stream_encoder //strict version
{
public:
	inline Stream_UCS4BE_Encoder_s(base_ostreamer& p_writer): stream_encoder(p_writer){}
	stream_error put_control(char8_t p_char) final;
	stream_error put_sequence(std::u32string_view p_string) final;
	stream_error put_flat(std::u8string_view p_string) final;

	bool requires_escape(std::u32string_view p_string) const final;
	bool requires_escape(char32_t p_char) const final;
};

}	// namespace ENCODER_P
}	// namespace scef
