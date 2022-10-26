/****************************************************************************************
* Copyright � 2018-2023 Jovibor https://github.com/jovibor/                             *
* This is a Hex Control for MFC/Win32 applications.                                     *
* Official git repository: https://github.com/jovibor/HexCtrl/                          *
* This software is available under "The HexCtrl License", see the LICENSE file.         *
****************************************************************************************/
#pragma once
#include "../../dep/ListEx/ListEx.h"
#include <afxdialogex.h>

namespace HEXCTRL::INTERNAL
{
	//Forward declarations.
	struct SHEXTEMPLATE;
	using PHEXTEMPLATE = SHEXTEMPLATE*;
	struct STEMPLATEFIELD;
	using VecFields = std::vector<std::unique_ptr<STEMPLATEFIELD>>; //Vector for the Fields.
	using PVecFields = VecFields*;
	using PSTEMPLATEFIELD = STEMPLATEFIELD*;

	struct STEMPLATEFIELD {
		std::wstring    wstrName { }; //Field name.
		int             iSize { };    //Field size.
		int             iOffset { };  //Field offset relative to Template beginning.
		COLORREF        clrBk { };
		COLORREF        clrText { };
		VecFields       vecInner { }; //Vector for nested fields.
		PSTEMPLATEFIELD pFieldParent { }; //Parent field in case of nested.
		PHEXTEMPLATE    pTemplate { };    //Template this field belongs to.
	};

	struct SHEXTEMPLATE {
		std::wstring wstrName;    //Template name.
		VecFields    vecFields;   //Template fields.
		int          iSizeTotal;  //Total size of all Template's fields, assigned internally by framework.
		int          iTemplateID; //Template ID, assigned internally by framework.
	};

	struct STEMPLATEAPPLIED {
		ULONGLONG    ullOffset;  //Offset, where to apply a template.
		PHEXTEMPLATE pTemplate;  //Template pointer in the m_vecTemplates.
		int          iAppliedID; //Applied/runtime ID. Any template can be applied more than once, assingned by framework.
	};
	using PSTEMPLATEAPPLIED = STEMPLATEAPPLIED*;

	class CHexDlgTemplMgr : public CDialogEx, public IHexTemplates
	{
	public:
		BOOL Create(UINT nIDTemplate, CWnd* pParent, IHexCtrl* pHexCtrl);
		void ApplyCurr(ULONGLONG ullOffset); //Apply currently selected template to offset.
		int ApplyTemplate(ULONGLONG ullOffset, int iTemplateID)override; //Apply template to a given offset.
		void ClearAll();
		void DisapplyByOffset(ULONGLONG ullOffset)override;
		[[nodiscard]] bool HasApplied()const;
		[[nodiscard]] bool HasTemplates()const;
		[[nodiscard]] auto HitTest(ULONGLONG ullOffset)const->PSTEMPLATEFIELD; //Template hittest by offset.
		[[nodiscard]] bool IsTooltips()const;
		void RefreshData();
	private:
		void DoDataExchange(CDataExchange* pDX)override;
		BOOL OnInitDialog()override;
		BOOL OnCommand(WPARAM wParam, LPARAM lParam)override;
		afx_msg void OnBnLoadTemplate();
		afx_msg void OnBnUnloadTemplate();
		afx_msg void OnBnApply();
		afx_msg void OnCheckTtShow();
		afx_msg void OnCheckHglSel();
		afx_msg void OnListGetDispInfo(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnListGetColor(NMHDR* pNMHDR, LRESULT* pResult);
		afx_msg void OnListItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
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
		afx_msg void OnDestroy();
		int LoadTemplate(const wchar_t* pFilePath)override; //Returns loaded template ID on success, zero otherwise.
		void UnloadTemplate(int iTemplateID)override;       //Unload/remove loaded template from memory.
		void EnableDynamicLayoutHelper(bool fEnable);
		auto GetAppliedFromItem(HTREEITEM hTreeItem)->PSTEMPLATEAPPLIED;
		void RemoveNodesWithTemplateID(int iTemplateID);
		void RemoveNodeWithAppliedID(int iAppliedID);
		void DisapplyByID(int iAppliedID)override; //Stop one template with the given AppliedID from applying.
		[[nodiscard]] bool IsHighlight()const;
		[[nodiscard]] auto TreeItemFromListItem(int iListItem)const->HTREEITEM;
		void SetHexSelByField(PSTEMPLATEFIELD pField);
		void ShowTooltips(bool fShow)override;
		static LRESULT TreeSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
		DECLARE_MESSAGE_MAP();
	private:
		enum class EMenuID : std::uint16_t { IDM_APPLIED_DISAPPLY = 0x8000, IDM_APPLIED_CLEARALL = 0x8001 };
		IHexCtrl* m_pHexCtrl { };
		std::vector<std::unique_ptr<SHEXTEMPLATE>> m_vecTemplates;            //Loaded Templates.
		std::vector<std::unique_ptr<STEMPLATEAPPLIED>> m_vecTemplatesApplied; //Currently Applied Templates.
		CComboBox m_stComboTemplates; //Currently available templates list.
		CEdit m_stEditOffset;         //"Offset" edit box.
		CButton m_stCheckTtShow;      //Check-box "Show tooltips"
		CButton m_stCheckHglSel;      //Check-box "Highlight selected"
		LISTEX::IListExPtr m_pListApplied { LISTEX::CreateListEx() };
		LISTEX::LISTEXCOLOR m_stCellClr { };
		CTreeCtrl m_stTreeApplied;
		CMenu m_stMenuList;   //Menu for the list control.
		HCURSOR m_hCurResize;
		HCURSOR m_hCurArrow;
		PSTEMPLATEAPPLIED m_pAppliedCurr { }; //Currently selected PApplied.
		PVecFields m_pVecCurrFields { };      //Pointer to currently selected vector with fields.
		HTREEITEM m_hTreeCurrNode { };        //Currently selected Tree node.
		HTREEITEM m_hTreeCurrParent { };      //Currently selected Tree node's parent.
		bool m_fCurInSplitter { };            //Indicates that mouse cursor is in the splitter area.
		bool m_fLMDownResize { };             //Left mouse pressed in splitter area to resize.
		bool m_fListGuardEvent { false };     //To not proceed with OnListItemChanged, same as pTree->action == TVC_UNKNOWN.
		bool m_fTooltips { true };            //Show tooltips or not.
		bool m_fHighlightSel { true };        //Highlight selected fields with a selection.
	};
}