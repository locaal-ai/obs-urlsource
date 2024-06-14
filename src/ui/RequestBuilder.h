#include <QtWidgets>

#include "request-data.h"
#include "mapping-data.h"

namespace Ui {
class RequestBuilder;
}

class RequestBuilder : public QDialog {
	Q_OBJECT
public:
	RequestBuilder(url_source_request_data *request_data,
		       // update handler lambda function
		       std::function<void()> update_handler, QWidget *parent = nullptr);

private slots:
	void show_response_dialog(const request_data_handler_response &response);
	void show_error_message(const std::string &error_message);

signals:
	void show_response_dialog_signal(const request_data_handler_response &response);
	void show_error_message_signal(const std::string &error_message);

private:
	Ui::RequestBuilder *ui;
	inputs_data inputs_data_;
};
