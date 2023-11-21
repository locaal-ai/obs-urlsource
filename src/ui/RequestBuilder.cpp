
#include "RequestBuilder.h"
#include "ui_requestbuilder.h"
#include "CollapseButton.h"
#include "plugin-support.h"

#include <obs-module.h>

#include "obs-source-util.h"

void set_form_row_visibility(QFormLayout *layout, QWidget *widget, bool visible)
{
	if (layout == nullptr) {
		return;
	}
	if (widget == nullptr) {
		return;
	}
	for (int i = 0; i < layout->rowCount(); i++) {
		QLayoutItem *item = layout->itemAt(i, QFormLayout::FieldRole);
		if (item == nullptr) {
			continue;
		}
		if (item->widget() == widget) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
			layout->setRowVisible(i, visible);
#else
			item->widget()->setVisible(visible);
			layout->itemAt(i, QFormLayout::LabelRole)->widget()->setVisible(visible);
#endif
			return;
		}
	}
}

bool add_sources_to_qcombobox(void *list, obs_source_t *source)
{
	const auto source_id = obs_source_get_id(source);
	// add all text sources and sources that produce an image (by checking source.output_flags = OBS_SOURCE_VIDEO)
	if (!is_obs_source_text(source) &&
	    ((obs_source_get_output_flags(source) & OBS_SOURCE_VIDEO) == 0)) {
		return true;
	}

	std::string name = obs_source_get_name(source);
	std::string prefix = "";
	if (strcmp(source_id, "text_ft2_source_v2") == 0 ||
	    strcmp(source_id, "text_gdiplus_v2") == 0)
		prefix = "(Text) ";
	else
		prefix = "(Image) ";
	std::string name_with_prefix = prefix + name;
	QComboBox *comboList = (QComboBox *)list;
	comboList->addItem(QString::fromStdString(name_with_prefix),
			   QVariant(QString::fromStdString(name)));
	return true;
}

RequestBuilder::RequestBuilder(url_source_request_data *request_data,
			       std::function<void()> update_handler, QWidget *parent)
	: QDialog(parent), ui(new Ui::RequestBuilder)
{
	ui->setupUi(this);

	setWindowTitle("HTTP Request Builder");
	// Make modal
	setModal(true);

	// set a minimum width for the dialog
	setMinimumWidth(500);

	if (request_data->url_or_file == "url") {
		ui->urlRadioButton->setChecked(true);
	} else {
		ui->fileRadioButton->setChecked(true);
	}

	ui->urlLineEdit->setText(QString::fromStdString(request_data->url));
	ui->urlOrFileButton->setEnabled(request_data->url_or_file == "file");

	// URL or file dialog
	connect(ui->urlOrFileButton, &QPushButton::clicked, this, [=]() {
		QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), "",
								tr("All Files (*.*)"));
		if (fileName != "") {
			ui->urlLineEdit->setText(fileName);
		}
	});

	ui->urlRequestOptionsGroup->setVisible(request_data->url_or_file == "url");

	auto toggleFileUrlButtons = [=]() {
		ui->urlOrFileButton->setEnabled(ui->fileRadioButton->isChecked());
		// hide the urlRequestOptionsGroup if file is selected
		ui->urlRequestOptionsGroup->setVisible(ui->urlRadioButton->isChecked());
		// adjust the size of the dialog to fit the content
		this->adjustSize();
	};
	// show file selector button if file is selected
	connect(ui->fileRadioButton, &QRadioButton::toggled, this, toggleFileUrlButtons);
	connect(ui->urlRadioButton, &QRadioButton::toggled, this, toggleFileUrlButtons);

	ui->methodComboBox->setCurrentText(QString::fromStdString(request_data->method));

	// populate headers in ui->tableView_headers from request_data->headers
	QStandardItemModel *model = new QStandardItemModel;
	for (auto &pair : request_data->headers) {
		// add a new row
		model->appendRow({new QStandardItem(QString::fromStdString(pair.first)),
				  new QStandardItem(QString::fromStdString(pair.second))});
	}
	ui->tableView_headers->setModel(model);
	connect(ui->pushButton_addHeader, &QPushButton::clicked, this, [=]() {
		// add a new row
		((QStandardItemModel *)ui->tableView_headers->model())
			->appendRow({new QStandardItem(""), new QStandardItem("")});
	});
	connect(ui->pushButton_removeHeader, &QPushButton::clicked, this, [=]() {
		// remove the selected row
		ui->tableView_headers->model()->removeRow(
			ui->tableView_headers->selectionModel()->currentIndex().row());
	});

	ui->sslCertFileLineEdit->setText(
		QString::fromStdString(request_data->ssl_client_cert_file));
	ui->sslKeyFileLineEdit->setText(QString::fromStdString(request_data->ssl_client_key_file));
	ui->sslKeyPasswordLineEdit->setText(
		QString::fromStdString(request_data->ssl_client_key_pass));
	ui->verifyPeerCheckBox->setChecked(request_data->ssl_verify_peer);

	// SSL certificate file dialog
	connect(ui->sslCertFileButton, &QPushButton::clicked, this, [=]() {
		QString fileName =
			QFileDialog::getOpenFileName(this, tr("Open SSL certificate file"), "",
						     tr("SSL certificate files (*.pem)"));
		if (fileName != "") {
			ui->sslCertFileLineEdit->setText(fileName);
		}
	});

	// SSL key file dialog
	connect(ui->sslKeyFileButton, &QPushButton::clicked, this, [=]() {
		QString fileName = QFileDialog::getOpenFileName(this, tr("Open SSL key file"), "",
								tr("SSL key files (*.pem)"));
		if (fileName != "") {
			ui->sslKeyFileLineEdit->setText(fileName);
		}
	});

	connect(ui->sslOptionsCheckbox, &QCheckBox::toggled, this, [=](bool checked) {
		// Show the SSL options if the checkbox is checked
		ui->sslOptionsGroup->setVisible(checked);
		// adjust the size of the dialog to fit the content
		this->adjustSize();
	});
	if (request_data->ssl_client_cert_file != "" || request_data->ssl_client_key_file != "" ||
	    request_data->ssl_client_key_pass != "" || request_data->ssl_verify_peer) {
		ui->sslOptionsCheckbox->setChecked(true);
	}
	ui->sslOptionsGroup->setVisible(ui->sslOptionsCheckbox->isChecked());

	// populate list of OBS text sources
	obs_enum_sources(add_sources_to_qcombobox, ui->obsTextSourceComboBox);
	// Select the current OBS text source, if any
	int itemIdx = ui->obsTextSourceComboBox->findData(
		QVariant(QString::fromStdString(request_data->obs_text_source)));
	if (itemIdx != -1) {
		ui->obsTextSourceComboBox->setCurrentIndex(itemIdx);
	} else {
		ui->obsTextSourceComboBox->setCurrentIndex(0);
	}
	ui->obsTextSourceEnabledCheckBox->setChecked(request_data->obs_text_source_skip_if_empty);
	ui->obsTextSourceSkipSameCheckBox->setChecked(request_data->obs_text_source_skip_if_same);
	ui->aggToEmpty->setChecked(request_data->aggregate_to_empty);
    ui->comboBox_resizeInput->setCurrentText(QString::fromStdString(request_data->obs_input_source_resize_option));

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
    connect(ui->obsTextSourceComboBox, &QComboBox::currentTextChanged, this, inputSourceSelected);
    inputSourceSelected();

	ui->bodyTextEdit->setText(QString::fromStdString(request_data->body));

	auto setVisibilityOfBody = [=]() {
		// If method is not GET, show the body input
		set_form_row_visibility(ui->urlRequestLayout, ui->bodyTextEdit,
					ui->methodComboBox->currentText() != "GET");
		this->adjustSize();
	};

	setVisibilityOfBody();

	// Method select from dropdown change
	connect(ui->methodComboBox, &QComboBox::currentTextChanged, this, setVisibilityOfBody);

	ui->outputTypeComboBox->setCurrentIndex(ui->outputTypeComboBox->findText(
		QString::fromStdString(request_data->output_type)));
	ui->outputJSONPointerLineEdit->setText(
		QString::fromStdString(request_data->output_json_pointer));
	ui->outputJSONPathLineEdit->setText(QString::fromStdString(request_data->output_json_path));
	ui->outputXPathLineEdit->setText(QString::fromStdString(request_data->output_xpath));
	ui->outputXQueryLineEdit->setText(QString::fromStdString(request_data->output_xquery));
	ui->outputRegexLineEdit->setText(QString::fromStdString(request_data->output_regex));
	ui->outputRegexFlagsLineEdit->setText(
		QString::fromStdString(request_data->output_regex_flags));
	ui->outputRegexGroupLineEdit->setText(
		QString::fromStdString(request_data->output_regex_group));
	ui->cssSelectorLineEdit->setText(QString::fromStdString(request_data->output_cssselector));
	auto setVisibilityOfOutputParsingOptions = [=]() {
		// Hide all output parsing options
		for (const auto &widget :
		     {ui->outputJSONPathLineEdit, ui->outputXPathLineEdit, ui->outputXQueryLineEdit,
		      ui->outputRegexLineEdit, ui->outputRegexFlagsLineEdit,
		      ui->outputRegexGroupLineEdit, ui->outputJSONPointerLineEdit,
		      ui->cssSelectorLineEdit, ui->postProcessRegexLineEdit}) {
			set_form_row_visibility(ui->formOutputParsing, widget, false);
		}

		// Show the output parsing options for the selected output type
		if (ui->outputTypeComboBox->currentText() == "JSON") {
			set_form_row_visibility(ui->formOutputParsing, ui->outputJSONPathLineEdit,
						true);
			set_form_row_visibility(ui->formOutputParsing,
						ui->outputJSONPointerLineEdit, true);
			set_form_row_visibility(ui->formOutputParsing, ui->postProcessRegexLineEdit,
						true);
		} else if (ui->outputTypeComboBox->currentText() == "XML (XPath)") {
			set_form_row_visibility(ui->formOutputParsing, ui->outputXPathLineEdit,
						true);
			set_form_row_visibility(ui->formOutputParsing, ui->postProcessRegexLineEdit,
						true);
		} else if (ui->outputTypeComboBox->currentText() == "XML (XQuery)") {
			set_form_row_visibility(ui->formOutputParsing, ui->outputXQueryLineEdit,
						true);
			set_form_row_visibility(ui->formOutputParsing, ui->postProcessRegexLineEdit,
						true);
		} else if (ui->outputTypeComboBox->currentText() == "HTML") {
			set_form_row_visibility(ui->formOutputParsing, ui->cssSelectorLineEdit,
						true);
			set_form_row_visibility(ui->formOutputParsing, ui->postProcessRegexLineEdit,
						true);
		} else if (ui->outputTypeComboBox->currentText() == "Text") {
			set_form_row_visibility(ui->formOutputParsing, ui->outputRegexLineEdit,
						true);
			set_form_row_visibility(ui->formOutputParsing, ui->outputRegexFlagsLineEdit,
						true);
			set_form_row_visibility(ui->formOutputParsing, ui->outputRegexGroupLineEdit,
						true);
			set_form_row_visibility(ui->formOutputParsing, ui->postProcessRegexLineEdit,
						true);
		}
	};

	// Show the output parsing options for the selected output type
	setVisibilityOfOutputParsingOptions();
	// Respond to changes in the output type
	connect(ui->outputTypeComboBox, &QComboBox::currentTextChanged, this,
		setVisibilityOfOutputParsingOptions);

	ui->postProcessRegexLineEdit->setText(
		QString::fromStdString(request_data->post_process_regex));
	ui->postProcessRegexIsReplaceCheckBox->setChecked(
		request_data->post_process_regex_is_replace);
	ui->postProcessRegexReplaceLineEdit->setText(
		QString::fromStdString(request_data->post_process_regex_replace));

	auto setVisibilityOfPostProcessRegexOptions = [=]() {
		// Hide the replace string if the checkbox is not checked
		ui->postProcessRegexReplaceLineEdit->setVisible(
			ui->postProcessRegexIsReplaceCheckBox->isChecked());
	};

	setVisibilityOfPostProcessRegexOptions();
	// Respond to changes in the checkbox
	connect(ui->postProcessRegexIsReplaceCheckBox, &QCheckBox::toggled, this,
		setVisibilityOfPostProcessRegexOptions);

	ui->errorMessageLabel->setVisible(false);

	// Lambda to save the request settings to a request_data struct
	auto saveSettingsToRequestData = [=](url_source_request_data *request_data_for_saving) {
		// Save the request settings to the request_data struct
		request_data_for_saving->url = ui->urlLineEdit->text().toStdString();
		request_data_for_saving->url_or_file = ui->urlRadioButton->isChecked() ? "url"
										       : "file";
		request_data_for_saving->method = ui->methodComboBox->currentText().toStdString();
		request_data_for_saving->body = ui->bodyTextEdit->toPlainText().toStdString();
		if (ui->obsTextSourceComboBox->currentData().toString().toStdString() != "None") {
			request_data_for_saving->obs_text_source =
				ui->obsTextSourceComboBox->currentData().toString().toStdString();
		} else {
			request_data_for_saving->obs_text_source = "";
		}
		request_data_for_saving->obs_text_source_skip_if_empty =
			ui->obsTextSourceEnabledCheckBox->isChecked();
		request_data_for_saving->obs_text_source_skip_if_same =
			ui->obsTextSourceSkipSameCheckBox->isChecked();
		request_data_for_saving->aggregate_to_empty = ui->aggToEmpty->isChecked();
        request_data_for_saving->obs_input_source_resize_option = ui->comboBox_resizeInput->currentText().toStdString();

		// Save the SSL certificate file
		request_data_for_saving->ssl_client_cert_file =
			ui->sslCertFileLineEdit->text().toStdString();

		// Save the SSL key file
		request_data_for_saving->ssl_client_key_file =
			ui->sslKeyFileLineEdit->text().toStdString();

		// Save the SSL key password
		request_data_for_saving->ssl_client_key_pass =
			ui->sslKeyPasswordLineEdit->text().toStdString();

		// Save the verify peer option
		request_data_for_saving->ssl_verify_peer = ui->verifyPeerCheckBox->isChecked();

		// Save the headers from ui->tableView_headers's model
		request_data_for_saving->headers.clear();
		QStandardItemModel *model = (QStandardItemModel *)ui->tableView_headers->model();
		for (int i = 0; i < model->rowCount(); i++) {
			request_data_for_saving->headers.push_back(
				std::make_pair(model->item(i, 0)->text().toStdString(),
					       model->item(i, 1)->text().toStdString()));
		}

		// Save the output parsing options
		request_data_for_saving->output_type =
			ui->outputTypeComboBox->currentText().toStdString();
		request_data_for_saving->output_json_pointer =
			ui->outputJSONPointerLineEdit->text().toStdString();
		request_data_for_saving->output_json_path =
			ui->outputJSONPathLineEdit->text().toStdString();
		request_data_for_saving->output_xpath =
			ui->outputXPathLineEdit->text().toStdString();
		request_data_for_saving->output_xquery =
			ui->outputXQueryLineEdit->text().toStdString();
		request_data_for_saving->output_regex =
			ui->outputRegexLineEdit->text().toStdString();
		request_data_for_saving->output_regex_flags =
			ui->outputRegexFlagsLineEdit->text().toStdString();
		request_data_for_saving->output_regex_group =
			ui->outputRegexGroupLineEdit->text().toStdString();
		request_data_for_saving->output_cssselector =
			ui->cssSelectorLineEdit->text().toStdString();

		// Save the postprocess regex options
		request_data_for_saving->post_process_regex =
			ui->postProcessRegexLineEdit->text().toStdString();
		request_data_for_saving->post_process_regex_is_replace =
			ui->postProcessRegexIsReplaceCheckBox->isChecked();
		request_data_for_saving->post_process_regex_replace =
			ui->postProcessRegexReplaceLineEdit->text().toStdString();
	};

	connect(this, &RequestBuilder::show_response_dialog_signal, this,
		&RequestBuilder::show_response_dialog);

	connect(ui->sendButton, &QPushButton::clicked, this, [=]() {
		// Hide the error message label
		ui->errorMessageLabel->setVisible(false);

		// disable the send button
		ui->sendButton->setEnabled(false);
		ui->sendButton->setText("Sending...");

		// Create a request_data_handler function that will be called by the thread
		auto request_data_handler_ex = [this, saveSettingsToRequestData]() {
			// Get an interim request_data struct with the current settings
			url_source_request_data request_data_test;
			saveSettingsToRequestData(&request_data_test);

			obs_log(LOG_INFO, "Sending request to %s", request_data_test.url.c_str());

			request_data_handler_response response =
				request_data_handler(&request_data_test);

			if (response.status_code != URL_SOURCE_REQUEST_SUCCESS) {
				// Show the error message label
				this->ui->errorMessageLabel->setText(
					QString::fromStdString(response.error_message));
				this->ui->errorMessageLabel->setVisible(true);
				// enable the send button
				this->ui->sendButton->setEnabled(true);
				this->ui->sendButton->setText("Test Request");
				return;
			}

			// Show the response dialog
			emit show_response_dialog_signal(response);
		};

		// Create a thread to send the request to prevent the UI from hanging
		std::thread request_data_handler_thread(request_data_handler_ex);
		request_data_handler_thread.detach();
	});

	connect(ui->buttonBox, &QDialogButtonBox::rejected, this, [=]() {
		// Close dialog
		close();
	});
	connect(ui->buttonBox, &QDialogButtonBox::accepted, this, [=]() {
		// Save the request settings to the request_data struct
		saveSettingsToRequestData(request_data);

		update_handler();

		// Close dialog
		close();
	});

	adjustSize();
}

void RequestBuilder::show_response_dialog(const request_data_handler_response &response)
{
	obs_log(LOG_INFO, "Response HTTP status code: %ld", response.http_status_code);

	// Display the response
	QDialog *responseDialog = new QDialog(this);
	responseDialog->setWindowTitle("Response");
	QVBoxLayout *responseLayout = new QVBoxLayout;
	responseDialog->setLayout(responseLayout);
	responseDialog->setMinimumWidth(500);
	responseDialog->setMinimumHeight(300);
	responseDialog->raise();
	responseDialog->activateWindow();
	responseDialog->setModal(true);

	// show request URL
	QGroupBox *requestGroupBox = new QGroupBox("Request");
	responseLayout->addWidget(requestGroupBox);
	QVBoxLayout *requestLayout = new QVBoxLayout;
	requestGroupBox->setLayout(requestLayout);
	QScrollArea *requestUrlScrollArea = new QScrollArea;
	QLabel *requestUrlLabel = new QLabel(QString::fromStdString(response.request_url));
	requestUrlScrollArea->setWidget(requestUrlLabel);
	requestLayout->addWidget(requestUrlScrollArea);

	// if there's a request body, add it to the dialog
	if (response.request_body != "") {
		QGroupBox *requestBodyGroupBox = new QGroupBox("Request Body");
		responseLayout->addWidget(requestBodyGroupBox);
		QVBoxLayout *requestBodyLayout = new QVBoxLayout;
		requestBodyGroupBox->setLayout(requestBodyLayout);
		// Add scroll area for the request body
		QScrollArea *requestBodyScrollArea = new QScrollArea;
		QLabel *requestLabel = new QLabel(QString::fromStdString(response.request_body));
		// Wrap the text
		requestLabel->setWordWrap(true);
		// Set the label as the scroll area's widget
		requestBodyScrollArea->setWidget(requestLabel);
		requestBodyLayout->addWidget(requestBodyScrollArea);
	}

	if (!response.body.empty()) {
		QGroupBox *responseBodyGroupBox = new QGroupBox("Response Body");
		responseBodyGroupBox->setLayout(new QVBoxLayout);
		// Add scroll area for the response body
		QScrollArea *responseBodyScrollArea = new QScrollArea;
		QLabel *responseLabel = new QLabel(QString::fromStdString(response.body).trimmed());
		// Wrap the text
		responseLabel->setWordWrap(true);
		// dont allow rich text
		responseLabel->setTextFormat(Qt::PlainText);
		// Set the label as the scroll area's widget
		responseBodyScrollArea->setWidget(responseLabel);
		responseBodyGroupBox->layout()->addWidget(responseBodyScrollArea);
		responseLayout->addWidget(responseBodyGroupBox);
	}

	// If there's a parsed output, add it to the dialog in a QGroupBox
	if (response.body_parts_parsed.size() > 0 && response.body_parts_parsed[0] != "") {
		QGroupBox *parsedOutputGroupBox = new QGroupBox("Parsed Output");
		responseLayout->addWidget(parsedOutputGroupBox);
		QVBoxLayout *parsedOutputLayout = new QVBoxLayout;
		parsedOutputGroupBox->setLayout(parsedOutputLayout);
		if (response.body_parts_parsed.size() > 1) {
			// Add a QTabWidget to show the parsed output parts
			QTabWidget *tabWidget = new QTabWidget;
			parsedOutputLayout->addWidget(tabWidget);
			for (auto &parsedOutput : response.body_parts_parsed) {
				// label each tab {outputN} where N is the index of the output part
				tabWidget->addTab(
					new QLabel(QString::fromStdString(parsedOutput)),
					QString::fromStdString("{output" +
							       std::to_string(tabWidget->count()) +
							       "}"));
			}
		} else {
			// Add a QLabel to show a single parsed output
			QLabel *parsedOutputLabel =
				new QLabel(QString::fromStdString(response.body_parts_parsed[0]));
			parsedOutputLabel->setWordWrap(true);
			parsedOutputLabel->setTextFormat(Qt::PlainText);
			parsedOutputLayout->addWidget(parsedOutputLabel);
		}
	}

	// enable the send button
	this->ui->sendButton->setEnabled(true);
	this->ui->sendButton->setText("Test Request");

	// Resize the dialog to fit the text
	responseDialog->adjustSize();

	// Center the dialog
	responseDialog->move(this->x() + (this->width() - responseDialog->width()) / 2,
			     this->y() + (this->height() - responseDialog->height()) / 2);

	// Show the dialog
	responseDialog->show();
}
