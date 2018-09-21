#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

typedef int BOOL;
typedef unsigned __int64 ULONG_PTR, *PULONG_PTR;
namespace Gdiplus
{
	enum EmfPlusRecordType;
}

class QSplitter;
class QTextEdit;
class ReplayWidget;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void CreateMenus();
    void CreateActions();

protected:
	virtual void closeEvent(QCloseEvent *event) override;

private:
    QAction* m_openAct;
    QAction* m_generateAct;
    QAction* m_rectAct;
    //QAction* m_saveAct;
    QAction* m_aboutAct;

	QSplitter* m_splitter;
	ReplayWidget* m_replayWidget;
    QTextEdit* m_gdiCallsWidget;

	ULONG_PTR m_gdiplusToken;
	QString m_iniFile;

    void ParseEmf(const QString& fileName);
	static BOOL __stdcall EnumMetafileCallback(
		Gdiplus::EmfPlusRecordType recordType,
		unsigned int flags,
		unsigned int dataSize,
		const unsigned char* pStr,
		void* callbackData);

private slots:
    void OpenEmf();
	void GenerateEmf();
    void About();

};

#endif // MAINWINDOW_H
