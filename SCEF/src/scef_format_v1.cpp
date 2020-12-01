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

#include "scef_format_v1.hpp"

#include <CoreLib/Core_String.hpp>

#include "SCEF/scef_items.hpp"
#include "scef_encoder.hpp"
#include "scef_danger_act_p.hpp"

namespace scef::format::v1
{


enum Format_Context
{
	context_root			= 0x00,
	context_space			= 0x01,
	context_object_header	= 0x02,
	context_object_body		= 0x03,
	context_keypair_l		= 0x04,
	context_keypair_r		= 0x05,
	context_singlet			= 0x06,
	context_list			= 0x07,
	context_escape			= 0x08,
	context_comment			= 0x09,
	context_error			= 0xFF
};



inline static constexpr bool is_dangerCodepoint(char32_t p_char)
{
	return p_char < ' ';
}

inline static constexpr bool is_badCodePoint(char32_t p_char)
{
	return is_dangerCodepoint(p_char) && !_p::is_no_printSpace(p_char);
}

constexpr uint8_t MAX_LEVEL = 10;

struct ReaderFlow
{
	inline ReaderFlow(stream_decoder& p_decoder, _Warning_Def& p_warn)
		: m_decoder(p_decoder)
		, m_warnDef(p_warn)
	{
	}

	stream_decoder& m_decoder;
	_Warning_Def&	m_warnDef;
	bool			m_skipSpaces;
	bool			m_skipComments;
};

static bool skipUntilNewLine(char32_t p_char, void*)
{
	return p_char != '\n' && !is_badCodePoint(p_char);
}

static bool loadUntilNewLine(char32_t p_char, void* p_context)
{
	if(p_char == '\n' || is_badCodePoint(p_char))
	{
		return false;
	}

	reinterpret_cast<std::u32string*>(p_context)->push_back(p_char);
	return true;
}

static bool skipInlineSpacing(char32_t p_char, void*)
{
	return _p::is_space_noLF(p_char);
}

static bool skipMultilineSpacing(char32_t p_char, void*)
{
	return _p::is_space(p_char);
}

static bool loadInlineSpacing(char32_t p_char, void* p_context)
{
	if(_p::is_space_noLF(p_char))
	{
		reinterpret_cast<std::u8string*>(p_context)->push_back(static_cast<char8_t>(p_char));
		return true;
	}
	return false;
}

struct multiline_spacing_helper
{
	std::u8string m_spacing;
	uint64_t m_line_count = 0;
};

static bool loadMultilineSpacing(char32_t p_char, void* p_context)
{
	multiline_spacing_helper* context = reinterpret_cast<multiline_spacing_helper*>(p_context);

	if(_p::is_space(p_char))
	{
		if(p_char == '\n')
		{
			++(context->m_line_count);
			context->m_spacing.clear();
		}
		else context->m_spacing.push_back(static_cast<char8_t>(p_char));
		return true;
	}

	return false;
}

static bool loadNameNoQuote(char32_t p_char, void* p_context)
{
	switch(p_char)
	{
		case ' ':
		case '\"':
		case '#':
		case '\'':
		case ',':
		case ':':
		case ';':
		case '<':
		case '=':
		case '>':
			return false;
		default:
			if(is_dangerCodepoint(p_char)) return false;
			reinterpret_cast<std::u32string*>(p_context)->push_back(p_char);
			break;
	}

	return true;
}

static bool loadSingleQuote(char32_t p_char, void* p_context)
{
	switch(p_char)
	{
		case '\n':
		case '\'':
		case '^':
			return false;
		default:
			if(is_badCodePoint(p_char)) return false;
			reinterpret_cast<std::u32string*>(p_context)->push_back(p_char);
			break;
	}

	return true;
}

static bool loadDoubleQuote(char32_t p_char, void* p_context)
{
	switch(p_char)
	{
		case '\n':
		case '\"':
		case '^':
			return false;
		default:
			if(is_badCodePoint(p_char)) return false;
			reinterpret_cast<std::u32string*>(p_context)->push_back(p_char);
			break;
	}

	return true;
}

static bool trashNameNoQuote(char32_t p_char, void*)
{
	switch(p_char)
	{
		case ' ':
		case '\"':
		case '#':
		case '\'':
		case ',':
		case ':':
		case ';':
		case '<':
		case '=':
		case '>':
			return false;
		default:
			if(is_dangerCodepoint(p_char)) return false;
			break;
	}
	return true;
}

static bool trashSingleQuote(char32_t p_char, void*)
{
	switch(p_char)
	{
		case '\n':
		case '\'':
		case '^':
			return false;
		default:
			if(is_badCodePoint(p_char)) return false;
			break;
	}
	return true;
}

static bool trashDoubleQuote(char32_t p_char, void*)
{
	switch(p_char)
	{
		case '\n':
		case '\"':
		case '^':
			return false;
		default:
			if(is_badCodePoint(p_char)) return false;
			break;
	}
	return true;
}


struct escape_helper
{
	char32_t buff[8];
	uint8_t count = 0;
};

static bool loadEscapeQuad(char32_t p_char, void* p_context)
{
	escape_helper& context = *reinterpret_cast<escape_helper*>(p_context);

	if(core::is_xdigit(p_char))
	{
		context.buff[context.count] = p_char;
		if(++context.count < 4) return true;
	}

	return false;
}

static bool loadEscapeOcta(char32_t p_char, void* p_context)
{
	escape_helper& context = *reinterpret_cast<escape_helper*>(p_context);

	if(core::is_xdigit(p_char))
	{
		context.buff[context.count] = p_char;
		if(++context.count < 8) return true;
	}

	return false;
}

static Error ReadComment(ReaderFlow& p_flow, comment& p_comment)
{
	p_comment.set_position(p_flow.m_decoder.line(), p_flow.m_decoder.column() - 1);
	std::u32string temp;
	stream_error ret = p_flow.m_decoder.read_while(loadUntilNewLine, &temp);
	p_comment.set(temp);
	if(ret != stream_error::None)
	{
		_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_criticalItem = &p_comment;
		return static_cast<Error>(ret);
	}
	if(p_flow.m_decoder.lastChar() != '\n')
	{
		return Error::BadFormat;
	}

	return static_cast<Error>(p_flow.m_decoder.get_char().error_code());
}

static Error ReadCommentSkip(ReaderFlow& p_flow)
{
	stream_error ret = p_flow.m_decoder.read_while(skipUntilNewLine, nullptr);
	if(ret != stream_error::None) return static_cast<Error>(ret);
	if(p_flow.m_decoder.lastChar() != '\n')
	{
		return Error::BadFormat;
	}

	return static_cast<Error>(p_flow.m_decoder.get_char().error_code());
}

static Error ReadSpaceSkip(ReaderFlow& p_flow)
{
	return static_cast<Error>(p_flow.m_decoder.read_while(skipMultilineSpacing, nullptr));
}

static Error ReadSpace(ReaderFlow& p_flow, spacer& p_spacer)
{
	p_spacer.set_position(p_flow.m_decoder.line(), p_flow.m_decoder.column() - 1);
	multiline_spacing_helper data;
	stream_error ret = p_flow.m_decoder.read_while(loadMultilineSpacing, &data);

	_p::Danger_Action::setlineCount(p_spacer, data.m_line_count);
	_p::Danger_Action::move_spacing(p_spacer, data.m_spacing);
	return static_cast<Error>(ret);
}


static Error ReadTrashEscapeSequence(ReaderFlow& p_flow)
{
	_Warning_Def& twarn		= p_flow.m_warnDef;
	stream_decoder& decoder	= p_flow.m_decoder;

	_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column());

	stream_decoder::result_t temp = decoder.get_char();
	if(temp.error_code() != stream_error::None)
	{
		return static_cast<Error>(temp.error_code());
	}

	switch(temp.value())
	{
		case '\'':
		case '\"':
		case '^':
			return static_cast<Error>(decoder.get_char().error_code());
		default:
			break;
	}

	return Error::None;
}


static Error ReadTrashSingleQuote(ReaderFlow& p_flow)
{
	stream_decoder& decoder	= p_flow.m_decoder;

	Error lastError;
	do
	{
		lastError = static_cast<Error>(decoder.read_while(trashSingleQuote, nullptr));

magic$continuation:
		if(lastError != Error::None)
		{
			return lastError;
		}

		switch(decoder.lastChar())
		{
			case '\n':
				return Error::None;
			case '\'':
				return static_cast<Error>(decoder.get_char().error_code());
			case '^':
				//Handle escape sequence
				lastError = ReadTrashEscapeSequence(p_flow);
				goto magic$continuation;
			default:
				break;
		}
	} while(true);
}

static Error ReadTrashDoubleQuote(ReaderFlow& p_flow)
{
	stream_decoder& decoder	= p_flow.m_decoder;

	Error lastError;
	do
	{
		lastError = static_cast<Error>(decoder.read_while(trashDoubleQuote, nullptr));

magic$continuation:
		if(lastError != Error::None)
		{
			return lastError;
		}

		switch(decoder.lastChar())
		{
			case '\n':
				return Error::None;
			case '\"':
				return static_cast<Error>(decoder.get_char().error_code());
			case '^':
				//Handle escape sequence
				lastError = ReadTrashEscapeSequence(p_flow);
				goto magic$continuation;
			default:
				break;
		}
	} while(true);
}

static Error ReadTrashSequence(ReaderFlow& p_flow)
{
	stream_decoder& decoder	= p_flow.m_decoder;
	Error lastError;

	switch(decoder.lastChar())
	{
		case '\'':
			lastError = ReadTrashSingleQuote(p_flow);
			break;
		case '\"':
			lastError = ReadTrashDoubleQuote(p_flow);
			break;
		default:
			lastError = static_cast<Error>(decoder.read_while(trashNameNoQuote, nullptr));
			break;
	}

	while(lastError == Error::None)
	{
		char32_t tchar = decoder.lastChar();
		switch(tchar)
		{
			case '\n':
			case '#':
			case ',':
			case ':':
			case ';':
			case '<':
			case '=':
			case '>':
				return Error::None;
			case '\'':
				lastError = ReadTrashSingleQuote(p_flow);
				break;
			case '\"':
				lastError = ReadTrashDoubleQuote(p_flow);
				break;
			default:
				if(_p::is_space_noLF(tchar)) return Error::None;
				lastError = static_cast<Error>(decoder.read_while(trashNameNoQuote, nullptr));
				break;
		}
	}

	return lastError;
}

static Error ReadEscapeSequence(ReaderFlow& p_flow, std::u32string& p_out)
{
	_Warning_Def& twarn		= p_flow.m_warnDef;
	stream_decoder& decoder	= p_flow.m_decoder;

	_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column());

	stream_decoder::result_t temp = decoder.get_char();
	if(temp.error_code() != stream_error::None)
	{
		return static_cast<Error>(temp.error_code());
	}

	char32_t tchar = temp.value();
	switch(tchar)
	{
		case '\'':
		case '\"':
		case '^':
			p_out.push_back(tchar);
			break;
		case 'n':
			p_out.push_back('\n');
			break;
		case 't':
			p_out.push_back('\t');
			break;
		case 'r':
			p_out.push_back('\r');
			break;
		case 'u':
			{
				escape_helper thelper;
				stream_error ret = decoder.read_while(loadEscapeQuad, &thelper);
				if(thelper.count == 4)
				{
					p_out.push_back(reinterpret_cast<char32_t&>(core::from_hex_chars<uint32_t>({thelper.buff, 4}).value()));
					break;
				}
				if(ret != stream_error::None)
				{
					if(ret != stream_error::Control_EndOfStream)
					{
						return static_cast<Error>(ret);
					}
					_p::Danger_Action::publicError(*twarn._error_context).SetErrorEscape(thelper.buff, thelper.count);
				}
				else
				{
					thelper.buff[thelper.count] = decoder.lastChar();
					_p::Danger_Action::publicError(*twarn._error_context).SetErrorEscape(thelper.buff, thelper.count + 1);
				}
				switch(p_flow.m_warnDef.Notify())
				{
					case warningBehaviour::Discard:
						break;
					case warningBehaviour::Continue:
						p_out.push_back('^');
						p_out.append(thelper.buff, thelper.count);
						break;
					case warningBehaviour::Default:
					case warningBehaviour::Accept:
						p_out.push_back(reinterpret_cast<char32_t&>(core::from_hex_chars<uint32_t>({thelper.buff, thelper.count}).value()));
						break;
					case warningBehaviour::Abort:
					default:
						return Error::BadEscape;
				}
				return static_cast<Error>(ret);
			}
			break;
		case 'U':
			{
				escape_helper thelper;
				stream_error ret = decoder.read_while(loadEscapeOcta, &thelper);
				if(thelper.count == 8)
				{
					p_out.push_back(reinterpret_cast<char32_t&>(core::from_hex_chars<uint32_t>({thelper.buff, 8}).value()));
					break;
				}
				if(ret != stream_error::None)
				{
					if(ret != stream_error::Control_EndOfStream)
					{
						return static_cast<Error>(ret);
					}
					_p::Danger_Action::publicError(*twarn._error_context).SetErrorEscape(thelper.buff, thelper.count);
				}
				else
				{
					thelper.buff[thelper.count] = decoder.lastChar();
					_p::Danger_Action::publicError(*twarn._error_context).SetErrorEscape(thelper.buff, thelper.count + 1);
				}
				switch(p_flow.m_warnDef.Notify())
				{
					case warningBehaviour::Discard:
						break;
					case warningBehaviour::Continue:
						p_out.push_back('^');
						p_out.append(thelper.buff, thelper.count);
						break;
					case warningBehaviour::Default:
					case warningBehaviour::Accept:
						p_out.push_back(reinterpret_cast<char32_t&>(core::from_hex_chars<uint32_t>({thelper.buff, thelper.count}).value()));
						break;
					case warningBehaviour::Abort:
					default:
						return Error::BadEscape;
				}
				return static_cast<Error>(ret);
			}
			break;
		default:
			if(core::is_xdigit(tchar))
			{
				char32_t buff[2];
				buff[0]= tchar;
				temp = decoder.get_char();
				if(temp.error_code() != stream_error::None)
				{
					if(temp.error_code() != stream_error::Control_EndOfStream)
					{
						return static_cast<Error>(temp.error_code());
					}
					_p::Danger_Action::publicError(*twarn._error_context).SetErrorEscape(buff, 1);
				}
				else
				{
					buff[1]= temp.value();
					if(core::is_xdigit(temp.value()))
					{
						p_out.push_back(reinterpret_cast<char32_t&>(core::from_hex_chars<uint32_t>({buff, 2}).value()));
						break;
					}
					_p::Danger_Action::publicError(*twarn._error_context).SetErrorEscape(buff, 2);
				}

				switch(p_flow.m_warnDef.Notify())
				{
					case warningBehaviour::Discard:
						break;
					case warningBehaviour::Continue:
						p_out.push_back('^');
						p_out.push_back(tchar);
						break;
					case warningBehaviour::Default:
					case warningBehaviour::Accept:
						p_out.push_back(reinterpret_cast<char32_t&>(core::from_hex_chars<uint32_t>({&tchar, 1}).value()));
						break;
					case warningBehaviour::Abort:
					default:
						return Error::BadEscape;
				}
				return static_cast<Error>(temp.error_code());
			}
			_p::Danger_Action::publicError(*twarn._error_context).SetErrorEscape(&tchar, 1);
			switch(p_flow.m_warnDef.Notify())
			{
				case warningBehaviour::Continue:
					p_out.push_back('^');
					break;
				case warningBehaviour::Discard:
				case warningBehaviour::Default:
				case warningBehaviour::Accept:
					break;
				case warningBehaviour::Abort:
				default:
					return Error::BadEscape;
			}

			return Error::None;
	}

	return static_cast<Error>(decoder.get_char().error_code());
}

static Error ReadSingleQuote(ReaderFlow& p_flow, std::u32string& p_out)
{
	_Warning_Def& twarn		= p_flow.m_warnDef;
	stream_decoder& decoder	= p_flow.m_decoder;

	Error lastError;
	do
	{
		lastError = static_cast<Error>(decoder.read_while(loadSingleQuote, &p_out));

magic$continuation:
		if(lastError != Error::None)
		{
			if(lastError == Error::Control_EndOfStream)
			{
				_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column());
				_p::Danger_Action::publicError(*twarn._error_context).SetErrorPrematureEnding('\'');
				switch(twarn.Notify())
				{
					case warningBehaviour::Continue:
					case warningBehaviour::Default:
					case warningBehaviour::Discard:
					case warningBehaviour::Accept:
						break;
					case warningBehaviour::Abort:
					default:
						return Error::PrematureEnd;
				}
			}
			return lastError;
		}

		switch(decoder.lastChar())
		{
			case '\n':
				_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
				_p::Danger_Action::publicError(*twarn._error_context).SetErrorInvalidChar(decoder.lastChar(), '\'');
				switch(twarn.Notify())
				{
					case warningBehaviour::Continue:
					case warningBehaviour::Default:
					case warningBehaviour::Discard:
					case warningBehaviour::Accept:
						break;
					case warningBehaviour::Abort:
					default:
						return Error::InvalidChar;
				}
				return Error::None;
			case '\'':
				return static_cast<Error>(decoder.get_char().error_code());
			case '^':
				//Handle escape sequence
				lastError = ReadEscapeSequence(p_flow, p_out);
				goto magic$continuation;
			default:
				p_out.push_back(decoder.lastChar());
				break;
		}
	} while(true);
}

static Error ReadDoubleQuote(ReaderFlow& p_flow, std::u32string& p_out)
{
	_Warning_Def& twarn		= p_flow.m_warnDef;
	stream_decoder& decoder	= p_flow.m_decoder;

	Error lastError;
	do
	{
		lastError = static_cast<Error>(decoder.read_while(loadDoubleQuote, &p_out));

magic$continuation:
		if(lastError != Error::None)
		{
			if(lastError == Error::Control_EndOfStream)
			{
				_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column());
				_p::Danger_Action::publicError(*twarn._error_context).SetErrorPrematureEnding('\"');
				switch(twarn.Notify())
				{
					case warningBehaviour::Continue:
					case warningBehaviour::Default:
					case warningBehaviour::Discard:
					case warningBehaviour::Accept:
						break;
					case warningBehaviour::Abort:
					default:
						return Error::PrematureEnd;
				}
			}
			return lastError;
		}

		switch(decoder.lastChar())
		{
			case '\n':
				_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
				_p::Danger_Action::publicError(*twarn._error_context).SetErrorInvalidChar(decoder.lastChar(), '\"');
				switch(twarn.Notify())
				{
					case warningBehaviour::Continue:
					case warningBehaviour::Default:
					case warningBehaviour::Discard:
					case warningBehaviour::Accept:
						break;
					case warningBehaviour::Abort:
					default:
						return Error::InvalidChar;
				}
				return Error::None;
			case '\"':
				return static_cast<Error>(decoder.get_char().error_code());
			case '^':
				//Handle escape sequence
				lastError = ReadEscapeSequence(p_flow, p_out);
				goto magic$continuation;
			default:
				p_out.push_back(decoder.lastChar());
				break;
		}
	} while(true);
}

static Error ReadName(ReaderFlow& p_flow, std::u32string& p_out, QuotationMode& p_quotMode)
{
	_Warning_Def& twarn		= p_flow.m_warnDef;
	stream_decoder& decoder	= p_flow.m_decoder;

	p_quotMode = QuotationMode::standard;
	Error lastError;

	switch(decoder.lastChar())
	{
		case '\'':
			p_quotMode = QuotationMode::singlemark;
			lastError = ReadSingleQuote(p_flow, p_out);
			break;
		case '\"':
			p_quotMode = QuotationMode::doublemark;
			lastError = ReadDoubleQuote(p_flow, p_out);
			break;
		default:
			p_out.push_back(decoder.lastChar());
			lastError = static_cast<Error>(decoder.read_while(loadNameNoQuote, &p_out));
			break;
	}

	switch(decoder.lastChar())
	{
		case '\n':
		case '#':
		case ',':
		case ':':
		case ';':
		case '<':
		case '=':
		case '>':
			return Error::None;
		default:
			if(_p::is_space_noLF(decoder.lastChar())) return Error::None;
			break;
	}

	_p::Danger_Action::publicError(*twarn._error_context).SetPlainError(Error::MergedText);
	switch(p_flow.m_warnDef.Notify())
	{
		case warningBehaviour::Default:
		case warningBehaviour::Continue:
		case warningBehaviour::Accept:
			break;
		case warningBehaviour::Discard:
			return ReadTrashSequence(p_flow);
		case warningBehaviour::Abort:
		default:
			return Error::MergedText;
	}

	while(lastError == Error::None)
	{
		char32_t tchar = decoder.lastChar();
		switch(tchar)
		{
			case '\n':
			case '#':
			case ',':
			case ':':
			case ';':
			case '<':
			case '=':
			case '>':
				return Error::None;
			case '\'':
				p_quotMode = QuotationMode::singlemark;
				lastError = ReadSingleQuote(p_flow, p_out);
				break;
			case '\"':
				p_quotMode = QuotationMode::doublemark;
				lastError = ReadDoubleQuote(p_flow, p_out);
				break;
			default:
				if(_p::is_space_noLF(tchar)) return Error::None;
				p_out.push_back(tchar);
				lastError = static_cast<Error>(decoder.read_while(loadNameNoQuote, &p_out));
				break;
		}
	}

	return lastError;
}

static Error ReadKeyValue(ReaderFlow& p_flow, keyedValue& p_keyValue, ItemList& p_list)
{
	_Warning_Def& twarn = p_flow.m_warnDef;
	stream_decoder& decoder = p_flow.m_decoder;
	uint64_t line = decoder.line();
	uint64_t column = decoder.column();

	stream_error str_err = decoder.get_char().error_code();
	_p::Danger_Action::publicError(*twarn._error_context).m_criticalItem = &p_keyValue;

	switch(str_err)
	{
		case stream_error::None:
			break;
		case stream_error::Control_EndOfStream:
			_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column());
			_p::Danger_Action::publicError(*twarn._error_context).SetErrorPrematureEnding(';');
			switch(twarn.Notify())
			{
				case warningBehaviour::Continue:
				case warningBehaviour::Default:
				case warningBehaviour::Discard:
				case warningBehaviour::Accept:
					break;
				case warningBehaviour::Abort:
				default:
					return Error::InvalidChar;
			}
			_p::Danger_Action::publicError(*twarn._error_context).m_criticalItem = nullptr;
			[[fallthrough]];
		default:
			return static_cast<Error>(str_err);
	}

	std::u8string tspacing;

	if(_p::is_space_noLF(decoder.lastChar()))
	{
		if(p_flow.m_skipSpaces)
		{
			str_err = decoder.read_while(skipInlineSpacing, nullptr);
		}
		else
		{
			str_err = decoder.read_while(loadInlineSpacing, &tspacing);
		}

		switch(str_err)
		{
			case stream_error::None:
				break;
			case stream_error::Control_EndOfStream:
				_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column());
				_p::Danger_Action::publicError(*twarn._error_context).SetErrorPrematureEnding(';');
				switch(twarn.Notify())
				{
					case warningBehaviour::Continue:
					case warningBehaviour::Default:
					case warningBehaviour::Discard:
					case warningBehaviour::Accept:
						break;
					case warningBehaviour::Abort:
					default:
						return Error::InvalidChar;
				}
				_p::Danger_Action::publicError(*twarn._error_context).m_criticalItem = nullptr;
				[[fallthrough]];
			default:
				return static_cast<Error>(str_err);
		}
	}

	char32_t tchar = decoder.lastChar();
	switch(tchar)
	{
		case ':':
			_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
			_p::Danger_Action::publicError(*twarn._error_context).SetErrorInvalidChar(tchar, ';');
			switch(twarn.Notify())
			{
				case warningBehaviour::Continue:
				case warningBehaviour::Default:
				case warningBehaviour::Discard:
				case warningBehaviour::Accept:
					break;
				case warningBehaviour::Abort:
				default:
					return Error::InvalidChar;
			}
			[[fallthrough]];
		case ',':
		case ';':
			_p::Danger_Action::move_spacing(p_keyValue.m_midSpace, tspacing);
			_p::Danger_Action::publicError(*twarn._error_context).m_criticalItem = nullptr;
			return static_cast<Error>(decoder.get_char().error_code());
		case '#':
		case '<':
		case '=':
		case '>':
			if(!tspacing.empty())
			{
				itemProxy<spacer> t_item = spacer::make();
				t_item->set_position(line, column);
				p_list.push_back(t_item);
				_p::Danger_Action::move_spacing(*t_item, tspacing);
			}
			_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
			_p::Danger_Action::publicError(*twarn._error_context).SetErrorInvalidChar(decoder.lastChar(), ';');
			switch(twarn.Notify())
			{
				case warningBehaviour::Continue:
				case warningBehaviour::Default:
				case warningBehaviour::Discard:
				case warningBehaviour::Accept:
					break;
				case warningBehaviour::Abort:
				default:
					return Error::InvalidChar;
			}
			_p::Danger_Action::publicError(*twarn._error_context).m_criticalItem = nullptr;
			return Error::None;
		case '\n':
			_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
			_p::Danger_Action::publicError(*twarn._error_context).SetErrorInvalidChar(decoder.lastChar(), ';');
			switch(twarn.Notify())
			{
				case warningBehaviour::Continue:
				case warningBehaviour::Default:
				case warningBehaviour::Discard:
				case warningBehaviour::Accept:
					break;
				case warningBehaviour::Abort:
				default:
					return Error::InvalidChar;
			}
			_p::Danger_Action::publicError(*twarn._error_context).m_criticalItem = nullptr;
			return Error::None;
		default:
			if(is_dangerCodepoint(tchar))
			{
				_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
				_p::Danger_Action::publicError(*twarn._error_context).SetPlainError(Error::BadFormat);
				return Error::BadFormat;
			}
			break;
	}

	_p::Danger_Action::move_spacing(p_keyValue.m_midSpace, tspacing);

	Error res;
	{
		QuotationMode tmode;
		p_keyValue.set_column_value(decoder.column() - 1);
		res = ReadName(p_flow, p_keyValue.value(), tmode);
		p_keyValue.set_value_quotation_mode(tmode);
	}

	column = decoder.column();

	if(res != Error::None)
	{
		if(res == Error::Control_EndOfStream)
		{
			_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column());
			_p::Danger_Action::publicError(*twarn._error_context).SetErrorPrematureEnding(';');

			switch(twarn.Notify())
			{
				case warningBehaviour::Continue:
				case warningBehaviour::Default:
				case warningBehaviour::Discard:
				case warningBehaviour::Accept:
					break;
				case warningBehaviour::Abort:
				default:
					return Error::InvalidChar;
			}
			_p::Danger_Action::publicError(*twarn._error_context).m_criticalItem = nullptr;
		}

		return res;
	}

	//post spacing
	if(_p::is_space_noLF(decoder.lastChar()))
	{
		if(p_flow.m_skipSpaces)
		{
			str_err = decoder.read_while(skipInlineSpacing, nullptr);
		}
		else
		{
			str_err = decoder.read_while(loadInlineSpacing, &tspacing);
		}

		switch(str_err)
		{
			case stream_error::None:
				break;
			case stream_error::Control_EndOfStream:
				_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column());
				_p::Danger_Action::publicError(*twarn._error_context).SetErrorPrematureEnding(';');
				switch(twarn.Notify())
				{
					case warningBehaviour::Continue:
					case warningBehaviour::Default:
					case warningBehaviour::Discard:
					case warningBehaviour::Accept:
						break;
					case warningBehaviour::Abort:
					default:
						return Error::InvalidChar;
				}
				[[fallthrough]];
			default:
				return static_cast<Error>(str_err);
		}
	}

	tchar = decoder.lastChar();
	switch(tchar)
	{
		case ':':
			_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
			_p::Danger_Action::publicError(*twarn._error_context).SetErrorInvalidChar(':', ';');
			switch(twarn.Notify())
			{
				case warningBehaviour::Continue:
				case warningBehaviour::Default:
				case warningBehaviour::Discard:
				case warningBehaviour::Accept:
					break;
				case warningBehaviour::Abort:
				default:
					return Error::InvalidChar;
			}
			[[fallthrough]];
		case ',':
		case ';':
			_p::Danger_Action::move_spacing(p_keyValue.m_postSpace, tspacing);
			_p::Danger_Action::publicError(*twarn._error_context).m_criticalItem = nullptr;
			return static_cast<Error>(decoder.get_char().error_code());
		case '\n':
			_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
			_p::Danger_Action::publicError(*twarn._error_context).SetErrorInvalidChar('\n', ';');
			switch(twarn.Notify())
			{
				case warningBehaviour::Continue:
				case warningBehaviour::Default:
				case warningBehaviour::Discard:
				case warningBehaviour::Accept:
					break;
				case warningBehaviour::Abort:
				default:
					return Error::InvalidChar;
			}
			break;
		default:
			if(!tspacing.empty())
			{
				itemProxy<spacer> t_item = spacer::make();
				t_item->set_position(line, column);
				p_list.push_back(t_item);
				_p::Danger_Action::move_spacing(*t_item, tspacing);
			}
			_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
			_p::Danger_Action::publicError(*twarn._error_context).SetErrorInvalidChar(tchar, ';');
			switch(twarn.Notify())
			{
				case warningBehaviour::Continue:
				case warningBehaviour::Default:
				case warningBehaviour::Discard:
				case warningBehaviour::Accept:
					break;
				case warningBehaviour::Abort:
				default:
					return Error::InvalidChar;
			}
			break;
	}

	_p::Danger_Action::publicError(*twarn._error_context).m_criticalItem = nullptr;
	return Error::None;
}

static Error ReadTValue(ReaderFlow& p_flow, ItemList& p_list)
{
	_Warning_Def& twarn = p_flow.m_warnDef;
	stream_decoder& decoder = p_flow.m_decoder;

	std::u32string tName;
	QuotationMode tmode;

	uint64_t line = decoder.line();
	uint64_t column = decoder.column() - 1;

	//name
	Error lastError = ReadName(p_flow, tName, tmode);
	if(lastError != Error::None)
	{
		if(lastError == Error::Control_EndOfStream)
		{
			itemProxy<value> t_value = value::make();
			t_value->set_position(line, column);
			p_list.push_back(t_value);
			t_value->set_name(tName);
			t_value->set_quotation_mode(tmode);
			_p::Danger_Action::publicError(*twarn._error_context).m_criticalItem = t_value.get();
			_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column());
			_p::Danger_Action::publicError(*twarn._error_context).SetErrorPrematureEnding(';');
			switch(twarn.Notify())
			{
				case warningBehaviour::Continue:
				case warningBehaviour::Default:
				case warningBehaviour::Discard:
				case warningBehaviour::Accept:
					break;
				case warningBehaviour::Abort:
				default:
					return Error::PrematureEnd;
			}
		}
		_p::Danger_Action::publicError(*twarn._error_context).m_criticalItem = nullptr;
		return lastError;
	}
	char32_t lchar = decoder.lastChar();

	std::u8string tspacing;
	uint64_t spacing_column = decoder.column() - 1;
	if(_p::is_space_noLF(decoder.lastChar()))
	{
		tspacing.push_back(static_cast<char8_t>(decoder.lastChar()));
		stream_error str_err;
		if(p_flow.m_skipSpaces)
		{
			str_err = decoder.read_while(skipInlineSpacing, nullptr);
		}
		else
		{
			str_err = decoder.read_while(loadInlineSpacing, &tspacing);
		}

		if(str_err != stream_error::None)
		{
			itemProxy<value> t_value = value::make();
			t_value->set_position(line, column);
			p_list.push_back(t_value);
			t_value->set_name(tName);
			t_value->set_quotation_mode(tmode);

			if(str_err != stream_error::Control_EndOfStream)
			{
				_p::Danger_Action::publicError(*twarn._error_context).m_criticalItem = t_value.get();
			}

			return static_cast<Error>(str_err);
		}
	}

	lchar = decoder.lastChar();
	if(lchar != '=')
	{
		itemProxy<value> t_value = value::make();
		t_value->set_position(line, column);
		p_list.push_back(t_value);
		t_value->set_name(tName);
		t_value->set_quotation_mode(tmode);

		switch(lchar)
		{
			case ':':
				_p::Danger_Action::publicError(*twarn._error_context).SetErrorInvalidChar(':', ';');
				_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
				switch(twarn.Notify())
				{
					case warningBehaviour::Continue:
					case warningBehaviour::Default:
					case warningBehaviour::Discard:
					case warningBehaviour::Accept:
						break;
					case warningBehaviour::Abort:
					default:
						return Error::InvalidChar;
				}
				[[fallthrough]];
			case ',':
			case ';':
				_p::Danger_Action::move_spacing(t_value->m_postSpace, tspacing);
				return static_cast<Error>(decoder.get_char().error_code());
			case '\n':
				{
					_p::Danger_Action::publicError(*twarn._error_context).SetErrorInvalidChar(lchar, ';');
					_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
					switch(twarn.Notify())
					{
						case warningBehaviour::Continue:
						case warningBehaviour::Default:
						case warningBehaviour::Discard:
						case warningBehaviour::Accept:
							break;
						case warningBehaviour::Abort:
						default:
							return Error::InvalidChar;
					}
				}
				break;
			default:
				//insert spacing here
				if(!tspacing.empty())
				{
					itemProxy<spacer> t_spacer = spacer::make();
					t_spacer->set_position(line, spacing_column);
					p_list.push_back(t_spacer);
					_p::Danger_Action::move_spacing(*t_spacer, tspacing);
				}

				if(is_dangerCodepoint(lchar))
				{
					_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
					_p::Danger_Action::publicError(*twarn._error_context).SetPlainError(Error::BadFormat);
					return Error::BadFormat;
				}

				_p::Danger_Action::publicError(*twarn._error_context).SetErrorInvalidChar(lchar, ';');
				_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
				switch(twarn.Notify())
				{
					case warningBehaviour::Continue:
					case warningBehaviour::Default:
					case warningBehaviour::Discard:
					case warningBehaviour::Accept:
						break;
					case warningBehaviour::Abort:
					default:
						return Error::InvalidChar;
				}
				break;
		}
		return Error::None;
	}

	//at this point it is certain to be a keyvalue
	itemProxy<keyedValue> t_keyValue = keyedValue::make();
	t_keyValue->set_position(line, column);
	t_keyValue->set_column_value(decoder.column());
	p_list.push_back(t_keyValue);
	t_keyValue->set_name(tName);
	t_keyValue->set_quotation_mode(tmode);
	_p::Danger_Action::move_spacing(t_keyValue->m_preSpace, tspacing);

	return ReadKeyValue(p_flow, *t_keyValue, p_list);
}

static Error ReadGroup(ReaderFlow& p_flow, group& p_group)
{
	_Warning_Def& twarn = p_flow.m_warnDef;
	stream_decoder& decoder = p_flow.m_decoder;
	Error lastError = Error::None;

	_p::Danger_Action::publicError(*twarn._error_context).m_criticalItem = &p_group;

	{
		stream_error str_err;
		if(p_flow.m_skipSpaces)
		{
			str_err = decoder.read_while(skipInlineSpacing, nullptr);
		}
		else
		{
			std::u8string tsrt;
			str_err = decoder.read_while(loadInlineSpacing, &tsrt);
			_p::Danger_Action::move_spacing(p_group.m_preSpace, tsrt);
		}
		switch(str_err)
		{
			case stream_error::None:
				break;
			case stream_error::Control_EndOfStream:
				_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column());
				_p::Danger_Action::publicError(*twarn._error_context).SetErrorPrematureEnding('>');
				switch(p_flow.m_warnDef.Notify())
				{
					case warningBehaviour::Continue:
					case warningBehaviour::Discard:
					case warningBehaviour::Accept:
						break;
					case warningBehaviour::Default:
					case warningBehaviour::Abort:
					default:
						return Error::PrematureEnd;
				}
				_p::Danger_Action::publicError(*twarn._error_context).m_criticalItem = nullptr;
				return Error::Control_EndOfStream;
			default:
				return static_cast<Error>(str_err);
		}
	}

	switch(decoder.lastChar())
	{
		case ',':
		case ';':
			//handle error, go to first item
			_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
			_p::Danger_Action::publicError(*twarn._error_context).SetErrorInvalidChar(decoder.lastChar(), ':');
			switch(p_flow.m_warnDef.Notify())
			{
				case warningBehaviour::Default:
				case warningBehaviour::Continue:
				case warningBehaviour::Accept:
				case warningBehaviour::Discard:
					break;
				case warningBehaviour::Abort:
				default:
					return Error::InvalidChar;
			}
			{
				stream_error str_err = decoder.get_char().error_code();
				switch(str_err)
				{
					case stream_error::None:
						break;
					case stream_error::Control_EndOfStream:
						_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column());
						_p::Danger_Action::publicError(*twarn._error_context).SetErrorPrematureEnding('>');
						switch(p_flow.m_warnDef.Notify())
						{
							case warningBehaviour::Continue:
							case warningBehaviour::Discard:
							case warningBehaviour::Accept:
								break;
							case warningBehaviour::Default:
							case warningBehaviour::Abort:
							default:
								return Error::PrematureEnd;
						}
						_p::Danger_Action::publicError(*twarn._error_context).m_criticalItem = nullptr;
						return Error::Control_EndOfStream;
					default:
						return static_cast<Error>(str_err);
				}
			}
			goto ReadGroup$HeaderEnd;
		case '\n':
		case '=':
		case '<':
		case '#':
			//handle error, go to first item
			_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
			_p::Danger_Action::publicError(*twarn._error_context).SetErrorInvalidChar(decoder.lastChar(), ':');
			switch(p_flow.m_warnDef.Notify())
			{
				case warningBehaviour::Default:
				case warningBehaviour::Continue:
				case warningBehaviour::Accept:
				case warningBehaviour::Discard:
					break;
				case warningBehaviour::Abort:
				default:
					return Error::InvalidChar;
			}
		[[fallthrough]];
		case ':':
			goto ReadGroup$HeaderEnd;
		case '>':
			_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
			_p::Danger_Action::publicError(*twarn._error_context).SetErrorInvalidChar(decoder.lastChar(), ':');
			switch(p_flow.m_warnDef.Notify())
			{
				case warningBehaviour::Default:
				case warningBehaviour::Continue:
				case warningBehaviour::Accept:
				case warningBehaviour::Discard:
					break;
				case warningBehaviour::Abort:
				default:
					return Error::InvalidChar;
			}
			_p::Danger_Action::publicError(*twarn._error_context).m_criticalItem = nullptr;
			return Error::None;
		default:
			if(is_dangerCodepoint(decoder.lastChar()))
			{
				_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
				_p::Danger_Action::publicError(*twarn._error_context).SetPlainError(Error::BadFormat);
				return Error::BadFormat;
			}
			//capture the group name
			else
			{
				QuotationMode tmode = QuotationMode::standard;
				Error t_err = ReadName(p_flow, p_group.name(), tmode);
				p_group.set_quotation_mode(tmode);
				if(t_err != Error::None)
				{
					if(t_err == Error::Control_EndOfStream)
					{
						_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column());
						_p::Danger_Action::publicError(*twarn._error_context).SetErrorPrematureEnding('>');
						switch(p_flow.m_warnDef.Notify())
						{
							case warningBehaviour::Continue:
							case warningBehaviour::Discard:
							case warningBehaviour::Accept:
								break;
							case warningBehaviour::Default:
							case warningBehaviour::Abort:
							default:
								return Error::PrematureEnd;
						}
						_p::Danger_Action::publicError(*twarn._error_context).m_criticalItem = nullptr;
					}
					return t_err;
				}
			}
			break;
	}

//capture post spacing
	switch(decoder.lastChar())
	{
		case ',':
		case ';':
			_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
			_p::Danger_Action::publicError(*twarn._error_context).SetErrorInvalidChar(decoder.lastChar(), ':');
			switch(p_flow.m_warnDef.Notify())
			{
				case warningBehaviour::Default:
				case warningBehaviour::Continue:
				case warningBehaviour::Accept:
				case warningBehaviour::Discard:
					break;
				case warningBehaviour::Abort:
				default:
					return Error::InvalidChar;
			}
			{
				stream_error str_err = decoder.get_char().error_code();
				switch(str_err)
				{
					case stream_error::None:
						break;
					case stream_error::Control_EndOfStream:
						_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column());
						_p::Danger_Action::publicError(*twarn._error_context).SetErrorPrematureEnding('>');
						switch(p_flow.m_warnDef.Notify())
						{
							case warningBehaviour::Continue:
							case warningBehaviour::Discard:
							case warningBehaviour::Accept:
								break;
							case warningBehaviour::Default:
							case warningBehaviour::Abort:
							default:
								return Error::PrematureEnd;
						}
						_p::Danger_Action::publicError(*twarn._error_context).m_criticalItem = nullptr;
						return Error::Control_EndOfStream;
					default:
						return static_cast<Error>(str_err);
				}
			}
			[[fallthrough]];
		case ':':
			lastError = static_cast<Error>(decoder.get_char().error_code());
			break;
		case '>':
			_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
			_p::Danger_Action::publicError(*twarn._error_context).SetErrorInvalidChar('>', ':');
			switch(p_flow.m_warnDef.Notify())
			{
				case warningBehaviour::Default:
				case warningBehaviour::Continue:
				case warningBehaviour::Accept:
				case warningBehaviour::Discard:
					break;
				case warningBehaviour::Abort:
				default:
					return Error::InvalidChar;
			}
			_p::Danger_Action::publicError(*twarn._error_context).m_criticalItem = nullptr;
			return Error::None;
		case '\n':
		case '=':
		case '<':
		case '#':
			//handle error, go to first item
			_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
			_p::Danger_Action::publicError(*twarn._error_context).SetErrorInvalidChar(decoder.lastChar(), ':');
			switch(p_flow.m_warnDef.Notify())
			{
				case warningBehaviour::Default:
				case warningBehaviour::Continue:
				case warningBehaviour::Accept:
				case warningBehaviour::Discard:
					break;
				case warningBehaviour::Abort:
				default:
					return Error::InvalidChar;
			}
			break;
		default:
			if(_p::is_space(decoder.lastChar()))
			{
				stream_error str_err;
				if(p_flow.m_skipSpaces)
				{
					str_err = decoder.read_while(skipInlineSpacing, nullptr);
				}
				else
				{
					std::u8string tsrt;
					str_err = decoder.read_while(loadInlineSpacing, &tsrt);
					_p::Danger_Action::move_spacing(p_group.m_postSpace, tsrt);
				}
				switch(str_err)
				{
					case stream_error::None:
						break;
					case stream_error::Control_EndOfStream:
						_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column());
						_p::Danger_Action::publicError(*twarn._error_context).SetErrorPrematureEnding('>');
						switch(p_flow.m_warnDef.Notify())
						{
							case warningBehaviour::Continue:
							case warningBehaviour::Discard:
							case warningBehaviour::Accept:
								break;
							case warningBehaviour::Default:
							case warningBehaviour::Abort:
							default:
								return Error::PrematureEnd;
						}
						_p::Danger_Action::publicError(*twarn._error_context).m_criticalItem = nullptr;
						return Error::Control_EndOfStream;
					default:
						return static_cast<Error>(str_err);
				}

				if(decoder.lastChar() == ':')
				{
					lastError = static_cast<Error>(decoder.get_char().error_code());
					break;
				}
			}
			if(is_badCodePoint(decoder.lastChar()))
			{
				_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
				_p::Danger_Action::publicError(*twarn._error_context).SetPlainError(Error::BadFormat);
				return Error::BadFormat;
			}

			//handle error, go to first item
			_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
			_p::Danger_Action::publicError(*twarn._error_context).SetErrorInvalidChar(decoder.lastChar(), ':');
			switch(p_flow.m_warnDef.Notify())
			{
				case warningBehaviour::Default:
				case warningBehaviour::Continue:
				case warningBehaviour::Accept:
				case warningBehaviour::Discard:
					break;
				case warningBehaviour::Abort:
				default:
					return Error::InvalidChar;
			}
			break;
	}

ReadGroup$HeaderEnd:
	_p::Danger_Action::publicError(*twarn._error_context).m_stack.push_back(&p_group);
	_p::Danger_Action::publicError(*twarn._error_context).m_criticalItem = nullptr;
	do
	{
		switch(lastError)
		{
			case Error::None:
				{
					char32_t lastChar = decoder.lastChar();
					switch(lastChar)
					{
						case ':':
							{
								_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
								_p::Danger_Action::publicError(*twarn._error_context).SetErrorInvalidChar(':', 0);
								switch(twarn.Notify())
								{
									case warningBehaviour::Continue:
									case warningBehaviour::Discard:
									case warningBehaviour::Default:
									case warningBehaviour::Accept:
										lastError = static_cast<Error>(decoder.get_char().error_code());
										break;
									case warningBehaviour::Abort:
									default:
										return Error::InvalidChar;
								}
							}
							break;
						case ',':
						case ';':
							_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
							_p::Danger_Action::publicError(*twarn._error_context).SetErrorInvalidChar(lastChar, 0);
							switch(p_flow.m_warnDef.Notify())
							{
								case warningBehaviour::Continue:
								case warningBehaviour::Discard:
								case warningBehaviour::Default:
									lastError = static_cast<Error>(decoder.get_char().error_code());
									break;
								case warningBehaviour::Accept:
									//Add Ghost value
									{
										itemProxy<value> t_item = value::make();
										t_item->set_position(decoder.line(), decoder.column() - 1);
										p_group.push_back(t_item);
									}
									lastError = static_cast<Error>(decoder.get_char().error_code());
									break;
								case warningBehaviour::Abort:
								default:
									return Error::InvalidChar;
							}
							break;
						case '=':
							_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
							_p::Danger_Action::publicError(*twarn._error_context).SetErrorInvalidChar('=', 0);
							switch(twarn.Notify())
							{
								case warningBehaviour::Default:
								case warningBehaviour::Continue:
								case warningBehaviour::Accept:
									//Start key
									{
										itemProxy<keyedValue> t_item = keyedValue::make();
										t_item->set_position(decoder.line(), decoder.column() - 1);
										t_item->set_column_value(decoder.column());
										p_group.push_back(t_item);
										lastError = ReadKeyValue(p_flow, *t_item, p_group);
									}
									break;
								case warningBehaviour::Discard:
									lastError = static_cast<Error>(decoder.get_char().error_code());
									break;
								case warningBehaviour::Abort:
								default:
									return Error::InvalidChar;
							}
							break;
						case '<':
							//Start group
							{
								itemProxy<group> t_item = group::make();
								t_item->set_position(decoder.line(), decoder.column() - 1);
								p_group.push_back(t_item);
								lastError = ReadGroup(p_flow, *t_item);
							}
							break;
						case '>':
							_p::Danger_Action::publicError(*twarn._error_context).m_stack.pop_back();
							return static_cast<Error>(decoder.get_char().error_code());
						case '#':
							//Start comment
							if(p_flow.m_skipComments)
							{
								lastError = ReadCommentSkip(p_flow);
							}
							else
							{
								itemProxy<comment> t_item = comment::make();
								p_group.push_back(t_item);
								lastError = ReadComment(p_flow, *t_item);
							}
							break;
						default:
							if(_p::is_space(lastChar)) //spaces
							{
								//Start spacing
								if(p_flow.m_skipSpaces)
								{
									lastError = ReadSpaceSkip(p_flow);
								}
								else
								{
									itemProxy<spacer> t_item = spacer::make();
									p_group.push_back(t_item);
									lastError = ReadSpace(p_flow, *t_item);
								}
							}
							else if(is_dangerCodepoint(lastChar))
							{
								_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column() - 1);
								_p::Danger_Action::publicError(*twarn._error_context).SetPlainError(Error::BadFormat);
								return Error::BadFormat;
							}
							else
							{
								lastError = ReadTValue(p_flow, p_group);
							}
							break;
					}
				}
				break;
			case Error::Control_EndOfStream:
				_p::Danger_Action::publicError(*twarn._error_context).set_position(decoder.line(), decoder.column());
				_p::Danger_Action::publicError(*twarn._error_context).SetErrorPrematureEnding('>');
				switch(p_flow.m_warnDef.Notify())
				{
					case warningBehaviour::Continue:
					case warningBehaviour::Discard:
					case warningBehaviour::Accept:
						break;
					case warningBehaviour::Default:
					case warningBehaviour::Abort:
					default:
						return Error::PrematureEnd;
				}
				_p::Danger_Action::publicError(*twarn._error_context).m_stack.pop_back();
				return Error::Control_EndOfStream;
			default:
				return lastError;
		}
	} while(true);
	//unreachable
}


void load(root& p_root, stream_decoder& p_decoder, Flag p_flags, [[maybe_unused]] uint16_t p_detected_version, _Warning_Def& p_warn)
{
	ReaderFlow t_flow(p_decoder, p_warn);

	t_flow.m_skipSpaces		= (p_flags & Flag::DisableSpacers) != Flag{};
	t_flow.m_skipComments	= (p_flags & Flag::DisableComments) != Flag{};

	Error lastError = static_cast<Error>(p_decoder.get_char().error_code());

	_p::Danger_Action::publicError(*p_warn._error_context).m_criticalItem = nullptr;

	do
	{
		switch(lastError)
		{
			case Error::None:
				{
					char32_t lastChar = p_decoder.lastChar();
					switch(lastChar)
					{
						case '#':
							//Start comment
							if(t_flow.m_skipComments)
							{
								lastError = ReadCommentSkip(t_flow);
							}
							else
							{
								itemProxy<comment> t_item = comment::make();
								p_root.push_back(t_item);
								lastError = ReadComment(t_flow, *t_item);
							}
							break;
						case '<':
							//Start group
							{
								itemProxy<group> t_item = group::make();
								t_item->set_position(p_decoder.line(), p_decoder.column() - 1);
								p_root.push_back(t_item);
								lastError = ReadGroup(t_flow, *t_item);
							}
							break;
						case ',':
						case ';':
							_p::Danger_Action::publicError(*p_warn._error_context).set_position(p_decoder.line(), p_decoder.column() - 1);
							_p::Danger_Action::publicError(*p_warn._error_context).SetErrorInvalidChar(lastChar, 0);

							switch(p_warn.Notify())
							{
								case warningBehaviour::Default:
								case warningBehaviour::Continue:
								case warningBehaviour::Discard:
									lastError = static_cast<Error>(p_decoder.get_char().error_code());
									break;
								case warningBehaviour::Accept:
									//Add Ghost value
									{
										itemProxy<value> t_item = value::make();
										t_item->set_position(p_decoder.line(), p_decoder.column() - 1);
										p_root.push_back(t_item);
									}
									lastError = static_cast<Error>(p_decoder.get_char().error_code());
									break;
								case warningBehaviour::Abort:
								default:
									return;
							}
							break;
						case '=':
							_p::Danger_Action::publicError(*p_warn._error_context).set_position(p_decoder.line(), p_decoder.column() - 1);
							_p::Danger_Action::publicError(*p_warn._error_context).SetErrorInvalidChar('=', 0);
							switch(p_warn.Notify())
							{
								case warningBehaviour::Default:
								case warningBehaviour::Continue:
								case warningBehaviour::Accept:
									//Start key
									{
										itemProxy<keyedValue> t_item = keyedValue::make();
										t_item->set_position(p_decoder.line(), p_decoder.column() - 1);
										t_item->set_column_value(p_decoder.column());
										p_root.push_back(t_item);
										lastError = ReadKeyValue(t_flow, *t_item, p_root);
									}
									break;
								case warningBehaviour::Discard:
									lastError = static_cast<Error>(p_decoder.get_char().error_code());
									break;
								case warningBehaviour::Abort:
								default:
									return;
							}
							break;
						case ':':
							_p::Danger_Action::publicError(*p_warn._error_context).set_position(p_decoder.line(), p_decoder.column() - 1);
							_p::Danger_Action::publicError(*p_warn._error_context).SetErrorInvalidChar(':', 0);
							switch(p_warn.Notify())
							{
								case warningBehaviour::Default:
								case warningBehaviour::Continue:
								case warningBehaviour::Accept:
								case warningBehaviour::Discard:
									lastError = static_cast<Error>(p_decoder.get_char().error_code());
									break;
								case warningBehaviour::Abort:
								default:
									return;
							}
							break;
						case '>':
							_p::Danger_Action::publicError(*p_warn._error_context).set_position(p_decoder.line(), p_decoder.column() - 1);
							_p::Danger_Action::publicError(*p_warn._error_context).SetErrorInvalidChar('>', 0);
							switch(p_warn.Notify())
							{
								case warningBehaviour::Continue:
								case warningBehaviour::Accept:
								case warningBehaviour::Discard:
									lastError = static_cast<Error>(p_decoder.get_char().error_code());
									break;
								case warningBehaviour::Default:
								case warningBehaviour::Abort:
								default:
									return;
							}
							break;
						default:
							if(_p::is_space(lastChar)) //spaces
							{
								//Start spacing
								if(t_flow.m_skipSpaces)
								{
									lastError = ReadSpaceSkip(t_flow);
								}
								else
								{
									itemProxy<spacer> t_item = spacer::make();
									p_root.push_back(t_item);
									lastError = ReadSpace(t_flow, *t_item);
								}
							}
							else if(is_dangerCodepoint(lastChar))
							{
								_p::Danger_Action::publicError(*p_warn._error_context).set_position(p_decoder.line(), p_decoder.column() - 1);
								_p::Danger_Action::publicError(*p_warn._error_context).SetPlainError(Error::BadFormat);
							}
							else
							{
								lastError = ReadTValue(t_flow, p_root);
							}
						break;
					}
				}
				break;
			case Error::Control_EndOfStream:
				p_warn._error_context->clear();
				_p::Danger_Action::publicError(*p_warn._error_context).set_position(p_decoder.line(), p_decoder.column());
				return;
			default:
				_p::Danger_Action::publicError(*p_warn._error_context).SetPlainError(lastError);
				return;
		}
	}
	while(true);
}


//======== ======== ======== ======== Writting ======== ======== ======== ======== 

struct WriterFlow;

using WriterList = bool (*)(WriterFlow&, const ItemList&, uint8_t);

struct WriterFlow
{
	WriterFlow(stream_encoder& p_encoder, _Warning_Def& p_warn):
		m_encoder(p_encoder),
		m_warnDef(p_warn)
	{
	}
	stream_encoder& m_encoder;
	_Warning_Def&	m_warnDef;
	WriterList		m_listWriter;
	bool			m_autoQuote;
};

static inline constexpr bool CharNeedsEscape(char32_t p_char)
{
	if(p_char < 36)
	{
		return p_char != '!';
	}
	if(p_char < 63)
	{
		return (p_char > 57) || p_char == '\'' || p_char == ',';
	}

	return (p_char > 0xD7FF && p_char < 0xE000);
}

//TODO: Stream instead of copy
static void EscapeNameSingle(std::u32string_view p_name, std::u32string& p_out)
{
	std::array<char32_t, 6> buff;
	p_out.clear();
	p_out.reserve(p_name.size());
	buff[0] = '^';
	for(char32_t tchar : p_name)
	{
		switch(tchar)
		{
			case '\t':
				p_out.push_back(tchar);
				break;
			case '\n':
				buff[1] = 'n';
				p_out.append(buff.data(), 2);
				break;
			case '\r':
				buff[1] = 'r';
				p_out.append(buff.data(), 2);
				break;
			case '\'':
			case '^':
				buff[1] = tchar;
				p_out.append(buff.data(), 2);
				break;
			default:
				if(tchar < 32)
				{
					core::to_hex_chars_fix(static_cast<uint8_t>(tchar), std::span<char32_t, 2>{buff.data() + 1, 2});
					p_out.append(buff.data(), 3);
				}
				else if(tchar > 0xD7FF && tchar < 0xE000)
				{
					buff[1] = 'u';
					core::to_hex_chars_fix(static_cast<uint16_t>(tchar), std::span<char32_t, 4>{buff.data() + 2, 4});
					p_out.append(buff.data(), 6);
				}
				else
				{
					p_out.push_back(tchar);
				}
				break;
		}
	}
}

//TODO: Stream instead of copy
static void EscapeNameDouble(std::u32string_view p_name, std::u32string& p_out)
{
	std::array<char32_t, 6> buff;
	p_out.clear();
	p_out.reserve(p_name.size());
	buff[0] = '^';
	for(char32_t tchar : p_name)
	{
		switch(tchar)
		{
			case '\t':
				buff[1] = 't';
				p_out.append(buff.data(), 2);
				break;
			case '\n':
				buff[1] = 'n';
				p_out.append(buff.data(), 2);
				break;
			case '\r':
				buff[1] = 'r';
				p_out.append(buff.data(), 2);
				break;
			case '\"':
			case '^':
				buff[1] = tchar;
				p_out.append(buff.data(), 2);
				break;
			default:
				if(tchar < 32)
				{
					core::to_hex_chars_fix(static_cast<uint8_t>(tchar), std::span<char32_t, 2>{buff.data() + 1, 2});
					p_out.append(buff.data(), 3);
				}
				else if(tchar > 0xD7FF && tchar < 0xE000)
				{
					buff[1] = 'u';
					core::to_hex_chars_fix(static_cast<uint16_t>(tchar), std::span<char32_t, 4>{buff.data() + 2, 4});
					p_out.append(buff.data(), 6);
				}
				else
				{
					p_out.push_back(tchar);
				}
				break;
		}
	}
}

static bool NameNeedsEscape(std::u32string_view p_name)
{
	if(p_name.empty()) return true;
	const char32_t *it, *it_end;
	it		= p_name.data();
	it_end	= it + p_name.size();
	for(; it < it_end; ++it)
	{
		if(CharNeedsEscape(*it))
		{
			return true;
		}
	}
	return false;
}


static inline bool WriteChar(WriterFlow& p_flow, char32_t p_char)
{
	stream_error t_err = p_flow.m_encoder.put(p_char);
	if(t_err != stream_error::None)
	{
		_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).SetPlainError(static_cast<Error>(t_err));
		return false;
	}
	return true;
}


bool WriteNameAuto(WriterFlow& p_flow, std::u32string_view p_name)
{
	stream_error t_err;
	if(NameNeedsEscape(p_name))
	{
		std::u32string t_escape;
		t_err = p_flow.m_encoder.put(U'\'');
		if(t_err == stream_error::None)
		{
			EscapeNameSingle(p_name, t_escape);
			t_err = p_flow.m_encoder.put(t_escape);
			if(t_err == stream_error::None)
			{
				t_err = p_flow.m_encoder.put(U'\'');
			}
		}
	}
	else
	{
		t_err = p_flow.m_encoder.put(p_name);
	}
	if(t_err != stream_error::None)
	{
		_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).SetPlainError(static_cast<Error>(t_err));
		return false;
	}
	return true;
}

bool WriteNamePrefered(WriterFlow& p_flow, std::u32string_view p_name, QuotationMode p_quoteMode)
{
	stream_error t_err;
	std::u32string t_escape;
	switch(p_quoteMode)
	{
		case QuotationMode::singlemark:
			t_err = p_flow.m_encoder.put(U'\'');
			if(t_err == stream_error::None)
			{
				EscapeNameSingle(p_name, t_escape);
				t_err = p_flow.m_encoder.put(t_escape);
				if(t_err == stream_error::None)
				{
					t_err = p_flow.m_encoder.put(U'\'');
				}
			}
			break;
		case QuotationMode::doublemark:
			t_err = p_flow.m_encoder.put(U'\"');
			if(t_err == stream_error::None)
			{
				EscapeNameDouble(p_name, t_escape);
				t_err = p_flow.m_encoder.put(t_escape);
				if(t_err == stream_error::None)
				{
					t_err = p_flow.m_encoder.put(U'\"');
				}
			}
			break;
		default:	//standard
			if(NameNeedsEscape(p_name))
			{
				t_err = p_flow.m_encoder.put(U'\'');
				if(t_err == stream_error::None)
				{
					EscapeNameSingle(p_name, t_escape);
					t_err = p_flow.m_encoder.put(t_escape);
					if(t_err == stream_error::None)
					{
						t_err = p_flow.m_encoder.put(U'\'');
					}
				}
			}
			else
			{
				t_err = p_flow.m_encoder.put(p_name);
			}
			break;
	}
	if(t_err != stream_error::None)
	{
		_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).SetPlainError(static_cast<Error>(t_err));
		return false;
	}
	return true;
}

static inline bool WriteNameOpional(WriterFlow& p_flow, std::u32string_view p_name, QuotationMode p_quotMode)
{
	if(p_flow.m_autoQuote)
	{
		if(!p_name.empty())
		{
			if(!WriteNameAuto(p_flow, p_name)) return false;
		}
	}
	else
	{
		if(!p_name.empty() || p_quotMode != QuotationMode::standard)
		{
			if(!WriteNamePrefered(p_flow, p_name, p_quotMode)) return false;
		}
	}
	return true;
}

static inline bool WriteSpacing(WriterFlow& p_flow, const _p::lineSpace& p_spacing)
{
	if(!p_spacing.spacing().empty())
	{
		stream_error t_err = p_flow.m_encoder.put(p_spacing.spacing());
		if(t_err != stream_error::None)
		{
			_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).SetPlainError(static_cast<Error>(t_err));
			return false;
		}
	}
	return true;
}

static inline bool WriteAutoTabulation(WriterFlow& p_flow, uint8_t p_level)
{
	if(!WriteChar(p_flow, U'\n')) return false;
	for(uint8_t it = 0; it < p_level; ++it)
	{
		if(!WriteChar(p_flow, U'\t')) return false;
	}
	return true;
}

static inline bool WriteComment(WriterFlow& p_flow, const comment& p_comment)
{
	std::u32string_view str = p_comment.str();
	size_t pos = str.find(U'\n');
	if(pos != std::u32string_view::npos)
	{
		if(!WriteChar(p_flow, U'#')) return false; //open comment
		if(pos)
		{
			std::u32string_view segment = str.substr(0, pos);
			stream_error t_err = p_flow.m_encoder.put(segment);
			if(t_err != stream_error::None)
			{
				_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).SetPlainError(static_cast<Error>(t_err));
				return false;
			}
		}
		if(!WriteChar(p_flow, U'\n')) return false;
		str = str.substr(pos + 1);
	}

	if(!WriteChar(p_flow, U'#')) return false; //open comment

	if(str.empty())
	{
		stream_error t_err = p_flow.m_encoder.put(str);
		if(t_err != stream_error::None)
		{
			_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).SetPlainError(static_cast<Error>(t_err));
			return false;
		}
	}
	return true;
}


static bool WriteGroupDefault(WriterFlow& p_flow, const group& p_group, uint8_t p_level)
{
	_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_criticalItem = &p_group;

	if(	!WriteChar(p_flow, U'<')	|| //open group
		!WriteSpacing(p_flow, p_group.m_preSpace) ||
		!WriteNameOpional(p_flow, p_group.view_name(), p_group.quotation_mode()) ||
		!WriteSpacing(p_flow, p_group.m_postSpace) ||
		!WriteChar(p_flow, U':') //close header
		) return false;

	_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_stack.push_back(&p_group);

	//write body
	if(p_level < MAX_LEVEL) ++p_level;
	if(!p_flow.m_listWriter(p_flow, p_group, p_level)) return false;

	_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_stack.pop_back();
	_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_criticalItem = &p_group;

	//close object
	return WriteChar(p_flow, U'>');
}

static bool WriteGroupAutoSpace(WriterFlow& p_flow, const group& p_group, uint8_t p_level)
{
	_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_criticalItem = &p_group;

	if(	!WriteAutoTabulation(p_flow, p_level) ||
		!WriteChar(p_flow, U'<') || 	//open group
		!WriteNameOpional(p_flow, p_group.view_name(), p_group.quotation_mode()) ||
		!WriteChar(p_flow, U':') //close header
		) return false;

	_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_stack.push_back(&p_group);

	//write body
	if(p_level < MAX_LEVEL) ++p_level;
	if(!p_flow.m_listWriter(p_flow, p_group, p_level)) return false;

	_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_stack.pop_back();
	_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_criticalItem = &p_group;

	//close group
	return WriteChar(p_flow, U'>');
}

static bool WriteGroupNoSpace(WriterFlow& p_flow, const group& p_group, uint8_t p_level)
{
	_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_criticalItem = &p_group;

	if(	!WriteChar(p_flow, U'<') || //open group
		!WriteNameOpional(p_flow, p_group.view_name(), p_group.quotation_mode()) ||
		!WriteChar(p_flow, U':') //close header
		) return false;

	_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_stack.push_back(&p_group);

	//write body
	if(p_level < MAX_LEVEL) ++p_level;
	if(!p_flow.m_listWriter(p_flow, p_group, p_level)) return false;
	
	_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_stack.pop_back();
	_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_criticalItem = &p_group;

	//close group
	return WriteChar(p_flow, U'>');
}

static bool WriteKeyValueDefault(WriterFlow& p_flow, const keyedValue& p_key, uint8_t /*p_level*/)
{
	_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_criticalItem = &p_key;

	//Key name
	if(p_flow.m_autoQuote)
	{
		if(!WriteNameAuto(p_flow, p_key.name())) return false;
	}
	else
	{
		if(!WriteNamePrefered(p_flow, p_key.name(), p_key.quotation_mode())) return false;
	}

	if(	!WriteSpacing(p_flow, p_key.m_preSpace) ||
		!WriteChar(p_flow, U'=') ||
		!WriteSpacing(p_flow, p_key.m_midSpace) ||
		!WriteNameOpional(p_flow, p_key.view_value(), p_key.value_quotation_mode()) ||
		!WriteSpacing(p_flow, p_key.m_postSpace)
		) return false;

	//close Key
	return WriteChar(p_flow, U';');
}

static bool WriteKeyValueAutoSpace(WriterFlow& p_flow, const keyedValue& p_key, uint8_t p_level)
{
	_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_criticalItem = &p_key;

	if(!WriteAutoTabulation(p_flow, p_level)) return false;

	//Key name
	if(p_flow.m_autoQuote)
	{
		if(!WriteNameAuto(p_flow, p_key.name())) return false;
	}
	else
	{
		if(!WriteNamePrefered(p_flow, p_key.name(), p_key.quotation_mode())) return false;
	}
	
	//pre-spacing and sign 
	if(!WriteChar(p_flow, U' ') ||
		!WriteChar(p_flow, U'=')) return false;

	if(p_flow.m_autoQuote)
	{
		if(!p_key.value().empty())
		{
			if(	!WriteChar(p_flow, U' ') ||
				!WriteNameAuto(p_flow, p_key.view_value())
				) return false;
		}
	}
	else
	{	if(!p_key.value().empty() || p_key.value_quotation_mode() != QuotationMode::standard)
		{
			if(	!WriteChar(p_flow, U' ') ||
				!WriteNamePrefered(p_flow, p_key.view_value(), p_key.value_quotation_mode())
				) return false;
		}
	}

	//close Key
	return WriteChar(p_flow, U';');
}

static bool WriteKeyValueNoSpace(WriterFlow& p_flow, const keyedValue& p_key, uint8_t /*p_level*/)
{
	_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_criticalItem = &p_key;

	//Key name
	if(p_flow.m_autoQuote)
	{
		if(!WriteNameAuto(p_flow, p_key.view_name())) return false;
	}
	else
	{
		if(!WriteNamePrefered(p_flow, p_key.view_name(), p_key.quotation_mode())) return false;
	}

	if(	!WriteChar(p_flow, U'=') ||
		!WriteNameOpional(p_flow, p_key.view_value(), p_key.value_quotation_mode())
		) return false;

	//close Key
	return WriteChar(p_flow, U';');
}

static bool WriteValueDefault(WriterFlow& p_flow, const value& p_value, uint8_t /*p_level*/)
{
	_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_criticalItem = &p_value;

	//Key name
	if(p_flow.m_autoQuote)
	{
		if(!WriteNameAuto(p_flow, p_value.name())) return false;
	}
	else
	{
		if(!WriteNamePrefered(p_flow, p_value.name(), p_value.quotation_mode())) return false;
	}

	if(!WriteSpacing(p_flow, p_value.m_postSpace)) return false;

	//close Key
	return WriteChar(p_flow, U';');
}

static bool WriteValueAutoSpace(WriterFlow& p_flow, const value& p_value, uint8_t p_level)
{
	_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_criticalItem = &p_value;

	if(!WriteAutoTabulation(p_flow, p_level)) return false;

	//value name
	if(p_flow.m_autoQuote)
	{
		if(!WriteNameAuto(p_flow, p_value.name())) return false;
	}
	else
	{
		if(!WriteNamePrefered(p_flow, p_value.name(), p_value.quotation_mode())) return false;
	}

	//close Key
	return WriteChar(p_flow, U';');
}

static bool WriteValueNoSpace(WriterFlow& p_flow, const value& p_value, uint8_t /*p_level*/)
{
	_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_criticalItem = &p_value;

	//value name
	if(p_flow.m_autoQuote)
	{
		if(!WriteNameAuto(p_flow, p_value.view_name())) return false;
	}
	else
	{
		if(!WriteNamePrefered(p_flow, p_value.view_name(), p_value.quotation_mode())) return false;
	}

	//close value
	return WriteChar(p_flow, U';');
}

static bool WriteSpacer(WriterFlow& p_flow, const spacer& p_spacer)
{
	_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_criticalItem = &p_spacer;

	//add new lines
	for(uint64_t it = 0, n_lines = p_spacer.num_lines(); it < n_lines; ++it)
	{
		if(!WriteChar(p_flow, U'\n')) return false;
	}

	//add inline space
	stream_error t_err = p_flow.m_encoder.put(p_spacer.flat_spacing());
	if(t_err != stream_error::None)
	{
		_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).SetPlainError(static_cast<Error>(t_err));
		return false;
	}
	return true;
}

static bool WriteSpacerNewLineOnly(WriterFlow& p_flow, const spacer& p_spacer)
{
	//add new lines
	for(uint64_t it = 0, n_lines = p_spacer.num_lines(); it < n_lines; ++it)
	{
		if(!WriteChar(p_flow, U'\n'))
		{
			_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_criticalItem = &p_spacer;
			return false;
		}
	}
	return true;
}

static bool WriteCommentAutoSpace(WriterFlow& p_flow, const comment& p_comment, uint8_t /*p_level*/)
{
	_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_criticalItem = &p_comment;
	return WriteComment(p_flow, p_comment);
}

static bool WriteCommentNoSpace(WriterFlow& p_flow, const comment& p_comment)
{
	_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_criticalItem = &p_comment;
	//force new line
	return WriteComment(p_flow, p_comment) && WriteChar(p_flow, U'\n');
}

static bool WriteListAll(WriterFlow& p_flow, const ItemList& p_list, uint8_t p_level)
{
	ItemList::const_iterator it, it_end, it_next;
	for(it = p_list.cbegin(), it_end = p_list.cend(); it != it_end; ++it)
	{
		switch((*it)->type())
		{
			case ItemType::group:
				if(!WriteGroupDefault(p_flow, *static_cast<const group*>(it->get()), p_level)) return false;
				break;
			case ItemType::value:
				if(!WriteValueDefault(p_flow, *static_cast<const value*>(it->get()), p_level)) return false;
				break;
			case ItemType::key_value:
				if(!WriteKeyValueDefault(p_flow, *static_cast<const keyedValue*>(it->get()), p_level)) return false;
				break;
			case ItemType::spacer:
				{
					it_next = it;
					++it_next;
					if(it_next != it_end && (*it_next)->type() == ItemType::spacer)
					{
						if(!WriteSpacerNewLineOnly(p_flow, *static_cast<const spacer*>(it->get()))) return false;
					}
					else if(!WriteSpacer(p_flow, *static_cast<const spacer*>(it->get()))) return false;
				}
				break;
			case ItemType::comment:
				if(!WriteCommentNoSpace(p_flow, *static_cast<const comment*>(it->get()))) return false;
				break;
			default:
				_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_criticalItem = it->get();
				_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).SetPlainError(Error::UnknownObject);
				//should never get here
				switch(p_flow.m_warnDef.Notify())
				{
					case warningBehaviour::Continue:
					case warningBehaviour::Default:
					case warningBehaviour::Discard:
					case warningBehaviour::Accept:
						break;
					case warningBehaviour::Abort:
					default:
						return false;
				}
				break;
		}
	}
	return true;
}

static bool WriteListAutoSpace(WriterFlow& p_flow, const ItemList& p_list, uint8_t p_level)
{
	uint64_t t_lastLine = 0;	// initialized because otherwise generates warning 4701
	bool b_hasitem		= false;
	bool b_lastRelevant	= false;

	for(const itemProxy<item>& tproxy : p_list)
	{
		switch(tproxy->type())
		{
			case ItemType::group:
				b_hasitem		= true;
				b_lastRelevant	= true;
				t_lastLine		= tproxy->line();
				if(!WriteGroupAutoSpace(p_flow, *static_cast<const group*>(tproxy.get()), p_level)) return false;
				break;
			case ItemType::value:
				b_hasitem		= true;
				b_lastRelevant	= true;
				t_lastLine		= tproxy->line();
				if(!WriteValueAutoSpace(p_flow, *static_cast<const value*>(tproxy.get()), p_level)) return false;
				break;
			case ItemType::key_value:
				b_hasitem		= true;
				b_lastRelevant	= true;
				t_lastLine		= tproxy->line();
				if(!WriteKeyValueAutoSpace(p_flow, *static_cast<const keyedValue*>(tproxy.get()), p_level)) return false;
				break;
			case ItemType::spacer:
				if(static_cast<const spacer*>(tproxy.get())->num_lines()) b_lastRelevant = false;
				break;
			case ItemType::comment:
				{
					//inline comment?
					if(b_lastRelevant && tproxy->line() == t_lastLine)
					{
						if(!WriteChar(p_flow, U' ')) return false;
					}
					else
					{
						//new line commnt
						if(!WriteChar(p_flow, U'\n')) return false;

						//add tabs
						for(uint8_t itl = 0; itl < p_level; ++itl)
						{
							if(!WriteChar(p_flow, U'\t')) return false;
						}

					}
					b_lastRelevant	= false;
					b_hasitem		= true;
					if(!WriteCommentAutoSpace(p_flow, *static_cast<const comment*>(tproxy.get()), p_level)) return false;
				}
				break;
			default:
				_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_criticalItem = tproxy.get();
				_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).SetPlainError(Error::UnknownObject);
				//should never get here
				switch(p_flow.m_warnDef.Notify())
				{
					case warningBehaviour::Continue:
					case warningBehaviour::Default:
					case warningBehaviour::Discard:
					case warningBehaviour::Accept:
						break;
					case warningBehaviour::Abort:
					default:
						return false;
				}
				break;
		}
	}

	if(b_hasitem)
	{
		if(!WriteChar(p_flow, U'\n')) return false;

		//add tabs
		for(uint8_t i = 1; i < p_level; ++i)
		{
			if(!WriteChar(p_flow, U'\t')) return false;
		}
	}

	return true;
}

static bool WriteListNoSpace(WriterFlow& p_flow, const ItemList& p_list, uint8_t p_level)
{
	for(const itemProxy<item>& tproxy : p_list)
	{
		switch(tproxy->type())
		{
			case ItemType::group:
				if(!WriteGroupNoSpace(p_flow, *static_cast<const group*>(tproxy.get()), p_level)) return false;
				break;
			case ItemType::value:
				if(!WriteValueNoSpace(p_flow, *static_cast<const value*>(tproxy.get()), p_level)) return false;
				break;
			case ItemType::key_value:
				if(!WriteKeyValueNoSpace(p_flow, *static_cast<const keyedValue*>(tproxy.get()), p_level)) return false;
				break;
			case ItemType::spacer:
				break;
			case ItemType::comment:
				if(!WriteCommentNoSpace(p_flow, *static_cast<const comment*>(tproxy.get()))) return false;
				break;
			default:
				_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_criticalItem = tproxy.get();
				_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).SetPlainError(Error::UnknownObject);
				//should never get here
				switch(p_flow.m_warnDef.Notify())
				{
					case warningBehaviour::Continue:
					case warningBehaviour::Default:
					case warningBehaviour::Discard:
					case warningBehaviour::Accept:
						break;
					case warningBehaviour::Abort:
					default:
						return false;
				}
				break;
		}
	}
	return true;
}

static bool WriteListNoComment(WriterFlow& p_flow, const ItemList& p_list, uint8_t p_level)
{
	ItemList::const_iterator it , it_end, it_next;
	for(it = p_list.cbegin(), it_end = p_list.cend(); it != it_end; ++it)
	{
		switch((*it)->type())
		{
			case ItemType::group:
				if(!WriteGroupDefault(p_flow, *static_cast<const group*>(it->get()), p_level)) return false;
				break;
			case ItemType::value:
				if(!WriteValueDefault(p_flow, *static_cast<const value*>(it->get()), p_level)) return false;
				break;
			case ItemType::key_value:
				if(!WriteKeyValueDefault(p_flow, *static_cast<const keyedValue*>(it->get()), p_level)) return false;
				break;
			case ItemType::spacer:
				it_next = it;
				++it_next;
				while(it_next != it_end && (*it_next)->type() == ItemType::comment) ++it_next;

				if(it_next != it_end && (*it_next)->type() == ItemType::spacer)
				{
					if(!WriteSpacerNewLineOnly(p_flow, *static_cast<const spacer*>(it->get()))) return false;
				}
				else if(!WriteSpacer(p_flow, *static_cast<const spacer*>(it->get()))) return false;

				break;
			case ItemType::comment:
				break;
			default:
				_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_criticalItem = it->get();
				_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).SetPlainError(Error::UnknownObject);
				//should never get here
				switch(p_flow.m_warnDef.Notify())
				{
					case warningBehaviour::Continue:
					case warningBehaviour::Default:
					case warningBehaviour::Discard:
					case warningBehaviour::Accept:
						break;
					case warningBehaviour::Abort:
					default:
						return false;
				}
				break;
		}
	}
	return true;
}

static bool WriteListAutoNoComment(WriterFlow& p_flow, const ItemList& p_list, uint8_t p_level)
{
	bool b_hasitem = false;
	for(const itemProxy<item>& tproxy : p_list)
	{
		switch(tproxy->type())
		{
			case ItemType::group:
				b_hasitem = true;
				if(!WriteGroupAutoSpace(p_flow, *static_cast<const group*>(tproxy.get()), p_level)) return false;
				break;
			case ItemType::value:
				if(!WriteValueAutoSpace(p_flow, *static_cast<const value*>(tproxy.get()), p_level)) return false;
				break;
			case ItemType::key_value:
				b_hasitem = true;
				if(!WriteKeyValueAutoSpace(p_flow, *static_cast<const keyedValue*>(tproxy.get()), p_level)) return false;
				break;
			case ItemType::spacer:
			case ItemType::comment:
				break;
			default:
				_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_criticalItem = tproxy.get();
				_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).SetPlainError(Error::UnknownObject);
				//should never get here
				switch(p_flow.m_warnDef.Notify())
				{
					case warningBehaviour::Continue:
					case warningBehaviour::Default:
					case warningBehaviour::Discard:
					case warningBehaviour::Accept:
						break;
					case warningBehaviour::Abort:
					default:
						return false;
				}
				break;
		}
	}

	if(b_hasitem)
	{
		//new line
		if(!WriteChar(p_flow, U'\n')) return false;
		//add tabs
		for(uint8_t i = 1; i < p_level; ++i)
		{
			if(!WriteChar(p_flow, U'\t')) return false;
		}
	}
	return true;
}

static bool WriteListCompact(WriterFlow& p_flow, const ItemList& p_list, uint8_t p_level)
{
	for(const itemProxy<item>& tproxy : p_list)
	{
		switch(tproxy->type())
		{
			case ItemType::group:
				if(!WriteGroupNoSpace(p_flow, *static_cast<const group*>(tproxy.get()), p_level)) return false;
				break;
			case ItemType::value:
				if(!WriteValueNoSpace(p_flow, *static_cast<const value*>(tproxy.get()), p_level)) return false;
				break;
			case ItemType::key_value:
				if(!WriteKeyValueNoSpace(p_flow, *static_cast<const keyedValue*>(tproxy.get()), p_level)) return false;
				break;
			case ItemType::spacer:
			case ItemType::comment:
				break;
			default:
				_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).m_criticalItem = tproxy.get();
				_p::Danger_Action::publicError(*p_flow.m_warnDef._error_context).SetPlainError(Error::UnknownObject);
				//should never get here
				switch(p_flow.m_warnDef.Notify())
				{
					case warningBehaviour::Continue:
					case warningBehaviour::Default:
					case warningBehaviour::Discard:
					case warningBehaviour::Accept:
						break;
					case warningBehaviour::Abort:
					default:
						return false;
				}
				break;
		}
	}
	return true;
}

void save(root& p_root, stream_encoder& p_encoder, Flag p_flags, [[maybe_unused]] uint16_t p_requested_version, _Warning_Def& p_warn)
{
	WriterFlow t_flow(p_encoder, p_warn);
	t_flow.m_autoQuote = (p_flags & Flag::AutoQuote) != Flag{};

	if((p_flags & Flag::DisableComments) != Flag{})	//no comments
	{
		if((p_flags & Flag::DisableSpacers) != Flag{})	//no spacing
		{
			t_flow.m_listWriter = &WriteListCompact;
		}
		else
		{
			if((p_flags & Flag::AutoSpacing) != Flag{})	//auto spacing
			{
				t_flow.m_listWriter = &WriteListAutoNoComment;
			}
			else	//regular spacing
			{
				t_flow.m_listWriter = &WriteListNoComment;
			}
		}
	}
	else	//has comments
	{
		if((p_flags & Flag::DisableSpacers) != Flag{})	//no spacing
		{
			t_flow.m_listWriter = &WriteListNoSpace;
		}
		else
		{
			if((p_flags & Flag::AutoSpacing) != Flag{})	//auto spacing
			{
				t_flow.m_listWriter = &WriteListAutoSpace;
			}
			else	//regular spacing
			{
				t_flow.m_listWriter = &WriteListAll;
			}
		}
	}

	if(t_flow.m_listWriter(t_flow, p_root, 0))
	{
		_p::Danger_Action::publicError(*p_warn._error_context).SetPlainError(Error::None);
	}
}

}	//namespace scef::format::v1
