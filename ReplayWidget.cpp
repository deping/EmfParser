#include <Windows.h>
#include <Gdiplus.h>

#include <QCheckBox>
#include <QEvent>
#include <QFormLayout>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>

#include "ReplayWidget.h"


ReplayWidget::ReplayWidget()
	: m_pMetafile(nullptr)
	, m_useRect(false)
	, m_x(), m_y(), m_w(100), m_h(100)
{
	setAutoFillBackground(false);
	//setAttribute(Qt::WA_NativeWindow);
	setAttribute(Qt::WA_PaintOnScreen);
}


ReplayWidget::~ReplayWidget()
{
}

void ReplayWidget::SetMetafile(const std::shared_ptr<Gdiplus::Metafile>& pMetafile)
{
	m_pMetafile = pMetafile;
}

void ReplayWidget::ResetMetafile()
{
	m_pMetafile.reset();
}

void ReplayWidget::SpecifyRect()
{
	RectWidget rw;
	rw.m_useRect->setChecked(m_useRect);
	rw.m_x->setText(QString::number(m_x));
	rw.m_y->setText(QString::number(m_y));
	rw.m_w->setText(QString::number(m_w));
	rw.m_h->setText(QString::number(m_h));
	int result = rw.exec();
	if (result == QDialog::Accepted)
	{
		m_useRect = rw.m_useRect->isChecked();
		m_x = rw.m_x->text().toInt();
		m_y = rw.m_y->text().toInt();
		m_w = rw.m_w->text().toInt();
		m_h = rw.m_h->text().toInt();
	}
}

QPaintEngine * ReplayWidget::paintEngine() const
{
	return nullptr;
}

bool ReplayWidget::event(QEvent * event) {
	//if (event->type() == QEvent::Paint) {
	//	bool result = base::event(event);
	//	paint();
	//	return result;
	//}
	if (event->type() == QEvent::UpdateRequest) {
		bool result = base::event(event);
		paint();
		return result;
	}
	return base::event(event);
}

void ReplayWidget::paint()
{
	HWND hwnd = (HWND)winId();
	HDC hdc = GetDC(hwnd);
	HGDIOBJ x;
	for (int i = 0; i < 19; ++i)
		x = GetStockObject(i);
	{
		Gdiplus::Graphics g(hdc);
		auto blueBrush = new Gdiplus::SolidBrush(Gdiplus::Color::Gray);
		RECT rect;
		GetClientRect((HWND)hwnd, &rect);
		g.FillRectangle(blueBrush, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
		delete blueBrush; blueBrush = nullptr;
		if (m_pMetafile)
		{
			Gdiplus::MetafileHeader header;
			m_pMetafile->GetMetafileHeader(&header);

			if (m_useRect)
				g.DrawImage(m_pMetafile.get(), m_x, m_y, m_w, m_h);
			else
				g.DrawImage(m_pMetafile.get(), header.X, header.Y);

			Gdiplus::Pen pen(Gdiplus::Color::HotPink);
			pen.SetDashStyle(Gdiplus::DashStyle::DashStyleDot);
			if (m_useRect)
				g.DrawRectangle(&pen, m_x, m_y, m_w, m_h);
			else
				g.DrawRectangle(&pen, header.X, header.Y, header.Width, header.Height);
		}
		//auto redPen = new Gdiplus::Pen(Gdiplus::Color::Red, 2);
		//g.DrawArc(redPen, 10, 10, 50, 50, 0, 360);
	}
	ReleaseDC(hwnd, hdc);
}

RectWidget::RectWidget()
{
	auto fl = new QFormLayout;
	m_useRect = new QCheckBox;
	fl->addRow(tr("Use Rect"), m_useRect);
	m_x = new QLineEdit;
	fl->addRow("X", m_x);
	m_y = new QLineEdit;
	fl->addRow("Y", m_y);
	m_w = new QLineEdit;
	fl->addRow(tr("Width"), m_w);
	m_h = new QLineEdit;
	fl->addRow(tr("Height"), m_h);

	auto hl = new QHBoxLayout;
	auto okBtn = new QPushButton(tr("OK"));
	connect(okBtn, SIGNAL(clicked()), this, SLOT(accept()));
	hl->addWidget(okBtn);
	auto cancelBtn = new QPushButton(tr("Cancel"));
	connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
	hl->addWidget(cancelBtn);

	auto vl = new QVBoxLayout;
	setLayout(vl);
	vl->addLayout(fl);
	vl->addLayout(hl);
}
