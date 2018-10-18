/***************************************************************************
* Copyright (C) 2017, Deping Chen, cdp97531@sina.com
*
* All rights reserved.
* For permission requests, write to the author.
*
* This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
* KIND, either express or implied.
***************************************************************************/
#pragma once

#include <memory>

#include <QDialog>
#include <QWidget>

namespace Gdiplus
{
	class Metafile;
}
class QCheckBox;
class QLineEdit;
class ReplayWidget : public QWidget
{
	Q_OBJECT

public:
	ReplayWidget();
	~ReplayWidget();
	void SetMetafile(const std::shared_ptr<Gdiplus::Metafile>& pMetafile);
	void ResetMetafile();

public slots:
	void SpecifyRect();

protected:
	typedef QWidget base;
	virtual QPaintEngine * paintEngine() const override;
	virtual bool event(QEvent * event) override;
	void paint();

	std::shared_ptr<Gdiplus::Metafile> m_pMetafile;
	bool m_useRect;
	int m_x, m_y, m_w, m_h;
	friend class RectWidget;
};

class RectWidget : public QDialog
{
	Q_OBJECT

public:
	RectWidget();
	QCheckBox* m_useRect;
	QLineEdit* m_x, *m_y, *m_w, *m_h;
};

