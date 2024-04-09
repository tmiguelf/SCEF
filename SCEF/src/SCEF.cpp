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

#include <SCEF/SCEF.hpp>

#include <memory>
#include <fstream>

#include <CoreLib/core_endian.hpp>
#include <CoreLib/core_file.hpp>

#include "scef_encoder.hpp"
#include "scef_format.hpp"
#include "scef_format_v1.hpp"
#include "scef_danger_act_p.hpp"

namespace scef
{

warningBehaviour DefaultWarningHandler(const Error_Context&, void*)
{
	return warningBehaviour::Default;
}

namespace _p
{
void _Error_Context::clear()
{
	m_error_code = Error::None;
	m_criticalItem = nullptr;
	m_line = 0;
	m_column = 0;
	m_stack.clear();
}

void _Error_Context::SetErrorInvalidChar(char32_t p_found, char32_t p_expected)
{
	m_error_code					= Error::InvalidChar;
	m_extra.invalid_char.found		= p_found;
	m_extra.invalid_char.expected	= p_expected;
}

void _Error_Context::SetErrorEscape(const char32_t* p_sequence, uintptr_t p_lenght)
{
	m_error_code					= Error::BadEscape;
	m_extra.invalid_escape.sequence	= p_sequence;
	m_extra.invalid_escape.lengh	= p_lenght;
}

void _Error_Context::SetErrorPrematureEnding(char32_t p_expected)
{
	m_error_code						= Error::PrematureEnd;
	m_extra.premature_ending.expected	= p_expected;
}

} //namespace _p

//======== document

void document::clear()
{
	m_document_properties.version	= __SCEF_NO_VERSION;
	m_document_properties.encoding	= Encoding::Unspecified;
	m_last_error.clear();
	m_rootObject.clear();
}

Error document::load(const std::filesystem::path& p_file, Flag p_flags, _warning_callback p_warning_callback, void* p_user_context)
{
	core::file_read f_reader;
	f_reader.open(p_file);
	if(f_reader.is_open())
	{
		file_istream t_reader{f_reader};
		return load(t_reader, p_flags, p_warning_callback, p_user_context);
	}
	m_last_error.clear();
	_p::Danger_Action::publicError(m_last_error).SetPlainError(Error::FileNotFound);

	m_document_properties.encoding	= Encoding::Unspecified;
	m_document_properties.version	= __SCEF_NO_VERSION;
	m_rootObject.clear();

	return m_last_error.error_code();
}

Error document::save(const std::filesystem::path& p_file, Flag p_flags, uint16_t p_version, Encoding p_encoding)
{
	if(!write_supports_version(p_version))
	{
		return Error::UnsuportedVersion;
	}

	core::file_write f_writer;
	f_writer.open(p_file, core::file_write::open_mode::create);
	if(f_writer.is_open())
	{
		file_ostream t_writer{f_writer};
		return save(t_writer, p_flags, p_version, p_encoding);
	}
	return Error::Unable2Write;
}

//Note:
//	1.	It is known that if the characters 0xEF, 0xFE, or 0xFF, appear at the beginning
//		of the document while not indicating an encoding, this parser is not capable
//		of handling the document, as read backtracking is not supported on this implementation
Error document::load(base_istreamer& p_stream, Flag p_flags, _warning_callback p_warning_callback, void* p_user_context)
{
	Encoding t_encoding	= Encoding::Unspecified;
	std::unique_ptr<stream_decoder> t_decoder;

	clear();

	if(p_warning_callback == nullptr) p_warning_callback = DefaultWarningHandler;

	format::_Warning_Def		t_warn;
	t_warn._error_context			= &m_last_error;
	t_warn._user_context			= p_user_context;
	t_warn._user_warning_callback	= p_warning_callback;

	{
		char8_t t_Sequence[4];
		uint64_t startPos = p_stream.pos();

		if(p_stream.read(t_Sequence, 4) != 4)
		{
			if(p_stream.stat() == stream_error::Control_EndOfStream)
			{
				_p::Danger_Action::publicError(m_last_error).SetPlainError(Error::BadFormat);
				return Error::BadFormat;
			}
			_p::Danger_Action::publicError(m_last_error).SetPlainError(Error::Unable2Read);
			return Error::Unable2Read;
		}

		switch(t_Sequence[0]) //Figure out encoding
		{
			case 0x00:	//UCS4 BE or bust
				{
					constexpr uintptr_t bom_size = ENCODER_P::BOM_UCS4BE.size();
					if(	std::u8string_view{ENCODER_P::BOM_UCS4BE.data(), bom_size} ==
						std::u8string_view{t_Sequence, bom_size})
					{
						t_encoding	= Encoding::UCS4_BE;
						break;
					}
				}
				_p::Danger_Action::publicError(m_last_error).SetPlainError(Error::BadEncoding);
				return Error::BadEncoding;
			case 0xEF:	//UTF8 or ANSI
				{
					constexpr uintptr_t bom_size = ENCODER_P::BOM_UTF8.size();
					if(	std::u8string_view{ENCODER_P::BOM_UTF8.data(), bom_size} ==
						std::u8string_view{t_Sequence + 1, bom_size})
					{
						p_stream.set_pos(startPos + bom_size);
						t_encoding	= Encoding::UTF8;
						break;
					}
				}
				goto backup_ANSI;
			case 0xFE:	//UTF16 BE or ANSI
				{
					constexpr uintptr_t bom_size = ENCODER_P::BOM_UTF16BE.size();
					if(t_Sequence[1] == ENCODER_P::BOM_UTF16BE[1])
					{
						p_stream.set_pos(startPos + bom_size);
						t_encoding	= Encoding::UTF16_BE;
						break;
					}
				}
				goto backup_ANSI;
			case 0xFF:	//UT16 LE or UCS4 LE or ANSI
				if(t_Sequence[1] == ENCODER_P::BOM_UTF16LE[1])
				{
					if(t_Sequence[2] == 0 && t_Sequence[3] == 0)
					{
						t_encoding	= Encoding::UCS4_LE;
						break;
					}
					p_stream.set_pos(startPos + 2);
					t_encoding	= Encoding::UTF16_LE;
					break;
				}
				[[fallthrough]];
			default:	//ANSI
backup_ANSI:
				p_stream.set_pos(startPos);
				t_encoding	= Encoding::ANSI;
				break;
		}
	}

	m_document_properties.encoding = t_encoding;

	//asks the user if they want to support this encoding
	_p::Danger_Action::publicError(m_last_error).SetPlainError(Error::Warning_EncodingDetected);
	_p::Danger_Action::publicError(m_last_error).m_extra.format = {0, t_encoding};
	if(t_warn.Notify() > warningBehaviour::Accept)
	{
		return Error::Warning_EncodingDetected;
	}

	switch(t_encoding)
	{
		case Encoding::UTF8:
			if((p_flags & Flag::LaxedEncoding) != Flag{})
			{
				t_decoder	= std::make_unique<ENCODER_P::Stream_UTF8_Decoder>(p_stream);
			}
			else
			{
				t_decoder	= std::make_unique<ENCODER_P::Stream_UTF8_Decoder_s>(p_stream);
			}
			break;
		case Encoding::UTF16_LE:
			if(p_stream.remaining() % 2)
			{
				_p::Danger_Action::publicError(m_last_error).SetPlainError(Error::BadPredictedEncoding);
				warningBehaviour res = t_warn.Notify();
				if(res != warningBehaviour::Accept && res != warningBehaviour::Continue)
				{
					return Error::BadPredictedEncoding;
				}
			}
			t_decoder	= std::make_unique<ENCODER_P::Stream_UTF16LE_Decoder>(p_stream);
			break;
		case Encoding::UTF16_BE:
			if(p_stream.remaining() % 2)
			{
				_p::Danger_Action::publicError(m_last_error).SetPlainError(Error::BadPredictedEncoding);
				warningBehaviour res = t_warn.Notify();
				if(res != warningBehaviour::Accept && res != warningBehaviour::Continue)
				{
					return Error::BadPredictedEncoding;
				}
			}
			t_decoder	= std::make_unique<ENCODER_P::Stream_UTF16BE_Decoder>(p_stream);
			break;
		case Encoding::UCS4_LE:
			if(p_stream.remaining() % 4)
			{
				_p::Danger_Action::publicError(m_last_error).SetPlainError(Error::BadPredictedEncoding);
				warningBehaviour res = t_warn.Notify();
				if(res != warningBehaviour::Accept && res != warningBehaviour::Continue)
				{
					return Error::BadPredictedEncoding;
				}
			}
			if((p_flags & Flag::LaxedEncoding) != Flag{})
			{
				t_decoder	= std::make_unique<ENCODER_P::Stream_UCS4LE_Decoder>(p_stream);
			}
			else
			{
				t_decoder	= std::make_unique<ENCODER_P::Stream_UCS4LE_Decoder_s>(p_stream);
			}
			break;
		case Encoding::UCS4_BE:
			if(p_stream.remaining() % 4)
			{
				_p::Danger_Action::publicError(m_last_error).SetPlainError(Error::BadPredictedEncoding);
				warningBehaviour res = t_warn.Notify();
				if(res != warningBehaviour::Accept && res != warningBehaviour::Continue)
				{
					return Error::BadPredictedEncoding;
				}
			}
			if((p_flags & Flag::LaxedEncoding) != Flag{})
			{
				t_decoder	= std::make_unique<ENCODER_P::Stream_UCS4BE_Decoder>(p_stream);
			}
			else
			{
				t_decoder	= std::make_unique<ENCODER_P::Stream_UCS4BE_Decoder_s>(p_stream);
			}
			break;
		default:	//ANSI
			t_decoder	= std::make_unique<ENCODER_P::Stream_ANSI_Decoder>(p_stream);
			break;
	}

	uint16_t t_version	= 0;
	{
		uint64_t startPos = p_stream.pos();

		//collects version number
		Error t_lasErr = format::FinishVersionDecoding(*t_decoder, t_version, m_last_error);
		if(t_lasErr != Error::None)
		{
			if(t_lasErr != Error::Control_NoHeader || (p_flags & Flag::ForceHeader) == Flag{}) //No header found in SCEF file
			{
				_p::Danger_Action::publicError(m_last_error).SetPlainError(t_lasErr);
				return t_lasErr;
			}
			p_stream.set_pos(startPos);
			t_decoder->reset_context();
		}
	}

	if(read_supports_version(t_version)) //does the API support this version?
	{
		//asks the user if they want to support this version
		_p::Danger_Action::publicError(m_last_error).SetPlainError(Error::Warning_VersionDetected);
		_p::Danger_Action::publicError(m_last_error).m_extra.format = {t_version, t_encoding};
		if(t_warn.Notify() > warningBehaviour::Accept)
		{
			return Error::Warning_VersionDetected;
		}
		else
		{
			//in case version = 0, i.e. no header, tries to deformat with most recent API version
			if(t_version == __SCEF_NO_VERSION) t_version = __SCEF_API_VERSION;
		}
		m_document_properties.version = t_version;

		//chooses the formater version to use
		switch(t_version)
		{
			case 1: //start decoding based on version
				{
					format::v1::load(m_rootObject, *t_decoder, p_flags, t_version, t_warn);
				}
				break;
			default: //cosmic rays maybe?
				_p::Danger_Action::publicError(m_last_error).SetPlainError(Error::UnknownInternal);
				break;
		}
	}
	else
	{
		_p::Danger_Action::publicError(m_last_error).SetPlainError(Error::UnsuportedVersion);
	}

	return m_last_error.error_code();
}

Error document::save(base_ostreamer& p_stream, Flag p_flags, uint16_t p_version, Encoding p_encoding)
{
	m_last_error.clear();
	if(p_version == __SCEF_NO_VERSION)
	{
		p_version = __SCEF_API_VERSION;
	}
	else
	{
		if(!write_supports_version(p_version))
		{
			_p::Danger_Action::publicError(m_last_error).SetPlainError(Error::UnsuportedVersion);
			_p::Danger_Action::publicError(m_last_error).m_extra.format = {p_version, p_encoding};
			return Error::UnsuportedVersion;
		}
	}

	std::unique_ptr<stream_encoder> t_encoder;

	switch(p_encoding)
	{
		case Encoding::Unspecified:
		case Encoding::UTF8:
			//Write BOM
			if(p_stream.write(ENCODER_P::BOM_UTF8.data(), ENCODER_P::BOM_UTF8.size()) != stream_error::None)
			{
				_p::Danger_Action::publicError(m_last_error).SetPlainError(Error::Unable2Write);
				return Error::Unable2Write;
			}

			if((p_flags & Flag::LaxedEncoding) != Flag{})
			{
				t_encoder = std::make_unique<ENCODER_P::Stream_UTF8_Encoder>(p_stream);
			}
			else
			{
				t_encoder = std::make_unique<ENCODER_P::Stream_UTF8_Encoder_s>(p_stream);
			}
			break;
		case Encoding::ANSI:
			t_encoder = std::make_unique<ENCODER_P::Stream_ANSI_Encoder>(p_stream);
			break;
		case Encoding::UTF16_LE:
			//Write BOM
			if(p_stream.write(ENCODER_P::BOM_UTF16LE.data(), ENCODER_P::BOM_UTF16LE.size()) != stream_error::None)
			{
				_p::Danger_Action::publicError(m_last_error).SetPlainError(Error::Unable2Write);
				return Error::Unable2Write;
			}

			t_encoder = std::make_unique<ENCODER_P::Stream_UTF16LE_Encoder>(p_stream);
			break;
		case Encoding::UTF16_BE:
			//Write BOM
			if(p_stream.write(ENCODER_P::BOM_UTF16BE.data(), ENCODER_P::BOM_UTF16BE.size()) != stream_error::None)
			{
				_p::Danger_Action::publicError(m_last_error).SetPlainError(Error::Unable2Write);
				return Error::Unable2Write;
			}

			t_encoder = std::make_unique<ENCODER_P::Stream_UTF16BE_Encoder>(p_stream);
			break;
		case Encoding::UCS4_LE:
			//Write BOM
			if(p_stream.write(ENCODER_P::BOM_UCS4LE.data(), ENCODER_P::BOM_UCS4LE.size()) != stream_error::None)
			{
				_p::Danger_Action::publicError(m_last_error).SetPlainError(Error::Unable2Write);
				return Error::Unable2Write;
			}

			if((p_flags & Flag::LaxedEncoding) != Flag{})
			{
				t_encoder = std::make_unique<ENCODER_P::Stream_UCS4LE_Encoder>(p_stream);
			}
			else
			{
				t_encoder = std::make_unique<ENCODER_P::Stream_UCS4LE_Encoder_s>(p_stream);
			}
			break;
		case Encoding::UCS4_BE:
			//Write BOM
			if(p_stream.write(ENCODER_P::BOM_UCS4BE.data(), ENCODER_P::BOM_UCS4BE.size()) != stream_error::None)
			{
				_p::Danger_Action::publicError(m_last_error).SetPlainError(Error::Unable2Write);
				return Error::Unable2Write;
			}

			if((p_flags & Flag::LaxedEncoding) != Flag{})
			{
				t_encoder = std::make_unique<ENCODER_P::Stream_UCS4BE_Encoder>(p_stream);
			}
			else
			{
				t_encoder = std::make_unique<ENCODER_P::Stream_UCS4BE_Encoder_s>(p_stream);
			}
			break;
		default:
			_p::Danger_Action::publicError(m_last_error).SetPlainError(Error::BadEncoding);
			_p::Danger_Action::publicError(m_last_error).m_extra.format = {p_version, p_encoding};
			return Error::BadEncoding;
	}

	if(t_encoder.get())
	{
		format::_Warning_Def t_warn;
		t_warn._error_context			= &m_last_error;
		t_warn._user_context			= nullptr;
		t_warn._user_warning_callback	= DefaultWarningHandler;
		//chooses the formater version to use
		switch(p_version)
		{
			case 1:
				{
					_p::Danger_Action::publicError(m_last_error).SetPlainError(format::WriteVersion(*t_encoder, 1));
					if(m_last_error.error_code() == Error::None)
					{
						format::v1::save(m_rootObject, *t_encoder, p_flags, p_version, t_warn);
					}
				}
				break;
			default: //cosmic rays maybe?
				_p::Danger_Action::publicError(m_last_error).SetPlainError(Error::UnknownInternal);
				break;
		}
	}

	return m_last_error.error_code();
}

}	// namespace scef
