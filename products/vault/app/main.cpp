// SPDX-License-Identifier: MPL-2.0
// MyDesk Vault — Engineering PDM Standalone Application
#include "mydesk/vault/vault_document.hpp"

#include <iostream>

int main(int argc, char** argv) {
    std::cout << "MyDesk Vault v0.1.0 — Autodesk Vault Class Engineering PDM\n";
    mydesk::vault::VaultRepository repo;
    repo.setName("ProductionPDM");

    const auto pId = repo.addFile("BasePlate.ipt", "$/Designs/Parts/BasePlate.ipt",
                                  "PART_DATA_V1", "engineer1", "Initial plate geometry");
    const auto aId = repo.addFile("GearboxAssembly.iam", "$/Designs/Assemblies/GearboxAssembly.iam",
                                  "ASSEMBLY_DATA_V1", "engineer1", "Top level assembly");
    repo.addDependency(aId, pId);

    std::cout << "Repository files: " << repo.files().size() << "\n";
    auto parents = repo.whereUsed(pId);
    std::cout << "BasePlate.ipt is used in " << parents.size() << " parent assembly/assemblies.\n";

    auto bom = repo.generateBom(aId);
    std::cout << "GearboxAssembly.iam BOM has " << bom.size() << " item(s).\n";

    (void)argc;
    (void)argv;
    return 0;
}
