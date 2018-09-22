#include <Windows.h>
#include <Gdiplus.h>

#include <sstream>

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

extern BOOL CALLBACK EnumMetafileCallback(
	Gdiplus::EmfPlusRecordType recordType,
	unsigned int flags,
	unsigned int dataSize,
	const unsigned char* data,
	void* callbackData);
void MainWindow::ParseEmf(const QString& fileName)
{
	HWND hwnd = (HWND)m_replayWidget->winId();
	HDC hdc = GetDC(hwnd);
	{
		Gdiplus::Graphics graphics(hdc);
		std::shared_ptr<Gdiplus::Metafile> pMeta(new Gdiplus::Metafile((wchar_t*)fileName.utf16()), Gdiplus::Metafile::operator delete);
		m_replayWidget->SetMetafile(pMeta);
		std::stringstream ss;
		graphics.EnumerateMetafile(pMeta.get(), Gdiplus::Rect(0, 0, 300, 50), EnumMetafileCallback, &ss, nullptr);
		m_gdiCallsWidget->append(ss.str().c_str());
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
