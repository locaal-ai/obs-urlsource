#ifndef TEXT_RENDER_HELPER_H
#define TEXT_RENDER_HELPER_H

void render_text_with_qtextdocument(const std::string &text, uint32_t &width, uint32_t &height,
				    uint8_t **data, const std::string &css_props);

#endif // TEXT_RENDER_HELPER_H
