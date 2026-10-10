#include "layers_panel.hpp"

#include <QColor>
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
        QString label = QString::fromStdString(layer.name);
        if (!layer.visible) {
            label += QStringLiteral(" [off]");
        }
        if (layer.frozen) {
            label += QStringLiteral(" [frozen]");
        }
        if (layer.locked) {
            label += QStringLiteral(" [lock]");
        }
        if (!layer.plottable) {
            label += QStringLiteral(" [noplot]");
        }
        if (layer.id == doc.activeLayerId()) {
            label += QStringLiteral(" *");
        }
        auto* item = new QListWidgetItem(label, list_);
        item->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(layer.id));
        const auto argb = layer.colorArgb;
        item->setForeground(QColor(static_cast<int>((argb >> 16) & 0xFF),
                                   static_cast<int>((argb >> 8) & 0xFF),
                                   static_cast<int>(argb & 0xFF)));
        if (layer.id == doc.activeLayerId()) {
            item->setSelected(true);
            list_->setCurrentItem(item);
        }
    }
}

} // namespace mydesk::desktop
