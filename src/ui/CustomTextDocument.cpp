#include "CustomTextDocument.h"

#include "plugin-support.h"
#include "request-data.h"

#include <QImageReader>
#include <QPixmap>
#include <QByteArray>
#include <QPainter>

#include <obs.h>

// Initialize the static member
QHash<QUrl, QImage> CustomTextDocument::imageCache;

CustomTextDocument::CustomTextDocument(QObject *parent) : QTextDocument(parent) {}

QVariant CustomTextDocument::loadResource(int type, const QUrl &name)
{
	if (type == QTextDocument::ImageResource) {
		if (imageCache.contains(name)) {
			return QVariant(imageCache.value(name));
		}
		obs_log(LOG_INFO, "fetch %s", name.toString().toStdString().c_str());
		std::string mime_type;
		std::vector<uint8_t> image_bytes =
			fetch_image(name.toString().toStdString(), mime_type);
		// Create a QByteArray from the std::vector<uint8_t>
		QByteArray imageData(reinterpret_cast<const char *>(image_bytes.data()),
				     image_bytes.size());
		QBuffer buffer(&imageData);
		buffer.open(QIODevice::ReadOnly);
		QImageReader reader(&buffer);
		reader.setDecideFormatFromContent(true);
		QImage image = reader.read();
		if (!image.isNull()) {
			imageCache[name] = image;
			return image;
		}
		obs_log(LOG_ERROR, "Unable to load image from: %s",
			name.toString().toStdString().c_str());
		return QVariant();
	}
	return QTextDocument::loadResource(type, name);
}
