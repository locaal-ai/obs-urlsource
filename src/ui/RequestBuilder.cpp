
#include "RequestBuilder.h"
#include "CollapseButton.h"

#include <obs-module.h>

class KeyValueWidget : public QWidget {
public:
	KeyValueWidget(QWidget *parent = nullptr) : QWidget(parent)
	{
		QHBoxLayout *layout = new QHBoxLayout;
		setLayout(layout);

		QLineEdit *keyLineEdit = new QLineEdit;
		keyLineEdit->setPlaceholderText("Key");
		layout->addWidget(keyLineEdit);

		QLineEdit *valueLineEdit = new QLineEdit;
		valueLineEdit->setPlaceholderText("Value");
		layout->addWidget(valueLineEdit);

		QPushButton *removeButton = new QPushButton("-");
		layout->addWidget(removeButton);
		connect(removeButton, &QPushButton::clicked, this, [=]() {
			// Remove this widget from the parent
			delete this;
		});
	}
};

class KeyValueListWidget : public QWidget {
private:
	QVBoxLayout *listLayout;

public:
	KeyValueListWidget(QWidget *parent = nullptr) : QWidget(parent), listLayout(new QVBoxLayout)
	{
		setLayout(listLayout);

		// Add a "+" button to add a new key-value widget
		QPushButton *addButton = new QPushButton("+");
		listLayout->addWidget(addButton);
		connect(addButton, &QPushButton::clicked, this, [=]() {
			// Add a new key-value widget
			KeyValueWidget *keyValueWidget = new KeyValueWidget(this);
			listLayout->insertWidget(listLayout->count() - 1, keyValueWidget);
			// adjust the size of the widget to fit the content
			adjustSize();
		});
	}

	void populateFromPairs(std::vector<std::pair<std::string, std::string>> &pairs)
	{
		for (auto &pair : pairs) {
			KeyValueWidget *keyValueWidget = new KeyValueWidget;
			QLineEdit *keyLineEdit =
				(QLineEdit *)keyValueWidget->layout()->itemAt(0)->widget();
			QLineEdit *valueLineEdit =
				(QLineEdit *)keyValueWidget->layout()->itemAt(1)->widget();
			keyLineEdit->setText(QString::fromStdString(pair.first));
			valueLineEdit->setText(QString::fromStdString(pair.second));
			listLayout->insertWidget(listLayout->count() - 1, keyValueWidget);
		}
		// adjust the size of the widget to fit the content
		adjustSize();
	}
};

void get_key_value_as_pairs_from_key_value_list_widget(
	KeyValueListWidget *widget, std::vector<std::pair<std::string, std::string>> &pairs)
{
	pairs.clear();
	for (int i = 0; i < widget->layout()->count() - 1; i++) {
		KeyValueWidget *keyValueWidget =
			(KeyValueWidget *)widget->layout()->itemAt(i)->widget();
		QLineEdit *keyLineEdit = (QLineEdit *)keyValueWidget->layout()->itemAt(0)->widget();
		QLineEdit *valueLineEdit =
			(QLineEdit *)keyValueWidget->layout()->itemAt(1)->widget();
		pairs.push_back(std::make_pair(keyLineEdit->text().toStdString(),
					       valueLineEdit->text().toStdString()));
	}
}

void set_form_row_visibility(QFormLayout *layout, QWidget *widget, bool visible)
{
	for (int i = 0; i < layout->rowCount(); i++) {
		if (layout->itemAt(i, QFormLayout::FieldRole)->widget() == widget) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
			layout->setRowVisible(i, visible);
#else
			layout->itemAt(i, QFormLayout::LabelRole)->widget()->setVisible(visible);
			layout->itemAt(i, QFormLayout::FieldRole)->widget()->setVisible(visible);
#endif
			return;
		}
	}
}

bool add_sources_to_qcombobox(void *list, obs_source_t *obs_source)
{
	const auto source_id = obs_source_get_id(obs_source);
	if (strcmp(source_id, "text_ft2_source_v2") != 0 &&
	    strcmp(source_id, "text_gdiplus_v2") != 0) {
		return true;
	}

	const char *name = obs_source_get_name(obs_source);
	QComboBox *comboList = (QComboBox *)list;
	comboList->addItem(name);
	return true;
}

RequestBuilder::RequestBuilder(url_source_request_data *request_data,
			       std::function<void()> update_handler, QWidget *parent)
	: QDialog(parent), layout(new QVBoxLayout)
{
	setWindowTitle("HTTP Request Builder");
	setLayout(layout);
	// Make modal
	setModal(true);

	// set a minimum width for the dialog
	setMinimumWidth(500);

	QFormLayout *formLayout = new QFormLayout;
	layout->addLayout(formLayout);
	// expand the form layout to fill the parent horizontally
	formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

	// radio buttons to choose URL or File
	QHBoxLayout *urlOrFileLayout = new QHBoxLayout;
	formLayout->addRow("Source:", urlOrFileLayout);
	QRadioButton *urlRadioButton = new QRadioButton("URL");
	urlOrFileLayout->addWidget(urlRadioButton);
	QRadioButton *fileRadioButton = new QRadioButton("File");
	urlOrFileLayout->addWidget(fileRadioButton);
	// mark selected radio button from request_data
	if (request_data->url_or_file == "url") {
		urlRadioButton->setChecked(true);
	} else {
		fileRadioButton->setChecked(true);
	}

	// URL or file path
	QHBoxLayout *urlOrFileInputLayout = new QHBoxLayout;
	formLayout->addRow("URL/File", urlOrFileInputLayout);

	QLineEdit *urlLineEdit = new QLineEdit;
	urlLineEdit->setPlaceholderText("URL/File");
	// set value from request_data
	urlLineEdit->setText(QString::fromStdString(request_data->url));
	urlOrFileInputLayout->addWidget(urlLineEdit);
	// add file selector button if file is selected
	QPushButton *urlOrFileButton = new QPushButton("...");
	urlOrFileInputLayout->addWidget(urlOrFileButton);
	// set value from request_data
	urlOrFileButton->setEnabled(request_data->url_or_file == "file");

	QGroupBox *urlRequestOptionsGroup = new QGroupBox("URL Request Options", this);

	// URL or file dialog
	connect(urlOrFileButton, &QPushButton::clicked, this, [=]() {
		QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), "",
								tr("All Files (*.*)"));
		if (fileName != "") {
			urlLineEdit->setText(fileName);
		}
	});

	urlRequestOptionsGroup->setVisible(request_data->url_or_file == "url");

	auto toggleFileUrlButtons = [=]() {
		urlOrFileButton->setEnabled(fileRadioButton->isChecked());
		// hide the urlRequestOptionsGroup if file is selected
		urlRequestOptionsGroup->setVisible(urlRadioButton->isChecked());
		// adjust the size of the dialog to fit the content
		this->adjustSize();
	};
	// show file selector button if file is selected
	connect(fileRadioButton, &QRadioButton::toggled, this, toggleFileUrlButtons);
	connect(urlRadioButton, &QRadioButton::toggled, this, toggleFileUrlButtons);

	QFormLayout *urlRequestLayout = new QFormLayout;
	urlRequestOptionsGroup->setLayout(urlRequestLayout);
	urlRequestLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
	layout->addWidget(urlRequestOptionsGroup);

	// Method select from dropdown: GET, POST, PUT, DELETE, PATCH
	QComboBox *methodComboBox = new QComboBox;
	methodComboBox->addItem("GET");
	methodComboBox->addItem("POST");
	// set value from request_data
	methodComboBox->setCurrentText(QString::fromStdString(request_data->method));
	// add a label and the method dropdown to the url request options group with horizontal layout
	urlRequestLayout->addRow("Method:", methodComboBox);

	/* --- Authentication via SSL certificates --- */

	QGroupBox *sslGroupBox = new QGroupBox("SSL", this);
	QVBoxLayout *sslLayout = new QVBoxLayout;
	sslGroupBox->setLayout(sslLayout);

	// SSL certificate file using a file selector
	QHBoxLayout *sslCertFileLayout = new QHBoxLayout;
	sslLayout->addLayout(sslCertFileLayout);
	QLineEdit *sslCertFileLineEdit = new QLineEdit;
	sslCertFileLineEdit->setPlaceholderText("SSL certificate file");
	sslCertFileLineEdit->setText(QString::fromStdString(request_data->ssl_client_cert_file));
	sslCertFileLayout->addWidget(sslCertFileLineEdit);
	QPushButton *sslCertFileButton = new QPushButton("...");
	sslCertFileLayout->addWidget(sslCertFileButton);

	// SSL certificate file dialog
	connect(sslCertFileButton, &QPushButton::clicked, this, [=]() {
		QString fileName =
			QFileDialog::getOpenFileName(this, tr("Open SSL certificate file"), "",
						     tr("SSL certificate files (*.pem)"));
		if (fileName != "") {
			sslCertFileLineEdit->setText(fileName);
		}
	});

	// SSL key file using a file selector
	QHBoxLayout *sslKeyFileLayout = new QHBoxLayout;
	sslLayout->addLayout(sslKeyFileLayout);
	QLineEdit *sslKeyFileLineEdit = new QLineEdit;
	sslKeyFileLineEdit->setPlaceholderText("SSL key file");
	sslKeyFileLineEdit->setText(QString::fromStdString(request_data->ssl_client_key_file));
	sslKeyFileLayout->addWidget(sslKeyFileLineEdit);
	QPushButton *sslKeyFileButton = new QPushButton("...");
	sslKeyFileLayout->addWidget(sslKeyFileButton);

	// SSL key file dialog
	connect(sslKeyFileButton, &QPushButton::clicked, this, [=]() {
		QString fileName = QFileDialog::getOpenFileName(this, tr("Open SSL key file"), "",
								tr("SSL key files (*.pem)"));
		if (fileName != "") {
			sslKeyFileLineEdit->setText(fileName);
		}
	});

	// SSL key password
	QLineEdit *sslKeyPasswordLineEdit = new QLineEdit;
	sslKeyPasswordLineEdit->setPlaceholderText("SSL key password");
	sslKeyPasswordLineEdit->setText(QString::fromStdString(request_data->ssl_client_key_pass));
	sslLayout->addWidget(sslKeyPasswordLineEdit);

	// Verify peer checkbox
	QCheckBox *verifyPeerCheckBox = new QCheckBox("Verify peer");
	verifyPeerCheckBox->setChecked(request_data->ssl_verify_peer);
	sslLayout->addWidget(verifyPeerCheckBox);

	CollapseButton *advancedOptionsCollapseButton = new CollapseButton(this);
	advancedOptionsCollapseButton->setText("SSL options");

	/* --- End SSL options --- */

	// Headers
	KeyValueListWidget *headersWidget = new KeyValueListWidget;
	headersWidget->populateFromPairs(request_data->headers);
	// add headers widget to urlRequestLayout with label
	urlRequestLayout->addRow("Headers:", headersWidget);

	// Dynamic data from OBS text source
	QWidget *obsTextSourceWidget = new QWidget;
	QHBoxLayout *obsTextSourceLayout = new QHBoxLayout;
	obsTextSourceWidget->setLayout(obsTextSourceLayout);
	// Add a dropdown to select a OBS text source or None
	QComboBox *obsTextSourceComboBox = new QComboBox;
	obsTextSourceComboBox->addItem("None");
	// populate list of OBS text sources
	obs_enum_sources(add_sources_to_qcombobox, obsTextSourceComboBox);
	// Select the current OBS text source, if any
	int itemIdx = obsTextSourceComboBox->findText(
		QString::fromStdString(request_data->obs_text_source));
	if (itemIdx != -1) {
		obsTextSourceComboBox->setCurrentIndex(itemIdx);
	} else {
		obsTextSourceComboBox->setCurrentIndex(0);
	}
	// add a checkbox to disable the request if the OBS text source is empty
	QCheckBox *obsTextSourceEnabledCheckBox = new QCheckBox("Skip empty");
	obsTextSourceEnabledCheckBox->setChecked(request_data->obs_text_source_skip_if_empty);
	QCheckBox *obsTextSourceSkipSameCheckBox = new QCheckBox("Skip same");
	obsTextSourceSkipSameCheckBox->setChecked(request_data->obs_text_source_skip_if_same);
	// add to urlRequestLayout with horizontal layout
	obsTextSourceLayout->addWidget(obsTextSourceComboBox);
	obsTextSourceLayout->addWidget(obsTextSourceEnabledCheckBox);
	obsTextSourceLayout->addWidget(obsTextSourceSkipSameCheckBox);
	// add to urlRequestLayout as a row
	urlRequestLayout->addRow("Dynamic Input:", obsTextSourceWidget);
	// add a tooltip to explain the dynamic input
	obsTextSourceComboBox->setToolTip(
		"Select a OBS text source to use its current text in the request body as `{input}`.");

	// Body
	QLineEdit *bodyLineEdit = new QLineEdit;
	bodyLineEdit->setPlaceholderText("Body");
	bodyLineEdit->setText(QString::fromStdString(request_data->body));
	// add to urlRequestLayout with horizontal layout
	urlRequestLayout->addRow("Body:", bodyLineEdit);

	auto setVisibilityOfBody = [=]() {
		// If method is not GET, show the body input
		set_form_row_visibility(urlRequestLayout, bodyLineEdit,
					methodComboBox->currentText() != "GET");
		set_form_row_visibility(urlRequestLayout, obsTextSourceWidget,
					methodComboBox->currentText() != "GET");
		this->adjustSize();
	};

	setVisibilityOfBody();

	// Method select from dropdown change
	connect(methodComboBox, &QComboBox::currentTextChanged, this, setVisibilityOfBody);

	urlRequestLayout->addWidget(advancedOptionsCollapseButton);
	sslGroupBox->adjustSize();
	urlRequestLayout->addWidget(sslGroupBox);
	advancedOptionsCollapseButton->setContent(sslGroupBox, this);

	// ------------ Output parsing options --------------
	QGroupBox *outputGroupBox = new QGroupBox("Output Parsing", this);
	layout->addWidget(outputGroupBox);

	QFormLayout *formOutputParsing = new QFormLayout;
	formOutputParsing->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
	outputGroupBox->setLayout(formOutputParsing);

	// Output type: Text, JSON, XML, HTML
	QComboBox *outputTypeComboBox = new QComboBox;
	outputTypeComboBox->addItem("Text");
	outputTypeComboBox->addItem("JSON");
	outputTypeComboBox->addItem("XML (XPath)");
	outputTypeComboBox->addItem("XML (XQuery)");
	outputTypeComboBox->addItem("HTML");
	outputTypeComboBox->setCurrentIndex(
		outputTypeComboBox->findText(QString::fromStdString(request_data->output_type)));
	formOutputParsing->addRow("Content-Type", outputTypeComboBox);

	QLineEdit *outputJSONPointerLineEdit = new QLineEdit;
	outputJSONPointerLineEdit->setText(
		QString::fromStdString(request_data->output_json_pointer));
	outputJSONPointerLineEdit->setPlaceholderText("JSON Pointer");
	formOutputParsing->addRow("JSON Pointer", outputJSONPointerLineEdit);
	QLineEdit *outputJSONPathLineEdit = new QLineEdit;
	outputJSONPathLineEdit->setText(QString::fromStdString(request_data->output_json_path));
	outputJSONPathLineEdit->setPlaceholderText("JSON Path");
	formOutputParsing->addRow("JSON Path", outputJSONPathLineEdit);
	QLineEdit *outputXPathLineEdit = new QLineEdit;
	outputXPathLineEdit->setText(QString::fromStdString(request_data->output_xpath));
	outputXPathLineEdit->setPlaceholderText("XPath");
	formOutputParsing->addRow("XPath", outputXPathLineEdit);
	QLineEdit *outputXQueryLineEdit = new QLineEdit;
	outputXQueryLineEdit->setText(QString::fromStdString(request_data->output_xquery));
	outputXQueryLineEdit->setPlaceholderText("XQuery");
	formOutputParsing->addRow("XQuery", outputXQueryLineEdit);
	QLineEdit *outputRegexLineEdit = new QLineEdit;
	outputRegexLineEdit->setText(QString::fromStdString(request_data->output_regex));
	outputRegexLineEdit->setPlaceholderText("Regex");
	formOutputParsing->addRow("Regex", outputRegexLineEdit);
	QLineEdit *outputRegexFlagsLineEdit = new QLineEdit;
	outputRegexFlagsLineEdit->setText(QString::fromStdString(request_data->output_regex_flags));
	outputRegexFlagsLineEdit->setPlaceholderText("Regex flags");
	formOutputParsing->addRow("Regex flags", outputRegexFlagsLineEdit);
	QLineEdit *outputRegexGroupLineEdit = new QLineEdit;
	outputRegexGroupLineEdit->setText(QString::fromStdString(request_data->output_regex_group));
	outputRegexGroupLineEdit->setPlaceholderText("Regex group");
	formOutputParsing->addRow("Regex group", outputRegexGroupLineEdit);

	auto setVisibilityOfOutputParsingOptions = [=]() {
		// Hide all output parsing options
		for (const auto &widget :
		     {outputJSONPathLineEdit, outputXPathLineEdit, outputXQueryLineEdit,
		      outputRegexLineEdit, outputRegexFlagsLineEdit, outputRegexGroupLineEdit,
		      outputJSONPointerLineEdit}) {
			set_form_row_visibility(formOutputParsing, widget, false);
		}

		// Show the output parsing options for the selected output type
		if (outputTypeComboBox->currentText() == "JSON") {
			set_form_row_visibility(formOutputParsing, outputJSONPathLineEdit, true);
			set_form_row_visibility(formOutputParsing, outputJSONPointerLineEdit, true);
		} else if (outputTypeComboBox->currentText() == "XML (XPath)" ||
			   outputTypeComboBox->currentText() == "HTML") {
			set_form_row_visibility(formOutputParsing, outputXPathLineEdit, true);
		} else if (outputTypeComboBox->currentText() == "XML (XQuery)") {
			set_form_row_visibility(formOutputParsing, outputXQueryLineEdit, true);
		} else if (outputTypeComboBox->currentText() == "Text") {
			set_form_row_visibility(formOutputParsing, outputRegexLineEdit, true);
			set_form_row_visibility(formOutputParsing, outputRegexFlagsLineEdit, true);
			set_form_row_visibility(formOutputParsing, outputRegexGroupLineEdit, true);
		}
	};

	// Show the output parsing options for the selected output type
	setVisibilityOfOutputParsingOptions();
	// Respond to changes in the output type
	connect(outputTypeComboBox, &QComboBox::currentTextChanged, this,
		setVisibilityOfOutputParsingOptions);

	// Add an error message label, hidden by default, color red
	QLabel *errorMessageLabel = new QLabel("Error message");
	errorMessageLabel->setStyleSheet("QLabel { color : red; }");
	errorMessageLabel->setVisible(false);
	layout->addWidget(errorMessageLabel);

	// Lambda to save the request settings to a request_data struct
	auto saveSettingsToRequestData = [=](url_source_request_data *request_data_for_saving) {
		// Save the request settings to the request_data struct
		request_data_for_saving->url = urlLineEdit->text().toStdString();
		request_data_for_saving->url_or_file = urlRadioButton->isChecked() ? "url" : "file";
		request_data_for_saving->method = methodComboBox->currentText().toStdString();
		request_data_for_saving->body = bodyLineEdit->text().toStdString();
		if (obsTextSourceComboBox->currentText() != "None") {
			request_data_for_saving->obs_text_source =
				obsTextSourceComboBox->currentText().toStdString();
		} else {
			request_data_for_saving->obs_text_source = "";
		}
		request_data_for_saving->obs_text_source_skip_if_empty =
			obsTextSourceEnabledCheckBox->isChecked();
		request_data_for_saving->obs_text_source_skip_if_same =
			obsTextSourceSkipSameCheckBox->isChecked();

		// Save the SSL certificate file
		request_data_for_saving->ssl_client_cert_file =
			sslCertFileLineEdit->text().toStdString();

		// Save the SSL key file
		request_data_for_saving->ssl_client_key_file =
			sslKeyFileLineEdit->text().toStdString();

		// Save the SSL key password
		request_data_for_saving->ssl_client_key_pass =
			sslKeyPasswordLineEdit->text().toStdString();

		// Save the verify peer option
		request_data_for_saving->ssl_verify_peer = verifyPeerCheckBox->isChecked();

		// Save the headers
		get_key_value_as_pairs_from_key_value_list_widget(headersWidget,
								  request_data_for_saving->headers);

		// Save the output parsing options
		request_data_for_saving->output_type =
			outputTypeComboBox->currentText().toStdString();
		request_data_for_saving->output_json_pointer =
			outputJSONPointerLineEdit->text().toStdString();
		request_data_for_saving->output_json_path =
			outputJSONPathLineEdit->text().toStdString();
		request_data_for_saving->output_xpath = outputXPathLineEdit->text().toStdString();
		request_data_for_saving->output_xquery = outputXQueryLineEdit->text().toStdString();
		request_data_for_saving->output_regex = outputRegexLineEdit->text().toStdString();
		request_data_for_saving->output_regex_flags =
			outputRegexFlagsLineEdit->text().toStdString();
		request_data_for_saving->output_regex_group =
			outputRegexGroupLineEdit->text().toStdString();
	};

	QPushButton *sendButton = new QPushButton("Test Request");

	connect(sendButton, &QPushButton::clicked, this, [=]() {
		// Hide the error message label
		errorMessageLabel->setVisible(false);

		// Get an interim request_data struct with the current settings
		url_source_request_data *request_data_test = new url_source_request_data;
		saveSettingsToRequestData(request_data_test);

		// Send the request
		struct request_data_handler_response response =
			request_data_handler(request_data_test);

		if (response.status_code == -1) {
			// Show the error message label
			errorMessageLabel->setText(QString::fromStdString(response.error_message));
			errorMessageLabel->setVisible(true);
			return;
		}

		// Display the response
		QDialog *responseDialog = new QDialog(this);
		responseDialog->setWindowTitle("Response");
		QVBoxLayout *responseLayout = new QVBoxLayout;
		responseDialog->setLayout(responseLayout);
		responseDialog->setMinimumWidth(500);
		responseDialog->setMinimumHeight(300);
		responseDialog->show();
		responseDialog->raise();
		responseDialog->activateWindow();
		responseDialog->setModal(true);

		// if there's a request body, add it to the dialog
		if (response.request_body != "") {
			QGroupBox *requestBodyGroupBox = new QGroupBox("Request Body");
			responseLayout->addWidget(requestBodyGroupBox);
			QVBoxLayout *requestBodyLayout = new QVBoxLayout;
			requestBodyGroupBox->setLayout(requestBodyLayout);
			// Add scroll area for the request body
			QScrollArea *requestBodyScrollArea = new QScrollArea;
			QLabel *requestLabel =
				new QLabel(QString::fromStdString(response.request_body));
			// Wrap the text
			requestLabel->setWordWrap(true);
			// Set the label as the scroll area's widget
			requestBodyScrollArea->setWidget(requestLabel);
			requestBodyLayout->addWidget(requestBodyScrollArea);
		}

		QGroupBox *responseBodyGroupBox = new QGroupBox("Response Body");
		responseBodyGroupBox->setLayout(new QVBoxLayout);
		// Add scroll area for the response body
		QScrollArea *responseBodyScrollArea = new QScrollArea;
		QLabel *responseLabel = new QLabel(QString::fromStdString(response.body));
		// Wrap the text
		responseLabel->setWordWrap(true);
		// Set the label as the scroll area's widget
		responseBodyScrollArea->setWidget(responseLabel);
		responseBodyGroupBox->layout()->addWidget(responseBodyScrollArea);
		responseLayout->addWidget(responseBodyGroupBox);

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
					tabWidget->addTab(
						new QLabel(QString::fromStdString(parsedOutput)),
						QString::fromStdString(parsedOutput));
				}
			} else {
				// Add a QLabel to show a single parsed output
				parsedOutputLayout->addWidget(new QLabel(
					QString::fromStdString(response.body_parts_parsed[0])));
			}
		}

		// Resize the dialog to fit the text
		responseDialog->adjustSize();
	});

	// Save button
	QPushButton *saveButton = new QPushButton("Save");

	// put send and save buttons in a horizontal layout
	QHBoxLayout *saveButtonLayout = new QHBoxLayout;
	saveButtonLayout->addWidget(sendButton);
	saveButtonLayout->addWidget(saveButton);
	layout->addLayout(saveButtonLayout);

	connect(saveButton, &QPushButton::clicked, this, [=]() {
		// Save the request settings to the request_data struct
		saveSettingsToRequestData(request_data);

		update_handler();

		// Close dialog
		close();
	});
}
