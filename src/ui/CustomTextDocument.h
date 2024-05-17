#ifndef CUSTOMTEXTDOCUMENT_H
#define CUSTOMTEXTDOCUMENT_H

#include <QtWidgets>
#include <QTextDocument>
#include <QUrl>
#include <QHash>

class CustomTextDocument : public QTextDocument {
	Q_OBJECT

public:
	CustomTextDocument(QObject *parent = nullptr);

protected:
	QVariant loadResource(int type, const QUrl &name) override;

private:
	static QHash<QUrl, QImage> imageCache; // Static member for the cache
};

#endif // CUSTOMTEXTDOCUMENT_H
