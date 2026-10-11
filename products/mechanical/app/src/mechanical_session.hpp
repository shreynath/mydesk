#pragma once

#include "mydesk/mechanical/assembly_document.hpp"
#include "mydesk/mechanical/part_document.hpp"

#include <QString>
#include <string>
#include <vector>

namespace mydesk::mechanical::desktop {

class MechanicalSession {
public:
    enum class DocumentKind { Part, Assembly };
    MechanicalSession();

    [[nodiscard]] DocumentKind kind() const { return kind_; }
    [[nodiscard]] const PartDocument& part() const { return part_; }
    [[nodiscard]] const AssemblyDocument& assembly() const { return assembly_; }
    [[nodiscard]] const QString& path() const { return path_; }
    [[nodiscard]] bool isDirty() const { return dirty_; }
    [[nodiscard]] bool canUndo() const { return !undo_.empty(); }
    [[nodiscard]] bool canRedo() const { return !redo_.empty(); }
    [[nodiscard]] TopoDS_Shape displayShape() const;

    void newPart();
    void newAssembly();
    bool createBox(const std::string& name, double width, double height, double depth,
                   QString* error = nullptr);
    bool createHole(const std::string& name, double radius, double depth, double x, double y,
                    QString* error = nullptr);
    bool setParameter(ParameterId id, double value, QString* error = nullptr);
    bool insertCurrentPart(const std::string& occurrenceName, double x, double y, double z,
                           QString* error = nullptr);
    bool setJointPosition(JointId id, double value, QString* error = nullptr);
    bool undo(QString* error = nullptr);
    bool redo(QString* error = nullptr);
    bool open(const QString& path, QString* error = nullptr);
    bool save(QString* error = nullptr);
    bool saveAs(const QString& path, QString* error = nullptr);
    bool exportStep(const QString& path, QString* error = nullptr) const;
    bool exportIges(const QString& path, QString* error = nullptr) const;

private:
    struct Snapshot { DocumentKind kind; std::string data; };
    [[nodiscard]] Snapshot snapshot() const;
    bool restore(const Snapshot& snapshot, QString* error);
    void beginEdit();
    bool regenerate(QString* error);

    DocumentKind kind_{DocumentKind::Part};
    PartDocument part_;
    AssemblyDocument assembly_;
    PartDocument insertionPart_;
    QString path_;
    bool dirty_{false};
    std::vector<Snapshot> undo_;
    std::vector<Snapshot> redo_;
};

} // namespace mydesk::mechanical::desktop
