#include <QtWidgets>
#include <util/bmem.h>

const QString template_text = R"(
<html>
  <head>
    <style>
      p {
				{css_props}
      }
    </style>
  </head>
  <body>
    <p>{text}</p>
  </body>
</html>
)";

/**
  * Render text to a buffer using QTextDocument
  * @param text Text to render
  * @param width Output width
  * @param height Output height
  * @param data Output buffer, user must free
	* @param css_props CSS properties to apply to the text
  */
void render_text_with_qtextdocument(const std::string &text, uint32_t &width, uint32_t &height,
				    uint8_t **data, const std::string &css_props)
{
	// apply response in template
	QString html = QString(template_text)
			       .replace("{text}", QString::fromStdString(text))
			       .replace("{css_props}", QString::fromStdString(css_props));
	QTextDocument textDocument;
	textDocument.setHtml(html);
	textDocument.setTextWidth(640);

	QPixmap pixmap(textDocument.size().toSize());
	pixmap.fill(Qt::transparent);
	QPainter painter;
	painter.begin(&pixmap);
	painter.setCompositionMode(QPainter::CompositionMode_Source);

	// render text
	textDocument.drawContents(&painter);

	painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
	painter.end();

	// save pixmap to buffer
	QImage image = pixmap.toImage();
	// crop to the idealWidth of the text
	image = image.copy(0, 0, textDocument.idealWidth(), image.height());
	// get width and height
	width = image.width();
	height = image.height();
	// allocate output buffer (RGBA), user must free
	*data = (uint8_t *)bzalloc(width * height * 4);
	// copy image data to output buffer
	memcpy(*data, image.bits(), width * height * 4);
}
