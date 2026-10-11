#include "mechanical_session.hpp"

#include <QFile>
#include <QSaveFile>
#include <exception>

namespace mydesk::mechanical::desktop {

MechanicalSession::MechanicalSession() { newPart(); }

void MechanicalSession::newPart() {
    kind_ = DocumentKind::Part;
    part_ = PartDocument{};
    insertionPart_ = PartDocument{};
    path_.clear();
    dirty_ = false;
    undo_.clear();
    redo_.clear();
}

void MechanicalSession::newAssembly() {
    insertionPart_ = part_;
    kind_ = DocumentKind::Assembly;
    assembly_ = AssemblyDocument{};
    path_.clear();
    dirty_ = false;
    undo_.clear();
    redo_.clear();
}

MechanicalSession::Snapshot MechanicalSession::snapshot() const {
    return {kind_, kind_ == DocumentKind::Part ? part_.serializeNative()
                                               : assembly_.serializeNative()};
}

void MechanicalSession::beginEdit() {
    undo_.push_back(snapshot());
    if (undo_.size() > 64) undo_.erase(undo_.begin());
    redo_.clear();
}

bool MechanicalSession::regenerate(QString* error) {
    if (kind_ == DocumentKind::Part) {
        const auto report = part_.regenerate();
        if (!report.ok) {
            if (error) *error = QString::fromStdString(report.message);
            return false;
        }
    } else {
        const auto report = assembly_.solve();
        if (!report.ok) {
            if (error) *error = QString::fromStdString(report.message);
            return false;
        }
    }
    return true;
}

bool MechanicalSession::createBox(const std::string& name, double width, double height,
                                  double depth, QString* error) {
    if (kind_ != DocumentKind::Part) {
        if (error) *error = "Box features can only be created in a part document.";
        return false;
    }
    const auto before = snapshot();
    try {
        beginEdit();
        part_.addRectangleExtrude(name + " Sketch", name, width, height, depth);
        if (!regenerate(error)) {
            restore(before, nullptr);
            undo_.pop_back();
            return false;
        }
    } catch (const std::exception& ex) {
        restore(before, nullptr);
        if (!undo_.empty()) undo_.pop_back();
        if (error) *error = QString::fromUtf8(ex.what());
        return false;
    }
    dirty_ = true;
    return true;
}

bool MechanicalSession::createHole(const std::string& name, double radius, double depth,
                                   double x, double y, QString* error) {
    if (kind_ != DocumentKind::Part || part_.features().empty() || !part_.hasSolid()) {
        if (error) *error = "Create and regenerate a solid before adding a hole.";
        return false;
    }
    const auto before = snapshot();
    try {
        beginEdit();
        part_.addHole(name, part_.features().back().id, radius, depth, x, y);
        if (!regenerate(error)) {
            restore(before, nullptr);
            undo_.pop_back();
            return false;
        }
    } catch (const std::exception& ex) {
        restore(before, nullptr);
        if (!undo_.empty()) undo_.pop_back();
        if (error) *error = QString::fromUtf8(ex.what());
        return false;
    }
    dirty_ = true;
    return true;
}

bool MechanicalSession::setParameter(ParameterId id, double value, QString* error) {
    if (kind_ != DocumentKind::Part) return false;
    const auto before = snapshot();
    beginEdit();
    if (!part_.setParameter(id, value) || !regenerate(error)) {
        restore(before, nullptr);
        undo_.pop_back();
        if (error && error->isEmpty()) *error = "The parameter value is invalid.";
        return false;
    }
    dirty_ = true;
    return true;
}

bool MechanicalSession::insertCurrentPart(const std::string& occurrenceName, double x, double y,
                                          double z, QString* error) {
    if (kind_ != DocumentKind::Assembly || !insertionPart_.hasSolid()) {
        if (error) *error = "No regenerated source part is available for insertion.";
        return false;
    }
    beginEdit();
    assembly_.addOccurrence(occurrenceName, insertionPart_, x, y, z,
                            assembly_.occurrences().empty());
    if (!regenerate(error)) {
        undo(nullptr);
        return false;
    }
    dirty_ = true;
    return true;
}

bool MechanicalSession::setJointPosition(JointId id, double value, QString* error) {
    if (kind_ != DocumentKind::Assembly) return false;
    beginEdit();
    if (!assembly_.setJointPosition(id, value) || !regenerate(error)) {
        undo(nullptr);
        if (error && error->isEmpty()) *error = "Position exceeds the joint limits.";
        return false;
    }
    dirty_ = true;
    return true;
}

bool MechanicalSession::restore(const Snapshot& state, QString* error) {
    std::string parseError;
    if (state.kind == DocumentKind::Part) {
        auto restored = PartDocument::deserializeNative(state.data, &parseError);
        if (!parseError.empty()) {
            if (error) *error = QString::fromStdString(parseError);
            return false;
        }
        part_ = std::move(restored);
    } else {
        auto restored = AssemblyDocument::deserializeNative(state.data, &parseError);
        if (!parseError.empty()) {
            if (error) *error = QString::fromStdString(parseError);
            return false;
        }
        assembly_ = std::move(restored);
    }
    kind_ = state.kind;
    return true;
}

bool MechanicalSession::undo(QString* error) {
    if (undo_.empty()) return false;
    const auto target = undo_.back();
    const auto current = snapshot();
    if (!restore(target, error)) return false;
    undo_.pop_back();
    redo_.push_back(current);
    dirty_ = true;
    return true;
}

bool MechanicalSession::redo(QString* error) {
    if (redo_.empty()) return false;
    const auto target = redo_.back();
    const auto current = snapshot();
    if (!restore(target, error)) return false;
    redo_.pop_back();
    undo_.push_back(current);
    dirty_ = true;
    return true;
}

bool MechanicalSession::open(const QString& filePath, QString* error) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    const QByteArray bytes = file.readAll();
    const std::string data(bytes.constData(), static_cast<std::size_t>(bytes.size()));
    Snapshot candidate{data.find("mydesk-mechanical-assembly") != std::string::npos
                           ? DocumentKind::Assembly : DocumentKind::Part, data};
    if (!restore(candidate, error)) return false;
    if (kind_ == DocumentKind::Part) insertionPart_ = part_;
    path_ = filePath;
    dirty_ = false;
    undo_.clear();
    redo_.clear();
    return true;
}

bool MechanicalSession::save(QString* error) {
    if (path_.isEmpty()) {
        if (error) *error = "Choose a document path first.";
        return false;
    }
    return saveAs(path_, error);
}

bool MechanicalSession::saveAs(const QString& filePath, QString* error) {
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    const auto data = snapshot().data;
    if (file.write(data.data(), static_cast<qint64>(data.size())) !=
        static_cast<qint64>(data.size()) || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    path_ = filePath;
    dirty_ = false;
    return true;
}

bool MechanicalSession::exportStep(const QString& filePath, QString* error) const {
    std::string nativeError;
    const bool ok = kind_ == DocumentKind::Part
                        ? part_.exportStep(filePath.toStdString(), &nativeError)
                        : assembly_.exportStep(filePath.toStdString(), &nativeError);
    if (!ok && error) *error = QString::fromStdString(nativeError);
    return ok;
}

bool MechanicalSession::exportIges(const QString& filePath, QString* error) const {
    if (kind_ != DocumentKind::Part) {
        if (error) *error = "IGES export is currently available for parts only.";
        return false;
    }
    std::string nativeError;
    const bool ok = part_.exportIges(filePath.toStdString(), &nativeError);
    if (!ok && error) *error = QString::fromStdString(nativeError);
    return ok;
}

TopoDS_Shape MechanicalSession::displayShape() const {
    return kind_ == DocumentKind::Part ? part_.shape() : assembly_.buildAssemblyShape();
}

} // namespace mydesk::mechanical::desktop
