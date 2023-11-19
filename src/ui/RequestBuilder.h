#include <QtWidgets>

#include "request-data.h"

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

signals:
	void show_response_dialog_signal(const request_data_handler_response &response);

private:
	Ui::RequestBuilder *ui;
};
