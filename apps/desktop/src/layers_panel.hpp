#pragma once

#include "mydesk/document/document.hpp"

#include <QWidget>

class QListWidget;

namespace mydesk::desktop {

class LayersPanel : public QWidget {
    Q_OBJECT
public:
    explicit LayersPanel(QWidget* parent = nullptr);
    void setDocument(const document::Document& doc);

signals:
    void layerActivated(quint64 layerId);

private:
    QListWidget* list_{nullptr};
};

} // namespace mydesk::desktop
