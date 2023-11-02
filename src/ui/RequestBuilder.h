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

private:
	QVBoxLayout *layout;
	Ui::RequestBuilder *ui;
};
