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

#include "scef_format.hpp"

#include <cstdint>
#include <CoreLib/Core_String.hpp>

#include "scef_encoder.hpp"
#include "scef_danger_act_p.hpp"


namespace scef::format
{

namespace
{
	static bool startWarp(char32_t p_char, void*)
	{
		return _p::is_space(p_char);
	}


	static bool spaceWarp(char32_t p_char, void*)
	{
		return _p::is_space_noLF(p_char);
	}


	struct genCount
	{
		stream_decoder& m_decoder;
		uint8_t m_count = 0;
	};

	static bool defValidator(char32_t p_char, void* p_context)
	{
		genCount& context = *reinterpret_cast<genCount*>(p_context);
		switch(context.m_count)
		{
			case 0:
				if(p_char != 'C' && p_char != 'c')
				{
					return false;
				}
				break;
			case 1:
				if(p_char != 'E' && p_char != 'e')
				{
					return false;
				}
				break;
			default:
				if(p_char != 'F' && p_char != 'f')
				{
					return false;
				}
				++context.m_count;
				return false;
		}
		++context.m_count;
		return true;
	}

	struct genVersion
	{
		stream_decoder& m_decoder;
		uint8_t m_count = 1;
		char8_t data[5];
	};

	static bool collectVersion(char32_t p_char, void* p_context)
	{
		if(!core::is_digit(p_char)) return false;
		genVersion& context = *reinterpret_cast<genVersion*>(p_context);
		if(++context.m_count > 4) return false;
		context.data[context.m_count] = static_cast<char8_t>(p_char);
		return true;
	}

} //namespace


Error FinishVersionDecoding(stream_decoder& p_decoder, uint16_t& p_version, Error_Context& p_err)
{
	//find first "!"
	// ! scef : v = 12345
	// ^
	stream_error ret = p_decoder.read_while(startWarp, nullptr);
	if(ret != stream_error::None)
	{
		if(ret == stream_error::Control_EndOfStream)
		{
			return Error::Control_NoHeader;
		}
	}
	if(p_decoder.lastChar() != '!') return Error::Control_NoHeader;


	_p::Danger_Action::publicError(p_err).set_position(p_decoder.line(), 0);

	// ! scef : v = 12345
	//   ^
	ret = p_decoder.read_while(spaceWarp, nullptr);
	if(ret != stream_error::None)
	{
		return Error::BadFormat;
	}
	//the next 4 characters better spell SCEF
	if((p_decoder.lastChar() != 'S' && p_decoder.lastChar() != 's'))
	{
		return Error::BadFormat;
	}

	{
		genCount context{p_decoder};
		ret = p_decoder.read_while(defValidator, &context);

		if(context.m_count < 3)
		{
			return Error::BadFormat;
		}
	}

	// ! scef : v = 12345
	//        ^
	ret = p_decoder.read_while(spaceWarp, nullptr);
	if(ret != stream_error::None || p_decoder.lastChar() != ':')
	{
		return Error::BadFormat;
	}

	// ! scef : v = 12345
	//          ^
	ret = p_decoder.read_while(spaceWarp, nullptr);
	if(ret != stream_error::None || (p_decoder.lastChar() != 'V' && p_decoder.lastChar() != 'v'))
	{
		return Error::BadFormat;
	}


	// ! scef : v = 12345
	//            ^
	ret = p_decoder.read_while(spaceWarp, nullptr);
	if(ret != stream_error::None || p_decoder.lastChar() != '=')
	{
		return Error::BadFormat;
	}


	// ! scef : v = 12345
	//              ^
	ret = p_decoder.read_while(spaceWarp, nullptr);
	if(ret != stream_error::None)
	{
		return Error::BadFormat;
	}

	if(!core::is_digit(p_decoder.lastChar()) || p_decoder.lastChar() == '0')
	{
		return Error::BadFormat;
	}

	{
		genVersion context{p_decoder};
		context.data[0] = static_cast<char8_t>(p_decoder.lastChar());

		ret = p_decoder.read_while(collectVersion, &context);
		if(ret != stream_error::None)
		{
			return Error::BadFormat;
		}
		if(!_p::is_space(p_decoder.lastChar()))
		{
			if(core::is_digit(p_decoder.lastChar()))
			{
				return Error::UnsuportedVersion;
			}
			return Error::BadFormat;
		}

		core::from_chars_result<uint16_t> num_res = core::from_chars<uint16_t>(std::u8string_view{context.data, context.m_count});
		if(!num_res.has_value())
		{
			return Error::UnsuportedVersion;
		}
		p_version = num_res.value();
	}

	// ! scef : v = 12345 
	//                    ^
	if(p_decoder.lastChar() != '\n')
	{
		ret = p_decoder.read_while(spaceWarp, nullptr);
		if(ret != stream_error::None || p_decoder.lastChar() != '\n')
		{
			return Error::BadFormat;
		}
	}

	return Error::None;
}


constexpr std::u8string_view SCEF_SIG = u8"!SCEF:V=";

Error WriteVersion(stream_encoder& p_encoder, uint16_t p_version)
{
	stream_error ret = p_encoder.put_flat(SCEF_SIG);

	if(ret != stream_error::None)
	{
		return static_cast<Error>(ret);
	}
	{
		std::array<char8_t, core::to_chars_dec_max_digits_v<uint16_t>> t_buff;
		ret = p_encoder.put_flat(std::u8string_view{t_buff.data(), core::to_chars(p_version, t_buff)});
		if(ret != stream_error::None)
		{
			return static_cast<Error>(ret);
		}
	}
	return static_cast<Error>(p_encoder.put_control(U'\n'));
}

}	//namespace acef::format
