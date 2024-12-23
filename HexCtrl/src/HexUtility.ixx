module;
/****************************************************************************************
* Copyright © 2018-present Jovibor https://github.com/jovibor/                          *
* Hex Control for Windows applications.                                                 *
* Official git repository: https://github.com/jovibor/HexCtrl/                          *
* This software is available under "The HexCtrl License", see the LICENSE file.         *
****************************************************************************************/
#include <SDKDDKVer.h>
#include "../HexCtrl.h"
#include <afxwin.h>
#include <algorithm>
#include <bit>
#include <cassert>
#include <cwctype>
#include <format>
#include <intrin.h>
#include <locale>
#include <optional>
#include <source_location>
#include <string>
#include <unordered_map>
export module HEXCTRL.HexUtility;

export import StrToNum;
export import ListEx;

export namespace HEXCTRL::INTERNAL {
	//Time calculation constants and structs.
	constexpr auto g_uFTTicksPerMS = 10000U;            //Number of 100ns intervals in a milli-second.
	constexpr auto g_uFTTicksPerSec = 10000000U;        //Number of 100ns intervals in a second.
	constexpr auto g_uHoursPerDay = 24U;                //24 hours per day.
	constexpr auto g_uSecondsPerHour = 3600U;           //3600 seconds per hour.
	constexpr auto g_uFileTime1582OffsetDays = 6653U;   //FILETIME is based upon 1 Jan 1601 whilst GUID time is from 15 Oct 1582. Add 6653 days to convert to GUID time.
	constexpr auto g_ulFileTime1970_LOW = 0xd53e8000U;  //1st Jan 1970 as FILETIME.
	constexpr auto g_ulFileTime1970_HIGH = 0x019db1deU; //Used for Unix and Java times.
	constexpr auto g_ullUnixEpochDiff = 11644473600ULL; //Number of ticks from FILETIME epoch of 1st Jan 1601 to Unix epoch of 1st Jan 1970.

	//Get data from IHexCtrl's given offset converted to a necessary type.
	template<typename T>
	[[nodiscard]] T GetIHexTData(const IHexCtrl& refHexCtrl, ULONGLONG ullOffset)
	{
		const auto spnData = refHexCtrl.GetData({ .ullOffset { ullOffset }, .ullSize { sizeof(T) } });
		assert(!spnData.empty());
		return *reinterpret_cast<T*>(spnData.data());
	}

	//Set data of a necessary type to IHexCtrl's given offset.
	template<typename T>
	void SetIHexTData(IHexCtrl& refHexCtrl, ULONGLONG ullOffset, T tData)
	{
		//Data overflow check.
		assert(ullOffset + sizeof(T) <= refHexCtrl.GetDataSize());
		if (ullOffset + sizeof(T) > refHexCtrl.GetDataSize())
			return;

		refHexCtrl.ModifyData({ .eModifyMode { EHexModifyMode::MODIFY_ONCE },
			.spnData { reinterpret_cast<std::byte*>(&tData), sizeof(T) },
			.vecSpan { { ullOffset, sizeof(T) } } });
	}

	template<typename T> concept TSize1248 = (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8);

	//Bytes swap for types of 2, 4, or 8 byte size.
	template<TSize1248 T> [[nodiscard]] constexpr T ByteSwap(T tData)noexcept
	{
		//Since a swapping-data type can be any type of 2, 4, or 8 bytes size,
		//we first bit_cast swapping-data to an integral type of the same size,
		//then byte-swapping and then bit_cast to the original type back.
		if constexpr (sizeof(T) == sizeof(std::uint8_t)) { //1 byte.
			return tData;
		}
		else if constexpr (sizeof(T) == sizeof(std::uint16_t)) { //2 bytes.
			auto wData = std::bit_cast<std::uint16_t>(tData);
			if (std::is_constant_evaluated()) {
				wData = static_cast<std::uint16_t>((wData << 8) | (wData >> 8));
				return std::bit_cast<T>(wData);
			}
			return std::bit_cast<T>(_byteswap_ushort(wData));
		}
		else if constexpr (sizeof(T) == sizeof(std::uint32_t)) { //4 bytes.
			auto ulData = std::bit_cast<std::uint32_t>(tData);
			if (std::is_constant_evaluated()) {
				ulData = (ulData << 24) | ((ulData << 8) & 0x00FF'0000U)
					| ((ulData >> 8) & 0x0000'FF00U) | (ulData >> 24);
				return std::bit_cast<T>(ulData);
			}
			return std::bit_cast<T>(_byteswap_ulong(ulData));
		}
		else if constexpr (sizeof(T) == sizeof(std::uint64_t)) { //8 bytes.
			auto ullData = std::bit_cast<std::uint64_t>(tData);
			if (std::is_constant_evaluated()) {
				ullData = (ullData << 56) | ((ullData << 40) & 0x00FF'0000'0000'0000ULL)
					| ((ullData << 24) & 0x0000'FF00'0000'0000ULL) | ((ullData << 8) & 0x0000'00FF'0000'0000ULL)
					| ((ullData >> 8) & 0x0000'0000'FF00'0000ULL) | ((ullData >> 24) & 0x0000'0000'00FF'0000ULL)
					| ((ullData >> 40) & 0x0000'0000'0000'FF00ULL) | (ullData >> 56);
				return std::bit_cast<T>(ullData);
			}
			return std::bit_cast<T>(_byteswap_uint64(ullData));
		}
	}

#if defined(_M_IX86) || defined(_M_X64)
	template<typename T> concept TVec128 = (std::is_same_v<T, __m128> || std::is_same_v<T, __m128i> || std::is_same_v<T, __m128d>);
	template<typename T> concept TVec256 = (std::is_same_v<T, __m256> || std::is_same_v<T, __m256i> || std::is_same_v<T, __m256d>);

	template<TSize1248 TIntegral, TVec128 TVec>	//Bytes swap inside vector types: __m128, __m128i, __m128d.
	[[nodiscard]] auto ByteSwapVec(const TVec m128T) -> TVec
	{
		if constexpr (std::is_same_v<TVec, __m128i>) { //Integrals.
			if constexpr (sizeof(TIntegral) == sizeof(std::uint8_t)) { //1 bytes.
				return m128T;
			}
			else if constexpr (sizeof(TIntegral) == sizeof(std::uint16_t)) { //2 bytes.
				const auto m128iMask = _mm_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
				return _mm_shuffle_epi8(m128T, m128iMask);
			}
			else if constexpr (sizeof(TIntegral) == sizeof(std::uint32_t)) { //4 bytes.
				const auto m128iMask = _mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);
				return _mm_shuffle_epi8(m128T, m128iMask);
			}
			else if constexpr (sizeof(TIntegral) == sizeof(std::uint64_t)) { //8 bytes.
				const auto m128iMask = _mm_setr_epi8(7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8);
				return _mm_shuffle_epi8(m128T, m128iMask);
			}
		}
		else if constexpr (std::is_same_v<TVec, __m128>) { //Floats.
			alignas(16) float flData[4];
			_mm_store_ps(flData, m128T); //Loading m128T into local float array.
			const auto m128iData = _mm_load_si128(reinterpret_cast<__m128i*>(flData)); //Loading array as __m128i (convert).
			const auto m128iMask = _mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);
			const auto m128iSwapped = _mm_shuffle_epi8(m128iData, m128iMask); //Swapping bytes.
			_mm_store_si128(reinterpret_cast<__m128i*>(flData), m128iSwapped); //Loading m128iSwapped back into local array.
			return _mm_load_ps(flData); //Returning local array as __m128.
		}
		else if constexpr (std::is_same_v<TVec, __m128d>) { //Doubles.
			alignas(16) double dbllData[2];
			_mm_store_pd(dbllData, m128T); //Loading m128T into local double array.
			const auto m128iData = _mm_load_si128(reinterpret_cast<__m128i*>(dbllData)); //Loading array as __m128i (convert).
			const auto m128iMask = _mm_setr_epi8(7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8);
			const auto m128iSwapped = _mm_shuffle_epi8(m128iData, m128iMask); //Swapping bytes.
			_mm_store_si128(reinterpret_cast<__m128i*>(dbllData), m128iSwapped); //Loading m128iSwapped back into local array.
			return _mm_load_pd(dbllData); //Returning local array as __m128d.
		}
	}

	template<TSize1248 TIntegral, TVec256 TVec>	//Bytes swap inside vector types: __m256, __m256i, __m256d.
	[[nodiscard]] auto ByteSwapVec(const TVec m256T) -> TVec
	{
		if constexpr (std::is_same_v<TVec, __m256i>) { //Integrals.
			if constexpr (sizeof(TIntegral) == sizeof(std::uint8_t)) { //1 bytes.
				return m256T;
			}
			else if constexpr (sizeof(TIntegral) == sizeof(std::uint16_t)) { //2 bytes.
				const auto m256iMask = _mm256_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14, 17, 16,
					19, 18, 21, 20, 23, 22, 25, 24, 27, 26, 29, 28, 31, 30);
				return _mm256_shuffle_epi8(m256T, m256iMask);
			}
			else if constexpr (sizeof(TIntegral) == sizeof(std::uint32_t)) { //4 bytes.
				const auto m256iMask = _mm256_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12,
					19, 18, 17, 16, 23, 22, 21, 20, 27, 26, 25, 24, 31, 30, 29, 28);
				return _mm256_shuffle_epi8(m256T, m256iMask);
			}
			else if constexpr (sizeof(TIntegral) == sizeof(std::uint64_t)) { //8 bytes.
				const auto m256iMask = _mm256_setr_epi8(7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8,
					23, 22, 21, 20, 19, 18, 17, 16, 31, 30, 29, 28, 27, 26, 25, 24);
				return _mm256_shuffle_epi8(m256T, m256iMask);
			}
		}
		else if constexpr (std::is_same_v<TVec, __m256>) { //Floats.
			alignas(32) float flData[8];
			_mm256_store_ps(flData, m256T); //Loading m256T into local float array.
			const auto m256iData = _mm256_load_si256(reinterpret_cast<__m256i*>(flData)); //Loading array as __m256i (convert).
			const auto m256iMask = _mm256_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12,
					19, 18, 17, 16, 23, 22, 21, 20, 27, 26, 25, 24, 31, 30, 29, 28);
			const auto m256iSwapped = _mm256_shuffle_epi8(m256iData, m256iMask); //Swapping bytes.
			_mm256_store_si256(reinterpret_cast<__m256i*>(flData), m256iSwapped); //Loading m256iSwapped back into local array.
			return _mm256_load_ps(flData); //Returning local array as __m256.
		}
		else if constexpr (std::is_same_v<TVec, __m256d>) { //Doubles.
			alignas(32) double dbllData[4];
			_mm256_store_pd(dbllData, m256T); //Loading m256T into local double array.
			const auto m256iData = _mm256_load_si256(reinterpret_cast<__m256i*>(dbllData)); //Loading array as __m256i (convert).
			const auto m256iMask = _mm256_setr_epi8(7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8,
					23, 22, 21, 20, 19, 18, 17, 16, 31, 30, 29, 28, 27, 26, 25, 24);
			const auto m256iSwapped = _mm256_shuffle_epi8(m256iData, m256iMask); //Swapping bytes.
			_mm256_store_si256(reinterpret_cast<__m256i*>(dbllData), m256iSwapped); //Loading m256iSwapped back into local array.
			return _mm256_load_pd(dbllData); //Returning local array as __m256d.
		}
	}

	[[nodiscard]] bool HasAVX2() {
		const static bool fHasAVX2 = []() {
			int arrInfo[4] { };
			__cpuid(arrInfo, 0);
			if (arrInfo[0] >= 7) {
				__cpuid(arrInfo, 7);
				return (arrInfo[1] & (1 << 5)) != 0;
			}
			return false;
			}();
		return fHasAVX2;
	}
#endif // ^^^ !defined(_M_IX86) && !defined(_M_X64)

	template<TSize1248 T> [[nodiscard]] constexpr T BitReverse(T tData) {
		T tReversed { };
		constexpr auto iBitsCount = sizeof(T) * 8;
		for (auto i = 0; i < iBitsCount; ++i, tData >>= 1) {
			tReversed = (tReversed << 1) | (tData & 1);
		}
		return tReversed;
	}

	//Converts every two numeric wchars to one respective hex character: "56"->V(0x56), "7A"->z(0x7A), etc...
	//chWc - a wildcard if any.
	[[nodiscard]] auto NumStrToHex(std::wstring_view wsv, char chWc = 0) -> std::optional<std::string>
	{
		const auto fWc = chWc != 0; //Is wildcard used?
		std::wstring wstrFilter = L"0123456789AaBbCcDdEeFf"; //Allowed characters.

		if (fWc) {
			wstrFilter += chWc;
		}

		if (wsv.find_first_not_of(wstrFilter) != std::wstring_view::npos)
			return std::nullopt;

		std::string strHexTmp;
		for (auto iterBegin = wsv.begin(); iterBegin != wsv.end();) {
			if (fWc && static_cast<char>(*iterBegin) == chWc) { //Skip wildcard.
				++iterBegin;
				strHexTmp += chWc;
				continue;
			}

			//Extract two current wchars and pass it to StringToNum as wstring.
			const std::size_t nOffsetCurr = iterBegin - wsv.begin();
			const auto nSize = nOffsetCurr + 2 <= wsv.size() ? 2 : 1;
			if (const auto optNumber = stn::StrToUInt8(wsv.substr(nOffsetCurr, nSize), 16); optNumber) {
				iterBegin += nSize;
				strHexTmp += *optNumber;
			}
			else
				return std::nullopt;
		}

		return { std::move(strHexTmp) };
	}

	[[nodiscard]] auto WstrToStr(std::wstring_view wsv, UINT uCodePage = CP_UTF8) -> std::string
	{
		const auto iSize = WideCharToMultiByte(uCodePage, 0, wsv.data(), static_cast<int>(wsv.size()), nullptr, 0, nullptr, nullptr);
		std::string str(iSize, 0);
		WideCharToMultiByte(uCodePage, 0, wsv.data(), static_cast<int>(wsv.size()), str.data(), iSize, nullptr, nullptr);
		return str;
	}

	[[nodiscard]] auto StrToWstr(std::string_view sv, UINT uCodePage = CP_UTF8) -> std::wstring
	{
		const auto iSize = MultiByteToWideChar(uCodePage, 0, sv.data(), static_cast<int>(sv.size()), nullptr, 0);
		std::wstring wstr(iSize, 0);
		MultiByteToWideChar(uCodePage, 0, sv.data(), static_cast<int>(sv.size()), wstr.data(), iSize);
		return wstr;
	}

	[[nodiscard]] auto StringToSystemTime(std::wstring_view wsv, DWORD dwFormat) -> std::optional<SYSTEMTIME>
	{
		//dwFormat is a locale specific date format https://docs.microsoft.com/en-gb/windows/win32/intl/locale-idate

		if (wsv.empty())
			return std::nullopt;

		//Normalise the input string by replacing non-numeric characters except space with a `/`.
		//This should regardless of the current date/time separator character.
		std::wstring wstrDateTimeCooked;
		for (const auto wch : wsv) {
			wstrDateTimeCooked += (std::iswdigit(wch) || wch == L' ') ? wch : L'/';
		}

		SYSTEMTIME stSysTime { };
		int iParsedArgs { };

		//Parse date component.
		switch (dwFormat) {
		case 0:	//Month-Day-Year.
			iParsedArgs = swscanf_s(wstrDateTimeCooked.data(), L"%2hu/%2hu/%4hu", &stSysTime.wMonth, &stSysTime.wDay, &stSysTime.wYear);
			break;
		case 1: //Day-Month-Year.
			iParsedArgs = swscanf_s(wstrDateTimeCooked.data(), L"%2hu/%2hu/%4hu", &stSysTime.wDay, &stSysTime.wMonth, &stSysTime.wYear);
			break;
		case 2:	//Year-Month-Day.
			iParsedArgs = swscanf_s(wstrDateTimeCooked.data(), L"%4hu/%2hu/%2hu", &stSysTime.wYear, &stSysTime.wMonth, &stSysTime.wDay);
			break;
		default:
			assert(true);
			return std::nullopt;
		}
		if (iParsedArgs != 3)
			return std::nullopt;

		//Find time seperator, if present.
		if (const auto nPos = wstrDateTimeCooked.find(L' '); nPos != std::wstring::npos) {
			wstrDateTimeCooked = wstrDateTimeCooked.substr(nPos + 1);

			//Parse time component HH:MM:SS.mmm.
			if (swscanf_s(wstrDateTimeCooked.data(), L"%2hu/%2hu/%2hu/%3hu", &stSysTime.wHour, &stSysTime.wMinute,
				&stSysTime.wSecond, &stSysTime.wMilliseconds) != 4)
				return std::nullopt;
		}

		return stSysTime;
	}

	[[nodiscard]] auto StringToFileTime(std::wstring_view wsv, DWORD dwFormat) -> std::optional<FILETIME>
	{
		std::optional<FILETIME> optFT { std::nullopt };
		if (auto optSysTime = StringToSystemTime(wsv, dwFormat); optSysTime) {
			if (FILETIME ftTime; SystemTimeToFileTime(&*optSysTime, &ftTime) != FALSE) {
				optFT = ftTime;
			}
		}
		return optFT;
	}

	[[nodiscard]] auto SystemTimeToString(SYSTEMTIME stSysTime, DWORD dwFormat, wchar_t wchSepar) -> std::wstring
	{
		if (dwFormat > 2 || stSysTime.wDay == 0 || stSysTime.wDay > 31 || stSysTime.wMonth == 0 || stSysTime.wMonth > 12
			|| stSysTime.wYear > 9999 || stSysTime.wHour > 23 || stSysTime.wMinute > 59 || stSysTime.wSecond > 59
			|| stSysTime.wMilliseconds > 999)
			return L"N/A";

		std::wstring_view wsvFmt;
		switch (dwFormat) {
		case 0:	//0:Month/Day/Year HH:MM:SS.mmm.
			wsvFmt = L"{1:02d}{7}{0:02d}{7}{2} {3:02d}:{4:02d}:{5:02d}.{6:03d}";
			break;
		case 1: //1:Day/Month/Year HH:MM:SS.mmm.
			wsvFmt = L"{0:02d}{7}{1:02d}{7}{2} {3:02d}:{4:02d}:{5:02d}.{6:03d}";
			break;
		case 2: //2:Year/Month/Day HH:MM:SS.mmm.
			wsvFmt = L"{2}{7}{1:02d}{7}{0:02d} {3:02d}:{4:02d}:{5:02d}.{6:03d}";
			break;
		default:
			break;
		}

		return std::vformat(wsvFmt, std::make_wformat_args(stSysTime.wDay, stSysTime.wMonth, stSysTime.wYear,
			stSysTime.wHour, stSysTime.wMinute, stSysTime.wSecond, stSysTime.wMilliseconds, wchSepar));
	}

	[[nodiscard]] auto FileTimeToString(FILETIME stFileTime, DWORD dwFormat, wchar_t wchSepar) -> std::wstring
	{
		if (SYSTEMTIME stSysTime; FileTimeToSystemTime(&stFileTime, &stSysTime) != FALSE) {
			return SystemTimeToString(stSysTime, dwFormat, wchSepar);
		}

		return L"N/A";
	}

	//String of a date/time in the given format with the given date separator.
	[[nodiscard]] auto GetDateFormatString(DWORD dwFormat, wchar_t wchSepar) -> std::wstring
	{
		std::wstring_view wsvFmt;
		switch (dwFormat) {
		case 0:	//Month-Day-Year.
			wsvFmt = L"MM{0}DD{0}YYYY HH:MM:SS.mmm";
			break;
		case 1: //Day-Month-Year.
			wsvFmt = L"DD{0}MM{0}YYYY HH:MM:SS.mmm";
			break;
		case 2:	//Year-Month-Day.
			wsvFmt = L"YYYY{0}MM{0}DD HH:MM:SS.mmm";
			break;
		default:
			assert(true);
			break;
		}

		return std::vformat(wsvFmt, std::make_wformat_args(wchSepar));
	}

	template<typename T>
	[[nodiscard]] auto RangeToVecBytes(const T& refData) -> std::vector<std::byte>
	{
		const std::byte* pBegin;
		const std::byte* pEnd;
		if constexpr (std::is_same_v<T, std::string>) {
			pBegin = reinterpret_cast<const std::byte*>(refData.data());
			pEnd = pBegin + refData.size();
		}
		else if constexpr (std::is_same_v<T, std::wstring>) {
			pBegin = reinterpret_cast<const std::byte*>(refData.data());
			pEnd = pBegin + refData.size() * sizeof(wchar_t);
		}
		else if constexpr (std::is_same_v<T, CStringW>) {
			pBegin = reinterpret_cast<const std::byte*>(refData.GetString());
			pEnd = pBegin + refData.GetLength() * sizeof(wchar_t);
		}
		else {
			pBegin = reinterpret_cast<const std::byte*>(&refData);
			pEnd = pBegin + sizeof(refData);
		}

		return { pBegin, pEnd };
	}

	[[nodiscard]] auto GetLocale() -> std::locale {
		static const auto loc { std::locale("en_US.UTF-8") };
		return loc;
	}

	namespace wnd { //Windows GUI related stuff.
		[[nodiscard]] auto GetHinstance() -> HINSTANCE {
			return AfxGetInstanceHandle();
		}

		auto DefMsgProc(const MSG& stMsg) -> LRESULT {
			return ::DefWindowProcW(stMsg.hwnd, stMsg.message, stMsg.wParam, stMsg.lParam);
		}

		template<typename T>
		auto CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)->LRESULT
		{
			//Different IHexCtrl objects will share the same WndProc<ExactTypeHere> function.
			//Hence, the map is needed to differentiate these objects. 
			//The DlgWndProc<T> works absolutely the same way.
			static std::unordered_map<HWND, T*> uMap;

			//CREATESTRUCTW::lpCreateParams always possesses `this` pointer, passed to the CreateWindowExW
			//function as lpParam. We save it to the static uMap to have access to this->ProcessMsg() method.
			if (uMsg == WM_CREATE) {
				const auto lpCS = reinterpret_cast<LPCREATESTRUCTW>(lParam);
				uMap[hWnd] = reinterpret_cast<T*>(lpCS->lpCreateParams);
				return 0;
			}

			if (const auto it = uMap.find(hWnd); it != uMap.end()) {
				const auto ret = it->second->ProcessMsg({ .hwnd { hWnd }, .message { uMsg },
					.wParam { wParam }, .lParam { lParam } });
				if (uMsg == WM_NCDESTROY) { //Remove hWnd from the map on window destruction.
					uMap.erase(it);
				}
				return ret;
			}

			return ::DefWindowProcW(hWnd, uMsg, wParam, lParam);
		}

		template<typename T>
		auto CALLBACK DlgWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)->INT_PTR {
			//DlgWndProc should return zero for all non-processed messages.
			//In that case messages will be processed by Windows default dialog proc.
			//Non-processed messages should not be passed neither to DefWindowProcW nor to DefDlgProcW.
			//Processed messages should return any non-zero value, depending on message type.

			static std::unordered_map<HWND, T*> uMap;

			//DialogBoxParamW and CreateDialogParamW dwInitParam arg is sent with WM_INITDIALOG as lParam.
			if (uMsg == WM_INITDIALOG) {
				uMap[hWnd] = reinterpret_cast<T*>(lParam);
			}

			if (const auto it = uMap.find(hWnd); it != uMap.end()) {
				const auto ret = it->second->ProcessMsg({ .hwnd { hWnd }, .message { uMsg },
					.wParam { wParam }, .lParam { lParam } });
				if (uMsg == WM_NCDESTROY) { //Remove hWnd from the map on dialog destruction.
					uMap.erase(it);
				}
				return ret;
			}

			return 0;
		}

		void FillSolidRect(HDC hDC, LPCRECT pRC, COLORREF clr) {
			//Replicates CDC::FillSolidRect.
			::SetBkColor(hDC, clr);
			::ExtTextOutW(hDC, 0, 0, ETO_OPAQUE, pRC, nullptr, 0, nullptr);
		}

		class CDynLayout {
		public:
			//Ratio settings, for how much to move or resize an item when parent is resized.
			struct ItemRatio { float m_flXRatio { }; float m_flYRatio { }; };
			struct MoveRatio : public ItemRatio { }; //To differentiate move from size in the AddItem.
			struct SizeRatio : public ItemRatio { };

			void AddItem(int iIDItem, MoveRatio move, SizeRatio size);
			void Enable(bool fTrack);
			void OnSize(int iWidth, int iHeight)const; //Should be hooked into the host window WM_SIZE handler.
			void RemoveAll() { m_vecItems.clear(); }
			void SetHost(HWND hWnd) { assert(hWnd != nullptr); m_hWndHost = hWnd; }

						//Static helper methods to use in the AddItem.
			[[nodiscard]] static MoveRatio MoveNone() { return { }; }
			[[nodiscard]] static MoveRatio MoveHorz(int iXRatio) {
				iXRatio = std::clamp(iXRatio, 0, 100); return { { .m_flXRatio { iXRatio / 100.F } } };
			}
			[[nodiscard]] static MoveRatio MoveVert(int iYRatio) {
				iYRatio = std::clamp(iYRatio, 0, 100); return { { .m_flYRatio { iYRatio / 100.F } } };
			}
			[[nodiscard]] static MoveRatio MoveHorzAndVert(int iXRatio, int iYRatio) {
				iXRatio = std::clamp(iXRatio, 0, 100); iYRatio = std::clamp(iYRatio, 0, 100);
				return { { .m_flXRatio { iXRatio / 100.F }, .m_flYRatio { iYRatio / 100.F } } };
			}
			[[nodiscard]] static SizeRatio SizeNone() { return { }; }
			[[nodiscard]] static SizeRatio SizeHorz(int iXRatio) {
				iXRatio = std::clamp(iXRatio, 0, 100); return { { .m_flXRatio { iXRatio / 100.F } } };
			}
			[[nodiscard]] static SizeRatio SizeVert(int iYRatio) {
				iYRatio = std::clamp(iYRatio, 0, 100); return { { .m_flYRatio { iYRatio / 100.F } } };
			}
			[[nodiscard]] static SizeRatio SizeHorzAndVert(int iXRatio, int iYRatio) {
				iXRatio = std::clamp(iXRatio, 0, 100); iYRatio = std::clamp(iYRatio, 0, 100);
				return { { .m_flXRatio { iXRatio / 100.F }, .m_flYRatio { iYRatio / 100.F } } };
			}
		private:
			struct ItemData {
				HWND hWnd { };   //Item window.
				RECT rcOrig { }; //Item original client area rect after EnableTrack(true).
				MoveRatio move;  //How much to move the item.
				SizeRatio size;  //How much to resize the item.
			};
			HWND m_hWndHost { };   //Host window.
			RECT m_rcHostOrig { }; //Host original client area rect after EnableTrack(true).
			std::vector<ItemData> m_vecItems; //All items to resize/move.
			bool m_fTrack { };
		};

		void CDynLayout::AddItem(int iIDItem, MoveRatio move, SizeRatio size) {
			const auto hWnd = ::GetDlgItem(m_hWndHost, iIDItem);
			assert(hWnd != nullptr);
			if (hWnd != nullptr) {
				m_vecItems.emplace_back(ItemData { .hWnd { hWnd }, .move { move }, .size { size } });
			}
		}

		void CDynLayout::Enable(bool fTrack) {
			m_fTrack = fTrack;
			if (m_fTrack) {
				::GetClientRect(m_hWndHost, &m_rcHostOrig);
				for (auto& [hWnd, rc, move, size] : m_vecItems) {
					::GetWindowRect(hWnd, &rc);
					::ScreenToClient(m_hWndHost, reinterpret_cast<LPPOINT>(&rc));
					::ScreenToClient(m_hWndHost, reinterpret_cast<LPPOINT>(&rc) + 1);
				}
			}
		}

		void CDynLayout::OnSize(int iWidth, int iHeight)const {
			if (!m_fTrack)
				return;

			const auto iHostWidth = m_rcHostOrig.right - m_rcHostOrig.left;
			const auto iHostHeight = m_rcHostOrig.bottom - m_rcHostOrig.top;
			const auto iDeltaX = iWidth - iHostWidth;
			const auto iDeltaY = iHeight - iHostHeight;

			const auto hDWP = ::BeginDeferWindowPos(static_cast<int>(m_vecItems.size()));
			for (const auto& [hWnd, rc, move, size] : m_vecItems) {
				const auto iNewLeft = static_cast<int>(rc.left + (iDeltaX * move.m_flXRatio));
				const auto iNewTop = static_cast<int>(rc.top + (iDeltaY * move.m_flYRatio));
				const auto iOrigWidth = rc.right - rc.left;
				const auto iOrigHeight = rc.bottom - rc.top;
				const auto iNewWidth = static_cast<int>(iOrigWidth + (iDeltaX * size.m_flXRatio));
				const auto iNewHeight = static_cast<int>(iOrigHeight + (iDeltaY * size.m_flYRatio));
				::DeferWindowPos(hDWP, hWnd, nullptr, iNewLeft, iNewTop, iNewWidth, iNewHeight, SWP_NOZORDER | SWP_NOACTIVATE);
			}
			::EndDeferWindowPos(hDWP);
		}

		class CRect : public RECT {
		public:
			CRect() { left = 0; top = 0; right = 0; bottom = 0; }
			CRect(const RECT& rc) { ::CopyRect(this, &rc); }
			CRect(LPCRECT pRC) { ::CopyRect(this, pRC); }
			[[nodiscard]] int Width()const { return right - left; }
			[[nodiscard]] int Height()const { return bottom - top; }
			[[nodiscard]] bool PtInRect(POINT pt)const { return ::PtInRect(this, pt); }
			[[nodiscard]] bool IsRectEmpty()const { return ::IsRectEmpty(this); }
			[[nodiscard]] bool IsRectNull()const { return (left == 0 && right == 0 && top == 0 && bottom == 0); }
			void DeflateRect(int x, int y) { ::InflateRect(this, -x, -y); }
			void DeflateRect(SIZE size) { ::InflateRect(this, -size.cx, -size.cy); }
			void DeflateRect(LPCRECT pRC) { left += pRC->left; top += pRC->top; right -= pRC->right; bottom -= pRC->bottom; }
			void DeflateRect(int l, int t, int r, int b) { left += l; top += t; right -= r; bottom -= b; }
			void OffsetRect(int x, int y) { ::OffsetRect(this, x, y); }
			void OffsetRect(POINT pt) { ::OffsetRect(this, pt.x, pt.y); }
			void SetRect(int x1, int y1, int x2, int y2) { ::SetRect(this, x1, y1, x2, y2); }
			operator LPRECT() { return this; }
			operator LPCRECT()const { return this; }
			CRect& operator=(const RECT& rc) { ::CopyRect(this, &rc); return *this; }
		};

		class CWnd {
		public:
			CWnd() = default;
			CWnd(HWND hWnd) { Attach(hWnd); }
			virtual ~CWnd() = default;
			CWnd operator=(const CWnd&) = delete;
			CWnd operator=(HWND) = delete;
			operator HWND()const { return m_hWnd; }
			[[nodiscard]] bool operator==(const CWnd& rhs)const { return m_hWnd == rhs.m_hWnd; }
			[[nodiscard]] bool operator==(HWND hWnd)const { return m_hWnd == hWnd; }
			void Attach(HWND hWnd) { assert(::IsWindow(hWnd)); m_hWnd = hWnd; }
			void CheckRadioButton(int iIDFirst, int iIDLast, int iIDCheck)const {
				assert(IsWindow()); ::CheckRadioButton(m_hWnd, iIDFirst, iIDLast, iIDCheck);
			}
			[[nodiscard]] auto ChildWindowFromPoint(POINT pt)const->HWND {
				assert(IsWindow()); return ::ChildWindowFromPoint(m_hWnd, pt);
			}
			void ClientToScreen(LPRECT pRC)const {
				assert(IsWindow()); ::ClientToScreen(m_hWnd, reinterpret_cast<LPPOINT>(pRC));
				::ClientToScreen(m_hWnd, (reinterpret_cast<LPPOINT>(pRC)) + 1);
			}
			void DestroyWindow() { assert(IsWindow()); ::DestroyWindow(m_hWnd); m_hWnd = nullptr; }
			void Detach() { m_hWnd = nullptr; }
			[[nodiscard]] auto GetClientRect()const->CRect {
				assert(IsWindow()); RECT rc; ::GetClientRect(m_hWnd, &rc); return rc;
			}
			bool EnableWindow(bool fEnable)const { assert(IsWindow()); return ::EnableWindow(m_hWnd, fEnable); }
			void EndDialog(INT_PTR iResult)const { assert(IsWindow()); ::EndDialog(m_hWnd, iResult); }
			[[nodiscard]] int GetCheckedRadioButton(int iIDFirst, int iIDLast)const {
				assert(IsWindow()); for (int iID = iIDFirst; iID <= iIDLast; ++iID) {
					if (::IsDlgButtonChecked(m_hWnd, iID) != 0) { return iID; }
				} return 0;
			}
			[[nodiscard]] auto GetDC()const->HDC { assert(IsWindow()); return ::GetDC(m_hWnd); }
			[[nodiscard]] auto GetDlgItem(int iIDCtrl)const->HWND { assert(IsWindow()); return ::GetDlgItem(m_hWnd, iIDCtrl); }
			[[nodiscard]] auto GetHFont()const->HFONT {
				assert(IsWindow()); return reinterpret_cast<HFONT>(::SendMessageW(m_hWnd, WM_GETFONT, 0, 0));
			}
			[[nodiscard]] auto GetHWND()const->HWND { return m_hWnd; }
			[[nodiscard]] auto GetLogFont()const->std::optional<LOGFONTW> {
				if (const auto hFont = GetHFont(); hFont != nullptr) {
					LOGFONTW lf { }; ::GetObjectW(hFont, sizeof(lf), &lf); return lf;
				}
				return std::nullopt;
			}
			[[nodiscard]] auto GetParent()const->HWND { assert(IsWindow()); return ::GetParent(m_hWnd); }
			[[nodiscard]] auto GetWindowDC()const->HDC { assert(IsWindow()); return ::GetWindowDC(m_hWnd); }
			[[nodiscard]] auto GetWindowRect()const->CRect {
				assert(IsWindow()); RECT rc; ::GetWindowRect(m_hWnd, &rc); return rc;
			}
			[[nodiscard]] auto GetWndText()const->std::wstring {
				assert(IsWindow());	wchar_t buff[MAX_PATH]; ::GetWindowTextW(m_hWnd, buff, std::size(buff)); return buff;
			}
			[[nodiscard]] auto GetWndTextSize()const->DWORD { assert(IsWindow()); return ::GetWindowTextLengthW(m_hWnd); }
			void Invalidate(bool fErase)const { assert(IsWindow()); ::InvalidateRect(m_hWnd, nullptr, fErase); }
			[[nodiscard]] bool IsDlgMessage(MSG* pMsg)const { return ::IsDialogMessageW(m_hWnd, pMsg); }
			[[nodiscard]] bool IsNull()const { return m_hWnd == nullptr; }
			[[nodiscard]] bool IsWindow()const { return ::IsWindow(m_hWnd); }
			[[nodiscard]] bool IsWindowEnabled()const { assert(IsWindow()); return ::IsWindowEnabled(m_hWnd); }
			[[nodiscard]] bool IsWindowVisible()const { assert(IsWindow()); return ::IsWindowVisible(m_hWnd); }
			[[nodiscard]] bool IsWndTextEmpty()const { return GetWndTextSize() == 0; }
			void KillTimer(UINT_PTR uID)const {
				assert(IsWindow()); ::KillTimer(m_hWnd, uID);
			}
			int MapWindowPoints(HWND hWndTo, LPRECT pRC)const {
				assert(IsWindow()); return ::MapWindowPoints(m_hWnd, hWndTo, reinterpret_cast<LPPOINT>(pRC), 2);
			}
			bool RedrawWindow(LPCRECT pRC = nullptr, HRGN hrgn = nullptr,
				UINT uFlags = RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE)const {
				assert(IsWindow()); return static_cast<bool>(::RedrawWindow(m_hWnd, pRC, hrgn, uFlags));
			}
			int ReleaseDC(HDC hDC)const { assert(IsWindow()); return ::ReleaseDC(m_hWnd, hDC); }
			auto SetTimer(UINT_PTR uID, UINT uElapse, TIMERPROC pFN = nullptr)const->UINT_PTR {
				assert(IsWindow()); return ::SetTimer(m_hWnd, uID, uElapse, pFN);
			}
			void ScreenToClient(LPPOINT pPT)const { assert(IsWindow()); ::ScreenToClient(m_hWnd, pPT); }
			void ScreenToClient(POINT& pt)const { ScreenToClient(&pt); }
			void ScreenToClient(LPRECT pRC)const {
				ScreenToClient(reinterpret_cast<LPPOINT>(pRC)); ScreenToClient(reinterpret_cast<LPPOINT>(pRC) + 1);
			}
			auto SendMsg(UINT uMsg, WPARAM wParam = 0, LPARAM lParam = 0)const {
				assert(IsWindow()); ::SendMessageW(m_hWnd, uMsg, wParam, lParam);
			}
			auto SetCapture()const->HWND { assert(IsWindow()); return ::SetCapture(m_hWnd); }
			auto SetWndClassLong(int iIndex, LONG_PTR dwNewLong)const->ULONG_PTR {
				assert(IsWindow()); return ::SetClassLongPtrW(m_hWnd, iIndex, dwNewLong);
			}
			void SetFocus()const { assert(IsWindow()); ::SetFocus(m_hWnd); }
			void SetWindowPos(HWND hWndAfter, int iX, int iY, int iWidth, int iHeight, UINT uFlags)const {
				assert(IsWindow()); ::SetWindowPos(m_hWnd, hWndAfter, iX, iY, iWidth, iHeight, uFlags);
			}
			void SetWndText(LPCWSTR pwszStr)const { assert(IsWindow()); ::SetWindowTextW(m_hWnd, pwszStr); }
			void SetWndText(const std::wstring& wstr)const { SetWndText(wstr.data()); }
			void SetRedraw(bool fRedraw)const { assert(IsWindow()); ::SendMessageW(m_hWnd, WM_SETREDRAW, fRedraw, 0); }
			bool ShowWindow(int iCmdShow)const { assert(IsWindow()); return ::ShowWindow(m_hWnd, iCmdShow); }
			[[nodiscard]] static auto FromHandle(HWND hWnd) -> CWnd { return hWnd; }
			[[nodiscard]] static auto GetFocus() -> CWnd { return ::GetFocus(); }
		protected:
			HWND m_hWnd { }; //Windows window handle.
		};

		class CWndBtn : public CWnd {
		public:
			[[nodiscard]] bool IsChecked()const { assert(IsWindow()); return ::SendMessageW(m_hWnd, BM_GETCHECK, 0, 0); }
			void SetBitmap(HBITMAP hBmp)const {
				assert(IsWindow()); ::SendMessageW(m_hWnd, BM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(hBmp));
			}
			void SetCheck(bool fCheck)const { assert(IsWindow()); ::SendMessageW(m_hWnd, BM_SETCHECK, fCheck, 0); }
		};

		class CWndCombo : public CWnd {
		public:
			int AddString(LPCWSTR pwszStr)const {
				assert(IsWindow());
				return static_cast<int>(::SendMessageW(m_hWnd, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(pwszStr)));
			}
			int DeleteString(int iIndex)const {
				assert(IsWindow()); return static_cast<int>(::SendMessageW(m_hWnd, CB_DELETESTRING, iIndex, 0));
			}
			[[nodiscard]] int FindStringExact(int iIndex, LPCWSTR pwszStr)const {
				assert(IsWindow());
				return static_cast<int>(::SendMessageW(m_hWnd, CB_FINDSTRINGEXACT, iIndex, reinterpret_cast<LPARAM>(pwszStr)));
			}
			[[nodiscard]] int GetCount()const {
				assert(IsWindow()); return static_cast<int>(::SendMessageW(m_hWnd, CB_GETCOUNT, 0, 0));
			}
			[[nodiscard]] int GetCurSel()const {
				assert(IsWindow()); return static_cast<int>(::SendMessageW(m_hWnd, CB_GETCURSEL, 0, 0));
			}
			[[nodiscard]] auto GetItemData(int iIndex)const->DWORD_PTR {
				assert(IsWindow()); return ::SendMessageW(m_hWnd, CB_GETITEMDATA, iIndex, 0);
			}
			[[nodiscard]] bool HasString(LPCWSTR pwszStr)const { return FindStringExact(0, pwszStr) != CB_ERR; };
			[[nodiscard]] bool HasString(const std::wstring& wstr)const { return HasString(wstr.data()); };
			int InsertString(int iIndex, const std::wstring& wstr)const { return InsertString(iIndex, wstr.data()); }
			int InsertString(int iIndex, LPCWSTR pwszStr)const {
				assert(IsWindow());
				return static_cast<int>(::SendMessageW(m_hWnd, CB_INSERTSTRING, iIndex, reinterpret_cast<LPARAM>(pwszStr)));
			}
			void LimitText(int iMaxChars)const { assert(IsWindow()); ::SendMessageW(m_hWnd, CB_LIMITTEXT, iMaxChars, 0); }
			void SetCurSel(int iIndex)const { assert(IsWindow()); ::SendMessageW(m_hWnd, CB_SETCURSEL, iIndex, 0); }
			void SetItemData(int iIndex, DWORD_PTR dwData)const {
				assert(IsWindow()); ::SendMessageW(m_hWnd, CB_SETITEMDATA, iIndex, static_cast<LPARAM>(dwData));
			}
		};

		class CWndTree : public CWnd {
		public:
			void DeleteAllItems()const { DeleteItem(TVI_ROOT); };
			void DeleteItem(HTREEITEM hItem)const {
				assert(IsWindow()); ::SendMessageW(m_hWnd, TVM_DELETEITEM, 0, reinterpret_cast<LPARAM>(hItem));
			}
			void Expand(HTREEITEM hItem, UINT uCode)const {
				assert(IsWindow()); ::SendMessageW(m_hWnd, TVM_EXPAND, uCode, reinterpret_cast<LPARAM>(hItem));
			}
			void GetItem(TVITEMW* pItem)const {
				assert(IsWindow()); ::SendMessageW(m_hWnd, TVM_GETITEM, 0, reinterpret_cast<LPARAM>(pItem));
			}
			[[nodiscard]] auto GetItemData(HTREEITEM hItem)const->DWORD_PTR {
				TVITEMW item { .mask { TVIF_PARAM }, .hItem { hItem } }; GetItem(&item); return item.lParam;
			}
			[[nodiscard]] auto GetNextItem(HTREEITEM hItem, UINT uCode)const->HTREEITEM {
				assert(IsWindow()); return reinterpret_cast<HTREEITEM>
					(::SendMessageW(m_hWnd, TVM_GETNEXTITEM, uCode, reinterpret_cast<LPARAM>(hItem)));
			}
			[[nodiscard]] auto GetNextSiblingItem(HTREEITEM hItem)const->HTREEITEM { return GetNextItem(hItem, TVGN_NEXT); }
			[[nodiscard]] auto GetParentItem(HTREEITEM hItem)const->HTREEITEM { return GetNextItem(hItem, TVGN_PARENT); }
			[[nodiscard]] auto GetRootItem()const->HTREEITEM { return GetNextItem(nullptr, TVGN_ROOT); }
			[[nodiscard]] auto GetSelectedItem()const->HTREEITEM { return GetNextItem(nullptr, TVGN_CARET); }
			[[nodiscard]] auto HitTest(TVHITTESTINFO* pHTI)const->HTREEITEM {
				assert(IsWindow());
				return reinterpret_cast<HTREEITEM>(::SendMessageW(m_hWnd, TVM_HITTEST, 0, reinterpret_cast<LPARAM>(pHTI)));
			}
			[[nodiscard]] auto HitTest(CPoint pt, UINT* pFlags = nullptr)const->HTREEITEM {
				TVHITTESTINFO hti { .pt { pt } }; auto ret = HitTest(&hti);	if (pFlags != nullptr) { *pFlags = hti.flags; }
				return ret;
			}

			auto InsertItem(LPTVINSERTSTRUCTW pTIS)const->HTREEITEM {
				assert(IsWindow()); return reinterpret_cast<HTREEITEM>
					(::SendMessageW(m_hWnd, TVM_INSERTITEM, 0, reinterpret_cast<LPARAM>(pTIS)));
			}
			void SelectItem(HTREEITEM hItem)const {
				assert(IsWindow()); ::SendMessageW(m_hWnd, TVM_SELECTITEM, TVGN_CARET, reinterpret_cast<LPARAM>(hItem));
			}
		};

		class CWndProgBar : public CWnd {
		public:
			int SetPos(int iPos)const {
				assert(IsWindow()); return static_cast<int>(::SendMessageW(m_hWnd, PBM_SETPOS, iPos, 0UL));
			}
			void SetRange(int iLower, int iUpper)const {
				assert(IsWindow());
				::SendMessageW(m_hWnd, PBM_SETRANGE32, static_cast<WPARAM>(iLower), static_cast<LPARAM>(iUpper));
			}
		};

		class CWndTab : public CWnd {
		public:
			[[nodiscard]] int GetCurSel()const {
				assert(IsWindow()); return static_cast<int>(::SendMessageW(m_hWnd, TCM_GETCURSEL, 0, 0L));
			}
			[[nodiscard]] auto GetItemRect(int nItem)const->CRect {
				assert(IsWindow());
				RECT rc { }; ::SendMessageW(m_hWnd, TCM_GETITEMRECT, nItem, reinterpret_cast<LPARAM>(&rc));
				return rc;
			}
			auto InsertItem(int iItem, TCITEMW* pItem)const->LONG {
				assert(IsWindow());
				return static_cast<LONG>(::SendMessageW(m_hWnd, TCM_INSERTITEMW, iItem, reinterpret_cast<LPARAM>(pItem)));
			}
			auto InsertItem(int iItem, LPCWSTR pwszStr)const->LONG {
				TCITEMW tci { .mask { TCIF_TEXT }, .pszText { const_cast<LPWSTR>(pwszStr) } };
				return InsertItem(iItem, &tci);
			}
			int SetCurSel(int iItem)const {
				assert(IsWindow()); return static_cast<int>(::SendMessageW(m_hWnd, TCM_SETCURSEL, iItem, 0L));
			}
		};

		class CMenu {
		public:
			CMenu() = default;
			CMenu(HMENU hMenu) { Attach(hMenu); }
			virtual ~CMenu() = default;
			CMenu operator=(const CWnd&) = delete;
			CMenu operator=(HMENU) = delete;
			operator HMENU()const { return m_hMenu; }
			void AppendItem(UINT uFlags, UINT_PTR uIDItem, LPCWSTR pNameItem)const {
				assert(IsMenu()); ::AppendMenuW(m_hMenu, uFlags, uIDItem, pNameItem);
			}
			void AppendSepar()const { AppendItem(MF_SEPARATOR, 0, nullptr); }
			void AppendString(UINT_PTR uIDItem, LPCWSTR pNameItem)const { AppendItem(MF_STRING, uIDItem, pNameItem); }
			void Attach(HMENU hMenu) { assert(::IsMenu(hMenu)); m_hMenu = hMenu; }
			void CheckItem(UINT uIDItem, bool fCheck, bool fByID = true)const {
				assert(IsMenu()); ::CheckMenuItem(m_hMenu, uIDItem, fCheck ? MF_CHECKED : MF_UNCHECKED |
				(fByID ? MF_BYCOMMAND : MF_BYPOSITION));
			}
			void CreatePopupMenu() { Attach(::CreatePopupMenu()); }
			void DestroyMenu() { assert(IsMenu()); ::DestroyMenu(m_hMenu); m_hMenu = nullptr; }
			void Detach() { m_hMenu = nullptr; }
			void EnableItem(UINT uIDItem, bool fEnable, bool fByID = true)const {
				assert(IsMenu()); ::EnableMenuItem(m_hMenu, uIDItem, fEnable ? MF_ENABLED : MF_GRAYED |
					(fByID ? MF_BYCOMMAND : MF_BYPOSITION));
			}
			[[nodiscard]] auto GetMenuState(UINT uID, UINT nFlags)const->UINT {
				assert(IsMenu()); return ::GetMenuState(m_hMenu, uID, nFlags);
			}
			[[nodiscard]] bool IsChecked(UINT uIDItem, bool fByID = true)const {
				return GetMenuState(uIDItem, fByID ? MF_BYCOMMAND : MF_BYPOSITION) & MF_CHECKED;
			}
			[[nodiscard]] bool IsMenu()const { return ::IsMenu(m_hMenu); }
			void TrackPopup(int iX, int iY, HWND hWndOwner, UINT uFlags = TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON)const {
				assert(IsMenu()); ::TrackPopupMenuEx(m_hMenu, uFlags, iX, iY, hWndOwner, nullptr);
			}
		private:
			HMENU m_hMenu { }; //Windows menu handle.
		};
	};

#if defined(DEBUG) || defined(_DEBUG)
	void DBG_REPORT(const wchar_t* pMsg, const std::source_location& loc = std::source_location::current()) {
		_wassert(pMsg, StrToWstr(loc.file_name()).data(), loc.line());
	}
#else
	void DBG_REPORT([[maybe_unused]] const wchar_t*) { }
#endif
}