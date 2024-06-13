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
