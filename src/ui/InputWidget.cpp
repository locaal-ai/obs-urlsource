
#include "InputWidget.h"
#include "plugin-support.h"
#include "ui_InputWidget.h"
#include "obs-ui-utils.h"
#include "obs-source-util.h"
#include "request-data.h"

InputWidget::InputWidget(QWidget *parent) : QWidget(parent), ui(new Ui::InputWidget)
{
	ui->setupUi(this);

	// populate list of OBS text sources
	obs_enum_sources(add_sources_to_combobox, ui->obsTextSourceComboBox);

	auto setObsTextSourceValueOptionsVisibility = [=]() {
		// Hide the options if no OBS text source is selected
		ui->widget_inputValueOptions->setEnabled(
			ui->obsTextSourceComboBox->currentIndex() != 0);
		// adjust the size of the dialog to fit the content
		this->adjustSize();
	};
	connect(ui->obsTextSourceComboBox, &QComboBox::currentTextChanged, this,
		setObsTextSourceValueOptionsVisibility);

	auto setAggTargetEnabled = [=]() {
		ui->comboBox_aggTarget->setEnabled(ui->aggToTarget->isChecked());
	};
	connect(ui->aggToTarget, &QCheckBox::toggled, this, setAggTargetEnabled);

	auto inputSourceSelected = [=]() {
		// if the source is a media source, show the resize option, otherwise hide it
		auto current_data = ui->obsTextSourceComboBox->currentData();
		bool hide_resize_option = true;
		if (current_data.isValid()) {
			const std::string source_name = current_data.toString().toStdString();
			hide_resize_option = is_obs_source_text(source_name);
		}
		ui->comboBox_resizeInput->setVisible(!hide_resize_option);
		ui->label_resizeInput->setVisible(!hide_resize_option);
	};
	connect(ui->obsTextSourceComboBox, &QComboBox::currentTextChanged, this,
		inputSourceSelected);
}

InputWidget::~InputWidget()
{
	delete ui;
}

void InputWidget::setInputData(const input_data &input)
{
	ui->obsTextSourceComboBox->setCurrentText(input.source.c_str());
	if (input.aggregate) {
		ui->comboBox_aggTarget->setCurrentIndex(input.agg_method);
		// enable ui->comboBox_aggTarget
		ui->comboBox_aggTarget->setEnabled(true);
	}
	ui->aggToTarget->setChecked(input.aggregate);
	ui->comboBox_resizeInput->setCurrentText(input.resize_method.c_str());
	ui->obsTextSourceEnabledCheckBox->setChecked(input.no_empty);
	ui->obsTextSourceSkipSameCheckBox->setChecked(input.no_same);
}

input_data InputWidget::getInputDataFromUI()
{
	input_data input;

	input.source = ui->obsTextSourceComboBox->currentText().toUtf8().constData();
	if (ui->aggToTarget->isChecked()) {
		input.agg_method = ui->comboBox_aggTarget->currentIndex();
	} else {
		input.agg_method = URL_SOURCE_AGG_TARGET_NONE;
	}
	input.aggregate = ui->aggToTarget->isChecked();
	input.resize_method = ui->comboBox_resizeInput->currentText().toUtf8().constData();
	input.no_empty = ui->obsTextSourceEnabledCheckBox->isChecked();
	input.no_same = ui->obsTextSourceSkipSameCheckBox->isChecked();

	return input;
}