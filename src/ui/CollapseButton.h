// from https://stackoverflow.com/a/68141638/5339857

#include <QToolButton>
#include <QApplication>
#include <QDebug>

#include "plugin-support.h"

#pragma once

class CollapseButton : public QToolButton {
public:
	CollapseButton(QWidget *parent) : QToolButton(parent), content_(nullptr)
	{
		setCheckable(true);
		setStyleSheet("background:none");
		setIconSize(QSize(8, 8));
		setFont(QApplication::font());
		setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		setArrowType(Qt::ArrowType::RightArrow);
		connect(this, &QToolButton::toggled, [=](bool checked) {
			setArrowType(checked ? Qt::ArrowType::DownArrow
					     : Qt::ArrowType::RightArrow);
			content_ != nullptr &&checked ? showContent() : hideContent();
		});
	}

	void setText(const QString &text) { QToolButton::setText(" " + text); }

	void setContent(QWidget *content, QWidget *parent)
	{
		assert(content != nullptr);
		content_ = content;
		auto animation_ = new QPropertyAnimation(
			content_, "maximumHeight"); // QObject with auto delete
		animation_->setStartValue(0);
		animation_->setEasingCurve(QEasingCurve::InOutQuad);
		animation_->setDuration(300);
		// log content->geometry().height()
		animation_->setEndValue(content->geometry().height() + 10);
		connect(animation_, &QPropertyAnimation::finished, [=]() { parent->adjustSize(); });
		animator_.addAnimation(animation_);
		if (!isChecked()) {
			content->setMaximumHeight(0);
		}
	}

	void hideContent()
	{
		animator_.setDirection(QAbstractAnimation::Backward);
		animator_.start();
	}

	void showContent()
	{
		animator_.setDirection(QAbstractAnimation::Forward);
		animator_.start();
	}

private:
	QWidget *content_;
	QParallelAnimationGroup animator_;
};
