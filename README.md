## **Hex control for MFC applications**
![](docs/img/hexctrl_mainwnd.jpg)
### Table of Contents
* [Introduction](#introduction)
* [Implementation](#implementation)
* [Using the Control](#using-the-control)
* [Create](#create)
  * [Classic Approach](#classic-approach)
  * [In Dialog](#in-dialog)
* [SetData](#setdata)
* [Virtual Mode](#virtual-mode)
  * [Message Window](#message-window)
  * [Virtual Handler](#virtual-handler)
* [OnDestroy](#ondestroy)
* [Scroll Bars](#scroll-bars)
* [Methods](#methods)
* [Example](#example)
* [Positioning and Sizing](#positioning-and-sizing)
* [Appearance](#appearance)
* [Licensing](#licensing)

## [](#)Introduction
Being good low level wrapper library for Windows API in general, **MFC** was always lacking a good native controls support.
This forced people to implement their own common stuff for everyday needs.<br>
This **HexControl** is a tiny attempt to expand standard **MFC** functionality, because at the moment, **MFC** doesn't have native support for such feature.

## [](#)Implementation
This **HexControl** is implemented as a `CWnd` derived class, and can be used as a *child* or *float* window in any place
of your existing **MFC** application. It was build and tested in Visual Studio 2017, under Windows 10.

## [](#)Using the Control
The usage of the control is quite simple:
1. Copy **HexCtrl** folder into your project's folder.
2. Add all files from **HexCtrl** folder into your project.
3. Add `#include "HexCtrl/HexCtrl.h"` where you suppose to use the control.
4. Declare `IHexCtrlPtr` member variable: `IHexCtrlPtr myHex { CreateHexCtrl() };`
5. Call `myHex->Create` method to create control instance.
6. Call `myHex->SetData` method to set the actual data to display as hex.

`IHexCtrlPtr` is, in fact, a pointer to the `IHexCtrl` pure abstract base class, wrapped either in `std::unique_ptr` or `std::shared_ptr`. You can choose whatever is best for you by comment/uncomment one of this alliases in `HexCtrl.h`:
```cpp
	//using IHexCtrlPtr = IHexCtrlUnPtr;
	using IHexCtrlPtr = IHexCtrlShPtr;
```
This wrapper is used mainly for convenience, so you don't have to bother about object lifetime, it will be destroyed automatically.
That's why there is a call to the factory function `CreateHexCtrl()`, to properly initialize a pointer.<br>

**Control** also uses its own namespace - `HEXCTRL`. So it's up to you, whether to use namespace prefix before declarations: 
```cpp
HEXCTRL::
```
or to define namespace in the source file's beginning:
```cpp
using namespace HEXCTRL;
```

## [](#)Create
### [](#)Classic Approach
`IHexCtrl::Create` method - the first method you call - takes `HEXCREATESTRUCT` as its argument. This is the main initialization struct which fields are described below:
```cpp
struct HEXCREATESTRUCT
{
	HEXCOLORSTRUCT  stColor { };			//All the colors of the control.
	CWnd*		    pwndParent { };			//Parent window's pointer.
	UINT		    uId { };				//Hex control Id.
	DWORD			dwStyle { };			//Window styles. Null for default.
	DWORD			dwExStyle { };			//Extended window styles. Null for default.
	CRect			rect { };				//Initial rect. If null, the window is screen centered.
	const LOGFONTW* pLogFont { };			//Font to be used, nullptr for default.
	bool			fFloat { false };		//Is float or child (incorporated into another window)?.
	bool			fCustomCtrl { false };	//It's a custom dialog control.
};
```

You can choose whether control will behave as a *child* or independent *floating* window, by setting `fFloat` member of this struct.
```cpp
HEXCREATESTRUCT hcs;
hcs.fFloat = true;
bool IHexCtrl::Create(hcs);
```
`PHEXCOLORSTRUCT pstColor` member of `HEXCREATESTRUCT` points to the `HEXCOLORSTRUCT` struct which fields are described below:
```cpp
struct HEXCOLORSTRUCT 
{
	COLORREF clrTextHex { GetSysColor(COLOR_WINDOWTEXT) };		//Hex chunks color.
	COLORREF clrTextAscii { GetSysColor(COLOR_WINDOWTEXT) };	//Ascii text color.
	COLORREF clrTextSelected { GetSysColor(COLOR_WINDOWTEXT) }; //Selected text color.
	COLORREF clrTextCaption { RGB(0, 0, 180) };					//Caption color
	COLORREF clrTextInfoRect { GetSysColor(COLOR_WINDOWTEXT) };	//Text color of the bottom "Info" rect.
	COLORREF clrTextCursor { RGB(255, 255, 255) };				//Cursor text color.
	COLORREF clrBk { GetSysColor(COLOR_WINDOW) };				//Background color.
	COLORREF clrBkSelected { RGB(200, 200, 255) };				//Background color of the selected Hex/Ascii.
	COLORREF clrBkInfoRect { RGB(250, 250, 250) };				//Background color of the bottom "Info" rect.
	COLORREF clrBkCursor { RGB(0, 0, 250) };					//Cursor background color.
};
```
This struct is also used in `IHexCtrl::SetColor` method.

### [](#)In Dialog
To use **HexCtrl** within `Dialog` you can, of course, create it with the [Classic Approach](#classic-approach): 
call `IHexCtrl::Create` method and provide all the necessary information.<br>
But there is another option you can use:
1. Put **Custom Control** control from the **Toolbox** in **Visual Studio** dialog designer into your dialog template and make it desired size.<br>
![](docs/img/hexctrl_vstoolbox.jpg) ![](docs/img/hexctrl_vscustomctrl.jpg)
2. Then go to the **Properties** of that control, and in the **Class** field, within the **Misc** section, write *HexCtrl*. Give the control appropriate 
**ID** of your choise (`IDC_MY_HEX` in this example). Also, here you can set the control's **Dynamic Layout** properties, so that control behaves appropriately when dialog is being resized.
![](docs/img/hexctrl_vsproperties.jpg)
3. Declare `IHexCtrlPtr` member varable within your dialog class: `IHexCtrlPtr m_myHex { CreateHexCtrl() };`
4. Add the folowing code to the `DoDataExchange` method of your dialog class:<br>
```cpp
DDX_Control(pDX, IDC_MY_HEX, *m_myHex);
```
So, that it looks like in the example below:
```cpp
void CMyDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_MY_HEX, *m_myHex);
}
```
5. Call `IHexCtrl::CreateDialogCtrl` function, `m_myHex->CreateDialogCtrl()`.

## [](#)SetData
`SetData` method takes `HEXDATASTRUCT` reference as argument. This struct fields are described below:
```cpp
struct HEXDATASTRUCT
{
	ULONGLONG		ullDataSize { };					//Size of the data to display, in bytes.
	ULONGLONG		ullSelectionStart { };				//Set selection at this position. Works only if ullSelectionSize > 0.
	ULONGLONG		ullSelectionSize { };				//How many bytes to set as selected.
	CWnd*			pwndMsg { };						//Window to send the control messages to. If nullptr then the parent window is used.
	IHexVirtual*	pHexHandler { };					//Pointer to Virtual data handler class.
	PBYTE			pData { };							//Pointer to the data. Not used if it's virtual control.
	HEXDATAMODEEN	enMode { HEXDATAMODEEN::HEXNORMAL };//Working data mode of control.
	bool			fMutable { false };					//Will data be mutable (editable) or just read mode.
};
```

## [](#)Virtual Mode
### [](#)Message window
The `enMode` member of `HEXDATASTRUCT` shows what data mode `HexControl` is working at. It is the `HEXDATAMODEEN` enum member.
If it's set to `HEXMSG` the control works in so called *Virtual* mode.<br>
What it means is that when control is about to display next byte, it will first ask for this byte from the `pwndMsg` window,
in the form of `WM_NOTIFY` message. This is pretty much the same as the standard **MFC** List Control works when created with `LVS_OWNERDATA` flag.<br>
The `pwndMsg` window pointer can be set as `HEXDATASTRUCT::pwndMsg` in `SetData` method. By default it is equal to the control's parent window.<br>
This mode can be quite useful, for instance, in cases where you need to display a very large amount of data that can't fit in memory all at once.<br>
To properly handle this mode process `WM_NOTIFY` messages, in `pwndMsg` window, as follows:
```cpp
BOOL CMyWnd::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
    PHEXNOTIFYSTRUCT pHexNtfy = (PHEXNOTIFYSTRUCT)lParam;
    if (pHexNtfy->hdr.idFrom == IDC_MY_HEX)
    {
        switch (pHexNtfy->hdr.code)
        {
        case HEXCTRL_MSG_GETDATA:
			pHexNtfy->chByte = /*code to set the byte, if pHexNtfy->ullSize=1*/;
            pHexNtfy->pData =  /*or code to set the pointer to a data*/;
            break;
        }
   }
}
```
`lParam` will hold a pointer to a `HEXNOTIFYSTRUCT` structure, which is described below:
```cpp
struct HEXNOTIFYSTRUCT
{
	NMHDR			hdr;			//Standard Windows header. For hdr.code values see HEXCTRL_MSG_* messages.
	ULONGLONG		ullByteIndex;	//Index of the start byte to get/send.
	ULONGLONG		ullSize;		//Size of the bytes to send.
	PBYTE			pData { };		//Pointer to a data to get/send.
	BYTE			chByte { };		//Single byte data - used for simplicity, when ullSize==1.
};
using PHEXNOTIFYSTRUCT = HEXNOTIFYSTRUCT * ;
```
Its first member is a standard Windows' `NMHDR` structure. It will have its code member equal to `HEXCTRL_MSG_GETDATA` indicating that **HexControl**'s byte request has arrived.
The second member is the index of the byte be displayed. And the third is the actual byte, that you have to set in response.

### [](#)Virtual Handler
If `enMode` member of `HEXDATASTRUCT` is set to `HEXDATAMODEEN::HEXVIRTUAL` then all the data routine will be done through
`HEXDATASTRUCT::pHexVirtual` pointer.<br>
This pointer is of type `IHexVirtual` class, which is a pure abstract base class.
You have to derive your own class from it and implement all its public methods:
```cpp
class IHexVirtual
{
public:
	virtual BYTE GetByte(ULONGLONG ullIndex) = 0; //Gets the byte data by index.
	virtual	void ModifyData(const HEXMODIFYSTRUCT& hmd) = 0; //Main routine to modify data, in fMutable=true mode.
};
```
Then provide a pointer to created object of this derived class to `SetData` method in form of `HEXDATASTRUCT::pHexVirtual=&yourDerivedObject`

## [](#)OnDestroy
When **HexControl** window, floating or child, is destroyed, it sends `WM_NOTIFY` message to its parent window with `NMHDR::code` equal to `HEXCTRL_MSG_DESTROY`. 
So, it basically indicates to its parent that the user clicked close button, or closed window in some other way.

## [](#)Scroll Bars
When I started to work with very big files, I immediately faced one very nasty inconvenience:
The standard **Windows** scrollbars can hold only signed integer value, which is too little to scroll through many gigabytes of data. 
It could be some workarounds and crutches involved to overcome this, but frankly saying i'm not a big fan of this kind of approach.

That's why **HexControl** uses its own scrollbars. They work with `unsigned long long` values, which is way bigger than standard signed ints. 
These scrollbars behave as normal **Windows** scrollbars, and even reside in the non client area, as the latter do.

## [](#)Methods
**HexControl** has plenty of methods that you can use to customize its appearance, and to manage its behaviour.<br>
These methods' usage is pretty straightforward, and clean, from their naming:
```cpp
bool Create(const HEXCREATESTRUCT& hcs); //Main initialization method.
bool CreateDialogCtrl();				 //Сreates custom dialog control.
bool IsCreated();						 //Shows whether control is created or not.
void SetData(const HEXDATASTRUCT& hds);  //Main method for setting data to display (and edit).	
bool IsDataSet();						 //Shows whether a data was set to control or not.
void ClearData();						 //Clears all data from HexCtrl's view (not touching data itself).
void EditEnable(bool fEnable);			 //Enable or disable edit mode.
void ShowOffset(ULONGLONG ullOffset, ULONGLONG ullSize = 1); //Shows (selects) given offset.
void SetFont(const LOGFONT* pLogFontNew);//Sets the control's font.
void SetFontSize(UINT uiSize);			 //Sets the control's font size.
long GetFontSize();						 //Gets the control's font size.
void SetColor(const HEXCOLORSTRUCT& clr);//Sets all the colors for the control.
void SetCapacity(DWORD dwCapacity);		 //Sets the control's current capacity.
void Search(HEXSEARCHSTRUCT& rSearch);	 //Search through currently set data.
```

## [](#)Example
The function you use to set a data to display as hex is `IHexCtrl::SetData`. 
The code below constructs `IHexCtrlPtr` object and displays first `0x1FF` bytes of your app's memory:
```cpp
IHexCtrlPtr myHex { CreateHexCtrl() };
HEXCREATESTRUCT hcs;
hcs.pwndParent = this;
hcs.rect = CRect(0, 0, 500, 300); //Control's rect.
myHex->Create(hcs);

HEXDATASTRUCT hds;
hds.pData = (unsigned char*)GetModuleHandle(0);
hds.ullDataSize = 0x1FF;
myHex->SetData(hds);
```
The next example displays `std::string`'s text string as hex:
```cpp
std::string str = "My string";
HEXDATASTRUCT hds;
hds.pData = (unsigned char*)str.data();
hds.ullDataSize = str.size();
myHex->SetData(hds);
```

## [](#)Positioning and Sizing
To properly resize and position your **HexControl**'s window you may have to handle `WM_SIZE` message in its parent window, in something like this way:
```cpp
void CMyWnd::OnSize(UINT nType, int cx, int cy)
{
    .
    .
    myHex->SetWindowPos(this, 0, 0, cx, cy, SWP_NOACTIVATE | SWP_NOZORDER);
}
```

## [](#)Appearance
To change control's font size - **«Ctrl+MouseWheel»**  
To change control's capacity - **«Ctrl+Shift+MouseWheel»**

## [](#)Licensing
This software is available under the **"MIT License modified with The Commons Clause".**
[https://github.com/jovibor/HexCtrl/blob/master/LICENSE](https://github.com/jovibor/HexCtrl/blob/master/LICENSE)