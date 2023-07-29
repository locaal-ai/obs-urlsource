
#include <obs.h>

#include "RequestBuilder.h"

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

	// set a minimum width for the dialog
	setMinimumWidth(500);

	QFormLayout *formLayout = new QFormLayout;
	layout->addLayout(formLayout);
	// expand the form layout to fill the parent horizontally
	formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

	// URL
	QLineEdit *urlLineEdit = new QLineEdit;
	urlLineEdit->setPlaceholderText("URL");
	formLayout->addRow("URL:", urlLineEdit);
	// set value from request_data
	urlLineEdit->setText(QString::fromStdString(request_data->url));

	// Body - used later
	QLineEdit *bodyLineEdit = new QLineEdit;

	// Method select from dropdown: GET, POST, PUT, DELETE, PATCH
	QComboBox *methodComboBox = new QComboBox;
	methodComboBox->addItem("GET");
	methodComboBox->addItem("POST");
	formLayout->addRow("Method:", methodComboBox);
	// set value from request_data
	methodComboBox->setCurrentText(QString::fromStdString(request_data->method));
	connect(methodComboBox, &QComboBox::currentTextChanged, this, [=]() {
		// If method is not GET, show the body input
		if (methodComboBox->currentText() != "GET") {
			formLayout->setRowVisible(bodyLineEdit, true);
		} else {
			formLayout->setRowVisible(bodyLineEdit, false);
		}
	});

	// Headers
	KeyValueListWidget *headersWidget = new KeyValueListWidget;
	headersWidget->populateFromPairs(request_data->headers);
	formLayout->addRow("Headers:", headersWidget);

	// Body
	bodyLineEdit->setPlaceholderText("Body");
	formLayout->addRow("Body:", bodyLineEdit);
	// Hide if method is GET
	formLayout->setRowVisible(bodyLineEdit, request_data->method == "POST");

	// Output parsing options
	QGroupBox *outputGroupBox = new QGroupBox("Output Parsing");
	layout->addWidget(outputGroupBox);

	QFormLayout *formOutputParsing = new QFormLayout;
	outputGroupBox->setLayout(formOutputParsing);

	// Output type: Text, JSON, XML, HTML
	QComboBox *outputTypeComboBox = new QComboBox;
	outputTypeComboBox->addItem("Text");
	outputTypeComboBox->addItem("JSON");
	outputTypeComboBox->addItem("XML");
	outputTypeComboBox->addItem("HTML");
	outputTypeComboBox->setCurrentIndex(1); // JSON is the default
	formOutputParsing->addRow("Content-Type", outputTypeComboBox);

	QLineEdit *outputJSONPathLineEdit = new QLineEdit;
	outputJSONPathLineEdit->setText(QString::fromStdString(request_data->output_json_path));
	formOutputParsing->addRow("JSON Pointer", outputJSONPathLineEdit);
	QLineEdit *outputXPathLineEdit = new QLineEdit;
	outputXPathLineEdit->setText(QString::fromStdString(request_data->output_xpath));
	formOutputParsing->addRow("XPath", outputXPathLineEdit);
	formOutputParsing->setRowVisible(outputXPathLineEdit, false);
	QLineEdit *outputRegexLineEdit = new QLineEdit;
	outputRegexLineEdit->setText(QString::fromStdString(request_data->output_regex));
	formOutputParsing->addRow("Regex", outputRegexLineEdit);
	formOutputParsing->setRowVisible(outputRegexLineEdit, false);
	QLineEdit *outputRegexFlagsLineEdit = new QLineEdit;
	outputRegexFlagsLineEdit->setText(QString::fromStdString(request_data->output_regex_flags));
	formOutputParsing->addRow("Regex flags", outputRegexFlagsLineEdit);
	formOutputParsing->setRowVisible(outputRegexFlagsLineEdit, false);
	QLineEdit *outputRegexGroupLineEdit = new QLineEdit;
	outputRegexGroupLineEdit->setText(QString::fromStdString(request_data->output_regex_group));
	formOutputParsing->addRow("Regex group", outputRegexGroupLineEdit);
	formOutputParsing->setRowVisible(outputRegexGroupLineEdit, false);

	connect(outputTypeComboBox, &QComboBox::currentTextChanged, this, [=]() {
		if (outputTypeComboBox->currentText() == "JSON") {
			// Show the JSONPath input, hide others
			formOutputParsing->setRowVisible(outputJSONPathLineEdit, true);
			formOutputParsing->setRowVisible(outputXPathLineEdit, false);
			formOutputParsing->setRowVisible(outputRegexLineEdit, false);
			formOutputParsing->setRowVisible(outputRegexFlagsLineEdit, false);
			formOutputParsing->setRowVisible(outputRegexGroupLineEdit, false);
		}

		// If XML or HTML is selected as the output type, show the XPath input
		if (outputTypeComboBox->currentText() == "XML" ||
		    outputTypeComboBox->currentText() == "HTML") {
			// Show the XPath input, hide others
			formOutputParsing->setRowVisible(outputJSONPathLineEdit, false);
			formOutputParsing->setRowVisible(outputXPathLineEdit, true);
			formOutputParsing->setRowVisible(outputRegexLineEdit, false);
			formOutputParsing->setRowVisible(outputRegexFlagsLineEdit, false);
			formOutputParsing->setRowVisible(outputRegexGroupLineEdit, false);
		}

		// If text is selected as the output type, show the regex input
		if (outputTypeComboBox->currentText() == "Text") {
			// Show the regex input, hide others
			formOutputParsing->setRowVisible(outputJSONPathLineEdit, false);
			formOutputParsing->setRowVisible(outputXPathLineEdit, false);
			formOutputParsing->setRowVisible(outputRegexLineEdit, true);
			formOutputParsing->setRowVisible(outputRegexFlagsLineEdit, true);
			formOutputParsing->setRowVisible(outputRegexGroupLineEdit, true);
		}
	});

	QPushButton *sendButton = new QPushButton("Test Request");
	layout->addWidget(sendButton);

	// Add an error message label, hidden by default, color red
	QLabel *errorMessageLabel = new QLabel("Error message");
	errorMessageLabel->setStyleSheet("QLabel { color : red; }");
	errorMessageLabel->setVisible(false);
	layout->addWidget(errorMessageLabel);

	// Lambda to save the request settings to a request_data struct
	auto saveSettingsToRequestData = [=](url_source_request_data *request_data_for_saving) {
		// Save the request settings to the request_data struct
		request_data_for_saving->url = urlLineEdit->text().toStdString();
		request_data_for_saving->method = methodComboBox->currentText().toStdString();
		request_data_for_saving->body = bodyLineEdit->text().toStdString();

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
		responseDialog->show();
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
	layout->addWidget(saveButton);
	connect(saveButton, &QPushButton::clicked, this, [=]() {
		// Save the request settings to the request_data struct
		saveSettingsToRequestData(request_data);

		update_handler();

		// Close dialog
		close();
	});
}
