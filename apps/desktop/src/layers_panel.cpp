#include "layers_panel.hpp"

#include <QLabel>
#include <QListWidget>
#include <QVariant>
#include <QVBoxLayout>

namespace mydesk::desktop {

LayersPanel::LayersPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    auto* title = new QLabel(QStringLiteral("Layers"), this);
    title->setStyleSheet("font-weight: 600; color: #e8eef4;");
    list_ = new QListWidget(this);
    list_->setStyleSheet("QListWidget { background: #151c26; color: #d7e2ec; border: 1px solid "
                         "#2a3644; }");
    layout->addWidget(title);
    layout->addWidget(list_, 1);

    connect(list_, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        if (!item) {
            return;
        }
        emit layerActivated(item->data(Qt::UserRole).toULongLong());
    });
}

void LayersPanel::setDocument(const document::Document& doc) {
    list_->clear();
    for (const auto& layer : doc.layers()) {
        auto* item = new QListWidgetItem(QString::fromStdString(layer.name), list_);
        item->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(layer.id));
        if (layer.id == doc.activeLayerId()) {
            item->setSelected(true);
            list_->setCurrentItem(item);
        }
    }
}

} // namespace mydesk::desktop
