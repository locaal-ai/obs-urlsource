#ifndef INPUT_WIDGET_H
#define INPUT_WIDGET_H

#include <QWidget>

#include "mapping-data.h"

namespace Ui {
class InputWidget;
}

class InputWidget : public QWidget {
	Q_OBJECT

public:
	explicit InputWidget(QWidget *parent = nullptr);
	~InputWidget();

	InputWidget(const InputWidget &) = delete;
	InputWidget &operator=(const InputWidget &) = delete;

	void setInputData(const input_data &data);
	input_data getInputDataFromUI();

private:
	Ui::InputWidget *ui;
};

#endif // INPUT_WIDGET_H
