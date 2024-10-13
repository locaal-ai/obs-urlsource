
#include "RequestBuilder.h"
#include "ui_requestbuilder.h"
#include "CollapseButton.h"
#include "plugin-support.h"
#include "InputsDialog.h"
#include "mapping-data.h"

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
	// add all text sources and sources that produce an image (by checking source.output_flags = OBS_SOURCE_VIDEO)
	if (!is_obs_source_text(source) &&
	    ((obs_source_get_output_flags(source) & OBS_SOURCE_VIDEO) == 0)) {
		return true;
	}

	std::string name = obs_source_get_name(source);
	std::string prefix = "";
	const auto source_id = obs_source_get_id(source);
	if (strncmp(source_id, "text_ft2_source", 15) == 0 ||
	    strncmp(source_id, "text_gdiplus", 12) == 0)
		prefix = "(Text) ";
	else
		prefix = "(Image) ";
	std::string name_with_prefix = prefix + name;
	QComboBox *comboList = (QComboBox *)list;
	comboList->addItem(QString::fromStdString(name_with_prefix),
			   QVariant(QString::fromStdString(name)));
	return true;
}

void set_widgets_enabled_in_layout(QLayout *layout, bool enabled,
				   std::vector<QWidget *> exclude = {})
{
	if (!layout)
		return;

	for (int i = 0; i < layout->count(); ++i) {
		QLayoutItem *item = layout->itemAt(i);
		QWidget *widget = item->widget();
		// check if widget is excluded
		bool excluded = false;
		for (const QWidget *excludeWidget : exclude) {
			if (widget == excludeWidget) {
				excluded = true;
			}
		}
		if (widget && !excluded) {
			widget->setEnabled(enabled);
		}
		if (item->layout()) {
			// Recursively enable/disable widgets in nested layouts, excluding the specified widget
			set_widgets_enabled_in_layout(item->layout(), enabled, exclude);
		}
	}
}

void addHeaders(const std::vector<std::pair<std::string, std::string>> &headers,
		QTableView *tableView)
{
	QStandardItemModel *model = new QStandardItemModel;
	for (auto &pair : headers) {
		// add a new row
		model->appendRow({new QStandardItem(QString::fromStdString(pair.first)),
				  new QStandardItem(QString::fromStdString(pair.second))});
	}
	tableView->setModel(model);
}

RequestBuilder::RequestBuilder(url_source_request_data *request_data,
			       std::function<void()> update_handler, QWidget *parent)
	: QDialog(parent),
	  ui(new Ui::RequestBuilder)
{
	ui->setupUi(this);

	setWindowTitle("HTTP Request Builder");
	// Make modal
	setModal(true);

	// set a minimum width for the dialog
	setMinimumWidth(400);

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
		if (ui->fileRadioButton->isChecked()) {
			ui->urlOrFileButton->setEnabled(true);
			ui->urlRequestOptionsGroup->setVisible(false);
		} else if (ui->urlRadioButton->isChecked()) {
			ui->urlOrFileButton->setEnabled(false);
			ui->urlRequestOptionsGroup->setVisible(true);
		}
		this->adjustSize();
	};
	// show file selector button if file is selected
	connect(ui->fileRadioButton, &QRadioButton::toggled, this, toggleFileUrlButtons);
	connect(ui->urlRadioButton, &QRadioButton::toggled, this, toggleFileUrlButtons);

	connect(ui->comboBox_presets, &QComboBox::currentIndexChanged, this, [=](int index) {
		ui->urlRadioButton->setChecked(true);
		toggleFileUrlButtons();
		ui->sslOptionsCheckbox->setChecked(false);
		if (index == 1 || index == 2 || index == 3) {
			//OpenAI
			ui->methodComboBox->setCurrentIndex(1);
			addHeaders({{"Content-Type", "application/json"},
				    {"Authorization", "Bearer $OPENAI_API_KEY"}},
				   ui->tableView_headers);
		}
		if (index == 1) {
			/* ------------------------------------------- */
			/* --------------- OpenAI Chat --------------- */
			/* ------------------------------------------- */
			ui->urlLineEdit->setText("https://api.openai.com/v1/chat/completions");
			ui->bodyTextEdit->setText(R"({
	"model": "gpt-4o-mini",
	"messages": [
		{
			"role": "system",
			"content": "You are a helpful assistant."
		},
		{
			"role": "user",
			"content": "{{input}}"
		}
	]
})");
			ui->outputTypeComboBox->setCurrentIndex(4);
			ui->outputJSONPathLineEdit->setText("$.choices.0.message.content");
		} else if (index == 2) {
			/* ------------------------------------------- */
			/* --------------- OpenAI TTS  --------------- */
			/* ------------------------------------------- */
			ui->urlLineEdit->setText("https://api.openai.com/v1/audio/speech");
			ui->bodyTextEdit->setText(R"({
    "model": "tts-1",
    "input": "{{input}}",
    "voice": "alloy"
  })");
			ui->sslOptionsCheckbox->setChecked(false);
			ui->outputTypeComboBox->setCurrentIndex(3);
		} else if (index == 3) {
			/* --------------------------------------------- */
			/* --------------- OpenAI Vision --------------- */
			/* --------------------------------------------- */
			ui->urlLineEdit->setText("https://api.openai.com/v1/chat/completions");
			ui->bodyTextEdit->setText(R"({
  "model": "gpt-4o-mini",
  "messages": [
    {
      "role": "user",
      "content": [
        {
          "type": "text",
          "text": "What's happening in the image?"
        },
        {
          "type": "image_url",
          "image_url": {
            "url": "data:image/png;base64,{{imageb64}}"
          }
        }
      ]
    }
  ],
  "max_tokens": 50
})");
			ui->outputTypeComboBox->setCurrentIndex(4);
			ui->outputJSONPathLineEdit->setText("$.choices.0.message.content");
		} else if (index == 4) {
			/* ------------------------------------------- */
			/* --------------- 11Labs  TTS --------------- */
			/* ------------------------------------------- */
			ui->methodComboBox->setCurrentIndex(1);
			addHeaders({{"Content-Type", "application/json"},
				    {"Accept", "application/json"},
				    {"xi-api-key", "$XI_API_KEY"}},
				   ui->tableView_headers);
			ui->urlLineEdit->setText(
				"https://api.elevenlabs.io/v1/text-to-speech/21m00Tcm4TlvDq8ikWAM");
			ui->bodyTextEdit->setText(R"({
  "model_id": "eleven_monolingual_v1",
  "text": "{{input}}"
})");
			ui->outputTypeComboBox->setCurrentIndex(3);
		} else if (index == 5) {
			/* --------------------------------------------- */
			/* --------------- Google Sheets --------------- */
			/* --------------------------------------------- */
			ui->methodComboBox->setCurrentIndex(0);
			addHeaders({}, ui->tableView_headers);
			ui->urlLineEdit->setText(
				"https://sheets.googleapis.com/v4/spreadsheets/$SHEET_ID$/values/$CELL_OR_RANGE$?key=$API_KEY$");
			ui->outputTypeComboBox->setCurrentIndex(4);
			ui->outputJSONPathLineEdit->setText("$.values.0.0");
		} else if (index == 6) {
			/* ----------------------------------------------- */
			/* --------------- DeepL Translate --------------- */
			/* ----------------------------------------------- */
			ui->methodComboBox->setCurrentIndex(1);
			ui->urlLineEdit->setText("https://api-free.deepl.com/v2/translate");
			addHeaders(
				{
					{"Content-Type", "application/json"},
					{"Authorization", "DeepL-Auth-Key $yourAuthKey"},
				},
				ui->tableView_headers);
			ui->bodyTextEdit->setText(R"({
  "text": [
    "{{input}}"
  ],
  "target_lang": "DE"
})");
			ui->outputTypeComboBox->setCurrentIndex(4);
			ui->outputJSONPathLineEdit->setText("$.translations.0.text");
		} else if (index == 7) {
			// Polyglot Translate
			ui->methodComboBox->setCurrentIndex(1);
			ui->urlLineEdit->setText("http://localhost:18080/translate");
			ui->bodyTextEdit->setText(
				"{\"text\":\"{{input}}\", \"source_lang\":\"eng_Latn\", \"target_lang\":\"spa_Latn\"}");
			ui->sslOptionsCheckbox->setChecked(false);
			ui->outputTypeComboBox->setCurrentIndex(0);
			ui->outputRegexLineEdit->setText("");
		} else if (index == 8) {
			// YouTube POST captions
			ui->methodComboBox->setCurrentIndex(1);
			addHeaders(
				{
					{"Content-Type", "text/plain"},
				},
				ui->tableView_headers);
			ui->urlLineEdit->setText(
				"http://upload.youtube.com/closedcaption?cid=xxxx-xxxx-xxxx-xxxx-xxxx&seq={{seq}}");
			ui->bodyTextEdit->setText(R"({{strftime("%Y-%m-%dT%H:%M:%S.000", true)}}
{{input}})");
			ui->sslOptionsCheckbox->setChecked(false);
			ui->outputTypeComboBox->setCurrentIndex(0);
			ui->outputRegexLineEdit->setText("");
		}
	});

	ui->methodComboBox->setCurrentText(QString::fromStdString(request_data->method));
	ui->checkBox_failonhttperrorcodes->setChecked(request_data->fail_on_http_error);

	// populate headers in ui->tableView_headers from request_data->headers
	addHeaders(request_data->headers, ui->tableView_headers);
	connect(ui->toolButton_addHeader, &QPushButton::clicked, this, [=]() {
		// add a new row
		((QStandardItemModel *)ui->tableView_headers->model())
			->appendRow({new QStandardItem(""), new QStandardItem("")});
	});
	connect(ui->toolButton_removeHeader, &QPushButton::clicked, this, [=]() {
		// remove the selected row
		ui->tableView_headers->model()->removeRow(
			ui->tableView_headers->selectionModel()->currentIndex().row());
	});

	connect(ui->pushButton_addInputs, &QPushButton::clicked, this, [=]() {
		// open the inputs modal
		InputsDialog inputsDialog(this);
		inputsDialog.setInputsData(this->inputs_data_);
		inputsDialog.exec();

		if (inputsDialog.result() == QDialog::Accepted) {
			// get the inputs data from the inputs modal
			// add the inputs to the request_data
			this->inputs_data_ = inputsDialog.getInputsDataFromUI();
		}
	});
	this->inputs_data_ = request_data->inputs;

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
	ui->lineEdit_delimiter->setText(QString::fromStdString(request_data->kv_delimiter));
	auto setVisibilityOfOutputParsingOptions = [=]() {
		// Hide all output parsing options
		for (const auto &widget :
		     {ui->outputJSONPathLineEdit, ui->outputXPathLineEdit, ui->outputXQueryLineEdit,
		      ui->outputRegexLineEdit, ui->outputRegexFlagsLineEdit,
		      ui->outputRegexGroupLineEdit, ui->outputJSONPointerLineEdit,
		      ui->cssSelectorLineEdit, ui->postProcessRegexLineEdit,
		      ui->lineEdit_delimiter}) {
			set_form_row_visibility(ui->formOutputParsing, widget, false);
		}

		// Show the output parsing options for the selected output type
		if (ui->outputTypeComboBox->currentText() == "Key-Value") {
			set_form_row_visibility(ui->formOutputParsing, ui->lineEdit_delimiter,
						true);
		} else if (ui->outputTypeComboBox->currentText() == "JSON") {
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
		request_data_for_saving->fail_on_http_error =
			ui->checkBox_failonhttperrorcodes->isChecked();
		request_data_for_saving->body = ui->bodyTextEdit->toPlainText().toStdString();

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
		QStandardItemModel *itemModel =
			(QStandardItemModel *)ui->tableView_headers->model();
		for (int i = 0; i < itemModel->rowCount(); i++) {
			request_data_for_saving->headers.push_back(
				std::make_pair(itemModel->item(i, 0)->text().toStdString(),
					       itemModel->item(i, 1)->text().toStdString()));
		}

		request_data_for_saving->inputs = this->inputs_data_;

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
		request_data_for_saving->kv_delimiter =
			ui->lineEdit_delimiter->text().toStdString();

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
	connect(this, &RequestBuilder::show_error_message_signal, this,
		&RequestBuilder::show_error_message);

	connect(ui->sendButton, &QPushButton::clicked, this, [=]() {
		// Hide the error message label
		ui->errorMessageLabel->setVisible(false);

		// disable the send button
		ui->sendButton->setEnabled(false);
		ui->sendButton->setText("Sending...");

		// Create a thread to send the request to prevent the UI from hanging
		std::thread request_data_handler_thread([this, saveSettingsToRequestData]() {
			// Get an interim request_data struct with the current settings
			url_source_request_data request_data_test;
			saveSettingsToRequestData(&request_data_test);

			obs_log(LOG_INFO, "Sending request to %s", request_data_test.url.c_str());

			request_data_handler_response response =
				request_data_handler(&request_data_test);

			if (response.status_code != URL_SOURCE_REQUEST_SUCCESS) {
				emit show_error_message_signal(response.error_message);
			} else {
				// Show the response dialog
				emit show_response_dialog_signal(response);
			}
		});
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

void RequestBuilder::show_error_message(const std::string &error_message)
{
	if (!error_message.empty()) {
		// Show the error message label
		this->ui->errorMessageLabel->setText(QString::fromStdString(error_message));
		this->ui->errorMessageLabel->setVisible(true);
	} else {
		this->ui->errorMessageLabel->setVisible(false);
	}
	// enable the send button
	this->ui->sendButton->setEnabled(true);
	this->ui->sendButton->setText("Test Request");
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
		QString request_body = QString::fromStdString(response.request_body).trimmed();
		// if the request body is too big to fit in the dialog, trim it
		if (request_body.length() > 1000) {
			request_body = request_body.left(1000) + "...";
		}
		QLabel *requestLabel = new QLabel(request_body);
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
		QString response_body = QString::fromStdString(response.body).trimmed();
		// if the response body is too big to fit in the dialog, trim it
		if (response_body.length() > 1000) {
			response_body = response_body.left(1000) + "...";
		}
		QLabel *responseLabel = new QLabel(response_body);
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
			if (response.body_parts_parsed.size() > 3) {
				// Use a dropdown to select the parsed output to show
				QComboBox *parsedOutputComboBox = new QComboBox;
				parsedOutputLayout->addWidget(parsedOutputComboBox);
				for (size_t i = 0; i < response.body_parts_parsed.size(); i++) {
					// add each parsed output to the dropdown
					parsedOutputComboBox->addItem(
						QString::number(i) + ": " +
							QString::fromStdString(
								response.body_parts_parsed[i]),
						QVariant(QString::fromStdString(
							response.body_parts_parsed[i])));
				}
				// Add a QLabel to show the selected parsed output
				QLabel *parsedOutputLabel = new QLabel;
				parsedOutputLayout->addWidget(parsedOutputLabel);
				// Show the selected parsed output
				connect(parsedOutputComboBox, &QComboBox::currentTextChanged, this,
					[=]() {
						parsedOutputLabel->setText(
							parsedOutputComboBox->currentData()
								.toString());
					});
			} else {
				// Add a QTabWidget to show the parsed output parts
				QTabWidget *tabWidget = new QTabWidget;
				parsedOutputLayout->addWidget(tabWidget);
				for (auto &parsedOutput : response.body_parts_parsed) {
					// label each tab {outputN} where N is the index of the output part
					tabWidget->addTab(
						new QLabel(QString::fromStdString(parsedOutput)),
						QString::fromStdString(
							"{output" +
							std::to_string(tabWidget->count()) + "}"));
				}
			}
		} else {
			QString parsed_output =
				QString::fromStdString(response.body_parts_parsed[0]).trimmed();
			// if the parsed output is too big to fit in the dialog, trim it
			if (parsed_output.length() > 1000) {
				parsed_output = parsed_output.left(1000) + "...";
			}
			// Add a QLabel to show a single parsed output
			QLabel *parsedOutputLabel = new QLabel(parsed_output);
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
