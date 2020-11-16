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

#include "udef_encoder.hpp"

#include <span>

#include <CoreLib/Core_Endian.hpp>
#include <CoreLib/string/core_string_encoding.hpp>

namespace udef
{

//======== ======== class: Stream_Decoder ======== ========
stream_error stream_decoder::read_while(read_f p_user_cb, void* p_context)
{
	while(true)
	{
		if(m_lastChar == '\n') nextLine();
		result_t res = v_get_char();
		if(!res.has_value())
		{
			m_lastChar = 0;
			return res.error_code();
		}
		m_lastChar = res.value();
		bool should_continue = p_user_cb(res.value(), p_context);
		++m_column;
		if(!should_continue) return stream_error::None;
	}
}


stream_decoder::result_t stream_decoder::get_char()
{
	if(m_lastChar == '\n') nextLine();
	result_t res = v_get_char();
	m_lastChar = res.value();
	++m_column;
	return res;
}


namespace ENCODER_P
{
//---- Decoders ----

stream_decoder::result_t Stream_ANSI_Decoder::v_get_char()
{
	char8_t r;
	if(m_reader.read(&r, 1) != 1)
	{
		return m_reader.stat() == stream_error::Control_EndOfStream ? stream_error::Control_EndOfStream : stream_error::Unable2Read;
	}
	return static_cast<char32_t>(r);
}


static inline stream_error extract_utf8_layers(base_istreamer& p_reader, std::span<char8_t> p_buffer)
{
	uintptr_t count = p_reader.read(p_buffer.data(), p_buffer.size());

	if(count != p_buffer.size())
	{
		if(p_reader.stat() == stream_error::Control_EndOfStream)
		{
			for(uintptr_t it = 0; it < count; ++it)
			{
				if((p_buffer[it] & 0xC0) != 0x80)
				{
					p_reader.set_pos(p_reader.pos() - p_buffer.size() - it);
				}
			}
			return stream_error::BadEncoding;
		}
		return stream_error::Unable2Read;
	}

	for(uintptr_t it = 0; it < count; ++it)
	{
		if((p_buffer[it] & 0xC0) != 0x80)
		{
			p_reader.set_pos(p_reader.pos() - (p_buffer.size() - it));
			return stream_error::BadEncoding;
		}
	}
	return stream_error::None;
}

stream_decoder::result_t Stream_UTF8_Decoder::v_get_char()
{
	char8_t r;
	if(m_reader.read(&r, 1) != 1)
	{
		return (m_reader.stat() == stream_error::Control_EndOfStream) ? stream_error::Control_EndOfStream : stream_error::Unable2Read;
	}

	if(r & 0x80)
	{
		if((r & 0xC0) == 0x80) return stream_error::BadEncoding;
		if((r & 0xE0) == 0xC0)	//level 1
		{
			char8_t r1;
			if(m_reader.read(&r1, 1) != 1)	return (m_reader.stat() == stream_error::Control_EndOfStream) ? stream_error::BadEncoding : stream_error::Unable2Read;
			if((r1 & 0xC0) != 0x80)
			{
				m_reader.set_pos(m_reader.pos() - 1);
				return stream_error::BadEncoding;
			}
			return
				((char32_t{r} &0x1F) << 6) |
				( char32_t{r1} &0x3F);
		}
		if((r & 0xF0) == 0xE0) //level 2
		{
			char8_t r1[2];
			stream_error ret = extract_utf8_layers(m_reader, std::span<char8_t>{r1, 2});
			if(ret != stream_error::None) return ret;
			return
				((char32_t{r} & 0x0F) << 12) |
				((char32_t{r1[0]} & 0x3F) << 6) |
				( char32_t{r1[1]} & 0x3F);
		}
		if((r & 0xF8) == 0xF0) //level 3
		{
			char8_t r1[3];
			stream_error ret = extract_utf8_layers(m_reader, std::span<char8_t>{r1, 3});
			if(ret != stream_error::None) return ret;
			//todo code point validation
			return ((char32_t{r} & 0x07) << 18) | ((char32_t{r1[0]} & 0x3F) << 12) | ((char32_t{r1[1]} & 0x3F) << 6) | (char32_t{r1[2]} & 0x3F);
		}
		if((r & 0xFC) == 0xF8) //level 4
		{
			char8_t r1[4];
			stream_error ret = extract_utf8_layers(m_reader, std::span<char8_t>{r1, 4});
			if(ret != stream_error::None) return ret;

			return
				((char32_t{r} & 0x03) << 24) |
				((char32_t{r1[0]} & 0x3F) << 18) |
				((char32_t{r1[1]} & 0x3F) << 12) |
				((char32_t{r1[2]} & 0x3F) << 6) |
				( char32_t{r1[3]} & 0x3F);
		}
		if((r & 0xFE) == 0xFC) //level 5
		{
			char8_t r1[5];
			stream_error ret = extract_utf8_layers(m_reader, std::span<char8_t>{r1, 5});
			if(ret != stream_error::None) return ret;

			return
				((char32_t{r} & 0x03) << 30) |
				((char32_t{r1[0]} & 0x3F) << 24) |
				((char32_t{r1[1]} & 0x3F) << 18) |
				((char32_t{r1[2]} & 0x3F) << 12) |
				((char32_t{r1[3]} & 0x3F) << 6) |
				( char32_t{r1[4]} & 0x3F);
		}
		if((r & 0xFF) == 0xFE) //level 6
		{
			char8_t r1[6];
			stream_error ret = extract_utf8_layers(m_reader, std::span<char8_t>{r1, 6});
			if(ret != stream_error::None) return ret;

			if((r1[0] & 0x3F) > 0x03) return stream_error::BadEncoding;

			return
				((char32_t{r1[0]} & 0x03) << 30) |
				((char32_t{r1[1]} & 0x3F) << 24) |
				((char32_t{r1[2]} & 0x3F) << 18) |
				((char32_t{r1[3]} & 0x3F) << 12) |
				((char32_t{r1[4]} & 0x3F) << 6) |
				( char32_t{r1[5]} & 0x3F);
		}
		return stream_error::BadEncoding;
	}
	return static_cast<char32_t>(r);
}

stream_decoder::result_t Stream_UTF8_Decoder_s::v_get_char()
{
	char8_t r;
	if(m_reader.read(&r, 1) != 1)
	{
		return (m_reader.stat() == stream_error::Control_EndOfStream) ? stream_error::Control_EndOfStream : stream_error::Unable2Read;
	}

	if(r & 0x80)
	{
		if((r & 0xC0) == 0x80) return stream_error::BadEncoding;
		if((r & 0xE0) == 0xC0) //level 1
		{
			char8_t r1;
			if(m_reader.read(&r1, 1) != 1)	return (m_reader.stat() == stream_error::Control_EndOfStream) ? stream_error::BadEncoding : stream_error::Unable2Read;
			if((r1 & 0xC0) != 0x80)
			{
				m_reader.set_pos(m_reader.pos() - 1);
				return stream_error::BadEncoding;
			}

			if(((r & 0x1F) < 0x02)) return stream_error::BadEncoding;
			return
				((char32_t{r} &0x1F) << 6) |
				( char32_t{r1} &0x3F);
		}
		if((r & 0xF0) == 0xE0) //level 2
		{
			char8_t r1[2];
			stream_error ret = extract_utf8_layers(m_reader, std::span<char8_t>{r1, 2});
			if(ret != stream_error::None) return ret;

			if((r & 0x0F) == 0 && (r1[0] & 0x3F) < 0x20) return stream_error::BadEncoding;
			return
				((char32_t{r} & 0x0F) << 12) |
				((char32_t{r1[0]} & 0x3F) << 6) |
				( char32_t{r1[1]} & 0x3F);
		}
		if((r & 0xF8) == 0xF0) //level 3
		{
			char8_t r1[3];
			stream_error ret = extract_utf8_layers(m_reader, std::span<char8_t>{r1, 3});
			if(ret != stream_error::None) return ret;

			if((r & 0x07) == 0 && (r1[0] & 0x3F) < 0x10) return stream_error::BadEncoding;
			return ((char32_t{r} & 0x07) << 18) | ((char32_t{r1[0]} & 0x3F) << 12) | ((char32_t{r1[1]} & 0x3F) << 6) | (char32_t{r1[2]} & 0x3F);
		}
		if((r & 0xFC) == 0xF8) //level 4
		{
			char8_t r1[4];
			stream_error ret = extract_utf8_layers(m_reader, std::span<char8_t>{r1, 4});
			if(ret != stream_error::None) return ret;
		}
		else if((r & 0xFE) == 0xFC) //level 5
		{
			char8_t r1[5];
			stream_error ret = extract_utf8_layers(m_reader, std::span<char8_t>{r1, 5});
			if(ret != stream_error::None) return ret;
		}
		else if((r & 0xFF) == 0xFE) //level 6
		{
			char8_t r1[6];
			stream_error ret = extract_utf8_layers(m_reader, std::span<char8_t>{r1, 6});
			if(ret != stream_error::None) return ret;
		}
		return stream_error::BadEncoding;
	}
	return static_cast<char32_t>(r);
}

stream_decoder::result_t Stream_UTF16LE_Decoder::v_get_char()
{
	char16_t r;
	uintptr_t t_col = m_reader.read(reinterpret_cast<void*>(&r), 2);
	if(t_col != 2)
	{
		if(m_reader.stat() == stream_error::Control_EndOfStream)
		{
			return (t_col == 0) ? stream_error::Control_EndOfStream : stream_error::BadEncoding;
		}
		return stream_error::Unable2Read;
	}

	r = core::endian_little2host(r);

	if(r > 0xD7FF && r < 0xE000)
	{
		if((r & 0xFC00) != 0xD800) return stream_error::BadEncoding;

		char16_t r1;

		t_col  = m_reader.read(reinterpret_cast<void*>(&r1), 2);

		if(t_col != 2) return (m_reader.stat() == stream_error::Control_EndOfStream) ? stream_error::BadEncoding : stream_error::Unable2Read;

		r1 = core::endian_little2host(r1);
		if((r1 & 0xFC00) != 0xDC0)
		{
			m_reader.set_pos(m_reader.pos() - 2);
			return stream_error::BadEncoding;
		}
		return (((char32_t{r} & 0x03FF) << 10) | (char32_t{r1} & 0x03FF)) + 0x10000;
	}
	return static_cast<char32_t>(r);
}

stream_decoder::result_t Stream_UTF16BE_Decoder::v_get_char()
{
	char16_t r;
	uintptr_t t_col = m_reader.read(reinterpret_cast<void*>(&r), 2);
	if(t_col != 2)
	{
		if(m_reader.stat() == stream_error::Control_EndOfStream)
		{
			return (t_col == 0) ? stream_error::Control_EndOfStream : stream_error::BadEncoding;
		}
		return stream_error::Unable2Read;
	}

	r = core::endian_big2host(r);

	if(r > 0xD7FF && r < 0xE000)
	{
		if((r & 0xFC00) != 0xD800) return stream_error::BadEncoding;

		char16_t r1;

		t_col  = m_reader.read(reinterpret_cast<void*>(&r1), 2);

		if(t_col != 2) return (m_reader.stat() == stream_error::Control_EndOfStream) ? stream_error::BadEncoding : stream_error::Unable2Read;

		r1 = core::endian_big2host(r1);
		if((r1 & 0xFC00) != 0xDC0)
		{
			m_reader.set_pos(m_reader.pos() - 2);
			return stream_error::BadEncoding;
		}
		return (((char32_t{r} & 0x03FF) << 10) | (char32_t{r1} & 0x03FF)) + 0x10000;
	}
	return static_cast<char32_t>(r);
}

stream_decoder::result_t Stream_UCS4LE_Decoder::v_get_char()
{
	char32_t r;
	uintptr_t t_col = m_reader.read(reinterpret_cast<void*>(&r), 4);
	if(t_col != 4)
	{
		if(m_reader.stat() == stream_error::Control_EndOfStream)
		{
			return (t_col == 0) ? stream_error::Control_EndOfStream : stream_error::BadEncoding;
		}
		return stream_error::Unable2Read;
	}
	return core::endian_little2host(r);
}

stream_decoder::result_t Stream_UCS4LE_Decoder_s::v_get_char()
{
	char32_t r;
	uintptr_t t_col = m_reader.read(reinterpret_cast<void*>(&r), 4);
	if(t_col != 4)
	{
		if(m_reader.stat() == stream_error::Control_EndOfStream)
		{
			return (t_col == 0) ? stream_error::Control_EndOfStream : stream_error::BadEncoding;
		}
		return stream_error::Unable2Read;
	}
	r = core::endian_little2host(r);
	if(core::UNICODE_Compliant(r)) return r;
	return stream_error::BadEncoding;
}

stream_decoder::result_t Stream_UCS4BE_Decoder::v_get_char()
{
	char32_t r;
	uintptr_t t_col = m_reader.read(reinterpret_cast<void*>(&r), 4);
	if(t_col != 4)
	{
		if(m_reader.stat() == stream_error::Control_EndOfStream)
		{
			return (t_col == 0) ? stream_error::Control_EndOfStream : stream_error::BadEncoding;
		}
		return stream_error::Unable2Read;
	}
	return core::endian_big2host(r);
}

stream_decoder::result_t Stream_UCS4BE_Decoder_s::v_get_char()
{
	char32_t r;
	uintptr_t t_col = m_reader.read(reinterpret_cast<void*>(&r), 4);
	if(t_col != 4)
	{
		if(m_reader.stat() == stream_error::Control_EndOfStream)
		{
			return (t_col == 0) ? stream_error::Control_EndOfStream : stream_error::BadEncoding;
		}
		return stream_error::Unable2Read;
	}
	r = core::endian_big2host(r);
	if(core::UNICODE_Compliant(r)) return r;
	return stream_error::BadEncoding;
}

//---- Encoders ----

//======== ======== class:  ======== ========
stream_error Stream_ANSI_Encoder::put(char32_t p_char)
{
	if(p_char > 0xFF) return stream_error::BadEncoding;
	char8_t temp = static_cast<char8_t>(p_char);
	return m_writer.write(&temp, 1);
}

stream_error Stream_ANSI_Encoder::put(std::u32string_view p_string)
{
	for(char32_t tchar : p_string)
	{
		if(tchar > 0xFF) return stream_error::BadEncoding;
		char8_t temp = static_cast<char8_t>(tchar);
		if(m_writer.write(&temp, 1) != stream_error::None) return stream_error::Unable2Write;
	}
	return stream_error::None;
}

stream_error Stream_ANSI_Encoder::put(std::u8string_view p_string)
{
	return m_writer.write(p_string.data(), p_string.size());
}


//======== ======== class:  ======== ========
stream_error Stream_UTF8_Encoder::put(char32_t p_char)
{
	char8_t temp[7];
	return m_writer.write(temp, core::encode_UTF8(p_char, temp));
}

stream_error Stream_UTF8_Encoder::put(std::u32string_view p_string)
{
	char8_t temp[7];
	for(char32_t tchar : p_string)
	{
		if(m_writer.write(temp, core::encode_UTF8(tchar, temp)) != stream_error::None) return stream_error::Unable2Write;
	}
	return stream_error::None;
}

stream_error Stream_UTF8_Encoder::put(std::u8string_view p_string)
{
	for(char8_t tchar : p_string)
	{
		if(tchar < 0x00000800)
		{
			if(m_writer.write(&tchar, 1) != stream_error::None) return stream_error::Unable2Write;
		}
		else
		{
			char8_t temp[2] {static_cast<char8_t>((tchar >>  6  ) | 0xC0), static_cast<char8_t>((tchar & 0x3F ) | 0x80)};
			if(m_writer.write(temp, 2) != stream_error::None) return stream_error::Unable2Write;
		}
	}
	return stream_error::None;
}


//======== ======== class:  ======== ========
stream_error Stream_UTF8_Encoder_s::put(char32_t p_char)
{
	if(!core::UNICODE_Compliant(p_char)) return stream_error::BadEncoding;

	char8_t temp[7];
	return m_writer.write(temp, core::encode_UTF8(p_char, temp));
}

stream_error Stream_UTF8_Encoder_s::put(std::u32string_view p_string)
{
	if(!core::UCS4_UNICODE_Compliant(p_string)) return stream_error::BadEncoding;
	char8_t temp[7];
	for(char32_t tchar : p_string)
	{
		if(m_writer.write(temp, core::encode_UTF8(tchar, temp)) != stream_error::None) return stream_error::Unable2Write;
	}
	return stream_error::None;
}

stream_error Stream_UTF8_Encoder_s::put(std::u8string_view p_string)
{
	for(char8_t tchar : p_string)
	{
		if(tchar < 0x00000800)
		{
			if(m_writer.write(&tchar, 1) != stream_error::None) return stream_error::Unable2Write;
		}
		else
		{
			char8_t temp[2] {static_cast<char8_t>((tchar >>  6  ) | 0xC0), static_cast<char8_t>((tchar & 0x3F ) | 0x80)};
			if(m_writer.write(temp, 2) != stream_error::None) return stream_error::Unable2Write;
		}
	}
	return stream_error::None;
}


//======== ======== class:  ======== ========
stream_error Stream_UTF16LE_Encoder::put(char32_t p_char)
{
	if(!core::UNICODE_Compliant(p_char)) return stream_error::BadEncoding;

	char16_t temp[2];
	uint8_t ret = core::encode_UTF16(p_char, temp);

	temp[0] = core::endian_host2little(temp[0]);
	if(ret == 2)
	{
		temp[1] = core::endian_host2little(temp[1]);
	}
	return m_writer.write(temp, ret * sizeof(char16_t));
}

stream_error Stream_UTF16LE_Encoder::put(std::u32string_view p_string)
{
	if(!core::UCS4_UNICODE_Compliant(p_string)) return stream_error::BadEncoding;
	for(char32_t tchar : p_string)
	{
		char16_t temp[2];
		uint8_t ret = core::encode_UTF16(tchar, temp);

		temp[0] = core::endian_host2little(temp[0]);
		if(ret == 2)
		{
			temp[1] = core::endian_host2little(temp[1]);
		}
		if(m_writer.write(temp, ret * sizeof(char16_t)) != stream_error::None) return stream_error::Unable2Write;
	}
	return stream_error::None;
}

stream_error Stream_UTF16LE_Encoder::put(std::u8string_view p_string)
{
	for(char8_t tchar : p_string)
	{
		char16_t temp = core::endian_host2little(static_cast<char16_t>(tchar));
		if(m_writer.write(&temp, sizeof(char16_t)) != stream_error::None) return stream_error::Unable2Write;
	}
	return stream_error::None;
}


//======== ======== class:  ======== ========
stream_error Stream_UTF16BE_Encoder::put(char32_t p_char)
{
	if(!core::UNICODE_Compliant(p_char)) return stream_error::BadEncoding;

	char16_t temp[2];
	uint8_t ret = core::encode_UTF16(p_char, temp);

	temp[0] = core::endian_host2big(temp[0]);
	if(ret == 2)
	{
		temp[1] = core::endian_host2big(temp[1]);
	}
	return m_writer.write(temp, ret * sizeof(char16_t));
}

stream_error Stream_UTF16BE_Encoder::put(std::u32string_view p_string)
{
	if(!core::UCS4_UNICODE_Compliant(p_string)) return stream_error::BadEncoding;
	for(char32_t tchar : p_string)
	{
		char16_t temp[2];
		uint8_t ret = core::encode_UTF16(tchar, temp);

		temp[0] = core::endian_host2big(temp[0]);
		if(ret == 2)
		{
			temp[1] = core::endian_host2big(temp[1]);
		}
		if(m_writer.write(temp, ret * sizeof(char16_t)) != stream_error::None) return stream_error::Unable2Write;
	}
	return stream_error::None;
}

stream_error Stream_UTF16BE_Encoder::put(std::u8string_view p_string)
{
	for(char8_t tchar : p_string)
	{
		char16_t temp = core::endian_host2big(static_cast<char16_t>(tchar));
		if(m_writer.write(&temp, sizeof(char16_t)) != stream_error::None) return stream_error::Unable2Write;
	}
	return stream_error::None;
}


//======== ======== class:  ======== ========
stream_error Stream_UCS4LE_Encoder::put(char32_t p_char)
{
	p_char = core::endian_host2little(p_char);
	return m_writer.write(reinterpret_cast<void*>(&p_char), sizeof(char32_t));
}

stream_error Stream_UCS4LE_Encoder::put(std::u32string_view p_string)
{
	for(char32_t tchar : p_string)
	{
		tchar = core::endian_host2little(tchar);
		if(m_writer.write(reinterpret_cast<void*>(&tchar), sizeof(char32_t)) != stream_error::None) return stream_error::Unable2Write;
	}
	return stream_error::None;
}

stream_error Stream_UCS4LE_Encoder::put(std::u8string_view p_string)
{
	for(char8_t lchar : p_string)
	{
		char32_t tchar = core::endian_host2little(static_cast<char32_t>(lchar));
		if(m_writer.write(reinterpret_cast<void*>(&tchar), sizeof(char32_t)) != stream_error::None) return stream_error::Unable2Write;
	}
	return stream_error::None;
}


//======== ======== class:  ======== ========
stream_error Stream_UCS4LE_Encoder_s::put(char32_t p_char)
{
	if(!core::UNICODE_Compliant(p_char)) return stream_error::BadEncoding;
	p_char = core::endian_host2little(p_char);
	return m_writer.write(reinterpret_cast<void*>(&p_char), sizeof(char32_t));
}

stream_error Stream_UCS4LE_Encoder_s::put(std::u32string_view p_string)
{
	if(!core::UCS4_UNICODE_Compliant(p_string)) return stream_error::BadEncoding;
	for(char32_t tchar : p_string)
	{
		tchar = core::endian_host2little(tchar);
		if(m_writer.write(reinterpret_cast<void*>(&tchar), sizeof(char32_t)) != stream_error::None) return stream_error::Unable2Write;
	}
	return stream_error::None;
}

stream_error Stream_UCS4LE_Encoder_s::put(std::u8string_view p_string)
{
	for(char8_t lchar : p_string)
	{
		char32_t tchar = core::endian_host2little(static_cast<char32_t>(lchar));
		if(m_writer.write(reinterpret_cast<void*>(&tchar), sizeof(char32_t)) != stream_error::None) return stream_error::Unable2Write;
	}
	return stream_error::None;
}


//======== ======== class:  ======== ========
stream_error Stream_UCS4BE_Encoder::put(char32_t p_char)
{
	p_char = core::endian_host2big(p_char);
	return m_writer.write(reinterpret_cast<void*>(&p_char), sizeof(char32_t));
}

stream_error Stream_UCS4BE_Encoder::put(std::u32string_view p_string)
{
	for(char32_t tchar : p_string)
	{
		tchar = core::endian_host2big(tchar);
		if(m_writer.write(reinterpret_cast<void*>(&tchar), sizeof(char32_t)) != stream_error::None) return stream_error::Unable2Write;
	}
	return stream_error::None;
}

stream_error Stream_UCS4BE_Encoder::put(std::u8string_view p_string)
{
	for(char8_t lchar : p_string)
	{
		char32_t tchar = core::endian_host2big(static_cast<char32_t>(lchar));
		if(m_writer.write(reinterpret_cast<void*>(&tchar), sizeof(char32_t)) != stream_error::None) return stream_error::Unable2Write;
	}
	return stream_error::None;
}


//======== ======== class:  ======== ========
stream_error Stream_UCS4BE_Encoder_s::put(char32_t p_char)
{
	if(!core::UNICODE_Compliant(p_char)) return stream_error::BadEncoding;
	p_char = core::endian_host2big(p_char);
	return m_writer.write(reinterpret_cast<void*>(&p_char), sizeof(char32_t));
}

stream_error Stream_UCS4BE_Encoder_s::put(std::u32string_view p_string)
{
	if(!core::UCS4_UNICODE_Compliant(p_string)) return stream_error::BadEncoding;
	for(char32_t tchar : p_string)
	{
		tchar = core::endian_host2big(tchar);
		if(m_writer.write(reinterpret_cast<void*>(&tchar), sizeof(char32_t)) != stream_error::None) return stream_error::Unable2Write;
	}
	return stream_error::None;
}

stream_error Stream_UCS4BE_Encoder_s::put(std::u8string_view p_string)
{
	for(char8_t lchar : p_string)
	{
		char32_t tchar = core::endian_host2big(static_cast<char32_t>(lchar));
		if(m_writer.write(reinterpret_cast<void*>(&tchar), sizeof(char32_t)) != stream_error::None) return stream_error::Unable2Write;
	}
	return stream_error::None;
}

}	// namespace ENCODER_P
}	// namespace udef
