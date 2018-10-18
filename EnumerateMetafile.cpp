/***************************************************************************
* Copyright (C) 2017, Deping Chen, cdp97531@sina.com
*
* All rights reserved.
* For permission requests, write to the author.
*
* This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
* KIND, either express or implied.
***************************************************************************/
#include <Windows.h>
#include <Gdiplus.h>

#include <functional>
#include <memory>
#include <sstream>
#include <type_traits>

#include "ConstantDictionary.h"

using ModeConverter = std::function<const char * (int mode)>;

struct ShortPoint
{
	short x, y;
};

template<typename T>
struct OutputType
{
	using type = T;
};

template<>
struct OutputType<ShortPoint>
{
	using type = POINT;
};

template<typename T>
void TypeToString(std::stringstream& ss, const T& t)
{
	ss << t;
}

template<>
void TypeToString<ShortPoint>(std::stringstream& ss, const ShortPoint& t)
{
	ss << '{' << t.x << ',' << t.y << '}';
}

template<>
void TypeToString<POINT>(std::stringstream& ss, const POINT& t)
{
	ss << '{' << t.x << ',' << t.y << '}';
}

template<>
void TypeToString<RECTL>(std::stringstream& ss, const RECTL& t)
{
	ss << '{' << t.left << ',' << t.top << ',' << t.right << ',' << t.bottom << '}';
}

template<>
void TypeToString<RECT>(std::stringstream& ss, const RECT& t)
{
	ss << '{' << t.left << ',' << t.top << ',' << t.right << ',' << t.bottom << '}';
}

template<typename T>
void ArrayToString(std::stringstream& ss, const T* array, int32_t count, const char* arrayName, int indentLevel)
{
	for (int i = 0; i < indentLevel; ++i)
		ss << '\t';

	if (count <= 0)
	{
		ss << "// Array count = 0";
		return;
	}

	// T arrayName[] = { // sizeof(T) = n
	using PT = std::remove_cv<T>::type;
	ss << typeid(OutputType<PT>::type).name() << ' ' << arrayName << "[] = { // sizeof(" << typeid(OutputType<PT>::type).name() << ") = " << sizeof(OutputType<PT>::type) << "\n";

	// t1, t2, t3 ...
	for (int j = 0; j < indentLevel + 1; ++j)
		ss << '\t';
	for (int i = 0; i < count; ++i)
	{
		TypeToString(ss, array[i]);
		ss << ',';
	}
	// erase last comma.
	ss.seekp((int)ss.tellp() - 1);
	ss << "\n";

	// };
	for (int i = 0; i < indentLevel; ++i)
		ss << '\t';
	ss << "};\n";
}

template<typename CharType>
struct ExtTextOutName;

template<>
struct ExtTextOutName<char>
{
	static const char* get()
	{
		return "ExtTextOutA";
	}
	static const char* prefix()
	{
		return "";
	}
};

template<>
struct ExtTextOutName<wchar_t>
{
	static const char* get()
	{
		return "ExtTextOutW";
	}
	static const char* prefix()
	{
		return "L";
	}
};

template<typename CharType>
struct CharTraits;

template<>
struct CharTraits<char>
{
	static const char* prefix()
	{
		return "";
	}
	static std::string convert(const std::string& buffer)
	{
		return buffer;
	}
};

template<>
struct CharTraits<wchar_t>
{
	static const char* prefix()
	{
		return "L";
	}

	static std::string convert(const std::wstring& buffer)
	{
		UINT acp = CP_UTF8;
		DWORD  num = WideCharToMultiByte(acp, 0, buffer.c_str(), buffer.length(), NULL, 0, NULL, NULL);
		std::string res(num, '?');
		WideCharToMultiByte(acp, 0, buffer.c_str(), buffer.length(), &res[0], num, NULL, NULL);
		return res;
	}
};

template<typename LOGBRUSHType>
void LogBrushToString(std::stringstream& ss, const LOGBRUSHType& logBrush, int indentLevel)
{
	const char* lbHatch;
	switch (logBrush.lbStyle)
	{
	case BS_HATCHED:
		lbHatch = ConstantDictionary::HatchStyle((int)logBrush.lbHatch);
		break;
	case BS_PATTERN:
	case BS_INDEXED:
	case BS_DIBPATTERN:
	case BS_DIBPATTERNPT:
	case BS_PATTERN8X8:
	case BS_DIBPATTERN8X8:
	case BS_MONOPATTERN:
		// TODO
		lbHatch = "0";
		break;
	case BS_SOLID:
	case BS_NULL:
	default:
		lbHatch = "0";
		break;
	}
	for (int i = 0; i < indentLevel; ++i)
		ss << '\t';
	ss << "LOGBRUSH logBrush = " << '{' << ConstantDictionary::BrushStyle(logBrush.lbStyle) << ','
		<< ConstantDictionary::RGBColor(logBrush.lbColor) << ", " << lbHatch << "};\n";
}

void LogFontWToString(std::stringstream& ss, const LOGFONTW& logFont, int indentLevel)
{
	for (int i = 0; i < indentLevel; ++i)
		ss << '\t';
	ss << "LOGFONTW logFont = " << '{' << logFont.lfHeight << ", " << logFont.lfWidth << ", "
		<< logFont.lfEscapement << ", " << logFont.lfOrientation << ", "
		<< ConstantDictionary::FontWeight(logFont.lfWeight) << ", "
		<< ConstantDictionary::BigBool(logFont.lfItalic) << ", "
		<< ConstantDictionary::BigBool(logFont.lfUnderline) << ", "
		<< ConstantDictionary::BigBool(logFont.lfStrikeOut) << ", "
		<< ConstantDictionary::CharSet(logFont.lfCharSet) << ", "
		<< ConstantDictionary::CharPrecision(logFont.lfOutPrecision) << ", "
		<< ConstantDictionary::ClipPrecision(logFont.lfClipPrecision) << ", "
		<< ConstantDictionary::CharQuality(logFont.lfQuality) << ", "
		<< ConstantDictionary::PitchAndFamily(logFont.lfPitchAndFamily) << ", "
		<< "L\"" << CharTraits<wchar_t>::convert(logFont.lfFaceName).c_str() << "\"};\n";
}

void AppendBMIText(const BITMAPINFO* pBmi, std::stringstream& ss)
{
	auto pBH = &pBmi->bmiHeader;
	const char* format = R"(	BITMAPINFO bmi = {
		{
			%d, // biSize
			%d, // biWidth
			%d, // biHeight
			%d, // biPlanes
			%d, // biBitCount
			%d, // biCompression
			%d, // biSizeImage
			%d, // biXPelsPerMeter
			%d, // biYPelsPerMeter
			%d, // biClrUsed
			%d // biClrImportant
		}
	};
)";
	char buffer[512];
	sprintf_s(buffer, format, (int)pBH->biSize, (int)pBH->biWidth, (int)pBH->biHeight, (int)pBH->biPlanes,
		(int)pBH->biBitCount, (int)pBH->biCompression, (int)pBH->biSizeImage, (int)pBH->biXPelsPerMeter,
		(int)pBH->biYPelsPerMeter, (int)pBH->biClrUsed, (int)pBH->biClrImportant);
	ss << buffer;
}

void AppendBits(const char* pBits, int byteCount, int height, std::stringstream& ss)
{
	ss << std::hex;

	ss << "\tconst char bits[] = {\n";
	int lineWidth = byteCount / height;
	for (int i = 0; i < height; ++i)
	{
		ss << "\t\t";
		for (int j = 0; j < lineWidth; ++j)
		{
			ss << "0x" << (int)pBits[j] << ',';
		}
		if (i == height - 1)
		{
			// erase last comma.
			ss.seekp((int)ss.tellp() - 1);
		}
		ss << "\n";
		pBits += lineWidth;
	}
	ss << "\t};\n";

	ss << std::dec;
}

void NoParams(const char* func, std::stringstream& ss)
{
	ss << func << "(hdc);";
}

void OneInt(const char* func, const unsigned char* data, std::stringstream& ss, ModeConverter converter = nullptr)
{
	int32_t x = *(int32_t*)(data);
	if (converter)
		ss << func << "(hdc, " << converter(x) << ");\n";
	else
		ss << func << "(hdc, " << x << ");\n";
}

void SelectObject(const unsigned char* data, std::stringstream& ss)
{
	int32_t x = *(int32_t*)(data);
	if (x & 0x80000000)
	{
		ss << "g_stockObject = GetStockObject(" << ConstantDictionary::StockObject(x & ~0x80000000) << ");\n";
		ss << "SelectObject(hdc, g_stockObject);\n";
	}
	else
	{
		ss << "SelectObject(hdc, gdiHandles[" << x << "]);\n";
	}
}

void DeleteObject(const unsigned char* data, std::stringstream& ss)
{
	int32_t x = *(int32_t*)(data);
	ss << "DeleteObject(gdiHandles[" << x << "]);\n";
}

void CreatePen(const unsigned char* data, std::stringstream& ss)
{
	int32_t elements[5];
	for (int i = 0; i < 5; ++i)
		elements[i] = *((int32_t*)data + i);
	ss << "gdiHandles[" << elements[0] << "] = CreatePen(" << ConstantDictionary::PenStyle(elements[1]) << ", " << elements[2] << ", " << ConstantDictionary::RGBColor(elements[4]) << ");\n";
}

void ExtCreatePen(const unsigned char* data, std::stringstream& ss)
{
	auto record = reinterpret_cast<const EMREXTCREATEPEN*>(data - sizeof(EMR));
	const auto& elp = record->elp;
	const auto& logBrush = *reinterpret_cast<const LOGBRUSH*>(&elp.elpBrushStyle);
	//auto pBmi = reinterpret_cast<const BITMAPINFO*>((const char*)record + record->offBmi);
	ss << "{\n";
	LogBrushToString(ss, logBrush, 1);
	const char* pstyle;
	if (elp.elpNumEntries)
	{
		ArrayToString<DWORD>(ss, elp.elpStyleEntry, elp.elpNumEntries, "style", 1);
		pstyle = "&style";
	}
	else
	{
		pstyle = "nullptr";
	}
	ss << "\tgdiHandles[" << record->ihPen << "] = ExtCreatePen(" << ConstantDictionary::PenStyle(elp.elpPenStyle) << ", " << elp.elpWidth << ", &logBrush, " << elp.elpNumEntries << ", " << pstyle << ");\n";
	ss << "}\n";
}

void CreateBrushIndirect(const unsigned char* data, std::stringstream& ss)
{
	auto record = reinterpret_cast<const EMRCREATEBRUSHINDIRECT*>(data - sizeof(EMR));
	const auto& logBrush = record->lb;
	ss << "{\n";
	LogBrushToString(ss, logBrush, 1);
	ss << "\tgdiHandles[" << record->ihBrush << "] = CreateBrushIndirect(&logBrush);\n";
	ss << "}\n";
}

void CreateFontIndirectW(const unsigned char* data, std::stringstream& ss)
{
	auto record = reinterpret_cast<const EMREXTCREATEFONTINDIRECTW*>(data - sizeof(EMR));
	const auto& logFont = record->elfw.elfLogFont;
	ss << "{\n";
	LogFontWToString(ss, logFont, 1);
	ss << "\tgdiHandles[" << record->ihFont << "] = CreateFontIndirectW(&logFont);\n";
	ss << "}\n";
}

void OnePoint(const char* func, const unsigned char* data, std::stringstream& ss, const char* suffix = "")
{
	int32_t x = *(int32_t*)(data);
	int32_t y = *(int32_t*)(data + sizeof(int32_t));
	ss << func << "(hdc, " << x << ", " << y << suffix << ");\n";
}

void TwoPoints(const char* func, const unsigned char* data, std::stringstream& ss)
{
	int32_t elements[4];
	for (int i = 0; i < 4; ++i)
		elements[i] = *((int32_t*)data + i);
	ss << func << "(hdc, " << elements[0] << ", " << elements[1] << ", " << elements[2] << ", " << elements[3] << ");\n";
}

void ThreePoints(const char* func, const unsigned char* data, std::stringstream& ss)
{
	int32_t elements[6];
	for (int i = 0; i < 6; ++i)
		elements[i] = *((int32_t*)data + i);
	ss << func << "(hdc, " << elements[0] << ", " << elements[1] << ", " << elements[2] << ", " << elements[3] << ", " << elements[4] << ", " << elements[5] << ");\n";
}

void FourPoints(const char* func, const unsigned char* data, std::stringstream& ss)
{
	int32_t elements[8];
	for (int i = 0; i < 8; ++i)
		elements[i] = *((int32_t*)data + i);
	ss << func << "(hdc, " << elements[0] << ", " << elements[1] << ", " << elements[2] << ", " << elements[3] << ", " << elements[4] << ", " << elements[5] << ", " << elements[6] << ", " << elements[7] << ");\n";
}

void AngleArc(const char* func, const unsigned char* data, std::stringstream& ss)
{
	int32_t x, y;
	DWORD r;
	float StartAngle;
	float SweepAngle;
	x = *(int32_t*)(data);
	y = *(int32_t*)(data + sizeof(int32_t));
	r = *(int32_t*)(data + sizeof(int32_t) + sizeof(int32_t));
	StartAngle = *(float*)(data + sizeof(int32_t) + sizeof(int32_t) + sizeof(DWORD));
	SweepAngle = *(float*)(data + sizeof(int32_t) + sizeof(int32_t) + sizeof(DWORD) + sizeof(float));
	ss << func << "(hdc, " << x << ", " << y << ", " << r << ", " << StartAngle << ", " << SweepAngle << ");\n";
}

template<typename PointType>
void Polyline(const char* func, const unsigned char* data, std::stringstream& ss)
{
	int32_t count = *(int32_t*)(data + 4 * sizeof(int32_t));
	ss << "{\n";
	auto points = reinterpret_cast<const PointType*>(data + 5 * sizeof(int32_t));
	ArrayToString(ss, points, count, "points", 1);
	ss << "\t" << func << "(hdc, points, " << count << ");\n";
	ss << "}\n";
}

template<typename PointType>
void PolyPolyline(const char* func, const unsigned char* data, std::stringstream& ss)
{
	data += 4 * sizeof(int32_t); // skip Bounds
	int32_t numberOfPolylines = *(int32_t*)(data);
	int32_t numberOfPoints = *(int32_t*)(data + sizeof(int32_t));
	data += 2 * sizeof(int32_t);

	ss << "{\n";
	const uint32_t* polys = reinterpret_cast<const uint32_t*>(data);
	ArrayToString(ss, polys, numberOfPolylines, "polys", 1);

	auto points = reinterpret_cast<const PointType*>(data + numberOfPolylines * sizeof(uint32_t));
	ArrayToString(ss, points, numberOfPoints, "points", 1);
	ss << "\t" << func << "(hdc, points, polys, " << numberOfPolylines << ");\n";
	ss << "}\n";
}

void BitBlt(unsigned int dataSize, const unsigned char* data, std::stringstream& ss)
{
	auto pEmrBitBlt = reinterpret_cast<const EMRBITBLT*>(data - sizeof(EMR));
	auto pBmi = reinterpret_cast<const BITMAPINFO*>((const char*)pEmrBitBlt + pEmrBitBlt->offBmiSrc);
	auto pBH = &pBmi->bmiHeader;
	auto pBits = (const char*)pEmrBitBlt + pEmrBitBlt->offBitsSrc;
	ss << "{\n";
	AppendBits(pBits, pEmrBitBlt->cbBitsSrc, pBH->biHeight, ss);
	AppendBMIText(pBmi, ss);
	const char* bitBlttext = R"(	HBITMAP hBitmap;
	HDC hMemDC;
	hBitmap = CreateCompatibleBitmap(hdc, pBH->biWidth, pBH->biHeight);
	hMemDC = CreateCompatibleDC(hdc);
	SetDIBits(hdc, hBitmap, 0, pBH->biHeight, bits, &bmi, pEmrBitBlt->iUsageSrc);
	HGDIOBJ holdBmp = SelectObject(hMemDC, hBitmap);
	BitBlt(hdc, " << %d << ", " << %d << ", " << %d << ", " << %d << ", hMemDC, " << %d << ", " << %d << ", " << %s << ");
	DeleteObject(SelectObject(hMemDC, holdBmp));
	DeleteDC(hMemDC);
)";
	char buffer[512];
	sprintf_s(buffer, bitBlttext, (int)pEmrBitBlt->xDest, (int)pEmrBitBlt->yDest, (int)pEmrBitBlt->cxDest, (int)pEmrBitBlt->cyDest,
		(int)pEmrBitBlt->xSrc, (int)pEmrBitBlt->ySrc, ConstantDictionary::ROP3(pEmrBitBlt->dwRop));
	ss << buffer;
	ss << "}\n";
}

void StretchBlt(unsigned int dataSize, const unsigned char* data, std::stringstream& ss)
{
	auto pEmrStretchBlt = reinterpret_cast<const EMRSTRETCHBLT*>(data - sizeof(EMR));
	auto pBmi = reinterpret_cast<const BITMAPINFO*>((const char*)pEmrStretchBlt + pEmrStretchBlt->offBmiSrc);
	auto pBH = &pBmi->bmiHeader;
	auto pBits = (const char*)pEmrStretchBlt + pEmrStretchBlt->offBitsSrc;
	ss << "{\n";
	AppendBits(pBits, pEmrStretchBlt->cbBitsSrc, pBH->biHeight, ss);
	AppendBMIText(pBmi, ss);
	const char* stretchBlttext = R"(	HBITMAP hBitmap;
	HDC hMemDC;
	hBitmap = CreateCompatibleBitmap(hdc, pBH->biWidth, pBH->biHeight);
	hMemDC = CreateCompatibleDC(hdc);
	SetDIBits(hdc, hBitmap, 0, pBH->biHeight, bits, &bmi, pEmrStretchBlt->iUsageSrc);
	HGDIOBJ holdBmp = SelectObject(hMemDC, hBitmap);
	StretchBlt(hdc, " << %d << ", " << %d << ", " << %d << ", " << %d << ", hMemDC, " << %d << ", " << %d << ", " << %d << ", " << %d << ", " << %s << ");
	DeleteObject(SelectObject(hMemDC, holdBmp));
	DeleteDC(hMemDC);
)";
	char buffer[512];
	sprintf_s(buffer, stretchBlttext, (int)pEmrStretchBlt->xDest, (int)pEmrStretchBlt->yDest, (int)pEmrStretchBlt->cxDest, (int)pEmrStretchBlt->cyDest,
		(int)pEmrStretchBlt->xSrc, (int)pEmrStretchBlt->ySrc, (int)pEmrStretchBlt->cxSrc, (int)pEmrStretchBlt->cySrc, ConstantDictionary::ROP3(pEmrStretchBlt->dwRop));
	ss << buffer;
	ss << "}\n";
}

void StretchDIBits(unsigned int dataSize, const unsigned char* data, std::stringstream& ss)
{
	auto pEmrStretchDIBits = reinterpret_cast<const EMRSTRETCHDIBITS*>(data - sizeof(EMR));
	auto pBmi = reinterpret_cast<const BITMAPINFO*>((const char*)pEmrStretchDIBits + pEmrStretchDIBits->offBmiSrc);
	auto pBH = &pBmi->bmiHeader;
	auto pBits = (const char*)pEmrStretchDIBits + pEmrStretchDIBits->offBitsSrc;
	ss << "{\n";
	AppendBits(pBits, pEmrStretchDIBits->cbBitsSrc, pBH->biHeight, ss);
	AppendBMIText(pBmi, ss);
	const char* stretchDIBitstext = R"(	StretchDIBits(hdc, " << %d << ", " << %d << ", " << %d << ", " << %d << ", " << %d << ", " << %d << ", " << %d << ", " << %d << ", &bits, &bmi, " << %s << ", " << %s << ");\n)";
	char buffer[512];
	sprintf_s(buffer, stretchDIBitstext, (int)pEmrStretchDIBits->xDest, (int)pEmrStretchDIBits->yDest, (int)pEmrStretchDIBits->cxDest, (int)pEmrStretchDIBits->cyDest,
		(int)pEmrStretchDIBits->xSrc, (int)pEmrStretchDIBits->ySrc, (int)pEmrStretchDIBits->cxSrc, (int)pEmrStretchDIBits->cySrc,
		ConstantDictionary::ColorTableUsage(pEmrStretchDIBits->iUsageSrc), ConstantDictionary::ROP3(pEmrStretchDIBits->dwRop));
	ss << buffer;
	ss << "}\n";
}

inline void AppendXForm(const unsigned char* data, std::stringstream& ss)
{
	auto pf = reinterpret_cast<const XFORM*>(data);
	ss << "\tXFORM xf = {" << pf->eM11 << ", " << pf->eM12 << ", " << pf->eM21 << ", " << pf->eM22 << ", " << pf->eDx << ", " << pf->eDy << "};\n";
}

void SetWorldTransform(const unsigned char* data, std::stringstream& ss)
{
	ss << "{\n";
	AppendXForm(data, ss);
	ss << "\tSetWorldTransform(hdc, &xf);\n";
	ss << "}\n";
}

void ModifyWorldTransform(const unsigned char* data, std::stringstream& ss)
{
	ss << "{\n";
	AppendXForm(data, ss);
	int32_t mode = *(int32_t*)(data + sizeof(XFORM));
	ss << "\tModifyWorldTransform(hdc, &xf, " << ConstantDictionary::WorldTransform(mode) << ");\n";
	ss << "}\n";
}

template<typename CharType>
void ExtTextOut(const unsigned char* data, std::stringstream& ss)
{
	auto emrText = reinterpret_cast<const EMREXTTEXTOUTA*>(data - sizeof(EMR))->emrtext;
	using xstring = std::basic_string<CharType, std::char_traits<CharType>, std::allocator<CharType>>;
	xstring buffer(reinterpret_cast<const CharType*>(data - sizeof(EMR) + emrText.offString), emrText.nChars);
	ss << "{\n";
	ss << "\tconst " << typeid(CharType).name() << "* text = " << CharTraits<CharType>::prefix() << "\"" << CharTraits<CharType>::convert(buffer).c_str() << "\";\n";
	ss << "\tRECT rect = ";
	TypeToString(ss, emrText.rcl);
	ss << ";\n";
	ss << "\t" << ExtTextOutName<CharType>::get() << "(hdc, " << emrText.ptlReference.x << ", "
		<< emrText.ptlReference.y << ", " << ConstantDictionary::ExtTextOutOptions(emrText.fOptions)
		<< ", &rect, text, " << emrText.nChars << ", nullptr);\n";
	ss << "}\n";
}

BOOL CALLBACK EnumMetafileCallback(
	Gdiplus::EmfPlusRecordType recordType,
	unsigned int flags,
	unsigned int dataSize,
	const unsigned char* data,
	void* callbackData)
{
	using namespace Gdiplus;

	std::stringstream& ss = *reinterpret_cast<std::stringstream*>(callbackData);
	switch (recordType)
	{
#pragma region "WmfRecord"
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeSetBkColor:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeSetBkMode:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeSetMapMode:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeSetROP2:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeSetRelAbs:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeSetPolyFillMode:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeSetStretchBltMode:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeSetTextCharExtra:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeSetTextColor:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeSetTextJustification:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeSetWindowOrg:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeSetWindowExt:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeSetViewportOrg:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeSetViewportExt:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeOffsetWindowOrg:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeScaleWindowExt:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeOffsetViewportOrg:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeScaleViewportExt:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeLineTo:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeMoveTo:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeExcludeClipRect:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeIntersectClipRect:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeArc:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeEllipse:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeFloodFill:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypePie:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeRectangle:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeRoundRect:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypePatBlt:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeSaveDC:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeSetPixel:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeOffsetClipRgn:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeTextOut:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeBitBlt:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeStretchBlt:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypePolygon:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypePolyline:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeEscape:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeRestoreDC:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeFillRegion:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeFrameRegion:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeInvertRegion:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypePaintRegion:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeSelectClipRegion:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeSelectObject:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeSetTextAlign:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeDrawText:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeChord:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeSetMapperFlags:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeExtTextOut:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeSetDIBToDev:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeSelectPalette:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeRealizePalette:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeAnimatePalette:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeSetPalEntries:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypePolyPolygon:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeResizePalette:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeDIBBitBlt:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeDIBStretchBlt:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeDIBCreatePatternBrush:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeStretchDIB:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeExtFloodFill:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeSetLayout:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeResetDC:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeStartDoc:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeStartPage:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeEndPage:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeAbortDoc:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeEndDoc:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeDeleteObject:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeCreatePalette:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeCreateBrush:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeCreatePatternBrush:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeCreatePenIndirect:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeCreateFontIndirect:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeCreateBrushIndirect:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeCreateBitmapIndirect:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeCreateBitmap:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::WmfRecordTypeCreateRegion:
		{
		}
		break;
#pragma endregion
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeHeader:
		{
			auto pH = reinterpret_cast<const ENHMETAHEADER*>(data - sizeof(EMR));
			ss << "//ENHMETAHEADER\n";
			ss << "//rclBounds=(" << pH->rclBounds.left << "," << pH->rclBounds.top << "," << pH->rclBounds.right << "," << pH->rclBounds.bottom << ") Inclusive-inclusive bounds in device units\n";
			ss << "//rclFrame=(" << pH->rclFrame.left << "," << pH->rclFrame.top << "," << pH->rclFrame.right << "," << pH->rclFrame.bottom << ") Inclusive-inclusive Picture Frame of metafile in .01 mm units\n";
			ss << "//nVersion=" << pH->nVersion << "\n";
			ss << "//nBytes=" << pH->nBytes << " Size of the metafile in bytes\n";
			ss << "//nRecords=" << pH->nRecords << " Number of records in the metafile\n";
			ss << "//nHandles=" << pH->nHandles << " Number of handles in the handle table. Handle index zero is reserved.\n";
			ss << "//szlDevice=(" << pH->szlDevice.cx << ", " << pH->szlDevice.cy << ") Size of the reference device in pixels\n";
			ss << "//szlMillimeters=(" << pH->szlMillimeters.cx << ", " << pH->szlMillimeters.cy << ") Size of the reference device in millimeters\n";
#if(WINVER >= 0x0400)
			if (dataSize > offsetof(ENHMETAHEADER, bOpenGL))
			{
				ss << "//Has" << (pH->bOpenGL ? "" : "n't") << " OpenGL commands\n";
			}
#endif /* WINVER >= 0x0400 */
#if(WINVER >= 0x0500)
			if (dataSize > offsetof(ENHMETAHEADER, szlMicrometers))
			{
				ss << "//szlMicrometers=(" << pH->szlMicrometers.cx << ", " << pH->szlMicrometers.cy << ") Size of the reference device in micrometers\n";
			}
#endif /* WINVER >= 0x0500 */
			ss << "SetGraphicsMode(hdc, GM_ADVANCED);\n";
			ss << "\nHGDIOBJ gdiHandles[" << pH->nHandles << "] = {0};\n";
			ss << "HGDIOBJ g_stockObject = NULL;\n";
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyBezier:
		{
			Polyline<POINT>("PolyBezier", data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypePolygon:
		{
			Polyline<POINT>("Polygon", data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyline:
		{
			Polyline<POINT>("Polyline", data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyBezierTo:
		{
			Polyline<POINT>("PolyBezierTo", data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyLineTo:
		{
			Polyline<POINT>("PolylineTo", data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyPolyline:
		{
			PolyPolyline<POINT>("PolyPolyline", data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyPolygon:
		{
			PolyPolyline<POINT>("PolyPolygon", data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetWindowExtEx:
		{
			OnePoint("SetWindowExtEx", data, ss, ", nullptr");
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetWindowOrgEx:
		{
			OnePoint("SetWindowOrgEx", data, ss, ", nullptr");
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetViewportExtEx:
		{
			OnePoint("SetViewportExtEx", data, ss, ", nullptr");
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetViewportOrgEx:
		{
			OnePoint("SetViewportOrgEx", data, ss, ", nullptr");
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetBrushOrgEx:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeEOF:
		{
			ss << "// EOF\n";
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetPixelV:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetMapperFlags:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetMapMode:
		{
			OneInt("SetMapMode", data, ss, ConstantDictionary::MapMode);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetBkMode:
		{
			OneInt("SetBkMode", data, ss, ConstantDictionary::BkMode);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetPolyFillMode:
		{
			OneInt("SetPolyFillMode", data, ss, ConstantDictionary::PolyFillMode);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetROP2:
		{
			OneInt("SetROP2", data, ss, ConstantDictionary::ROP2);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetStretchBltMode:
		{
			OneInt("SetStretchBltMode", data, ss, ConstantDictionary::StretchBltMode);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetTextAlign:
		{
			OneInt("SetTextAlign", data, ss, ConstantDictionary::TextAlign);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetColorAdjustment:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetTextColor:
		{
			OneInt("SetTextColor", data, ss, ConstantDictionary::RGBColor);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetBkColor:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeOffsetClipRgn:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeMoveToEx:
		{
			OnePoint("MoveToEx", data, ss, ", nullptr");
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetMetaRgn:
		{
			NoParams("SetMetaRgn", ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeExcludeClipRect:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeIntersectClipRect:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeScaleViewportExtEx:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeScaleWindowExtEx:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSaveDC:
		{
			NoParams("SaveDC", ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeRestoreDC:
		{
			OneInt("RestoreDC", data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetWorldTransform:
		{
			SetWorldTransform(data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeModifyWorldTransform:
		{
			ModifyWorldTransform(data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSelectObject:
		{
			SelectObject(data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeCreatePen:
		{
			CreatePen(data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeCreateBrushIndirect:
		{
			CreateBrushIndirect(data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeDeleteObject:
		{
			DeleteObject(data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeAngleArc:
		{
			AngleArc("AngleArc", data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeEllipse:
		{
			TwoPoints("Ellipse", data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeRectangle:
		{
			TwoPoints("Rectangle", data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeRoundRect:
		{
			ThreePoints("RoundRect", data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeArc:
		{
			FourPoints("Arc", data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeChord:
		{
			FourPoints("Chord", data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypePie:
		{
			FourPoints("Pie", data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSelectPalette:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeCreatePalette:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetPaletteEntries:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeResizePalette:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeRealizePalette:
		{
			NoParams("RealizePalette", ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeExtFloodFill:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeLineTo:
		{
			OnePoint("LineTo", data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeArcTo:
		{
			FourPoints("ArcTo", data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyDraw:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetArcDirection:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetMiterLimit:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeBeginPath:
		{
			NoParams("BeginPath", ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeEndPath:
		{
			NoParams("EndPath", ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeCloseFigure:
		{
			NoParams("CloseFigure", ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeFillPath:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeStrokeAndFillPath:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeStrokePath:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeFlattenPath:
		{
			NoParams("FlattenPath", ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeWidenPath:
		{
			NoParams("WidenPath", ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSelectClipPath:
		{
			OneInt("SelectClipPath", data, ss, ConstantDictionary::ClipRgnMergeMode);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeAbortPath:
		{
			NoParams("AbortPath", ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeReserved_069:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeGdiComment:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeFillRgn:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeFrameRgn:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeInvertRgn:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypePaintRgn:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeExtSelectClipRgn:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeBitBlt:
		{
			BitBlt(dataSize, data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeStretchBlt:
		{
			StretchBlt(dataSize, data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeMaskBlt:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypePlgBlt:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetDIBitsToDevice:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeStretchDIBits:
		{
			StretchDIBits(dataSize, data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeExtCreateFontIndirect:
		{
			CreateFontIndirectW(data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeExtTextOutA:
		{
			ExtTextOut<char>(data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeExtTextOutW:
		{
			ExtTextOut<wchar_t>(data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyBezier16:
		{
			Polyline<ShortPoint>("PolyBezier", data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypePolygon16:
		{
			Polyline<ShortPoint>("Polygon", data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyline16:
		{
			Polyline<ShortPoint>("Polyline", data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyBezierTo16:
		{
			Polyline<ShortPoint>("PolyBezierTo", data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypePolylineTo16:
		{
			Polyline<ShortPoint>("PolylineTo", data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyPolyline16:
		{
			PolyPolyline<ShortPoint>("PolyPolyline", data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyPolygon16:
		{
			PolyPolyline<ShortPoint>("PolyPolygon", data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyDraw16:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeCreateMonoBrush:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeCreateDIBPatternBrushPt:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeExtCreatePen:
		{
			ExtCreatePen(data, ss);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyTextOutA:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyTextOutW:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetICMMode:
		{
			OneInt("SetICMMode", data, ss, ConstantDictionary::ICMMode);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeCreateColorSpace:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetColorSpace:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeDeleteColorSpace:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeGLSRecord:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeGLSBoundedRecord:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypePixelFormat:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeDrawEscape:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeExtEscape:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeStartDoc:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSmallTextOut:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeForceUFIMapping:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeNamedEscape:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeColorCorrectPalette:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetICMProfileA:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetICMProfileW:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeAlphaBlend:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetLayout:
		{
			OneInt("SetLayout", data, ss, ConstantDictionary::Layout);
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeTransparentBlt:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeReserved_117:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeGradientFill:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetLinkedUFIs:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetTextJustification:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeColorMatchToTargetW:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfRecordTypeCreateColorSpaceW:
		{
		}
		break;
#pragma region "EmfPlusRecord"
		// That is the END of the GDI EMF records.

		// Now we start the list of EMF+ records.  We leave quite
		// a bit of room here for the addition of any new GDI
		// records that may be added later.
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeInvalid:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeHeader:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeEndOfFile:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeComment:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeGetDC:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeMultiFormatStart:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeMultiFormatSection:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeMultiFormatEnd:
		{
		}
		break;

		// For all persistent objects
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeObject:
		{
		}
		break;

		// Drawing Records
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeClear:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeFillRects:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeDrawRects:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeFillPolygon:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeDrawLines:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeFillEllipse:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeDrawEllipse:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeFillPie:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeDrawPie:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeDrawArc:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeFillRegion:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeFillPath:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeDrawPath:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeFillClosedCurve:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeDrawClosedCurve:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeDrawCurve:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeDrawBeziers:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeDrawImage:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeDrawImagePoints:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeDrawString:
		{
		}
		break;

		// Graphics State Records
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeSetRenderingOrigin:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeSetAntiAliasMode:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeSetTextRenderingHint:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeSetTextContrast:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeSetInterpolationMode:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeSetPixelOffsetMode:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeSetCompositingMode:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeSetCompositingQuality:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeSave:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeRestore:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeBeginContainer:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeBeginContainerNoParams:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeEndContainer:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeSetWorldTransform:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeResetWorldTransform:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeMultiplyWorldTransform:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeTranslateWorldTransform:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeScaleWorldTransform:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeRotateWorldTransform:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeSetPageTransform:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeResetClip:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeSetClipRect:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeSetClipPath:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeSetClipRegion:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeOffsetClip:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeDrawDriverString:
		{
		}
		break;
#if (GDIPVER >= 0x0110)
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeStrokeFillPath:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeSerializableObject:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeSetTSGraphics:
		{
		}
		break;
	case Gdiplus::EmfPlusRecordType::EmfPlusRecordTypeSetTSClip:
		{
		}
		break;
#endif
#pragma endregion
	default:
		break;
	}
	ss << "//End of " << ConstantDictionary::EmfPlusRecordType(recordType) << "\n";
	return TRUE;
}
