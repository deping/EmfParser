#include <Windows.h>
#include <Gdiplus.h>

#include <functional>
#include <memory>
#include <sstream>
#include <type_traits>

#include <QAction>
#include <QCoreApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenuBar>
#include <QSplitter>
#include <QSettings>
#include <QTextEdit>
#include <QWindow>

#include "mainwindow.h"
#include "ConstantDictionary.h"
#include "ReplayWidget.h"

const char* g_geometry = "MainGeometry";
const char* g_stateKey = "SplitterState";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_splitter = new QSplitter(Qt::Vertical, this);

    m_replayWidget = new ReplayWidget;
    m_splitter->addWidget(m_replayWidget);//QWidget::createWindowContainer

    m_gdiCallsWidget = new QTextEdit;
    m_splitter->addWidget(m_gdiCallsWidget);

	QFont font;
	font.setFamily("Courier");
	font.setStyleHint(QFont::Monospace);
	font.setFixedPitch(true);
	font.setPointSize(10);
	m_gdiCallsWidget->setFont(font);
	QFontMetrics metrics(font);
	m_gdiCallsWidget->setTabStopDistance(4 * metrics.width('X')); // 4 characters

    setCentralWidget(m_splitter);

	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);

	setMinimumSize(640, 480);
	m_iniFile = QCoreApplication::applicationDirPath() + "\\EmfParser.ini";
	QSettings settings(m_iniFile, QSettings::IniFormat);
	auto geometry = settings.value(g_geometry).toByteArray();
	if (!geometry.isEmpty())
	{
		restoreGeometry(geometry);
		auto state = settings.value(g_stateKey).toByteArray();
		if (!state.isEmpty())
			m_splitter->restoreState(state);
	}

    CreateActions();
    CreateMenus();
}

MainWindow::~MainWindow()
{
	// Must reset metafile before GdiplusShutdown.
	// Or program will crash in ~ReplayWidget() because ReplayWidget::m_pMetafile is invalid at that time.
	m_replayWidget->ResetMetafile();
	Gdiplus::GdiplusShutdown(m_gdiplusToken);
}

void MainWindow::CreateActions()
{
    m_openAct = new QAction(tr("&Open Emf..."), this);
    m_openAct->setShortcuts(QKeySequence::Open);
    m_openAct->setStatusTip(tr("Open Windows Emf"));
    connect(m_openAct, &QAction::triggered, this, &MainWindow::OpenEmf);

	m_generateAct = new QAction(tr("&Generate Emf"), this);
	m_generateAct->setShortcut(QKeySequence(tr("Ctrl+G", "File|Generate Emf")));
	m_generateAct->setStatusTip(tr("Generate Windows Emf"));
    connect(m_generateAct, &QAction::triggered, this, &MainWindow::GenerateEmf);

	m_rectAct = new QAction(tr("&Specify Retangle to Play Emf..."), this);
	m_rectAct->setShortcut(QKeySequence(tr("Ctrl+S", "File|Specify Retangle to Play Emf")));
	m_rectAct->setStatusTip(tr("Specify Retangle to Play Emf"));
    connect(m_rectAct, &QAction::triggered, m_replayWidget, &ReplayWidget::SpecifyRect);

    m_aboutAct = new QAction(tr("&About"), this);
    m_aboutAct->setShortcuts(QKeySequence::Open);
    m_aboutAct->setStatusTip(tr("Show the application's About box"));
    connect(m_aboutAct, &QAction::triggered, this, &MainWindow::About);
}

void MainWindow::closeEvent(QCloseEvent * event)
{
	QSettings settings(m_iniFile, QSettings::IniFormat);
	settings.setValue(g_geometry, saveGeometry());
	settings.setValue(g_stateKey, m_splitter->saveState());
}

void MainWindow::CreateMenus()
{
    {
        QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
        fileMenu->addAction(m_openAct);
        fileMenu->addAction(m_generateAct);
        fileMenu->addAction(m_rectAct);
    }

    {
        QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
        helpMenu->addAction(m_aboutAct);
    }
}

void MainWindow::OpenEmf()
{
    QSettings settings(m_iniFile, QSettings::IniFormat);
	const char* key = "OpenPath";
    QString path = settings.value(key, "").toString();
    auto fileName = QFileDialog::getOpenFileName(this, tr("Open Emf"), path, tr("Emf Files (*.wmf *.emf)"), nullptr, QFileDialog::ReadOnly);
    if (fileName.isEmpty())
        return;
	QString dir = QFileInfo(fileName).path();
	settings.setValue(key, dir);
	this->m_gdiCallsWidget->clear();
    ParseEmf(fileName);
}

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

template<typename T>
std::string ArrayToString(const T* array, int32_t count, const char* arrayName, int indentLevel)
{
	std::stringstream ss;
	for (int i = 0; i < indentLevel; ++i)
		ss << '\t';

	if (count <= 0)
		return "// Array count = 0";

	// T arrayName[] = {
	using PT = std::remove_cv<T>::type;
	ss << typeid(OutputType<PT>::type).name() << ' ' << arrayName << " = { // sizeof(" << typeid(OutputType<PT>::type).name() << ") = " << sizeof(OutputType<PT>::type) << "\n";

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
	ss << "};";
	return ss.str();
}

void NoParams(const char* func, QTextEdit* pEdit)
{
	QString item = QString("%1(hdc);").arg(func);
	pEdit->append(item);
}

void OneInt(const char* func, const unsigned char* data, QTextEdit* pEdit, ModeConverter converter=nullptr)
{
	int32_t x = *(int32_t*)(data);
	QString item;
	if (converter)
		item = QString("%2(hdc, %1);").arg(converter(x)).arg(func);
	else
		item = QString("%2(hdc, %1);").arg(x).arg(func);
	pEdit->append(item);
}

void SelectObject(const unsigned char* data, QTextEdit* pEdit)
{
	int32_t x = *(int32_t*)(data);
	if (x & 0x80000000)
	{
		QString item = QString("g_stockObject = GetStockObject(%1);").arg(ConstantDictionary::StockObject(x & ~0x80000000));
		pEdit->append(item);
		item = QString("SelectObject(hdc, g_stockObject);");
		pEdit->append(item);
	}
	else
	{
		QString item = QString("SelectObject(hdc, gdiHandles[%1]);").arg(x);
		pEdit->append(item);
	}
}

void DeleteObject(const unsigned char* data, QTextEdit* pEdit)
{
	int32_t x = *(int32_t*)(data);
	QString item = QString("DeleteObject(gdiHandles[%1]);").arg(x);
	pEdit->append(item);
}

void CreatePen(const unsigned char* data, QTextEdit* pEdit)
{
	int32_t elements[5];
	for (int i = 0; i < 5; ++i)
		elements[i] = *((int32_t*)data + i);
	QString item = QString("gdiHandles[%1] = CreatePen(hdc, %2, %3, RGB(%4, %5, %6));").arg(elements[0]).arg(ConstantDictionary::PenStyle(elements[1])).arg(elements[2]).arg(GetRValue(elements[4])).arg(GetGValue(elements[4])).arg(GetBValue(elements[4]));
	pEdit->append(item);
}

void OnePoint(const char* func, const unsigned char* data, QTextEdit* pEdit, const char* suffix="")
{
	int32_t x = *(int32_t*)(data);
	int32_t y = *(int32_t*)(data + sizeof(int32_t));
	QString item = QString("%3(hdc, %1, %2%4);").arg(x).arg(y).arg(func).arg(suffix);
	pEdit->append(item);
}

void TwoPoints(const char* func, const unsigned char* data, QTextEdit* pEdit)
{
	int32_t elements[4];
	for (int i = 0; i < 4; ++i)
		elements[i] = *((int32_t*)data + i);
	QString item = QString("%5(hdc, %1, %2, %3, %4);").arg(elements[0]).arg(elements[1]).arg(elements[2]).arg(elements[3]).arg(func);
	pEdit->append(item);
}

void ThreePoints(const char* func, const unsigned char* data, QTextEdit* pEdit)
{
	int32_t elements[6];
	for (int i = 0; i < 6; ++i)
		elements[i] = *((int32_t*)data + i);
	QString item = QString("%7(hdc, %1, %2, %3, %4, %5, %6);").arg(elements[0]).arg(elements[1]).arg(elements[2]).arg(elements[3]).arg(elements[4]).arg(elements[5]).arg(func);
	pEdit->append(item);
}

void FourPoints(const char* func, const unsigned char* data, QTextEdit* pEdit)
{
	int32_t elements[8];
	for (int i = 0; i < 8; ++i)
		elements[i] = *((int32_t*)data + i);
	QString item = QString("%9(hdc, %1, %2, %3, %4, %5, %6, %7, %8);").arg(elements[0]).arg(elements[1]).arg(elements[2]).arg(elements[3]).arg(elements[4]).arg(elements[5]).arg(elements[6]).arg(elements[7]).arg(func);
	pEdit->append(item);
}

void AngleArc(const char* func, const unsigned char* data, QTextEdit* pEdit)
{
	int32_t x, y;
	DWORD r;
	float StartAngle;
	float SweepAngle;
	x = *(int32_t*)(data);
	y = *(int32_t*)(data + sizeof(int32_t));
	r = *(int32_t*)(data + sizeof(int32_t) + sizeof(int32_t));
	StartAngle = *(int32_t*)(data + sizeof(int32_t) + sizeof(int32_t) + sizeof(DWORD));
	SweepAngle = *(int32_t*)(data + sizeof(int32_t) + sizeof(int32_t) + sizeof(DWORD) + sizeof(float));
	QString item = QString("%6(hdc, %1, %2, %3, %4, %5);").arg(x).arg(y).arg(r).arg(StartAngle).arg(SweepAngle).arg(func);
	pEdit->append(item);
}

template<typename PointType>
void Polyline(const char* func, const unsigned char* data, QTextEdit* pEdit)
{
	int32_t count = *(int32_t*)(data + 4 * sizeof(int32_t));
	QString item("{");
	pEdit->append(item);
	auto points = reinterpret_cast<const PointType*>(data + 5 * sizeof(int32_t));
	auto ptsText = ArrayToString(points, count, "points", 1);
	pEdit->append(ptsText.c_str());
	item = QString("\t%2(hdc, points, %1);").arg(count).arg(func);
	pEdit->append(item);
	item = ("}");
	pEdit->append(item);
}

template<typename PointType>
void PolyPolyline(const char* func, const unsigned char* data, QTextEdit* pEdit)
{
	data += 4 * sizeof(int32_t); // skip Bounds
	int32_t numberOfPolylines = *(int32_t*)(data);
	int32_t numberOfPoints = *(int32_t*)(data + sizeof(int32_t));
	data += 2 * sizeof(int32_t);

	QString item("{");
	pEdit->append(item);
	const uint32_t* polys = reinterpret_cast<const uint32_t*>(data);
	auto polysText = ArrayToString(polys, numberOfPolylines, "polys", 1);
	pEdit->append(polysText.c_str());

	auto points = reinterpret_cast<const PointType*>(data + numberOfPolylines * sizeof(uint32_t));
	auto ptsText = ArrayToString(points, numberOfPoints, "points", 1);
	pEdit->append(ptsText.c_str());
	item = QString("%2(hdc, points, polys, %1);").arg(numberOfPolylines).arg(func);
	pEdit->append(item);
	item = ("}");
	pEdit->append(item);
}

void AppendBMIText(const BITMAPINFO* pBmi, QTextEdit* pEdit)
{
	auto pBH = &pBmi->bmiHeader;
	QString bmiText = QString(R"(	BITMAPINFO bmi = {
		{
			%1, // biSize
			%2, // biWidth
			%3, // biHeight
			%4, // biPlanes
			%5, // biBitCount
			%6, // biCompression
			%7, // biSizeImage
			%8, // biXPelsPerMeter
			%9, // biYPelsPerMeter
			%10, // biClrUsed
			%11 // biClrImportant
		}
	})")
		.arg(pBH->biSize)
		.arg(pBH->biWidth)
		.arg(pBH->biHeight)
		.arg(pBH->biPlanes)
		.arg(pBH->biBitCount)
		.arg(pBH->biCompression)
		.arg(pBH->biSizeImage)
		.arg(pBH->biXPelsPerMeter)
		.arg(pBH->biYPelsPerMeter)
		.arg(pBH->biClrUsed)
		.arg(pBH->biClrImportant);
	pEdit->append(bmiText);
}

void AppendBits(const char* pBits, int byteCount, int height, QTextEdit* pEdit)
{
	std::stringstream ss;
	ss << "\tconst char bits[] = {\n";
	ss << std::hex;
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
	ss << "\t};";
	pEdit->append(ss.str().c_str());
}

void BitBlt(unsigned int dataSize, const unsigned char* data, QTextEdit* pEdit)
{
	auto pEmrBitBlt = reinterpret_cast<const EMRBITBLT*>(data - sizeof(EMR));
	auto pBmi = reinterpret_cast<const BITMAPINFO*>((const char*)pEmrBitBlt + pEmrBitBlt->offBmiSrc);
	auto pBH = &pBmi->bmiHeader;
	auto pBits = (const char*)pEmrBitBlt + pEmrBitBlt->offBitsSrc;
	QString item("{");
	pEdit->append(item);
	AppendBits(pBits, pEmrBitBlt->cbBitsSrc, pBH->biHeight, pEdit);
	AppendBMIText(pBmi, pEdit);
	QString bitBlttext(R"(	HBITMAP hBitmap;
	HDC hMemDC;
	hBitmap = CreateCompatibleBitmap(hdc, pBH->biWidth, pBH->biHeight);
	hMemDC = CreateCompatibleDC(hdc);
	SetDIBits(hdc, hBitmap, 0, pBH->biHeight, bits, &bmi, pEmrBitBlt->iUsageSrc);
	HGDIOBJ holdBmp = SelectObject(hMemDC, hBitmap);
	BitBlt(hdc, %1, %2, %3, %4, hMemDC, %5, %6, %7);
	DeleteObject(SelectObject(hMemDC, holdBmp));
	DeleteDC(hMemDC);)");
	bitBlttext = bitBlttext.arg(pEmrBitBlt->xDest).arg(pEmrBitBlt->yDest).arg(pEmrBitBlt->cxDest).arg(pEmrBitBlt->cyDest)
		.arg(pEmrBitBlt->xSrc).arg(pEmrBitBlt->ySrc).arg(ConstantDictionary::ROP3(pEmrBitBlt->dwRop));
	pEdit->append(bitBlttext);
	item = ("}");
	pEdit->append(item);
}

void StretchBlt(unsigned int dataSize, const unsigned char* data, QTextEdit* pEdit)
{
	auto pEmrStretchBlt = reinterpret_cast<const EMRSTRETCHBLT*>(data - sizeof(EMR));
	auto pBmi = reinterpret_cast<const BITMAPINFO*>((const char*)pEmrStretchBlt + pEmrStretchBlt->offBmiSrc);
	auto pBH = &pBmi->bmiHeader;
	auto pBits = (const char*)pEmrStretchBlt + pEmrStretchBlt->offBitsSrc;
	QString item("{");
	pEdit->append(item);
	AppendBits(pBits, pEmrStretchBlt->cbBitsSrc, pBH->biHeight, pEdit);
	AppendBMIText(pBmi, pEdit);
	QString stretchBlttext(R"(	HBITMAP hBitmap;
	HDC hMemDC;
	hBitmap = CreateCompatibleBitmap(hdc, pBH->biWidth, pBH->biHeight);
	hMemDC = CreateCompatibleDC(hdc);
	SetDIBits(hdc, hBitmap, 0, pBH->biHeight, bits, &bmi, pEmrStretchBlt->iUsageSrc);
	HGDIOBJ holdBmp = SelectObject(hMemDC, hBitmap);
	StretchBlt(hdc, %1, %2, %3, %4, hMemDC, %5, %6, %7, %8, %9);
	DeleteObject(SelectObject(hMemDC, holdBmp));
	DeleteDC(hMemDC);)");
	stretchBlttext = stretchBlttext.arg(pEmrStretchBlt->xDest).arg(pEmrStretchBlt->yDest).arg(pEmrStretchBlt->cxDest).arg(pEmrStretchBlt->cyDest)
		.arg(pEmrStretchBlt->xSrc).arg(pEmrStretchBlt->ySrc).arg(pEmrStretchBlt->cxSrc).arg(pEmrStretchBlt->cySrc).arg(ConstantDictionary::ROP3(pEmrStretchBlt->dwRop));
	pEdit->append(stretchBlttext);
	item = ("}");
	pEdit->append(item);
}

void StretchDIBits(unsigned int dataSize, const unsigned char* data, QTextEdit* pEdit)
{
	auto pEmrStretchDIBits = reinterpret_cast<const EMRSTRETCHDIBITS*>(data - sizeof(EMR));
	auto pBmi = reinterpret_cast<const BITMAPINFO*>((const char*)pEmrStretchDIBits + pEmrStretchDIBits->offBmiSrc);
	auto pBH = &pBmi->bmiHeader;
	auto pBits = (const char*)pEmrStretchDIBits + pEmrStretchDIBits->offBitsSrc;
	QString item("{");
	pEdit->append(item);
	AppendBits(pBits, pEmrStretchDIBits->cbBitsSrc, pBH->biHeight, pEdit);
	AppendBMIText(pBmi, pEdit);
	QString stretchDIBitstext(R"(	StretchDIBits(hdc, %1, %2, %3, %4, %5, %6, %7, %8, &bits, &bmi, %9, %10);)");
	stretchDIBitstext = stretchDIBitstext.arg(pEmrStretchDIBits->xDest).arg(pEmrStretchDIBits->yDest).arg(pEmrStretchDIBits->cxDest).arg(pEmrStretchDIBits->cyDest)
		.arg(pEmrStretchDIBits->xSrc).arg(pEmrStretchDIBits->ySrc).arg(pEmrStretchDIBits->cxSrc).arg(pEmrStretchDIBits->cySrc).arg(ConstantDictionary::ColorTableUsage(pEmrStretchDIBits->iUsageSrc)).arg(ConstantDictionary::ROP3(pEmrStretchDIBits->dwRop));
	pEdit->append(stretchDIBitstext);
	item = ("}");
	pEdit->append(item);
}

inline void AppendXForm(const unsigned char* data, QTextEdit* pEdit)
{
	auto pf = reinterpret_cast<const XFORM*>(data);
	QString item = QString("\tXFORM xf = {%1, %2, %3, %4, %5, %6};").arg(pf->eM11).arg(pf->eM12).arg(pf->eM21).arg(pf->eM22).arg(pf->eDx).arg(pf->eDy);
	pEdit->append(item);
}

void SetWorldTransform(const unsigned char* data, QTextEdit* pEdit)
{
	QString item("{");
	pEdit->append(item);
	AppendXForm(data, pEdit);
	item = QString("\tSetWorldTransform(hdc, &xf);");
	pEdit->append(item);
	item = ("}");
	pEdit->append(item);
}

void ModifyWorldTransform(const unsigned char* data, QTextEdit* pEdit)
{
	QString item("{");
	pEdit->append(item);
	AppendXForm(data, pEdit);
	int32_t mode = *(int32_t*)(data + sizeof(XFORM));
	item = QString("\tModifyWorldTransform(hdc, &xf, %1);").arg(ConstantDictionary::WorldTransform(mode));
	pEdit->append(item);
	item = ("}");
	pEdit->append(item);
}

BOOL CALLBACK MainWindow::EnumMetafileCallback(
   Gdiplus::EmfPlusRecordType recordType,
   unsigned int flags,
   unsigned int dataSize,
   const unsigned char* data,
   void* callbackData)
{
    using namespace Gdiplus;

    MainWindow* pThis = reinterpret_cast<MainWindow*>(callbackData);
    switch(recordType)
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
			pThis->m_gdiCallsWidget->append("//ENHMETAHEADER");
			pThis->m_gdiCallsWidget->append(QString("//rclBounds=(%1,%2,%3,%4) Inclusive-inclusive bounds in device units").arg(pH->rclBounds.left).arg(pH->rclBounds.top).arg(pH->rclBounds.right).arg(pH->rclBounds.bottom));
			pThis->m_gdiCallsWidget->append(QString("//rclFrame=(%1,%2,%3,%4) Inclusive-inclusive Picture Frame of metafile in .01 mm units").arg(pH->rclFrame.left).arg(pH->rclFrame.top).arg(pH->rclFrame.right).arg(pH->rclFrame.bottom));
			pThis->m_gdiCallsWidget->append(QString("//nVersion=%1").arg(pH->nVersion));
			pThis->m_gdiCallsWidget->append(QString("//nBytes=%1 Size of the metafile in bytes").arg(pH->nBytes));
			pThis->m_gdiCallsWidget->append(QString("//nRecords=%1 Number of records in the metafile").arg(pH->nRecords));
			pThis->m_gdiCallsWidget->append(QString("//nHandles=%1 Number of handles in the handle table. Handle index zero is reserved.").arg(pH->nHandles));
			pThis->m_gdiCallsWidget->append(QString("//szlDevice=(%1, %2) Size of the reference device in pixels").arg(pH->szlDevice.cx).arg(pH->szlDevice.cy));
			pThis->m_gdiCallsWidget->append(QString("//szlMillimeters=(%1, %2) Size of the reference device in millimeters").arg(pH->szlMillimeters.cx).arg(pH->szlMillimeters.cy));
#if(WINVER >= 0x0400)
			if (dataSize > offsetof(ENHMETAHEADER, bOpenGL))
			{
				pThis->m_gdiCallsWidget->append(QString("//Has%1 OpenGL commands").arg(pH->bOpenGL?"":"n't"));
			}
#endif /* WINVER >= 0x0400 */
#if(WINVER >= 0x0500)
			if (dataSize > offsetof(ENHMETAHEADER, szlMicrometers))
			{
				pThis->m_gdiCallsWidget->append(QString("//szlMicrometers=(%1, %2) Size of the reference device in micrometers").arg(pH->szlMicrometers.cx).arg(pH->szlMicrometers.cy));
			}
#endif /* WINVER >= 0x0500 */
			pThis->m_gdiCallsWidget->append(QString("\nHGDIOBJ gdiHandles[%1] = {0};").arg(pH->nHandles));
			pThis->m_gdiCallsWidget->append(QString("HGDIOBJ g_stockObject = NULL;\n").arg(pH->nHandles));
		}
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyBezier:
        {
 			Polyline<POINT>("PolyBezier", data, pThis->m_gdiCallsWidget);
       }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypePolygon:
        {
			Polyline<POINT>("Polygon", data, pThis->m_gdiCallsWidget);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyline:
        {
			Polyline<POINT>("Polyline", data, pThis->m_gdiCallsWidget);
		}
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyBezierTo:
        {
 			Polyline<POINT>("PolyBezierTo", data, pThis->m_gdiCallsWidget);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyLineTo:
        {
			Polyline<POINT>("PolylineTo", data, pThis->m_gdiCallsWidget);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyPolyline:
        {
			PolyPolyline<POINT>("PolyPolyline", data, pThis->m_gdiCallsWidget);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyPolygon:
        {
			PolyPolyline<POINT>("PolyPolygon", data, pThis->m_gdiCallsWidget);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetWindowExtEx:
        {
			OnePoint("SetWindowExtEx", data, pThis->m_gdiCallsWidget, ", nullptr");
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetWindowOrgEx:
        {
			OnePoint("SetWindowOrgEx", data, pThis->m_gdiCallsWidget, ", nullptr");
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetViewportExtEx:
        {
			OnePoint("SetViewportExtEx", data, pThis->m_gdiCallsWidget, ", nullptr");
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetViewportOrgEx:
        {
			OnePoint("SetViewportOrgEx", data, pThis->m_gdiCallsWidget, ", nullptr");
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetBrushOrgEx:
        {
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeEOF:
        {
			pThis->m_gdiCallsWidget->append("// EOF");
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
			OneInt("SetMapMode", data, pThis->m_gdiCallsWidget, ConstantDictionary::MapMode);
		}
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetBkMode:
        {
			OneInt("SetBkMode", data, pThis->m_gdiCallsWidget, ConstantDictionary::BkMode);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetPolyFillMode:
        {
			OneInt("SetPolyFillMode", data, pThis->m_gdiCallsWidget, ConstantDictionary::PolyFillMode);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetROP2:
        {
			OneInt("SetROP2", data, pThis->m_gdiCallsWidget, ConstantDictionary::ROP2);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetStretchBltMode:
        {
			OneInt("SetStretchBltMode", data, pThis->m_gdiCallsWidget, ConstantDictionary::StretchBltMode);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetTextAlign:
        {
			OneInt("SetTextAlign", data, pThis->m_gdiCallsWidget, ConstantDictionary::TextAlign);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetColorAdjustment:
        {
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetTextColor:
        {
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
			OnePoint("MoveToEx", data, pThis->m_gdiCallsWidget, ", nullptr");
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetMetaRgn:
        {
			NoParams("SetMetaRgn", pThis->m_gdiCallsWidget);
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
			NoParams("SaveDC", pThis->m_gdiCallsWidget);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeRestoreDC:
        {
			OneInt("RestoreDC", data, pThis->m_gdiCallsWidget);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeSetWorldTransform:
        {
			SetWorldTransform(data, pThis->m_gdiCallsWidget);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeModifyWorldTransform:
        {
			ModifyWorldTransform(data, pThis->m_gdiCallsWidget);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeSelectObject:
        {
			SelectObject(data, pThis->m_gdiCallsWidget);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeCreatePen:
        {
			CreatePen(data, pThis->m_gdiCallsWidget);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeCreateBrushIndirect:
        {
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeDeleteObject:
        {
			DeleteObject(data, pThis->m_gdiCallsWidget);
		}
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeAngleArc:
        {
			AngleArc("AngleArc", data, pThis->m_gdiCallsWidget);
		}
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeEllipse:
        {
			TwoPoints("Ellipse", data, pThis->m_gdiCallsWidget);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeRectangle:
        {
			TwoPoints("Rectangle", data, pThis->m_gdiCallsWidget);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeRoundRect:
        {
			ThreePoints("RoundRect", data, pThis->m_gdiCallsWidget);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeArc:
        {
			FourPoints("Arc", data, pThis->m_gdiCallsWidget);
		}
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeChord:
        {
  			FourPoints("Chord", data, pThis->m_gdiCallsWidget);
       }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypePie:
        {
 			FourPoints("Pie", data, pThis->m_gdiCallsWidget);
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
			NoParams("RealizePalette", pThis->m_gdiCallsWidget);
		}
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeExtFloodFill:
        {
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeLineTo:
        {
			OnePoint("LineTo", data, pThis->m_gdiCallsWidget);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeArcTo:
        {
			FourPoints("ArcTo", data, pThis->m_gdiCallsWidget);
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
			NoParams("BeginPath", pThis->m_gdiCallsWidget);
		}
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeEndPath:
        {
			NoParams("EndPath", pThis->m_gdiCallsWidget);
		}
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeCloseFigure:
        {
			NoParams("CloseFigure", pThis->m_gdiCallsWidget);
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
			NoParams("FlattenPath", pThis->m_gdiCallsWidget);
		}
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeWidenPath:
        {
			NoParams("WidenPath", pThis->m_gdiCallsWidget);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeSelectClipPath:
        {
			OneInt("SelectClipPath", data, pThis->m_gdiCallsWidget, ConstantDictionary::ClipRgnMergeMode);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeAbortPath:
        {
			NoParams("AbortPath", pThis->m_gdiCallsWidget);
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
			BitBlt(dataSize, data, pThis->m_gdiCallsWidget);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeStretchBlt:
        {
			StretchBlt(dataSize, data, pThis->m_gdiCallsWidget);
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
			StretchDIBits(dataSize, data, pThis->m_gdiCallsWidget);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeExtCreateFontIndirect:
        {
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeExtTextOutA:
        {
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypeExtTextOutW:
        {
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyBezier16:
        {
			Polyline<ShortPoint>("PolyBezier", data, pThis->m_gdiCallsWidget);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypePolygon16:
        {
			Polyline<ShortPoint>("Polygon", data, pThis->m_gdiCallsWidget);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyline16:
        {
			Polyline<ShortPoint>("Polyline", data, pThis->m_gdiCallsWidget);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyBezierTo16:
        {
			Polyline<ShortPoint>("PolyBezierTo", data, pThis->m_gdiCallsWidget);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypePolylineTo16:
        {
			Polyline<ShortPoint>("PolylineTo", data, pThis->m_gdiCallsWidget);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyPolyline16:
        {
			PolyPolyline<ShortPoint>("PolyPolyline", data, pThis->m_gdiCallsWidget);
        }
        break;
    case Gdiplus::EmfPlusRecordType::EmfRecordTypePolyPolygon16:
        {
			PolyPolyline<ShortPoint>("PolyPolygon", data, pThis->m_gdiCallsWidget);
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
			OneInt("SetICMMode", data, pThis->m_gdiCallsWidget, ConstantDictionary::ICMMode);
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
			OneInt("SetLayout", data, pThis->m_gdiCallsWidget, ConstantDictionary::Layout);
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
	QString item("//End of %1");
	pThis->m_gdiCallsWidget->append(item.arg(ConstantDictionary::EmfPlusRecordType(recordType)).arg(recordType));
    return TRUE;
}

void MainWindow::ParseEmf(const QString& fileName)
{
	HWND hwnd = (HWND)m_replayWidget->winId();
	HDC hdc = GetDC(hwnd);
	{
		Gdiplus::Graphics graphics(hdc);
		std::shared_ptr<Gdiplus::Metafile> pMeta(new Gdiplus::Metafile((wchar_t*)fileName.utf16()), Gdiplus::Metafile::operator delete);
		m_replayWidget->SetMetafile(pMeta);
		graphics.EnumerateMetafile(pMeta.get(), Gdiplus::Rect(0, 0, 300, 50), EnumMetafileCallback, this, nullptr);
	}
	ReleaseDC(hwnd, hdc);
}

void MainWindow::GenerateEmf()
{
	HWND hwnd = (HWND)m_replayWidget->winId();
	HDC hdc = GetDC(hwnd);
	HDC metafileDC = CreateEnhMetaFileA(hdc, "xxx.emf", nullptr, nullptr);

	// Draw
	SetGraphicsMode(metafileDC, GM_ADVANCED);
	XFORM xf = {2, 0, 0, 2, 0, 0};
	SetWorldTransform(metafileDC, &xf);
	MoveToEx(metafileDC, 10, 10, NULL);
	LineTo(metafileDC, 100, 100);
	MoveToEx(metafileDC, 100, 10, NULL);
	LineTo(metafileDC, 10, 100);
	SetMapMode(metafileDC, MM_ANISOTROPIC);
	SetWindowExtEx(metafileDC, 1, 1, NULL);
	SetViewportExtEx(metafileDC, 2, 2, NULL);
	SelectObject(metafileDC, GetStockObject(NULL_BRUSH));
	Ellipse(metafileDC, 10, 10, 100, 100);

	POINT points[7] = { {20, 50},
	{180, 50},
	{180, 20},
	{230, 70},
	{180, 120},
	{180, 90},
	{20, 90} };

	Polyline(metafileDC, points, 7);

	DWORD  ptNums[] = { 4, 4, 7 };

	// Left Triangle
	POINT Pt[15];
	Pt[0] = Pt[3] = { 50, 20 };
	Pt[1] = {20, 60};
	Pt[2] = {80, 60};

	// Second Triangle
	Pt[4] = Pt[7] = {70, 20};
	Pt[5] = {100, 60};
	Pt[6] = {130, 20};

	// Hexagon
	Pt[8] = Pt[14] = {145, 20};
	Pt[9] = {130, 40};
	Pt[10] = {145, 60};
	Pt[11] = {165, 60};
	Pt[12] = {180, 40};
	Pt[13] = {165, 20};

	PolyPolyline(metafileDC, Pt, ptNums, 3);

	HENHMETAFILE metafile = CloseEnhMetaFile(metafileDC);
	DeleteEnhMetaFile(metafile);
	ReleaseDC(hwnd, hdc);
}

void MainWindow::About()
{
    QMessageBox::about(this, tr("Windows EMF Parser"),
            tr("Author: Deping Chen\n"
			   "Email: cdp97531@sina.com\n"
               "Version 1.0"));
}
