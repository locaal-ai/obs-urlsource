#include <QtWidgets>
#include <util/bmem.h>

const QString template_text = R"(
<html>
  <head>
    <style>
      body {
        background-color: #00000000;
        color: #FFFFFF;
      }
    </style>
  </head>
  <body>
    <h1>{text}</h1>
  </body>
</html>
)";

/**
  * Render text to a buffer using QTextDocument
  * @param text Text to render
  * @param width Output width
  * @param height Output height
  * @param data Output buffer, user must free
  */
void render_text_with_qtextdocument(std::string &text, uint32_t &width, uint32_t &height,
				    uint8_t **data)
{
	// apply response in template
	QString html = QString(template_text).replace("{text}", QString::fromStdString(text));
	QTextDocument textDocument;
	textDocument.setHtml(html);

	QPixmap pixmap(textDocument.size().toSize());
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);

	// render text
	textDocument.drawContents(&painter);

	// save pixmap to buffer
	QImage image = pixmap.toImage();
	// get width and height
	width = image.width();
	height = image.height();
	// allocate output buffer (RGBA), user must free
	*data = (uint8_t *)bzalloc(width * height * 4);
	// copy image data to output buffer
	memcpy(*data, image.bits(), width * height * 4);
}
