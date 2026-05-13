/*
// Copyright (c) 2026, Joachim Schurig (jschurig@gmx.net)
//
// +-------------------------------------------------------------------------+
// | /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\|
// |/+---------------------------------------------------------------------+/|
// |/|                                                                     |/|
// |\|  ** THIS NOTICE MUST NOT BE REMOVED FROM THE SOURCE CODE MODULE **  |\|
// |/|                                                                     |/|
// |\|   OPEN SOURCE LICENSE                                               |\|
// |/|                                                                     |/|
// |\|   Permission is hereby granted, free of charge, to any person       |\|
// |/|   obtaining a copy of this software and associated                  |/|
// |\|   documentation files (the "Software"), to deal in the              |\|
// |/|   Software without restriction, including without limitation        |/|
// |\|   the rights to use, copy, modify, merge, publish,                  |\|
// |/|   distribute, sublicense, and/or sell copies of the Software,       |/|
// |\|   and to permit persons to whom the Software is furnished to        |\|
// |/|   do so, subject to the following conditions:                       |/|
// |\|                                                                     |\|
// |/|   The above copyright notice and this permission notice shall       |/|
// |\|   be included in all copies or substantial portions of the          |\|
// |/|   Software.                                                         |/|
// |\|                                                                     |\|
// |/|   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY         |/|
// |\|   KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE        |\|
// |/|   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR           |/|
// |\|   PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS        |\|
// |/|   OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR          |/|
// |\|   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |\|
// |/|   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE         |/|
// |\|   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.            |\|
// |/|                                                                     |/|
// |/+---------------------------------------------------------------------+/|
// |\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/ |
// +-------------------------------------------------------------------------+
*/

#pragma once

/// @file kwebobjects_templates.h
/// Template implementations for the bound-input KWebObjects
/// (TextInput, NumericInput, DurationInput, CheckBox, RadioButton,
/// Selection). Separate from kwebobjects.h purely to keep the main
/// header readable.
///
/// Each bound input registers a side-map binding in the document
/// (`khtml::InteractiveBinding`) so that `Synchronize(QueryParms)` can
/// invoke type-specific reset/sync logic without virtual dispatch.

#include <dekaf2/core/strings/kstring.h>
#include <dekaf2/core/strings/kstringview.h>
#include <dekaf2/web/url/kurl.h>
#include <chrono>
#include <type_traits>

DEKAF2_NAMESPACE_BEGIN

namespace html {

namespace detail {

	//-----------------------------------------------------------------------------
	// Convert App-Var → KStringView for emission as the initial `value=""`
	// attribute. Per-type specialisations cover strings, arithmetics, bools,
	// chrono-durations.
	//-----------------------------------------------------------------------------

	template<typename T, typename = void>
	struct InitialAttrValue
	{
		static KString Get(const T& v) { return kFormat("{}", v); }
	};

	template<typename T>
	struct InitialAttrValue<T, std::enable_if_t<std::is_convertible<T, KStringView>::value>>
	{
		static KStringView Get(const T& v) { return KStringView(v); }
	};

	template<>
	struct InitialAttrValue<bool>
	{
		static KStringView Get(bool /*v*/) { return KStringView{}; }
	};

	//-----------------------------------------------------------------------------
	// Generic "is empty?" — used to decide whether to emit the initial value.
	//-----------------------------------------------------------------------------

	template<typename T, typename = void>
	struct IsEmpty
	{
		static bool Check(const T&) { return false; }
	};

	template<typename T>
	struct IsEmpty<T, std::enable_if_t<std::is_arithmetic<T>::value && !std::is_same<T, bool>::value>>
	{
		static bool Check(const T& v) { return v == T{}; }
	};

	template<>
	struct IsEmpty<bool>
	{
		static bool Check(bool v) { return !v; }
	};

	template<typename T>
	struct IsEmpty<T, std::enable_if_t<std::is_convertible<T, KStringView>::value>>
	{
		static bool Check(const T& v) { return KStringView(v).empty(); }
	};

	//-----------------------------------------------------------------------------
	// Conversion from query-parm KStringView → App-Var. Per-type.
	//-----------------------------------------------------------------------------

	template<typename T, typename = void>
	struct FromQueryParm
	{
		static T Apply(KStringView v) { return T(v); }
	};

	template<typename T>
	struct FromQueryParm<T, std::enable_if_t<std::is_integral<T>::value && !std::is_same<T, bool>::value>>
	{
		static T Apply(KStringView v) { return static_cast<T>(KString(v).Int64()); }
	};

	template<typename T>
	struct FromQueryParm<T, std::enable_if_t<std::is_enum<T>::value>>
	{
		static T Apply(KStringView v) { return static_cast<T>(KString(v).Int64()); }
	};

	template<typename T>
	struct FromQueryParm<T, std::enable_if_t<std::is_floating_point<T>::value>>
	{
		static T Apply(KStringView v)
		{
			KString s(v);
			s.Replace(',', '.');                  // tolerate European decimal point
			return static_cast<T>(s.Double());
		}
	};

	template<>
	struct FromQueryParm<bool>
	{
		static bool Apply(KStringView v) { return !v.empty(); }
	};

	template<>
	struct FromQueryParm<KString>
	{
		static KString Apply(KStringView v) { return KString(v); }
	};

	template<>
	struct FromQueryParm<std::string>
	{
		static std::string Apply(KStringView v) { return std::string(v); }
	};

	//-----------------------------------------------------------------------------
	// Clamp helper for NumericInput — honour min/max attributes when syncing.
	//-----------------------------------------------------------------------------

	template<typename T>
	T ClampToAttribute(T value, KStringView sMin, KStringView sMax,
	                   std::enable_if_t<std::is_arithmetic<T>::value, int> = 0)
	{
		if (!sMin.empty()) { T mn = FromQueryParm<T>::Apply(sMin); if (value < mn) value = mn; }
		if (!sMax.empty()) { T mx = FromQueryParm<T>::Apply(sMax); if (value > mx) value = mx; }
		return value;
	}

} // namespace detail

// -- TextInput -------------------------------------------------------------

template<typename String>
TextInput<String>::TextInput(KHTMLNode parent,
                             String&    rResult,
                             KStringView sName,
                             const html::Classes& cls, KStringView sID)
: base(parent, sName, KStringView{}, Input::TEXT, cls, sID)
{
	if (!detail::IsEmpty<String>::Check(rResult))
	{
		this->m_Base.KHTMLNode::SetAttribute("value", KStringView(detail::InitialAttrValue<String>::Get(rResult)));
	}

	khtml::InteractiveBinding binding;
	binding.pResult  = static_cast<void*>(&rResult);
	binding.pfnSync  = [](void* p, khtml::NodePOD* pInput, KStringView v)
	{
		*static_cast<String*>(p) = detail::FromQueryParm<String>::Apply(v);
		KHTMLNode(pInput).SetAttribute("value", v);
	};
	binding.pfnReset = nullptr;
	this->RegisterBinding(this->m_Base, binding);
}

// -- NumericInput ----------------------------------------------------------

template<typename Arithmetic>
NumericInput<Arithmetic>::NumericInput(KHTMLNode parent,
                                       Arithmetic& rResult,
                                       KStringView sName,
                                       const html::Classes& cls, KStringView sID)
: base(parent, sName, KStringView{}, Input::NUMBER, cls, sID)
{
	if (rResult != Arithmetic{})
	{
		this->m_Base.KHTMLNode::SetAttribute("value", kFormat("{}", rResult));
	}

	khtml::InteractiveBinding binding;
	binding.pResult  = static_cast<void*>(&rResult);
	binding.pfnSync  = [](void* p, khtml::NodePOD* pInput, KStringView v)
	{
		KHTMLNode input(pInput);
		Arithmetic val = detail::FromQueryParm<Arithmetic>::Apply(v);
		val = detail::ClampToAttribute<Arithmetic>(val, input.GetAttribute("min"), input.GetAttribute("max"));
		*static_cast<Arithmetic*>(p) = val;
		input.KHTMLNode::SetAttribute("value", kFormat("{}", val));
	};
	binding.pfnReset = nullptr;
	this->RegisterBinding(this->m_Base, binding);
}

// -- DurationInput ---------------------------------------------------------

template<typename Unit, typename Duration>
DurationInput<Unit, Duration>::DurationInput(KHTMLNode parent,
                                             Duration&   rResult,
                                             KStringView sName,
                                             const html::Classes& cls, KStringView sID)
: base(parent, sName, KStringView{}, Input::NUMBER, cls, sID)
{
	auto iCount = std::chrono::duration_cast<Unit>(rResult).count();
	if (iCount != 0)
	{
		this->m_Base.KHTMLNode::SetAttribute("value", kFormat("{}", iCount));
	}

	khtml::InteractiveBinding binding;
	binding.pResult  = static_cast<void*>(&rResult);
	binding.pfnSync  = [](void* p, khtml::NodePOD* pInput, KStringView v)
	{
		KHTMLNode input(pInput);
		auto iVal = detail::FromQueryParm<int64_t>::Apply(v);
		iVal = detail::ClampToAttribute<int64_t>(iVal, input.GetAttribute("min"), input.GetAttribute("max"));
		*static_cast<Duration*>(p) = std::chrono::duration_cast<Duration>(Unit(iVal));
		input.KHTMLNode::SetAttribute("value", kFormat("{}", iVal));
	};
	binding.pfnReset = nullptr;
	this->RegisterBinding(this->m_Base, binding);
}

// -- CheckBox --------------------------------------------------------------

template<typename Boolean>
CheckBox<Boolean>::CheckBox(KHTMLNode parent,
                            Boolean&   rResult,
                            KStringView sName,
                            const html::Classes& cls, KStringView sID)
: base(parent, sName, KStringView{}, Input::CHECKBOX, cls, sID)
{
	if (rResult)
	{
		this->m_Base.KHTMLNode::SetAttribute("checked", "");
	}

	khtml::InteractiveBinding binding;
	binding.pResult  = static_cast<void*>(&rResult);
	binding.pfnSync  = [](void* p, khtml::NodePOD* pInput, KStringView /*v*/)
	{
		*static_cast<Boolean*>(p) = true;
		KHTMLNode(pInput).KHTMLNode::SetAttribute("checked", "");
	};
	binding.pfnReset = [](void* p, khtml::NodePOD* pInput)
	{
		*static_cast<Boolean*>(p) = false;
		KHTMLNode(pInput).KHTMLNode::RemoveAttribute("checked");
	};
	this->RegisterBinding(this->m_Base, binding);
}

// -- RadioButton -----------------------------------------------------------

template<typename ValueType>
RadioButton<ValueType>::RadioButton(KHTMLNode parent,
                                    ValueType& rResult,
                                    KStringView sName,
                                    const html::Classes& cls, KStringView sID)
: base(parent, sName, KStringView{}, Input::RADIO, cls, sID)
{
	khtml::InteractiveBinding binding;
	binding.pResult  = static_cast<void*>(&rResult);
	binding.pfnSync  = [](void* p, khtml::NodePOD* pInput, KStringView v)
	{
		KHTMLNode input(pInput);
		auto sValAttr = input.GetAttribute("value");
		// the radio that matches the query-parm value wins
		if (sValAttr == v)
		{
			*static_cast<ValueType*>(p) = detail::FromQueryParm<ValueType>::Apply(sValAttr);
			input.KHTMLNode::SetAttribute("checked", "");
		}
	};
	binding.pfnReset = [](void* /*p*/, khtml::NodePOD* pInput)
	{
		KHTMLNode input(pInput);
		input.KHTMLNode::RemoveAttribute("checked");
	};
	this->RegisterBinding(this->m_Base, binding);
}

// -- Selection -------------------------------------------------------------

template<typename ValueType>
Selection<ValueType>::Selection(KHTMLNode parent,
                                ValueType& rResult,
                                KStringView sName,
                                uint16_t    iSize,
                                const html::Classes& cls, KStringView sID)
: base(parent, sName, iSize, cls, sID)
, m_pResult(&rResult)
{
	khtml::InteractiveBinding binding;
	binding.pResult  = static_cast<void*>(&rResult);
	binding.pfnSync  = [](void* p, khtml::NodePOD* pSelect, KStringView v)
	{
		KHTMLNode select(pSelect);
		// Walk all <option> children. The one whose `value` attribute
		// matches v (or whose text label equals v) becomes selected; all
		// others are reset.
		for (auto c = select.FirstChild(); c; c = c.NextSibling())
		{
			if (c.Kind() != khtml::NodeKind::Element || c.Name() != "option") continue;

			auto sValAttr = c.GetAttribute("value");
			KStringView sLabel;
			for (auto cc = c.FirstChild(); cc; cc = cc.NextSibling())
			{
				if (cc.IsText()) { sLabel = cc.Name(); break; }
			}

			if ((!sValAttr.empty() && sValAttr == v) || sLabel == v)
			{
				c.KHTMLNode::SetAttribute("selected", "");
				*static_cast<ValueType*>(p) = detail::FromQueryParm<ValueType>::Apply(sLabel);
			}
			else
			{
				c.KHTMLNode::RemoveAttribute("selected");
			}
		}
	};
	binding.pfnReset = nullptr;
	this->RegisterBinding(this->m_Base, binding);
}

template<typename ValueType>
void Selection<ValueType>::AddOption(KStringView sLabel)
{
	auto opt = this->m_Base.template Add<Option>(sLabel);
	if (m_pResult != nullptr)
	{
		// Compare textual representation of result with the option label.
		// For string types this is direct; for arithmetics we kFormat it.
		auto sCurrent = KString(detail::InitialAttrValue<ValueType>::Get(*m_pResult));
		if (sCurrent == sLabel)
		{
			opt.SetSelected(true);
		}
	}
	// no per-option binding — the Select's pfnSync drives all options.
}

template<typename ValueType>
Selection<ValueType>& Selection<ValueType>::SetOptions(KStringView sOptions)
{
	for (const auto& sOption : sOptions.Split())
	{
		AddOption(sOption);
	}
	return *this;
}

} // namespace html

DEKAF2_NAMESPACE_END
