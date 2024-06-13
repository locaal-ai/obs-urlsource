#include "InputsDialog.h"

#include "plugin-support.h"
#include "ui_InputsDialog.h"
#include "ui_InputWidget.h"
#include "mapping-data.h"
#include "obs-ui-utils.h"

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
	QWidget *widget = new QWidget(ui->listWidget);
	Ui::InputWidget *inputWidget = new Ui::InputWidget;
	inputWidget->setupUi(widget);
	obs_enum_sources(add_sources_to_combobox, inputWidget->obsTextSourceComboBox);

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
		Ui::InputWidget *inputWidget = (Ui::InputWidget *)widget;

		inputWidget->obsTextSourceComboBox->setCurrentText(input.source.c_str());
		inputWidget->comboBox_aggTarget->setCurrentText(input.agg_method.c_str());
		inputWidget->aggToTarget->setChecked(input.aggregate);
		inputWidget->comboBox_resizeInput->setCurrentText(input.resize_method.c_str());
		inputWidget->obsTextSourceEnabledCheckBox->setChecked(input.no_empty);
		inputWidget->obsTextSourceSkipSameCheckBox->setChecked(input.no_same);
	}
}

inputs_data InputsDialog::getInputsData()
{
	inputs_data data;

	for (int i = 0; i < ui->listWidget->count(); i++) {
		QListWidgetItem *item = ui->listWidget->item(i);
		QWidget *widget = ui->listWidget->itemWidget(item);
		Ui::InputWidget *inputWidget = (Ui::InputWidget *)widget;

		input_data input;
		input.source =
			inputWidget->obsTextSourceComboBox->currentText().toUtf8().constData();
		input.agg_method =
			inputWidget->comboBox_aggTarget->currentText().toUtf8().constData();
		input.aggregate = inputWidget->aggToTarget->isChecked();
		input.resize_method =
			inputWidget->comboBox_resizeInput->currentText().toUtf8().constData();
		input.no_empty = inputWidget->obsTextSourceEnabledCheckBox->isChecked();
		input.no_same = inputWidget->obsTextSourceSkipSameCheckBox->isChecked();
		data.push_back(input);
	}

	return data;
}
