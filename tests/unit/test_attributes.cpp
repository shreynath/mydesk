#include "../test_assert.hpp"

#include "mydesk/commands/session.hpp"
#include "mydesk/document/document.hpp"
#include "mydesk/exchange/dxf_reader.hpp"
#include "mydesk/exchange/dxf_writer.hpp"
#include "mydesk/exchange/native_format.hpp"

#include <cmath>
#include <fstream>

void test_block_attributes_and_exchange() {
    using namespace mydesk;

    // 1. Document API & Block attributes definition
    document::Document doc;
    auto lId = doc.addLine({0.0, 0.0}, {10.0, 0.0});
    auto att1 = doc.addAttdef({0.0, 2.0}, 2.5, "DESCR", "Enter Description", "Valve", 0.0, false);
    auto att2 = doc.addAttdef({0.0, -2.0}, 2.0, "ID", "Tag Number", "V-101", 0.0, true);

    expect(doc.findEntity(att1) != nullptr, "attdef 1 created");
    expect(doc.findEntity(att2) != nullptr, "attdef 2 created");

    doc.select(lId);
    doc.select(att1);
    doc.select(att2);

    bool blockOk = doc.defineBlockFromSelection("TAGGED_VALVE", {0.0, 0.0});
    expect(blockOk, "defineBlockFromSelection succeeded");

    const auto* blk = doc.findBlock("TAGGED_VALVE");
    expect(blk != nullptr, "block found");
    expect(blk->primitives.size() == 1, "1 primitive in block");
    expect(blk->attributes.size() == 2, "2 attribute defs in block");
    expect(blk->attributes[0].tag == "DESCR", "tag 0 is DESCR");
    expect(blk->attributes[0].defaultValue == "Valve", "default 0 is Valve");
    expect(!blk->attributes[0].invisible, "attr 0 visible");
    expect(blk->attributes[1].tag == "ID", "tag 1 is ID");
    expect(blk->attributes[1].defaultValue == "V-101", "default 1 is V-101");
    expect(blk->attributes[1].invisible, "attr 1 invisible");

    // Source entities should be replaced by insert of the newly defined block
    expect(doc.entities().size() == 1 && doc.entities().front().kind() == document::EntityKind::Insert,
           "source entities replaced by insert on block define");

    // 2. Insert with default attributes
    auto insId1 = doc.addInsert("TAGGED_VALVE", {100.0, 200.0}, 2.0, 0.0);
    const auto* insEnt1 = doc.findEntity(insId1);
    expect(insEnt1 != nullptr, "insert 1 found");
    const auto* insGeo1 = std::get_if<document::InsertEntity>(&insEnt1->geometry);
    expect(insGeo1 != nullptr, "is InsertEntity");
    expect(insGeo1->attributes.size() == 2, "insert 1 has 2 attributes");
    expect(insGeo1->attributes[0].tag == "DESCR", "attr 0 tag DESCR");
    expect(insGeo1->attributes[0].value == "Valve", "attr 0 default value");
    expect(std::abs(insGeo1->attributes[0].position.x - 100.0) < 1e-4, "attr 0 x position");
    expect(std::abs(insGeo1->attributes[0].position.y - 204.0) < 1e-4, "attr 0 y position");
    expect(std::abs(insGeo1->attributes[0].height - 5.0) < 1e-4, "attr 0 scaled height");

    // 3. Insert with attribute overrides
    std::unordered_map<std::string, std::string> overrides;
    overrides["DESCR"] = "Check Valve";
    overrides["ID"] = "CV-202";
    auto insId2 = doc.addInsert("TAGGED_VALVE", {300.0, 400.0}, 1.0, 0.0, overrides);
    const auto* insEnt2 = doc.findEntity(insId2);
    const auto* insGeo2 = std::get_if<document::InsertEntity>(&insEnt2->geometry);
    expect(insGeo2 != nullptr, "insert 2 is InsertEntity");
    expect(insGeo2->attributes.size() == 2, "insert 2 has 2 attributes");
    expect(insGeo2->attributes[0].value == "Check Valve", "override value 0");
    expect(insGeo2->attributes[1].value == "CV-202", "override value 1");

    // 4. Attribute editing & Undo / Redo
    bool edited = doc.editInsertAttribute(insId2, "DESCR", "Ball Valve");
    expect(edited, "attribute edited");
    insEnt2 = doc.findEntity(insId2);
    insGeo2 = std::get_if<document::InsertEntity>(&insEnt2->geometry);
    expect(insGeo2->attributes[0].value == "Ball Valve", "value updated to Ball Valve");

    expect(doc.undo(), "undo attribute edit");
    insEnt2 = doc.findEntity(insId2);
    insGeo2 = std::get_if<document::InsertEntity>(&insEnt2->geometry);
    expect(insGeo2->attributes[0].value == "Check Valve", "value restored after undo");

    expect(doc.redo(), "redo attribute edit");
    insEnt2 = doc.findEntity(insId2);
    insGeo2 = std::get_if<document::InsertEntity>(&insEnt2->geometry);
    expect(insGeo2->attributes[0].value == "Ball Valve", "value reapplied after redo");

    // 5. ATTDISP visibility modes
    expect(doc.attributeDisplayMode() == document::AttributeDisplayMode::Normal, "default is Normal");
    doc.setAttributeDisplayMode(document::AttributeDisplayMode::On);
    expect(doc.attributeDisplayMode() == document::AttributeDisplayMode::On, "mode On");
    doc.setAttributeDisplayMode(document::AttributeDisplayMode::Off);
    expect(doc.attributeDisplayMode() == document::AttributeDisplayMode::Off, "mode Off");
    doc.setAttributeDisplayMode(document::AttributeDisplayMode::Normal);

    // 6. Explode insert with attributes
    auto explodeRes = doc.explodeEntities({insId1});
    expect(explodeRes.created.size() == 3, "1 line + 2 attribute texts created on explode");
    bool foundDescrText = false;
    for (auto cid : explodeRes.created) {
        const auto* ce = doc.findEntity(cid);
        if (ce && std::holds_alternative<document::TextEntity>(ce->geometry)) {
            const auto& txt = std::get<document::TextEntity>(ce->geometry);
            if (txt.contents == "Valve") {
                foundDescrText = true;
                expect(std::abs(txt.height - 5.0) < 1e-4, "exploded text height matches scaled attr");
            }
        }
    }
    expect(foundDescrText, "exploded attribute converted to TextEntity");

    // 7. Native JSON roundtrip
    const std::string nativeJson = exchange::serializeNative(doc);
    document::Document docNative;
    auto reportNative = exchange::deserializeNative(docNative, nativeJson);
    expect(reportNative.ok, "native deserialization succeeded");
    expect(docNative.blocks().size() == 1, "blocks restored in native format");
    const auto* blkNative = docNative.findBlock("TAGGED_VALVE");
    expect(blkNative != nullptr, "block found in restored doc");
    expect(blkNative->attributes.size() == 2, "attributes restored in block def");
    expect(blkNative->attributes[0].tag == "DESCR", "tag restored");
    expect(blkNative->attributes[1].invisible, "invisible flag restored");

    const auto* insNative = docNative.findEntity(insId2);
    expect(insNative != nullptr, "insert 2 restored in native format");
    const auto* insNativeGeo = std::get_if<document::InsertEntity>(&insNative->geometry);
    expect(insNativeGeo != nullptr, "restored entity is insert");
    expect(insNativeGeo->attributes.size() == 2, "insert attributes restored");
    expect(insNativeGeo->attributes[0].value == "Ball Valve", "insert attribute value restored");

    // 8. DXF export & import roundtrip
    const auto dir = std::filesystem::temp_directory_path() / "mydesk_tests";
    std::filesystem::create_directories(dir);
    const auto dxfPath = dir / "attributes_roundtrip.dxf";

    auto expDxf = exchange::exportDxfR12(doc, dxfPath);
    expect(expDxf.ok, "DXF export ok");

    document::Document docDxf;
    auto reportDxf = exchange::importDxf(docDxf, dxfPath);
    expect(reportDxf.ok, "DXF import succeeded");
    expect(docDxf.blocks().size() >= 1, "DXF block imported");
    const auto* blkDxf = docDxf.findBlock("TAGGED_VALVE");
    expect(blkDxf != nullptr, "TAGGED_VALVE block defined from DXF");
    expect(blkDxf->attributes.size() == 2, "DXF imported ATTDEFs in block");

    // 9. Interactive command execution via Session
    commands::Session session;
    auto resAtt = session.executeLine("ATTDEF PART PART_NO P-100 10,10 3.0 0");
    expect(resAtt.ok, resAtt.message);
    expect(session.document().entities().size() == 1, "attdef entity created by command");

    session.document().select(session.document().entities().front().id);
    auto resBlk = session.executeLine("BLOCK PUMP 0,0");
    expect(resBlk.ok, resBlk.message);
    expect(session.document().findBlock("PUMP") != nullptr, "PUMP block created");

    auto resIns = session.executeLine("INSERT PUMP 50,50 1 0");
    expect(resIns.ok, resIns.message);
    auto pumpInsId = session.document().entities().back().id;

    session.document().clearSelection();
    session.document().select(pumpInsId);
    auto resEdit = session.executeLine("ATTEDIT PART P-999");
    expect(resEdit.ok, resEdit.message);
    const auto* pumpIns = session.document().findEntity(pumpInsId);
    const auto* pumpGeo = std::get_if<document::InsertEntity>(&pumpIns->geometry);
    expect(pumpGeo != nullptr && !pumpGeo->attributes.empty(), "pump insert has attributes");
    expect(pumpGeo->attributes[0].value == "P-999", "ATTEDIT updated attribute value via selection");

    // Also test ATTEDIT with explicit entity ID: ATTEDIT <id> <tag> <new_val>
    auto resEditId = session.executeLine("ATTEDIT " + std::to_string(pumpInsId) + " PART P-888");
    expect(resEditId.ok, resEditId.message);
    pumpIns = session.document().findEntity(pumpInsId);
    pumpGeo = std::get_if<document::InsertEntity>(&pumpIns->geometry);
    expect(pumpGeo != nullptr && pumpGeo->attributes[0].value == "P-888", "ATTEDIT updated via entity ID");

    auto resDisp = session.executeLine("ATTDISP ON");
    expect(resDisp.ok, resDisp.message);
    expect(session.document().attributeDisplayMode() == document::AttributeDisplayMode::On, "ATTDISP ON applied");

    auto resDispOff = session.executeLine("ATTDISP OFF");
    expect(resDispOff.ok, resDispOff.message);
    expect(session.document().attributeDisplayMode() == document::AttributeDisplayMode::Off, "ATTDISP OFF applied");
}

void test_attributes_regression_and_nested_blocks() {
    using namespace mydesk;

    // 1. Nested blocks regression
    document::Document doc;
    auto l1 = doc.addLine({0.0, 0.0}, {5.0, 0.0});
    auto attInner = doc.addAttdef({0.0, 1.0}, 1.5, "IN_TAG", "Inner Prompt", "IN_VAL");
    doc.select(l1);
    doc.select(attInner);
    expect(doc.defineBlockFromSelection("INNER", {0.0, 0.0}), "define INNER block");

    // Clear and create outer geometry including an insert of INNER
    doc.clearSelection();
    auto l2 = doc.addLine({0.0, 0.0}, {20.0, 0.0});
    auto attOuter = doc.addAttdef({0.0, 5.0}, 2.0, "OUT_TAG", "Outer Prompt", "OUT_VAL");
    auto innerInsId = doc.addInsert("INNER", {10.0, 5.0}, 1.0, 0.0);

    doc.select(l2);
    doc.select(attOuter);
    doc.select(innerInsId);
    expect(doc.defineBlockFromSelection("OUTER", {0.0, 0.0}), "define OUTER block nesting INNER");

    const auto* blkOuter = doc.findBlock("OUTER");
    expect(blkOuter != nullptr, "OUTER block found");
    expect(blkOuter->attributes.size() == 1, "OUTER has 1 attribute definition");
    expect(blkOuter->primitives.size() == 2, "OUTER has 2 primitives (line + nested insert)");

    // Insert OUTER at (100, 200) with scale 2.0, rotation 0
    auto outerInsId = doc.addInsert("OUTER", {100.0, 200.0}, 2.0, 0.0);
    const auto* outerEnt = doc.findEntity(outerInsId);
    expect(outerEnt != nullptr, "outer insert created");
    const auto* outerGeo = std::get_if<document::InsertEntity>(&outerEnt->geometry);
    expect(outerGeo != nullptr, "is InsertEntity");
    expect(outerGeo->attributes.size() == 1, "outer insert has OUT_TAG attribute");
    expect(outerGeo->attributes[0].value == "OUT_VAL", "OUT_TAG value is OUT_VAL");

    // Native roundtrip with nested block
    const std::string nativeJson = exchange::serializeNative(doc);
    document::Document docLoaded;
    auto repNative = exchange::deserializeNative(docLoaded, nativeJson);
    expect(repNative.ok, "nested blocks native roundtrip succeeded");
    expect(docLoaded.findBlock("INNER") != nullptr, "INNER restored");
    expect(docLoaded.findBlock("OUTER") != nullptr, "OUTER restored");

    // Explode OUTER insert: should produce 1 line, 1 TextEntity (OUT_VAL), and 1 INNER InsertEntity
    auto expOuter = doc.explodeEntities({outerInsId});
    expect(expOuter.created.size() == 3, "explode OUTER creates 3 pieces");
    document::EntityId explodedInnerId = document::kInvalidEntityId;
    for (auto cid : expOuter.created) {
        const auto* ent = doc.findEntity(cid);
        if (ent && std::holds_alternative<document::InsertEntity>(ent->geometry)) {
            const auto& ins = std::get<document::InsertEntity>(ent->geometry);
            if (ins.blockName == "INNER") {
                explodedInnerId = cid;
                // Transformed position: 100 + 10 * 2 = 120, 200 + 5 * 2 = 210
                expect(std::abs(ins.insertion.x - 120.0) < 1e-4, "nested insert transformed X");
                expect(std::abs(ins.insertion.y - 210.0) < 1e-4, "nested insert transformed Y");
            }
        }
    }
    expect(explodedInnerId != document::kInvalidEntityId, "nested INNER insert materialized on explode");

    // Explode the nested INNER insert
    auto expInner = doc.explodeEntities({explodedInnerId});
    expect(expInner.created.size() == 2, "explode nested INNER creates line + text");

    // 2. Attribute flags and multiline switches
    commands::Session session;
    auto resFlag = session.executeLine("ATTDEF TITLE DrawingTitle Schematics 0,0 3.0 0 /I /C /M");
    expect(resFlag.ok, resFlag.message);
    const auto* attEnt = session.document().findEntity(session.document().entities().back().id);
    expect(attEnt != nullptr, "attdef entity found");
    const auto* attDefGeo = std::get_if<document::AttdefEntity>(&attEnt->geometry);
    expect(attDefGeo != nullptr, "is AttdefEntity");
    expect(attDefGeo->invisible, "invisible flag applied via /I");
    expect((attDefGeo->flags & document::kAttrFlagConstant) != 0, "constant flag applied via /C");
    expect(attDefGeo->isMultiline, "multiline flag applied via /M");

    // Test EATTEDIT command
    session.document().select(attEnt->id);
    expect(session.executeLine("BLOCK TITLEBLK 0,0").ok, "BLOCK TITLEBLK");
    expect(session.executeLine("INSERT TITLEBLK 10,10 1 0").ok, "INSERT TITLEBLK");
    auto insEattId = session.document().entities().back().id;
    session.document().clearSelection();
    session.document().select(insEattId);
    auto resEatt = session.executeLine("EATTEDIT TITLE Architectural_Plan");
    expect(resEatt.ok, resEatt.message);
    const auto* insEatt = session.document().findEntity(insEattId);
    const auto* insEattGeo = std::get_if<document::InsertEntity>(&insEatt->geometry);
    expect(insEattGeo != nullptr && insEattGeo->attributes[0].value == "Architectural_Plan",
           "EATTEDIT updated attribute value");

    // 3. Malformed DXF regression testing
    const auto tempDir = std::filesystem::temp_directory_path() / "mydesk_tests";
    std::filesystem::create_directories(tempDir);

    // Malformed DXF 1: Truncated DXF inside BLOCKS section
    {
        const auto badPath1 = tempDir / "bad_dxf_truncated.dxf";
        std::ofstream ofs(badPath1);
        ofs << "0\nSECTION\n2\nBLOCKS\n0\nBLOCK\n2\nINCOMPLETE_BLK\n";
        ofs.close();

        document::Document docBad1;
        auto rep1 = exchange::importDxf(docBad1, badPath1);
        // Must handle without throwing or crashing
        expect(rep1.ok, "handled truncated DXF safely");
    }

    // Malformed DXF 2: Missing SEQEND after group 66 ATTRIBs
    {
        const auto badPath2 = tempDir / "bad_dxf_missing_seqend.dxf";
        std::ofstream ofs(badPath2);
        ofs << "0\nSECTION\n2\nENTITIES\n0\nINSERT\n2\nBLK1\n10\n0.0\n20\n0.0\n66\n1\n"
            << "0\nATTRIB\n2\nTAG1\n1\nVAL1\n0\nEOF\n";
        ofs.close();

        document::Document docBad2;
        auto rep2 = exchange::importDxf(docBad2, badPath2);
        expect(rep2.ok, "handled missing SEQEND safely");
    }

    // Malformed DXF 3: Corrupted group codes and non-numeric values
    {
        const auto badPath3 = tempDir / "bad_dxf_garbage.dxf";
        std::ofstream ofs(badPath3);
        ofs << "9999\nGARBAGE\n0\nSECTION\n2\nENTITIES\n0\nLINE\n10\nNOT_A_NUMBER\n20\n0.0\n"
            << "0\nENDSEC\n0\nEOF\n";
        ofs.close();

        document::Document docBad3;
        auto rep3 = exchange::importDxf(docBad3, badPath3);
        expect(rep3.ok, "handled non-numeric group values safely");
        expect(!rep3.warnings.empty(), "warnings emitted for bad numeric groups");
    }
}

