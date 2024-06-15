#include "InputsDialog.h"

#include "plugin-support.h"
#include "ui_inputsdialog.h"
#include "mapping-data.h"
#include "obs-ui-utils.h"
#include "request-data.h"
#include "InputWidget.h"

#include <QComboBox>
#include <QListWidgetItem>

InputsDialog::InputsDialog(QWidget *parent) : QDialog(parent), ui(new Ui::InputsDialog)
{
	ui->setupUi(this);

	connect(ui->toolButton_addinput, &QToolButton::clicked, this, &InputsDialog::addInput);
	connect(ui->toolButton_removeinput, &QToolButton::clicked, this,
		&InputsDialog::removeInput);
}

InputsDialog::~InputsDialog()
{
	delete ui;
}

void InputsDialog::addInput()
{
	// add a new Input widget to the tableView
	InputWidget *widget = new InputWidget(ui->listWidget);
	QListWidgetItem *item = new QListWidgetItem(ui->listWidget);

	item->setSizeHint(widget->sizeHint());
	ui->listWidget->setItemWidget(item, widget);

	// enable the remove button
	ui->toolButton_removeinput->setEnabled(true);
}

void InputsDialog::removeInput()
{
	// remove the selected Input widget from the tableView
	QListWidgetItem *item = ui->listWidget->currentItem();
	if (item) {
		delete item;
	}

	// if there are no more items in the tableView, disable the remove button
	if (ui->listWidget->count() == 0) {
		ui->toolButton_removeinput->setEnabled(false);
	}
}

void InputsDialog::setInputsData(const inputs_data &data)
{
	for (const auto &input : data) {
		addInput();
		QWidget *widget = ui->listWidget->itemWidget(
			ui->listWidget->item(ui->listWidget->count() - 1));
		InputWidget *inputWidget = (InputWidget *)widget;
		inputWidget->setInputData(input);
	}
}

inputs_data InputsDialog::getInputsDataFromUI()
{
	inputs_data data;

	for (int i = 0; i < ui->listWidget->count(); i++) {
		QListWidgetItem *item = ui->listWidget->item(i);
		QWidget *widget = ui->listWidget->itemWidget(item);
		InputWidget *inputWidget = (InputWidget *)widget;
		data.push_back(inputWidget->getInputDataFromUI());
	}

	return data;
}
