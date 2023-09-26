
#include "RequestBuilder.h"
#include "CollapseButton.h"

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

	// Body - used later
	QLineEdit *bodyLineEdit = new QLineEdit;

	// Method select from dropdown: GET, POST, PUT, DELETE, PATCH
	QComboBox *methodComboBox = new QComboBox;
	methodComboBox->addItem("GET");
	methodComboBox->addItem("POST");
	// set value from request_data
	methodComboBox->setCurrentText(QString::fromStdString(request_data->method));
	connect(methodComboBox, &QComboBox::currentTextChanged, this, [=]() {
		// If method is not GET, show the body input
		urlRequestLayout->setRowVisible(bodyLineEdit,
						methodComboBox->currentText() != "GET");
		this->adjustSize();
	});
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
	urlRequestLayout->addWidget(advancedOptionsCollapseButton);
	sslGroupBox->adjustSize();
	urlRequestLayout->addWidget(sslGroupBox);
	advancedOptionsCollapseButton->setContent(sslGroupBox, this);

	/* --- End SSL options --- */

	// Headers
	KeyValueListWidget *headersWidget = new KeyValueListWidget;
	headersWidget->populateFromPairs(request_data->headers);
	// add headers widget to urlRequestLayout with label
	urlRequestLayout->addRow("Headers:", headersWidget);

	// Body
	bodyLineEdit->setPlaceholderText("Body");
	bodyLineEdit->setText(QString::fromStdString(request_data->body));
	// add to urlRequestLayout with horizontal layout
	urlRequestLayout->addRow("Body:", bodyLineEdit);
	// Hide if method is GET
	urlRequestLayout->setRowVisible(bodyLineEdit, request_data->method == "POST");

	// Output parsing options
	QGroupBox *outputGroupBox = new QGroupBox("Output Parsing", this);
	layout->addWidget(outputGroupBox);

	QFormLayout *formOutputParsing = new QFormLayout;
	formOutputParsing->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
	outputGroupBox->setLayout(formOutputParsing);

	// Output type: Text, JSON, XML, HTML
	QComboBox *outputTypeComboBox = new QComboBox;
	outputTypeComboBox->addItem("Text");
	outputTypeComboBox->addItem("JSON");
	outputTypeComboBox->addItem("XML");
	outputTypeComboBox->addItem("HTML");
	outputTypeComboBox->setCurrentIndex(
		outputTypeComboBox->findText(QString::fromStdString(request_data->output_type)));
	formOutputParsing->addRow("Content-Type", outputTypeComboBox);

	QLineEdit *outputJSONPathLineEdit = new QLineEdit;
	outputJSONPathLineEdit->setText(QString::fromStdString(request_data->output_json_path));
	outputJSONPathLineEdit->setPlaceholderText("JSON Pointer");
	formOutputParsing->addRow("JSON Pointer", outputJSONPathLineEdit);
	QLineEdit *outputXPathLineEdit = new QLineEdit;
	outputXPathLineEdit->setText(QString::fromStdString(request_data->output_xpath));
	outputXPathLineEdit->setPlaceholderText("XPath");
	formOutputParsing->addRow("XPath", outputXPathLineEdit);
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

	formOutputParsing->setRowVisible(outputJSONPathLineEdit,
					 outputTypeComboBox->currentText() == "JSON");
	formOutputParsing->setRowVisible(outputXPathLineEdit,
					 outputTypeComboBox->currentText() == "XML" ||
						 outputTypeComboBox->currentText() == "HTML");
	formOutputParsing->setRowVisible(outputRegexLineEdit,
					 outputTypeComboBox->currentText() == "Text");
	formOutputParsing->setRowVisible(outputRegexFlagsLineEdit,
					 outputTypeComboBox->currentText() == "Text");
	formOutputParsing->setRowVisible(outputRegexGroupLineEdit,
					 outputTypeComboBox->currentText() == "Text");

	connect(outputTypeComboBox, &QComboBox::currentTextChanged, this, [=]() {
		formOutputParsing->setRowVisible(outputJSONPathLineEdit,
						 outputTypeComboBox->currentText() == "JSON");
		formOutputParsing->setRowVisible(
			outputXPathLineEdit, outputTypeComboBox->currentText() == "XML" ||
						     outputTypeComboBox->currentText() == "HTML");
		formOutputParsing->setRowVisible(outputRegexLineEdit,
						 outputTypeComboBox->currentText() == "Text");
		formOutputParsing->setRowVisible(outputRegexFlagsLineEdit,
						 outputTypeComboBox->currentText() == "Text");
		formOutputParsing->setRowVisible(outputRegexGroupLineEdit,
						 outputTypeComboBox->currentText() == "Text");
		// adjust the size of the dialog to fit the content
		this->adjustSize();
	});

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
		request_data_for_saving->output_json_path =
			outputJSONPathLineEdit->text().toStdString();
		request_data_for_saving->output_xpath = outputXPathLineEdit->text().toStdString();
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
		// Add scroll area for the response body
		QScrollArea *responseBodyScrollArea = new QScrollArea;
		QLabel *responseLabel = new QLabel(QString::fromStdString(response.body));
		// Wrap the text
		responseLabel->setWordWrap(true);
		// Set the label as the scroll area's widget
		responseBodyScrollArea->setWidget(responseLabel);
		responseLayout->addWidget(responseBodyScrollArea);

		// If there's a parsed output, add it to the dialog in a QGroupBox
		if (response.body_parsed != "") {
			QGroupBox *parsedOutputGroupBox = new QGroupBox("Parsed Output");
			responseLayout->addWidget(parsedOutputGroupBox);
			QVBoxLayout *parsedOutputLayout = new QVBoxLayout;
			parsedOutputGroupBox->setLayout(parsedOutputLayout);
			parsedOutputLayout->addWidget(
				new QLabel(QString::fromStdString(response.body_parsed)));
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
