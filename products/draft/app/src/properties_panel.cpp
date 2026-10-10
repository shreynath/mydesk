#include "properties_panel.hpp"

#include <QLabel>
#include <QVBoxLayout>

namespace mydesk::desktop {

PropertiesPanel::PropertiesPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    auto* title = new QLabel(QStringLiteral("Properties"), this);
    title->setStyleSheet("font-weight: 600; color: #e8eef4;");
    summary_ = new QLabel(this);
    summary_->setWordWrap(true);
    summary_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    summary_->setStyleSheet("color: #c5d2de;");
    layout->addWidget(title);
    layout->addWidget(summary_, 1);
}

void PropertiesPanel::setDocument(const document::Document& doc) {
    const auto selected = doc.selectionOrdered();
    if (selected.empty()) {
        summary_->setText(QStringLiteral("No selection.\nEntities: %1\nLayers: %2")
                              .arg(doc.entities().size())
                              .arg(doc.layers().size()));
        return;
    }
    QString text;
    for (const auto id : selected) {
        const auto* entity = doc.findEntity(id);
        if (!entity) {
            continue;
        }
        const auto kind = document::entityKindName(entity->kind());
        const auto* layer = doc.findLayer(entity->layerId);
        text += QStringLiteral("#%1 %2\n")
                    .arg(id)
                    .arg(QString::fromUtf8(kind.data(), static_cast<int>(kind.size())));
        text += QStringLiteral("  layer: %1\n")
                    .arg(layer ? QString::fromStdString(layer->name) : QStringLiteral("?"));
        if (entity->colorArgb == document::kColorByLayer) {
            text += QStringLiteral("  color: ByLayer\n");
        } else {
            text += QStringLiteral("  color: #%1\n")
                        .arg(entity->colorArgb & 0xFFFFFFu, 6, 16, QLatin1Char('0'));
        }
        if (const auto* line = std::get_if<document::LineEntity>(&entity->geometry)) {
            text += QStringLiteral("  start: %1, %2\n  end: %3, %4\n")
                        .arg(line->start.x)
                        .arg(line->start.y)
                        .arg(line->end.x)
                        .arg(line->end.y);
        } else if (const auto* circle = std::get_if<document::CircleEntity>(&entity->geometry)) {
            text += QStringLiteral("  center: %1, %2\n  radius: %3\n")
                        .arg(circle->center.x)
                        .arg(circle->center.y)
                        .arg(circle->radius);
        } else if (const auto* arc = std::get_if<document::ArcEntity>(&entity->geometry)) {
            text += QStringLiteral("  center: %1, %2\n  radius: %3\n  angles: %4 → %5 rad\n")
                        .arg(arc->center.x)
                        .arg(arc->center.y)
                        .arg(arc->radius)
                        .arg(arc->startAngle)
                        .arg(arc->endAngle);
        } else if (const auto* pl = std::get_if<document::PolylineEntity>(&entity->geometry)) {
            text += QStringLiteral("  vertices: %1\n  closed: %2\n")
                        .arg(pl->vertices.size())
                        .arg(pl->closed ? QStringLiteral("yes") : QStringLiteral("no"));
        } else if (const auto* dim =
                       std::get_if<document::DimensionLinearEntity>(&entity->geometry)) {
            const auto layout = document::layoutDimension(*dim);
            text += QStringLiteral("  measurement: %1\n")
                        .arg(layout.measurement, 0, 'f', 4);
        }
        text += '\n';
    }
    summary_->setText(text);
}

} // namespace mydesk::desktop
