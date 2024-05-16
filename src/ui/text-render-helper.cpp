#include "CustomTextDocument.h"
#include <util/bmem.h>

const QString template_text = R"(
<html>

<head>
	<style>
		html,
		body,
		body div,
		span,
		object,
		iframe,
		h1,
		h2,
		h3,
		h4,
		h5,
		h6,
		p,
		blockquote,
		pre,
		abbr,
		address,
		cite,
		code,
		del,
		dfn,
		em,
		img,
		ins,
		kbd,
		q,
		samp,
		small,
		strong,
		sub,
		sup,
		var,
		b,
		i,
		dl,
		dt,
		dd,
		ol,
		ul,
		li,
		fieldset,
		form,
		label,
		legend,
		table,
		caption,
		tbody,
		tfoot,
		thead,
		tr,
		th,
		td,
		article,
		aside,
		figure,
		footer,
		header,
		hgroup,
		menu,
		nav,
		section,
		time,
		mark,
		audio,
		video {
			margin: 0;
			padding: 0;
			border: 0;
			outline: 0;
			font-size: 100%;
			vertical-align: baseline;
			background: transparent;
		}

		#text {
			{{css_props}}
		}
	</style>
</head>

<body>
	<div id="text">{{text}}</div>
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
			       .replace("{{text}}", QString::fromStdString(text))
			       .replace("{{css_props}}", QString::fromStdString(css_props));
	CustomTextDocument textDocument;
	textDocument.setHtml(html);
	textDocument.setTextWidth(width);

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
	// get width and height
	width = image.width();
	height = image.height();
	// allocate output buffer (RGBA), user must free
	*data = (uint8_t *)bzalloc(width * height * 4);
	// copy image data to output buffer
	memcpy(*data, image.bits(), width * height * 4);
}
