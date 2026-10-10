#pragma once

#include "mydesk/document/document.hpp"

#include <QWidget>

class QLabel;

namespace mydesk::desktop {

class PropertiesPanel : public QWidget {
    Q_OBJECT
public:
    explicit PropertiesPanel(QWidget* parent = nullptr);
    void setDocument(const document::Document& doc);

private:
    QLabel* summary_{nullptr};
};

} // namespace mydesk::desktop
