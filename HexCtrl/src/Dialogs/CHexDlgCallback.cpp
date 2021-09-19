/****************************************************************************************
* Copyright © 2018-2021 Jovibor https://github.com/jovibor/                             *
* This is a Hex Control for MFC/Win32 applications.                                     *
* Official git repository: https://github.com/jovibor/HexCtrl/                          *
* This software is available under "The HexCtrl License", see the LICENSE file.         *
****************************************************************************************/
#include "stdafx.h"
#include "../../res/HexCtrlRes.h"
#include "CHexDlgCallback.h"
#include <cassert>

using namespace HEXCTRL::INTERNAL;

BEGIN_MESSAGE_MAP(CHexDlgCallback, CDialogEx)
	ON_COMMAND(IDCANCEL, &CHexDlgCallback::OnBtnCancel)
	ON_WM_TIMER()
END_MESSAGE_MAP()

CHexDlgCallback::CHexDlgCallback(std::wstring_view wstrOperName, ULONGLONG ullProgBarMin,
	ULONGLONG ullProgBarMax, CWnd* pParent) : CDialogEx(IDD_HEXCTRL_CALLBACK, pParent),
	m_wstrOperName(wstrOperName), m_ullProgBarMin(ullProgBarMin), m_ullProgBarMax(ullProgBarMax)
{
	assert(ullProgBarMin <= ullProgBarMax);
}

BOOL CHexDlgCallback::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetWindowTextW(m_wstrOperName.data());
	GetDlgItem(IDC_HEXCTRL_CALLBACK_STATIC_OPERNAME)->SetWindowTextW(m_wstrOperName.data());
	SetTimer(IDT_EXITCHECK, 100, nullptr);

	constexpr auto iRange { 1000 };
	m_stProgBar.SetRange32(0, iRange);
	m_stProgBar.SetPos(0);

	m_ullThousandth = (m_ullProgBarMax - m_ullProgBarMin) >= iRange ? (m_ullProgBarMax - m_ullProgBarMin) / iRange : 1;

	return TRUE;
}

void CHexDlgCallback::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_HEXCTRL_CALLBACK_PROGBAR, m_stProgBar);
}

void CHexDlgCallback::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == IDT_EXITCHECK && m_fCancel)
	{
		KillTimer(IDT_EXITCHECK);
		OnCancel();
	}

	//How many thousandth parts have already passed.
	int iPos = static_cast<int>((m_ullProgBarCurr - m_ullProgBarMin) / m_ullThousandth);
	m_stProgBar.SetPos(iPos);

	CDialogEx::OnTimer(nIDEvent);
}

void CHexDlgCallback::OnBtnCancel()
{
	m_fCancel = true;
}

bool CHexDlgCallback::IsCanceled()const
{
	return m_fCancel;
}

void CHexDlgCallback::SetProgress(ULONGLONG ullProgCurr)
{
	m_ullProgBarCurr = ullProgCurr;
}

void CHexDlgCallback::ExitDlg()
{
	OnBtnCancel();
}