/****************************************************************************************
* Copyright © 2018-2023 Jovibor https://github.com/jovibor/                             *
* This is a Hex Control for MFC/Win32 applications.                                     *
* Official git repository: https://github.com/jovibor/HexCtrl/                          *
* This software is available under "The HexCtrl License", see the LICENSE file.         *
****************************************************************************************/
#pragma once
#include "../../dep/ListEx/ListEx.h"
#include "../../HexCtrl.h"
#include <afxdialogex.h>

namespace HEXCTRL::INTERNAL
{
	//Forward declarations.
	struct HEXTEMPLATE;
	struct HEXTEMPLATEFIELD;
	using PHEXTEMPLATE = HEXTEMPLATE*;
	using PtrField = std::unique_ptr<HEXTEMPLATEFIELD>;
	using VecFields = std::vector<PtrField>; //Vector for the Fields.
	using PVecFields = VecFields*;
	using PHEXTEMPLATEFIELD = HEXTEMPLATEFIELD*;
	using PCHEXTEMPLATEFIELD = const HEXTEMPLATEFIELD*;

	enum class EFieldType : std::uint8_t {
		custom_size, type_custom,
		type_bool, type_char, type_uchar, type_short, type_ushort, type_int,
		type_uint, type_ll, type_ull, type_float, type_double, type_time32,
		type_time64, type_filetime, type_systemtime, type_guid
	};

	struct HEXCUSTOMTYPE {
		std::wstring wstrTypeName;
		std::uint8_t uTypeID { };
	};
	using VecCT = std::vector<HEXCUSTOMTYPE>;

	struct HEXTEMPLATEFIELD {
		std::wstring      wstrName { };     //Field name.
		std::wstring      wstrDescr { };    //Field description.
		int               iSize { };        //Field size.
		int               iOffset { };      //Field offset relative to the Template's beginning.
		COLORREF          clrBk { };        //Background color in HexCtrl.
		COLORREF          clrText { };      //Text color in HexCtrl.
		VecFields         vecNested { };    //Vector for nested fields.
		PHEXTEMPLATEFIELD pFieldParent { }; //Parent field, in case of nested.
		PHEXTEMPLATE      pTemplate { };    //Template pointer, this field belongs to.
		EFieldType        eType { };        //Field type.
		std::uint8_t      uTypeID { };      //Type ID for custom types.
		bool              fBigEndian { };   //Field endianness.
	};

	struct HEXTEMPLATE {
		std::wstring wstrName;      //Template name.
		VecFields    vecFields;     //Template fields.
		VecCT        vecCustomType; //Custom types of this template.
		int          iSizeTotal;    //Total size of all Template's fields, assigned internally by framework.
		int          iTemplateID;   //Template ID, assigned by framework.
	};

	struct HEXTEMPLATEAPPLIED {
		ULONGLONG    ullOffset;  //Offset, where to apply a template.
		PHEXTEMPLATE pTemplate;  //Template pointer.
		int          iAppliedID; //Applied/runtime ID, assigned by framework. Any template can be applied more than once.
	};
	using PHEXTEMPLATEAPPLIED = HEXTEMPLATEAPPLIED*;

	class CHexDlgTemplMgr : public CDialogEx, public IHexTemplates
	{
	public:
		void ApplyCurr(ULONGLONG ullOffset); //Apply currently selected template to offset.
		int ApplyTemplate(ULONGLONG ullOffset, int iTemplateID)override; //Apply template to a given offset.
		void DisapplyAll()override;
		void DisapplyByID(int iAppliedID)override; //Disapply template with the given AppliedID.
		void DisapplyByOffset(ULONGLONG ullOffset)override;
		[[nodiscard]] bool HasApplied()const;
		[[nodiscard]] bool HasCurrent()const;
		[[nodiscard]] bool HasTemplates()const;
		[[nodiscard]] auto HitTest(ULONGLONG ullOffset)const->PHEXTEMPLATEFIELD; //Template hittest by offset.
		void Initialize(UINT nIDTemplate, IHexCtrl* pHexCtrl);
		[[nodiscard]] bool IsTooltips()const;
		void RefreshData();
		BOOL ShowWindow(int nCmdShow);
		void UnloadAll()override;
	private:
		void DoDataExchange(CDataExchange* pDX)override;
		BOOL OnInitDialog()override;
		afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);
		BOOL OnCommand(WPARAM wParam, LPARAM lParam)override;
		afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
		afx_msg void OnBnLoadTemplate();
		afx_msg void OnBnUnloadTemplate();
		afx_msg void OnBnRandomizeColors();
		afx_msg void OnBnApply();
		afx_msg void OnCheckHexadecimal();
		afx_msg void OnCheckBigEndian();
		afx_msg void OnCheckTtShow();
		afx_msg void OnCheckHglSel();
		afx_msg void OnListGetDispInfo(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnListGetColor(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnListItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnListEditBegin(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnListDataChanged(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnListHdrRClick(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnListDblClick(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnListEnterPressed(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnListRClick(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnTreeGetDispInfo(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnTreeItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnTreeRClick(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnMouseMove(UINT nFlags, CPoint point);
		afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
		afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
		void OnOK()override;
		void OnTemplateLoadUnload(int iTemplateID, bool fLoad);
		afx_msg void OnDestroy();
		void ShowListDataBool(LPWSTR pwsz, unsigned char uchData)const;
		void ShowListDataChar(LPWSTR pwsz, char chData)const;
		void ShowListDataUChar(LPWSTR pwsz, unsigned char uchData)const;
		void ShowListDataShort(LPWSTR pwsz, short shortData, bool fShouldSwap)const;
		void ShowListDataUShort(LPWSTR pwsz, unsigned short wData, bool fShouldSwap)const;
		void ShowListDataInt(LPWSTR pwsz, int intData, bool fShouldSwap)const;
		void ShowListDataUInt(LPWSTR pwsz, unsigned int dwData, bool fShouldSwap)const;
		void ShowListDataLL(LPWSTR pwsz, long long llData, bool fShouldSwap)const;
		void ShowListDataULL(LPWSTR pwsz, unsigned long long ullData, bool fShouldSwap)const;
		void ShowListDataFloat(LPWSTR pwsz, float flData, bool fShouldSwap)const;
		void ShowListDataDouble(LPWSTR pwsz, double dblData, bool fShouldSwap)const;
		void ShowListDataTime32(LPWSTR pwsz, __time32_t lTime32, bool fShouldSwap)const;
		void ShowListDataTime64(LPWSTR pwsz, __time64_t llTime64, bool fShouldSwap)const;
		void ShowListDataFILETIME(LPWSTR pwsz, FILETIME stFTime, bool fShouldSwap)const;
		void ShowListDataSYSTEMTIME(LPWSTR pwsz, SYSTEMTIME stSTime, bool fShouldSwap)const;
		void ShowListDataGUID(LPWSTR pwsz, GUID stGUID, bool fShouldSwap)const;
		[[nodiscard]] bool SetDataBool(LPCWSTR pwszText, ULONGLONG ullOffset)const;
		[[nodiscard]] bool SetDataChar(LPCWSTR pwszText, ULONGLONG ullOffset)const;
		[[nodiscard]] bool SetDataUChar(LPCWSTR pwszText, ULONGLONG ullOffset)const;
		[[nodiscard]] bool SetDataShort(LPCWSTR pwszText, ULONGLONG ullOffset, bool fShouldSwap)const;
		[[nodiscard]] bool SetDataUShort(LPCWSTR pwszText, ULONGLONG ullOffset, bool fShouldSwap)const;
		[[nodiscard]] bool SetDataInt(LPCWSTR pwszText, ULONGLONG ullOffset, bool fShouldSwap)const;
		[[nodiscard]] bool SetDataUInt(LPCWSTR pwszText, ULONGLONG ullOffset, bool fShouldSwap)const;
		[[nodiscard]] bool SetDataLL(LPCWSTR pwszText, ULONGLONG ullOffset, bool fShouldSwap)const;
		[[nodiscard]] bool SetDataULL(LPCWSTR pwszText, ULONGLONG ullOffset, bool fShouldSwap)const;
		[[nodiscard]] bool SetDataFloat(LPCWSTR pwszText, ULONGLONG ullOffset, bool fShouldSwap)const;
		[[nodiscard]] bool SetDataDouble(LPCWSTR pwszText, ULONGLONG ullOffset, bool fShouldSwap)const;
		[[nodiscard]] bool SetDataTime32(LPCWSTR pwszText, ULONGLONG ullOffset, bool fShouldSwap)const;
		[[nodiscard]] bool SetDataTime64(LPCWSTR pwszText, ULONGLONG ullOffset, bool fShouldSwap)const;
		[[nodiscard]] bool SetDataFILETIME(LPCWSTR pwszText, ULONGLONG ullOffset, bool fShouldSwap)const;
		[[nodiscard]] bool SetDataSYSTEMTIME(LPCWSTR pwszText, ULONGLONG ullOffset, bool fShouldSwap)const;
		[[nodiscard]] bool SetDataGUID(LPCWSTR pwszText, ULONGLONG ullOffset, bool fShouldSwap)const;
		void EnableDynamicLayoutHelper(bool fEnable);
		auto GetAppliedFromItem(HTREEITEM hTreeItem) -> PHEXTEMPLATEAPPLIED;
		[[nodiscard]] bool IsHighlight()const;
		int LoadTemplate(const wchar_t* pFilePath)override; //Returns loaded template ID on success, zero otherwise.
		void RandomizeTemplateColors(int iTemplateID);
		void RemoveNodesWithTemplateID(int iTemplateID);
		void RemoveNodeWithAppliedID(int iAppliedID);
		void SetDlgButtonStates(); //Enable/disable button states depending on templates existence.
		void SetHexSelByField(PCHEXTEMPLATEFIELD pField);
		void ShowTooltips(bool fShow)override;
		[[nodiscard]] auto TreeItemFromListItem(int iListItem)const->HTREEITEM;
		void UnloadTemplate(int iTemplateID)override;       //Unload/remove loaded template from memory.
		void UpdateStaticText();
		static LRESULT CALLBACK TreeSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
			UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
		DECLARE_MESSAGE_MAP();
	private:
		enum class EMenuID : std::uint16_t;
		IHexCtrl* m_pHexCtrl { };
		std::vector<std::unique_ptr<HEXTEMPLATE>> m_vecTemplates;               //Loaded Templates.
		std::vector<std::unique_ptr<HEXTEMPLATEAPPLIED>> m_vecTemplatesApplied; //Currently Applied Templates.
		CComboBox m_stComboTemplates; //Currently available templates list.
		CEdit m_stEditOffset;         //"Offset" edit box.
		CButton m_stCheckTtShow;      //Check-box "Show tooltips"
		CButton m_stCheckHglSel;      //Check-box "Highlight selected"
		CButton m_stCheckHex;         //Check-box "Highlight selected"
		CWnd m_stStaticOffset;        //Static text "Template offset:".
		CWnd m_stStaticSize;          //Static text Template size:".
		UINT m_nIDTemplate { };       //Resource ID of the Dialog, for creation.
		LISTEX::IListExPtr m_pListApplied { LISTEX::CreateListEx() };
		LISTEX::LISTEXCOLOR m_stCellClr { };
		CTreeCtrl m_stTreeApplied;
		CMenu m_stMenuTree;           //Menu for the tree control.
		CMenu m_stMenuHdr;            //Menu for the list header.
		HCURSOR m_hCurResize;
		HCURSOR m_hCurArrow;
		PHEXTEMPLATEAPPLIED m_pAppliedCurr { }; //Currently selected PApplied.
		PVecFields m_pVecCurrFields { };        //Pointer to currently selected vector with fields.
		HTREEITEM m_hTreeCurrParent { };        //Currently selected Tree node's parent.
		DWORD m_dwDateFormat { };               //Date format.
		wchar_t m_wchDateSepar { };             //Date separator.
		bool m_fCurInSplitter { };              //Indicates that mouse cursor is in the splitter area.
		bool m_fLMDownResize { };               //Left mouse pressed in splitter area to resize.
		bool m_fListGuardEvent { false };       //To not proceed with OnListItemChanged, same as pTree->action == TVC_UNKNOWN.
		bool m_fTooltips { true };              //Show tooltips or not.
		bool m_fHighlightSel { true };          //Highlight selected fields with a selection.
		bool m_fShowAsHex { true };
		bool m_fSwapEndian { false };
	};
}