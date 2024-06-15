#ifndef INPUTSDIALOG_H
#define INPUTSDIALOG_H

#include <QDialog>
#include <QStandardItemModel>

#include "mapping-data.h"

namespace Ui {
class InputsDialog;
}

class InputsDialog : public QDialog {
	Q_OBJECT

public:
	explicit InputsDialog(QWidget *parent = nullptr);
	~InputsDialog();

	InputsDialog(const InputsDialog &) = delete;
	InputsDialog &operator=(const InputsDialog &) = delete;

	inputs_data getInputsDataFromUI();

	// populate the listWidget with the inputs_data
	void setInputsData(const inputs_data &data);

private:
	Ui::InputsDialog *ui;

public slots:
	void addInput();
	void removeInput();
};

#endif // INPUTSDIALOG_H
