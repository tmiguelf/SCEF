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
#include <string_view>
#include <string>
#include <vector>
#include <memory>
#include <type_traits>

#include <CoreLib/string/core_string_encoding.hpp>
#include <CoreLib/string/core_string_numeric.hpp>
#include <CoreLib/core_type.hpp>

namespace scef
{

//======== ======== ======== Type Handling ======== ======== ========

class item;
class group;
class singlet;
class keyedValue;
class spacer;
class comment;


///	\brief Indicates the underlying type of item
enum class ItemType: uint8_t
{
	root			= 0x00,	//!< null or root item, functional, contains all items in document
	group			= 0x01,	//!< Object item, can contain child items,
	singlet			= 0x02,	//!< Lonely value
	key_value		= 0x03,	//!< Contains key and value

	Mask_Basic		= 0x03,	// mask basic, functional only

	//--- None relevant objects start here ---
	spacer			= 0x10,	//!< Item is a spacing information, tabs, newlines, etc...
	comment			= 0x20,	//!< Item is a comment
	Mask_Irrelevant	= 0x30,	// mask irrelevant, functional only

	Mask_All		= 0xFF
};

CORE_MAKE_ENUM_FLAG(ItemType);

///	\brief
///		On loading indicates the type of quotation mode used
///		On saving indicates the type of quotation mode that should be used
enum class QuotationMode: uint8_t
{
	standard	= 0x00,	//!< On load = no quote; on save = no quote if not needed, if needed quote depending on context
	singlemark	= 0x01,	//!< String is escaped with a single-quote mark '
	doublemark	= 0x02,	//!< String is escaped with a double-quote mark "
};

namespace _p
{
template <typename T> struct valid_scef_proxy	{ static constexpr bool value = false; };
template <> struct valid_scef_proxy<item>		{ static constexpr bool value = true; };
template <> struct valid_scef_proxy<group>		{ static constexpr bool value = true; };
template <> struct valid_scef_proxy<singlet>	{ static constexpr bool value = true; };
template <> struct valid_scef_proxy<keyedValue>	{ static constexpr bool value = true; };
template <> struct valid_scef_proxy<spacer>		{ static constexpr bool value = true; };
template <> struct valid_scef_proxy<comment>	{ static constexpr bool value = true; };

template<typename T>
concept is_valid_scef_proxy_c = valid_scef_proxy<std::remove_const_t<T>>::value;
} //namespace _p

/// \brief handles ownership model for items in lists
/// \tparam T - Item type
template<_p::is_valid_scef_proxy_c T>
using itemProxy = std::shared_ptr<T>;


//======== ======== ======== List Handling ======== ======== ========

class ItemList;
class TypeListProxy;
class constTypeListProxy;

namespace _p
{

using _p_item_list			= std::vector<itemProxy<item>>;
using _list_iterator		= _p_item_list::iterator;
using _list_const_iterator	= _p_item_list::const_iterator;

class _t_list_iterator;
class _t_list_const_iterator;

///	\internal
///	\brief
///		Special typed iterator. When used in a list, only the items of a specific type will be retrieved
class _t_list_iterator
{
	friend class _t_list_const_iterator;
	friend class ::scef::ItemList;
	friend class ::scef::TypeListProxy;
	friend class ::scef::constTypeListProxy;
private:
	_list_iterator	_node;
	_list_iterator	_end;
	ItemType		_mask;

	_t_list_iterator(const _list_iterator& p_node, const _list_iterator& p_end, ItemType p_mask);
public:
	_t_list_iterator() = default;
	_t_list_iterator(const _t_list_iterator& p_other) = default;

	_t_list_iterator& operator = (const _t_list_iterator& p_other) = default;

	[[nodiscard]] bool operator == (const _list_iterator&			p_other) const;
	[[nodiscard]] bool operator != (const _list_iterator&			p_other) const;
	[[nodiscard]] bool operator == (const _list_const_iterator&		p_other) const;
	[[nodiscard]] bool operator != (const _list_const_iterator&		p_other) const;
	[[nodiscard]] bool operator == (const _t_list_iterator&			p_other) const;
	[[nodiscard]] bool operator != (const _t_list_iterator&			p_other) const;
	[[nodiscard]] bool operator == (const _t_list_const_iterator&	p_other) const;
	[[nodiscard]] bool operator != (const _t_list_const_iterator&	p_other) const;

	_t_list_iterator&	operator ++ ();		//++i
	_t_list_iterator	operator ++ (int);	//i++

	[[nodiscard]] itemProxy<item> operator *() const;

	[[nodiscard]] bool		is_end		() const;
	[[nodiscard]] ItemType	mask		() const;
	void reSetMask(ItemType p_mask);

	[[nodiscard]] _list_iterator		to_it		();
	[[nodiscard]] _list_const_iterator	to_it		() const;
	[[nodiscard]] _list_const_iterator	to_const_it	() const;
};

///	\internal
///	\brief
///		Special typed const iterator. When used in a list, only the items of a specific type will be retrieved
class _t_list_const_iterator
{
	friend class _t_list_iterator;
	friend class ::scef::ItemList;
	friend class ::scef::TypeListProxy;
	friend class ::scef::constTypeListProxy;

private:
	_list_const_iterator	_node;
	_list_const_iterator	_end;
	ItemType				_mask{};

	_t_list_const_iterator(const _list_const_iterator& p_node, const _list_const_iterator& p_end, ItemType p_mask);
public:
	_t_list_const_iterator() = default;
	_t_list_const_iterator(const _t_list_const_iterator& p_other) = default;
	_t_list_const_iterator& operator = (const _t_list_const_iterator& p_other) = default;

	_t_list_const_iterator(const _t_list_iterator& p_other);
	_t_list_const_iterator& operator = (const _t_list_iterator& p_other);

	[[nodiscard]] bool operator == (const _list_iterator&			p_other) const;
	[[nodiscard]] bool operator != (const _list_iterator&			p_other) const;
	[[nodiscard]] bool operator == (const _list_const_iterator&		p_other) const;
	[[nodiscard]] bool operator != (const _list_const_iterator&		p_other) const;
	[[nodiscard]] bool operator == (const _t_list_iterator&			p_other) const;
	[[nodiscard]] bool operator != (const _t_list_iterator&			p_other) const;
	[[nodiscard]] bool operator == (const _t_list_const_iterator&	p_other) const;
	[[nodiscard]] bool operator != (const _t_list_const_iterator&	p_other) const;

	_t_list_const_iterator&	operator ++ ();		//++i
	_t_list_const_iterator	operator ++ (int);	//i++

	[[nodiscard]] itemProxy<const item> operator *() const;

	[[nodiscard]] bool is_end() const;

	[[nodiscard]] ItemType mask() const;
	void reSetMask(ItemType p_mask);

	[[nodiscard]] _list_const_iterator to_const_it() const;
};

} //namespace _p

using iterator				= _p::_list_iterator;
using const_iterator		= _p::_list_const_iterator;
using type_iterator			= _p::_t_list_iterator;
using const_type_iterator	= _p::_t_list_const_iterator;


class constTypeListProxy
{
public:
	using iterator			= scef::type_iterator;
	using const_iterator	= scef::const_type_iterator;
	friend class ItemList;
protected:
	constTypeListProxy(const ItemList& p_list, ItemType p_type);

public:
	[[nodiscard]] const_iterator begin	() const;
	[[nodiscard]] const_iterator end	() const;
	[[nodiscard]] const_iterator cbegin	() const;
	[[nodiscard]] const_iterator cend	() const;

	void mutate(ItemType p_type);
private:
	const ItemList& _list;
	ItemType _type;
};

class TypeListProxy
{
public:
	using iterator			= scef::type_iterator;
	using const_iterator	= scef::const_type_iterator;
	friend class ItemList;
protected:
	TypeListProxy(ItemList& p_list, ItemType p_type);

public:
	[[nodiscard]] iterator			begin	();
	[[nodiscard]] iterator			end		();
	[[nodiscard]] const_iterator	begin	() const;
	[[nodiscard]] const_iterator	end		() const;
	[[nodiscard]] const_iterator	cbegin	() const;
	[[nodiscard]] const_iterator	cend	() const;

	void mutate(ItemType p_type);
private:
	ItemList& _list;
	ItemType _type;
};

///	\brief
///		Generic list capable of containing SCEF items
class ItemList: public _p::_p_item_list
{
public:
	using type_iterator			= scef::type_iterator;
	using const_type_iterator	= scef::const_type_iterator;

	[[nodiscard]] TypeListProxy proxyList(ItemType p_type);
	[[nodiscard]] const constTypeListProxy proxyList(ItemType p_type) const;

	[[nodiscard]] type_iterator			convert2type_iterator		(iterator p_other,			ItemType p_type);
	[[nodiscard]] const_type_iterator	convert2const_type_iterator	(iterator p_other,			ItemType p_type) const;
	[[nodiscard]] const_type_iterator	convert2const_type_iterator	(const_iterator p_other,	ItemType p_type) const;

	[[nodiscard]] itemProxy<group>				find_group_by_name	(std::u32string_view p_name);
	[[nodiscard]] itemProxy<singlet>			find_singlet_by_name(std::u32string_view p_name);
	[[nodiscard]] itemProxy<keyedValue>			find_key_by_name	(std::u32string_view p_name);
	[[nodiscard]] itemProxy<const group>		find_group_by_name	(std::u32string_view p_name) const;
	[[nodiscard]] itemProxy<const singlet>		find_singlet_by_name(std::u32string_view p_name) const;
	[[nodiscard]] itemProxy<const keyedValue>	find_key_by_name	(std::u32string_view p_name) const;
};


//======== ======== ======== Common data model ======== ======== ========
namespace _p
{

class Danger_Action;

inline constexpr bool is_no_printSpace(char32_t p_val)
{
	return (p_val > 0x08 && p_val < 0x0E);
}

inline constexpr bool is_space(char32_t p_val)
{
	return is_no_printSpace(p_val) || p_val == ' ';
}

inline constexpr bool is_space_noLF(char32_t p_val)
{
	return p_val != '\n' && is_space(p_val);
}

/// \brief Wraps the property of an SCEF item having a name
class NamedItem
{
protected:
	NamedItem();

	QuotationMode	_quotation_mode;
	std::u32string	_name;
public:

	[[nodiscard]] std::u32string&		name			();
	[[nodiscard]] const std::u32string&	name			() const;
	[[nodiscard]] std::u32string_view	view_name		() const;
	[[nodiscard]] QuotationMode			quotation_mode	() const;

	void set_name		(std::u32string_view p_text);
	void set_quotation_mode(QuotationMode p_mode);
	void clear_name		();
};

/// \brief Wraps the property of an SCEF item having an inline spacing
class lineSpace
{
	friend Danger_Action;
private:
	std::u8string	_space;
public:
	[[nodiscard]] std::u8string_view spacing() const;

	void set_spacing	(std::u8string_view p_spacing);
	void append_spacing	(std::u8string_view p_spacing);
	void clear			();
};

/// \brief Wraps the property of an SCEF item having a multiline spacing
class multiLineSpace
{
	friend Danger_Action;
private:
	uint64_t		_lines = 0;
	std::u8string	_space;
public:
	[[nodiscard]] std::u8string_view	flat_spacing	() const;
	[[nodiscard]] uint64_t				num_lines		() const;

	void set_spacing	(uint64_t p_lines, const std::u8string& p_spacing);
	void append_spacing	(uint64_t p_lines, const std::u8string& p_spacing);
	void clear			();
};

} //namespace _p


//======== ======== ======== Items ======== ======== ========

/// \brief Abstract class representing an SCEF entity
class item
{
protected:
	item(ItemType p_type);

public:
	virtual ~item();

	[[nodiscard]] ItemType	type	() const;
	[[nodiscard]] uint64_t	line	() const;
	[[nodiscard]] uint64_t	column	() const;
	void set_position(uint64_t p_line, uint64_t p_column);

	uintptr_t m_userToken = 0;
private:
	item(const item&)				= delete;
	item(item&&)					= delete;
	item& operator = (const item&)	= delete;
	item& operator = (item&&)		= delete;

	const ItemType _type;
	uint64_t _line	 = 0;
	uint64_t _column = 0;
};

///	\brief
///		SCEF entity designed to contain spacing information, such hat sapcing information can be saved when loading then saving a document
///
///	\note
///		On saving:
///			1. Contiguous spacers will be merged
///			2. Spacing information at the end of file is suppressed
///			3. White spaces, horizontal tabs and non-breaking-spaces before end of line will be suppressed
///			4. Carriage return not followed by a new line will be suppressed
///			5. Characters not classified for spacing will be suppressed
///			6. Empty spacers will be removed
class spacer final: public item, public _p::multiLineSpace
{
private:
	spacer();

public:
	static itemProxy<spacer> make();
	static constexpr ItemType static_type(){ return ItemType::spacer; }
};



///	\brief
///		SCEF entity designed to contain comments in the document
///
/// \note
///		1. New line after comment is implicit
///		2. On Saving:
///			2.1. New line in the middle of comment forces a comment split
///			2.2. White spaces and horizontal tabs before end of line will be suppressed
class comment final: public item
{
private:
	comment();
	std::u32string	_text;

public:
	[[nodiscard]] static itemProxy<comment> make();
	[[nodiscard]] static constexpr ItemType static_type() { return ItemType::comment; }

	[[nodiscard]] std::u32string&		str		();
	[[nodiscard]] const std::u32string&	str		() const;
	[[nodiscard]] std::u32string_view	view	() const;

	void set	(std::u32string_view p_text);
	void clear	();
};

///	\brief SCEF entity that can contain child items
class group final: public item, public _p::NamedItem, public ItemList
{
private:
	group();

public:
	[[nodiscard]] static itemProxy<group> make();
	[[nodiscard]] static constexpr ItemType static_type() { return ItemType::group; }

	_p::lineSpace m_preSpace;
	_p::lineSpace m_postSpace;
};

/// \brief SCEF entity that contains a lonely value
class singlet: public item, public _p::NamedItem
{
private:
	singlet();

public:
	[[nodiscard]] static itemProxy<singlet> make();
	[[nodiscard]] static constexpr ItemType static_type() { return ItemType::singlet; }

	_p::lineSpace m_postSpace;
};


///	\brief
///		SCEF entity that contains a key and a value
///
///	\note
///		1. Semi-colon at the end current item is implicit
class keyedValue final: public item, public _p::NamedItem
{
private:
	QuotationMode	_value_quotation_mode = QuotationMode::standard;
	std::u32string	_value;
	uint64_t m_valueColumn = 0;

private:
	keyedValue();

public:
	[[nodiscard]] static itemProxy<keyedValue> make();
	[[nodiscard]] static constexpr ItemType static_type(){ return ItemType::key_value; }

	[[nodiscard]] std::u32string&		value();
	[[nodiscard]] const std::u32string&	value() const;
	[[nodiscard]] std::u32string_view	view_value() const;

	template<core::char_conv_dec_supported_c T>
	[[nodiscard]] inline ::core::from_chars_result<T> value_as_num() const { return core::from_chars<T>(_value); };

	void set_value(std::u32string_view p_text);

	[[nodiscard]] QuotationMode value_quotation_mode() const;

	void set_value_quotation_mode(QuotationMode p_mode);
	void clear_value();

	uint64_t column_value() const;
	void set_column_value(uint64_t p_column);

	_p::lineSpace m_preSpace;
	_p::lineSpace m_midSpace;
	_p::lineSpace m_postSpace;
};


//======== ======== ======== Inline optimizations ======== ======== ========

namespace _p
{
//======== ======== class _t_list_iterator
inline bool _t_list_iterator::operator == (const _list_iterator&			p_other) const { return _node == p_other; }
inline bool _t_list_iterator::operator != (const _list_iterator&			p_other) const { return _node != p_other; }
inline bool _t_list_iterator::operator == (const _list_const_iterator&		p_other) const { return _node == p_other; }
inline bool _t_list_iterator::operator != (const _list_const_iterator&		p_other) const { return _node != p_other; }
inline bool _t_list_iterator::operator == (const _t_list_iterator&			p_other) const { return _node == p_other._node; }
inline bool _t_list_iterator::operator != (const _t_list_iterator&			p_other) const { return _node != p_other._node; }
inline bool _t_list_iterator::operator == (const _t_list_const_iterator&	p_other) const { return _node == p_other._node; }
inline bool _t_list_iterator::operator != (const _t_list_const_iterator&	p_other) const { return _node != p_other._node; }

inline itemProxy<item> _t_list_iterator::operator * () const { return _node.operator *(); }

inline bool					_t_list_iterator::is_end		() const	{ return _node == _end; }
inline ItemType				_t_list_iterator::mask			() const	{ return _mask; }
inline _list_iterator		_t_list_iterator::to_it			()			{ return _node; }
inline _list_const_iterator	_t_list_iterator::to_it			() const	{ return _node; }
inline _list_const_iterator	_t_list_iterator::to_const_it	() const	{ return _node; }

//======== ======== class _t_list_const_iterator
inline bool _t_list_const_iterator::operator == (const _list_iterator&			p_other) const { return _node == p_other; }
inline bool _t_list_const_iterator::operator != (const _list_iterator&			p_other) const { return _node != p_other; }
inline bool _t_list_const_iterator::operator == (const _list_const_iterator&	p_other) const { return _node == p_other; }
inline bool _t_list_const_iterator::operator != (const _list_const_iterator&	p_other) const { return _node != p_other; }
inline bool _t_list_const_iterator::operator == (const _t_list_iterator&		p_other) const { return _node == p_other._node; }
inline bool _t_list_const_iterator::operator != (const _t_list_iterator&		p_other) const { return _node != p_other._node; }
inline bool _t_list_const_iterator::operator == (const _t_list_const_iterator&	p_other) const { return _node == p_other._node; }
inline bool _t_list_const_iterator::operator != (const _t_list_const_iterator&	p_other) const { return _node != p_other._node; }

inline itemProxy<const item> _t_list_const_iterator::operator * () const { return _node.operator *(); }

inline bool					_t_list_const_iterator::is_end		() const { return _node == _end; }
inline ItemType				_t_list_const_iterator::mask		() const { return _mask; }
inline _list_const_iterator	_t_list_const_iterator::to_const_it	() const { return _node; }

//======== ======== class NamedItem
inline NamedItem::NamedItem(): _quotation_mode(QuotationMode::standard){}

inline std::u32string&			NamedItem::name					()								{ return _name; }
inline const std::u32string&	NamedItem::name					() const 						{ return _name; }
inline std::u32string_view		NamedItem::view_name			() const						{ return _name; }
inline void						NamedItem::set_name				(std::u32string_view p_text)	{ _name = p_text; }
inline QuotationMode			NamedItem::quotation_mode		() const						{ return _quotation_mode; }
inline void						NamedItem::set_quotation_mode	(QuotationMode p_mode)			{ _quotation_mode = p_mode; }
inline void						NamedItem::clear_name			()								{ _name.clear(); }


//======== ======== class lineSpace
inline std::u8string_view	lineSpace::spacing	() const	{ return _space; }
inline void					lineSpace::clear	()			{ _space.clear(); }


//======== ======== class multiLineSpace
inline std::u8string_view	multiLineSpace::flat_spacing	() const	{ return _space; }
inline uint64_t				multiLineSpace::num_lines		() const	{ return _lines; }
inline void					multiLineSpace::clear			()			{ _lines = 0; _space.clear(); }

} //namespace _p

//======== ======== class constTypeListProxy
inline constTypeListProxy::constTypeListProxy(const ItemList& p_list, ItemType p_type): _list{p_list}, _type{p_type} {}
inline void constTypeListProxy::mutate(ItemType p_type) { _type = p_type; }
inline const_type_iterator constTypeListProxy::begin	() const	{ return const_type_iterator{_list.cbegin(), _list.cend(), _type}; }
inline const_type_iterator constTypeListProxy::end		() const	{ return const_type_iterator{_list.cend  (), _list.cend(), _type}; }
inline const_type_iterator constTypeListProxy::cbegin	() const	{ return const_type_iterator{_list.cbegin(), _list.cend(), _type}; }
inline const_type_iterator constTypeListProxy::cend		() const	{ return const_type_iterator{_list.cend  (), _list.cend(), _type}; }

//======== ======== class TypeListProxy
inline TypeListProxy::TypeListProxy(ItemList& p_list, ItemType p_type): _list{p_list}, _type{p_type} {}
inline void TypeListProxy::mutate(ItemType p_type) { _type = p_type; }
inline type_iterator		TypeListProxy::begin	()			{ return type_iterator		{_list.begin (), _list.end (), _type}; }
inline type_iterator		TypeListProxy::end		()			{ return type_iterator		{_list.end   (), _list.end (), _type}; }
inline const_type_iterator	TypeListProxy::begin	() const	{ return const_type_iterator{_list.cbegin(), _list.cend(), _type}; }
inline const_type_iterator	TypeListProxy::end		() const	{ return const_type_iterator{_list.cend  (), _list.cend(), _type}; }
inline const_type_iterator	TypeListProxy::cbegin	() const	{ return const_type_iterator{_list.cbegin(), _list.cend(), _type}; }
inline const_type_iterator	TypeListProxy::cend		() const	{ return const_type_iterator{_list.cend  (), _list.cend(), _type}; }

//======== ======== class ItemList
inline type_iterator		ItemList::convert2type_iterator			(iterator p_other,			ItemType p_type)		{ return type_iterator		(p_other, end (), p_type); }
inline const_type_iterator	ItemList::convert2const_type_iterator	(iterator p_other,			ItemType p_type) const	{ return const_type_iterator(p_other, cend(), p_type); }
inline const_type_iterator	ItemList::convert2const_type_iterator	(const_iterator p_other,	ItemType p_type) const	{ return const_type_iterator(p_other, cend(), p_type); }

inline TypeListProxy			ItemList::proxyList(ItemType p_type)		{ return TypeListProxy		{*this, p_type}; }
inline const constTypeListProxy	ItemList::proxyList(ItemType p_type) const	{ return constTypeListProxy	{*this, p_type}; }

//======== ======== class item
inline item::item(ItemType p_type): _type(p_type) {}

inline ItemType		item::type		() const			{ return _type; }
inline uint64_t		item::line		() const			{ return _line; }
inline uint64_t		item::column	() const			{ return _column; }
inline void			item::set_position(uint64_t p_line, uint64_t p_column) { _line = p_line; _column = p_column; }

//======== ======== class spacer
inline spacer::spacer(): item(static_type()) {}
inline itemProxy<spacer> spacer::make() { return itemProxy<spacer>{new spacer()}; }

//======== ======== class comment
inline comment::comment(): item(static_type()) {}
inline itemProxy<comment> comment::make() { return itemProxy<comment>{new comment()}; }

inline std::u32string&			comment::str	()								{ return _text; }
inline const std::u32string&	comment::str	() const						{ return _text; }
inline std::u32string_view		comment::view	() const						{ return _text; }
inline void						comment::set	(std::u32string_view p_text)	{ _text = p_text; }
inline void						comment::clear	()								{ _text.clear(); }

//======== ======== class singlet
inline singlet::singlet(): item(static_type()), NamedItem() {}
inline itemProxy<singlet> singlet::make() { return itemProxy<singlet>{new singlet()}; }

//======== ======== class keyedValue
inline keyedValue::keyedValue(): item(static_type()), NamedItem() {}
inline itemProxy<keyedValue> keyedValue::make() { return itemProxy<keyedValue>{new keyedValue()}; }

inline std::u32string&			keyedValue::value()									{ return _value; }
inline const std::u32string&	keyedValue::value() const 							{ return _value; }
inline std::u32string_view		keyedValue::view_value() const						{ return _value; }
inline void						keyedValue::set_value(std::u32string_view p_text)	{ _value = p_text; }

inline QuotationMode	keyedValue::value_quotation_mode	() const				{ return _value_quotation_mode; }
inline void				keyedValue::set_value_quotation_mode(QuotationMode p_mode)	{ _value_quotation_mode = p_mode; }
inline void				keyedValue::clear_value				()						{ _value.clear(); }
inline uint64_t			keyedValue::column_value			() const				{ return m_valueColumn; }
inline void				keyedValue::set_column_value		(uint64_t p_column)		{ m_valueColumn = p_column; }

//======== ======== class group
inline group::group(): item(static_type()), NamedItem() {}
inline itemProxy<group> group::make() { return itemProxy<group>{new group()}; }

} //namespace scef
