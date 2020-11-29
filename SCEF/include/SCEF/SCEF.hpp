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


//Notes:
//	1. Multi line values for translation purposes will be handle at the application level instead. I.e. with a list of values

//---- Standard Libraries ----
#include <fstream>
#include <string>
#include <string_view>
#include <filesystem>
#include <vector>

//---- Other ----
#include "scef_stream.hpp"
#include "scef_items.hpp"

///	\n
//
namespace scef
{

constexpr uint16_t __SCEF_API_VERSION	= 1;	//!< Latest supported version of the API
constexpr uint16_t __SCEF_NO_VERSION	= 0;	//!< Defaults to __SCEF_API_VERSION on save, == Error on load

constexpr uint64_t noline = 0;		//!< Used to indicate an error context that is not tied to a line in the document

///	\note
///		1. ANSI encoding should be interpreted as a raw C like single byte stream. No assumption about the underlying encoding is made except for the following:
///			1.1. Encoding is a superset of ASCII
///			If the encoding is CP1252, iso8859-1 or other is up to the user's interpretation
enum class Encoding: uint8_t
{
	Unspecified	= 0x00,	//!< defaults to UTF8 on saving, == to Error on loading
	ANSI		= 0x01,	//!< Encoding is extended 8bit ANSI
	UTF8		= 0x02,	//!< Encoding is UTF-8
	UTF16_LE	= 0x03,	//!< Encoding is UTF-16 Little-Endian
	UTF16_BE	= 0x04,	//!< Encoding is UTF-16 Big-Endian
	UCS4_LE		= 0x05,	//!< Encoding is UCS-4 (UTF-32) Little-Endian
	UCS4_BE		= 0x06,	//!< Encoding is UCS-4 (UTF-32) Big-Endian
};

///	\note
///		1. On load, if saving flags are specified, they are ignored (and vice-versa)
///		2. DisableSpacers and AutoSpacing are mutually exclusive, if both are specified AutoSpacing takes precedence
///		3. If encoding is ANSI, no check is made to confirm that data is in 7bit ASCII range, even if Flag_LaxedEncoding is not set
enum class Flag: uint8_t
{
	Default			= 0x00,	//!< All flags disabled
	DisableSpacers	= 0x01,	//!< Removes all spacing information
	DisableComments	= 0x02,	//!< Removes all comments
	LaxedEncoding	= 0x04,	//!< Laxes the encoding/decoding rules, ex. on UTF-8 characters outside UNICODE range do not cause an error
	//AcceptLowChars	= 0x08,	//accepts chars bellow 0x09

	//only works for saving
	AutoSpacing		= 0x10,	//!< Ignores all spacing information, and automatically adds new lines and indentation based on context
	AutoQuote		= 0x20,	//!< Defaults all quotation hints too Quotation_standard
	//DisableEncodeEscaping	= 0x40,

	//only works for loading
	ForceHeader		= 0x80, //!< Only accepts file if scef header exists
};

CORE_MAKE_ENUM_FLAG(Flag);

enum class Error: uint8_t
{
	None				= 0x00,		//!< No error
	FileNotFound		= static_cast<uint8_t>(stream_error::FileNotFound),	//!< The specified file was not found
	Unable2Read			= static_cast<uint8_t>(stream_error::Unable2Read),	//!< Failed to read from stream
	Unable2Write		= static_cast<uint8_t>(stream_error::Unable2Write),	//!< Failed to write to stream
	BadEncoding			= static_cast<uint8_t>(stream_error::BadEncoding),	//!< The data stream does not conform to encoding
	BadPredictedEncoding = 0x05,	//!< A bad encoding error is guaranteed based on stream size

	InvalidChar			= 0x06,	//!< Unexpected chararcter found within the document format, if reported with \ref Error_Context, then \ref Error_Context::extra.invalid_char is used
	BadEscape			= 0x07,	//!< Invalid escape sequence found within the document, if reported with \ref Error_Context, then \ref Error_Context::extra.invalid_escape is used
	UnsuportedVersion	= 0x08,	//!< The specified scef specification version is not suported. \warning \ref Error_Context::extra.format is not used
	BadFormat			= 0x09,	//!< An unresolvable error was dtected while trying to interpret the format
	UnknownObject		= 0x0A,	//!< The item type is a custom type. Type is unsuported. Users should not define their own data types.
	PrematureEnd		= 0x0B,	//!< Parser unexpectedly reached end of stream where such was not expected, file maybe truncated
	MergedText			= 0x0C,

	UnknownInternal				= 0x80,	//!< An unclassified internal error ocured
	Warning_First				= 0x81,	// Functional indicates first warning
	Warning_EncodingDetected	= 0x81,	//!< Issued when the encoding characteristics are determined, if reported with \ref Error_Context, then \ref Error_Context::extra.format.encoding is used
	Warning_VersionDetected		= 0x82,	//!< Issued when the format version of the document is determined, if reported with \ref Error_Context, then \ref Error_Context::extra.format.version is used
	Control_NoHeader			= 0xFE,	// Functional, indicates that document has no header
	Control_EndOfStream			= static_cast<uint8_t>(stream_error::Control_EndOfStream) //< Functional reached the end of stream
};

/// \brief Type of control flow to adopt when using user mode error reporting
enum class warningBehaviour: uint8_t
{
	Default		= 0x00,	//!< Chose best in context
	Continue	= 0x01,	//!< Chose best in context between accept or discard as long as parsing continues
	Accept		= 0x02,	//!< Accepts the item, as if it was ok
	Discard		= 0x03,	//!< Discards the item, as if it didn't exist
	Abort		= 0xFF	//!< Fails the parsing
};

CORE_MAKE_ENUM_ORDERABLE(warningBehaviour);

namespace _p
{
	class Danger_Action;

	struct _Error_Context
	{
	public:
		union ExtraInfo_t
		{
			struct
			{
				uint16_t	version;		//!< Document version. Used when \ref error_Code = \ref scef::Warning_VersionDetected
				Encoding	encoding;		//!< Document encoding. Used when \ref error_Code = \ref scef::Warning_EncodingDetected
			} format;

			struct
			{
				char32_t	found;			//!< Character that was found
				char32_t	expected;		//!< Character that was expected to find (sugestive).
			} invalid_char;					//!< Invalid character context information. Used when \ref error_Code = \ref scef::error_InvalidChar

			struct
			{
				const char32_t*	sequence;	//!< Characters in the escape sequence.
				uintptr_t		lengh;	//!< Number of characters in \ref sequence
			} invalid_escape;				//!< Invalid escape sequence context information. Used when \ref error_Code = \ref scef::error_BadEscape

			struct
			{
				char32_t expected;
			} premature_ending;
		};

		void clear();

		inline Error		error_code	() const { return m_error_code; }
		inline uint64_t		line		() const { return m_line; }
		inline uint64_t		column		() const { return m_column; }
		inline ExtraInfo_t	extra_info() const { return m_extra; }

		inline void set_position(uint64_t p_line, uint64_t p_column)
		{
			m_line		= p_line;
			m_column	= p_column;
		}

		inline const scef::item* cirtical_item() const { return m_criticalItem; }
		inline const std::vector<const scef::item*>& item_stack() { return m_stack; }


		void SetErrorInvalidChar	(char32_t p_found, char32_t p_expected);
		void SetErrorEscape			(const char32_t* p_sequence, uintptr_t p_lenght);
		void SetErrorPrematureEnding(char32_t p_expected);
		inline void SetPlainError	(Error p_code) { m_error_code = p_code; }

		Error m_error_code = Error::None;	//!< Error code of type \ref scef::error
		uint64_t m_line = 0;				//!< Line number where the error ocured, or \ref scef::noline if the error is not associated to a line in the document
		uint64_t m_column = 0;				//!< Column where the error ocured, count starts from 0
		ExtraInfo_t m_extra;


		std::vector<const scef::item*> m_stack;
		const scef::item* m_criticalItem = nullptr;		//!< Last item being constructed when the error ocured
	};



} //namespace _p


/// \brief Used to store an error context
class Error_Context: private _p::_Error_Context
{
	friend _p::Danger_Action;
public:
	using _p::_Error_Context::clear;
	using _p::_Error_Context::error_code;
	using _p::_Error_Context::line;
	using _p::_Error_Context::column;
	using _p::_Error_Context::extra_info;
	using _p::_Error_Context::cirtical_item;
	using _p::_Error_Context::item_stack;
};

///	\brief
///		The default warning handler
//
warningBehaviour DefaultWarningHandler(const Error_Context&, void*);

using _warning_callback = warningBehaviour (*)(const Error_Context&, void*);


class document;

///	\brief
///		Functional item representing the data content (or root node) of the document
class root final: public ItemList
{
	friend class document;
private:
	root() = default;
};

class document
{
public:
	struct doc_prop
	{
		uint16_t version = __SCEF_NO_VERSION;		//!< SCEF specification version
		Encoding encoding = Encoding::Unspecified;	//!< Document encoding
	};

public:
	document() = default;
	~document() = default;

	[[nodiscard]] inline		doc_prop& prop()		{ return m_document_properties; }
	[[nodiscard]] inline const	doc_prop& prop() const	{ return m_document_properties; }

	[[nodiscard]] inline		Error_Context& last_error()			{ return m_last_error; }
	[[nodiscard]] inline const	Error_Context& last_error() const	{ return m_last_error; }

	[[nodiscard]] inline		::scef::root& root()		{ return m_rootObject; }
	[[nodiscard]] inline const	::scef::root& root() const	{ return m_rootObject; }

	void clear();

	Error load(const std::filesystem::path& p_file, Flag p_flags, _warning_callback p_warning_callback = nullptr, void* p_user_context = nullptr);
	Error save(const std::filesystem::path& p_file, Flag p_flags, uint16_t p_version = __SCEF_NO_VERSION, Encoding p_encoding = Encoding::Unspecified);

	Error load(base_istreamer& p_stream, Flag p_flags, _warning_callback p_warning_callback = nullptr, void* p_user_context = nullptr);
	Error save(base_ostreamer& p_stream, Flag p_flags, uint16_t p_version = __SCEF_NO_VERSION, Encoding p_encoding = Encoding::Unspecified);

	static constexpr bool  read_supports_version(uint16_t p_version) { return p_version <= __SCEF_API_VERSION; }
	static constexpr bool write_supports_version(uint16_t p_version) { return p_version <= __SCEF_API_VERSION; }

private:
	doc_prop		m_document_properties;	//!< Document information, automatically filled when loading
	Error_Context	m_last_error;			//!< last error
	scef::root		m_rootObject;			//!< Root node of document. Contains all items in teh document
};


}	//namespace scef
