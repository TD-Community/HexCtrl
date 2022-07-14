/****************************************************************************************
* Copyright © 2018-2023 Jovibor https://github.com/jovibor/                             *
* This is a Hex Control for MFC/Win32 applications.                                     *
* Official git repository: https://github.com/jovibor/HexCtrl/                          *
* This software is available under "The HexCtrl License", see the LICENSE file.         *
****************************************************************************************/
#include "stdafx.h"
#include "../../res/HexCtrlRes.h"
#include "../HexUtility.h"
#include "CHexDlgEncoding.h"
#include <algorithm>
#include <cassert>
#include <format>

using namespace HEXCTRL;
using namespace HEXCTRL::INTERNAL;

BEGIN_MESSAGE_MAP(CHexDlgEncoding, CDialogEx)
	ON_WM_ACTIVATE()
	ON_NOTIFY(LVN_GETDISPINFOW, IDC_HEXCTRL_ENCODING_LIST, &CHexDlgEncoding::OnListGetDispInfo)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_HEXCTRL_ENCODING_LIST, &CHexDlgEncoding::OnListItemChanged)
	ON_NOTIFY(LISTEX_MSG_GETCOLOR, IDC_HEXCTRL_ENCODING_LIST, &CHexDlgEncoding::OnListGetColor)
	ON_NOTIFY(LISTEX_MSG_LINKCLICK, IDC_HEXCTRL_ENCODING_LIST, &CHexDlgEncoding::OnListLinkClick)
	ON_WM_DESTROY()
END_MESSAGE_MAP()

BOOL CHexDlgEncoding::EnumCodePagesProc(LPWSTR pwszCP)
{
	m_pThis->AddCP(pwszCP);

	return TRUE;
}

void CHexDlgEncoding::AddCP(std::wstring_view wstr)
{
	if (const auto optCPID = StrToUInt(wstr); optCPID) {
		if (CPINFOEXW stCP; GetCPInfoExW(*optCPID, 0, &stCP) != FALSE)
			m_vecCodePage.emplace_back(static_cast<int>(*optCPID), stCP.CodePageName, stCP.MaxCharSize);
	}
}

BOOL CHexDlgEncoding::Create(UINT nIDTemplate, CWnd* pParent, IHexCtrl* pHexCtrl)
{
	assert(pHexCtrl);
	if (pHexCtrl == nullptr)
		return FALSE;

	m_pHexCtrl = pHexCtrl;

	return CDialogEx::Create(nIDTemplate, pParent);
}

void CHexDlgEncoding::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BOOL CHexDlgEncoding::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	LISTEXCREATE lcs;
	lcs.uID = IDC_HEXCTRL_ENCODING_LIST;
	lcs.pParent = this;
	lcs.fDialogCtrl = true;
	lcs.fSortable = true;
	m_pListMain->Create(lcs);
	m_pListMain->SetExtendedStyle(LVS_EX_HEADERDRAGDROP);

	m_pListMain->InsertColumn(0, L"Code page", 0, 80);
	m_pListMain->InsertColumn(1, L"Name", 0, 280);
	m_pListMain->InsertColumn(2, L"Max chars", 0, 80);

	m_vecCodePage.emplace_back(-1, L"<link=\"https://en.wikipedia.org/wiki/ASCII\">ASCII 7-bit</link> (default)", 1);
	m_vecCodePage.emplace_back(0, L"Windows Internal UTF-16 (wchar_t)", 4);
	m_pThis = this;
	EnumSystemCodePagesW(EnumCodePagesProc, CP_INSTALLED);
	m_pListMain->SetItemCountEx(static_cast<int>(m_vecCodePage.size()), LVSICF_NOSCROLL);

	return TRUE;
}

void CHexDlgEncoding::OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized)
{
	if (nState == WA_ACTIVE || nState == WA_CLICKACTIVE)
	{
		if (m_pHexCtrl->IsCreated())
		{
			m_pListMain->SetItemState(-1, 0, LVIS_SELECTED);
			if (auto iter = std::find_if(m_vecCodePage.begin(), m_vecCodePage.end(),
				[this](const SCODEPAGE& ref) { return ref.iCPID == m_pHexCtrl->GetEncoding(); });
				iter != m_vecCodePage.end())
			{
				const auto iItem = static_cast<int>(iter - m_vecCodePage.begin());
				m_pListMain->SetItemState(iItem, LVIS_SELECTED, LVIS_SELECTED);
				m_pListMain->EnsureVisible(iItem, FALSE);
			}
		}
	}

	CDialogEx::OnActivate(nState, pWndOther, bMinimized);
}

BOOL CHexDlgEncoding::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	if (const auto* const pNMI = reinterpret_cast<LPNMITEMACTIVATE>(lParam); pNMI->hdr.idFrom == IDC_HEXCTRL_ENCODING_LIST)
	{
		switch (pNMI->hdr.code)
		{
		case LVN_COLUMNCLICK:
			SortList();
			break;
		default:
			break;
		}
	}

	return CDialogEx::OnNotify(wParam, lParam, pResult);
}

void CHexDlgEncoding::OnListGetDispInfo(NMHDR* pNMHDR, LRESULT* /*pResult*/)
{
	const auto pDispInfo = reinterpret_cast<NMLVDISPINFOW*>(pNMHDR);
	const auto pItem = &pDispInfo->item;

	if (pItem->mask & LVIF_TEXT)
	{
		const auto nItemID = static_cast<size_t>(pItem->iItem);
		switch (pItem->iSubItem)
		{
		case 0: //Code page ID.
			*std::format_to(pItem->pszText, L"{}", m_vecCodePage[nItemID].iCPID) = L'\0';
			break;
		case 1: //Name.
			pItem->pszText = m_vecCodePage[nItemID].wstrName.data();
			break;
		case 2: //Max chars.
			*std::format_to(pItem->pszText, L"{}", m_vecCodePage[nItemID].uMaxChars) = L'\0';
			break;
		default:
			break;
		}
	}
}

void CHexDlgEncoding::OnListItemChanged(NMHDR* pNMHDR, LRESULT* /*pResult*/)
{
	if (const auto* const pNMI = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
		pNMI->iItem != -1 && pNMI->iSubItem != -1 && (pNMI->uNewState & LVIS_SELECTED))
	{
		m_pHexCtrl->SetEncoding(m_vecCodePage[static_cast<std::size_t>(pNMI->iItem)].iCPID);
	}
}

void CHexDlgEncoding::OnListGetColor(NMHDR* pNMHDR, LRESULT* /*pResult*/)
{
	if (const auto pNMI = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
		m_vecCodePage[static_cast<std::size_t>(pNMI->iItem)].uMaxChars > 1)
	{
		static LISTEXCOLOR stClr { RGB(245, 245, 245), RGB(70, 70, 70) };
		pNMI->lParam = reinterpret_cast<LPARAM>(&stClr);
	}
}

void CHexDlgEncoding::OnListLinkClick(NMHDR* pNMHDR, LRESULT* /*pResult*/)
{
	const auto* const pNMI = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	ShellExecuteW(nullptr, L"open", reinterpret_cast<LPWSTR>(pNMI->lParam), nullptr, nullptr, SW_SHOWNORMAL);
}

void CHexDlgEncoding::SortList()
{
	const auto iColumn = m_pListMain->GetSortColumn();
	const auto fAscending = m_pListMain->GetSortAscending();
	std::sort(m_vecCodePage.begin() + 1, m_vecCodePage.end(),
		[iColumn, fAscending](const SCODEPAGE& st1, const SCODEPAGE& st2)
		{
			int iCompare { };
			switch (iColumn)
			{
			case 0: //CP ID.
				iCompare = st1.iCPID != st2.iCPID ? (st1.iCPID < st2.iCPID ? -1 : 1) : 0;
				break;
			case 1: //CP name.
				iCompare = st1.wstrName.compare(st2.wstrName);
				break;
			case 2: //Max chars.
				iCompare = st1.uMaxChars != st2.uMaxChars ? (st1.uMaxChars < st2.uMaxChars ? -1 : 1) : 0;
				break;
			default:
				break;
			}

			return fAscending ? iCompare < 0 : iCompare > 0;
		});

	m_pListMain->RedrawWindow();
}

void CHexDlgEncoding::OnDestroy()
{
	CDialogEx::OnDestroy();

	m_vecCodePage.clear();
	m_pListMain->DestroyWindow();
}