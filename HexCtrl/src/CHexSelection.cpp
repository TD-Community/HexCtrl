/****************************************************************************************
* Copyright © 2018-2021 Jovibor https://github.com/jovibor/                             *
* This is a Hex Control for MFC/Win32 applications.                                     *
* Official git repository: https://github.com/jovibor/HexCtrl/                          *
* This software is available under the "MIT License modified with The Commons Clause".  *
* https://github.com/jovibor/HexCtrl/blob/master/LICENSE                                *
****************************************************************************************/
#include "stdafx.h"
#include "CHexSelection.h"
#include <algorithm>
#include <cassert>

using namespace HEXCTRL;
using namespace HEXCTRL::INTERNAL;

void CHexSelection::ClearAll()
{
	m_vecSelection.clear();
	m_vecSelHighlight.clear();
	m_ullMarkSelStart = 0xFFFFFFFFFFFFFFFFULL;
	m_ullMarkSelEnd = 0xFFFFFFFFFFFFFFFFULL;
}

bool CHexSelection::HasSelection()const
{
	return !m_vecSelection.empty();
}

bool CHexSelection::HasSelHighlight() const
{
	return !m_vecSelHighlight.empty();
}

bool CHexSelection::HitTest(ULONGLONG ullOffset)const
{
	return std::any_of(m_vecSelection.begin(), m_vecSelection.end(),
		[ullOffset](const HEXSPAN& ref)
		{ return ullOffset >= ref.ullOffset && ullOffset < (ref.ullOffset + ref.ullSize); });
}

bool CHexSelection::HitTestHighlight(ULONGLONG ullOffset) const
{
	return std::any_of(m_vecSelHighlight.begin(), m_vecSelHighlight.end(),
		[ullOffset](const HEXSPAN& ref)
		{ return ullOffset >= ref.ullOffset && ullOffset < (ref.ullOffset + ref.ullSize); });
}

bool CHexSelection::HitTestRange(const HEXSPAN& hss)const
{
	return std::any_of(m_vecSelection.begin(), m_vecSelection.end(),
		[&](const HEXSPAN& ref)
		{
			return (hss.ullOffset >= ref.ullOffset && hss.ullOffset < (ref.ullOffset + ref.ullSize))
				|| (ref.ullOffset >= hss.ullOffset && ref.ullOffset < (hss.ullOffset + hss.ullSize))
				|| (hss.ullOffset + hss.ullSize > ref.ullOffset && hss.ullOffset + hss.ullSize <= (ref.ullOffset + ref.ullSize));
		});
}

ULONGLONG CHexSelection::GetSelEnd()const
{
	if (!HasSelection())
		return 0xFFFFFFFFFFFFFFFFULL;

	return m_vecSelection.back().ullOffset + m_vecSelection.back().ullSize - 1;
}

ULONGLONG CHexSelection::GetSelSize()const
{
	if (!HasSelection())
		return 0;

	return m_vecSelection.size() * m_vecSelection.at(0).ullSize;
}

ULONGLONG CHexSelection::GetSelStart()const
{
	if (!HasSelection())
		return 0xFFFFFFFFFFFFFFFFULL;

	return m_vecSelection.front().ullOffset;
}

DWORD CHexSelection::GetLineLength()const
{
	if (!HasSelection())
		return 0;

	return static_cast<DWORD>(m_vecSelection.front().ullSize);
}

ULONGLONG CHexSelection::GetOffsetByIndex(ULONGLONG ullIndex)const
{
	ULONGLONG ullOffset { 0xFFFFFFFFFFFFFFFFULL };

	if (ullIndex >= GetSelSize())
		return ullOffset;

	ULONGLONG ullTotal { };
	for (const auto& iterData : m_vecSelection)
	{
		ullTotal += iterData.ullSize;
		if (ullIndex < ullTotal)
		{
			ullOffset = iterData.ullOffset + (ullIndex - (ullTotal - iterData.ullSize));
			break;
		}
	}
	return ullOffset;
}

auto CHexSelection::GetData()const->std::vector<HEXSPAN>
{
	return m_vecSelection;
}

void CHexSelection::SetSelection(const std::vector<HEXSPAN>& vecSel, bool fHighlight)
{
	if (fHighlight)
		m_vecSelHighlight = vecSel;
	else
	{
		m_vecSelHighlight.clear(); //On new selection clear all highlights.
		m_vecSelection = vecSel;
	}
}

void CHexSelection::SetSelStartEnd(ULONGLONG ullOffset, bool fStart)
{
	if (fStart)
	{
		m_ullMarkSelStart = ullOffset;
		if (m_ullMarkSelEnd == 0xFFFFFFFFFFFFFFFFULL || m_ullMarkSelStart > m_ullMarkSelEnd)
			return;
	}
	else
	{
		m_ullMarkSelEnd = ullOffset;
		if (m_ullMarkSelStart == 0xFFFFFFFFFFFFFFFFULL || m_ullMarkSelEnd < m_ullMarkSelStart)
			return;
	}

	m_vecSelection.clear();
	m_vecSelection.emplace_back(HEXSPAN { m_ullMarkSelStart, m_ullMarkSelEnd - m_ullMarkSelStart + 1 });
}