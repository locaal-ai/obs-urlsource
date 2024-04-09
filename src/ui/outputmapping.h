#ifndef OUTPUTMAPPING_H
#define OUTPUTMAPPING_H

#include <QDialog>
#include <QStandardItemModel>
#include <QComboBox>
#include <functional>

#include "mapping-data.h"

namespace Ui {
class OutputMapping;
}

class OutputMapping : public QDialog {
	Q_OBJECT

public:
	explicit OutputMapping(output_mapping_data *mapping_data_in,
			       std::function<void()> update_handler, QWidget *parent = nullptr);
	~OutputMapping();

	OutputMapping(const OutputMapping &) = delete;
	OutputMapping &operator=(const OutputMapping &) = delete;

private:
	Ui::OutputMapping *ui;
	QStandardItemModel model;
	output_mapping_data *mapping_data;
	QComboBox *createSourcesComboBox();
	std::function<void()> update_handler;

private slots:
	void addMapping();
	void removeMapping();
};

#endif // OUTPUTMAPPING_H
