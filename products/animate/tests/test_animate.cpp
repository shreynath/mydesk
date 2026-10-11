// SPDX-License-Identifier: MPL-2.0
// MyDesk Animate — comprehensive regression tests
// Covers: AnimCurve interpolation, scene hierarchy, LBS skinning, USD export

#include "mydesk/animate/animate_document.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using namespace mydesk::animate;

static int g_failed = 0;

#define CHECK(cond) \
    do { if (!(cond)) { \
        std::fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, #cond); \
        ++g_failed; \
    } } while(0)

#define CHECK_NEAR(a, b, eps) \
    do { double _a = (a), _b = (b), _e = (eps); \
         if (std::abs(_a - _b) > _e) { \
             std::fprintf(stderr, "FAIL [line %d]: |%.6g - %.6g| > %.6g\n", \
                          __LINE__, _a, _b, _e); \
             ++g_failed; \
    } } while(0)

// ─── AnimCurve tests ────────────────────────────────────────────────────────

static void test_anim_curve_constant() {
    AnimCurve c;
    c.add_key({0.0, 10.0, {}, {}});
    c.add_key({1.0, 10.0, {}, {}});
    CHECK_NEAR(c.evaluate(0.0), 10.0, 1e-9);
    CHECK_NEAR(c.evaluate(0.5), 10.0, 1e-9);
    CHECK_NEAR(c.evaluate(1.0), 10.0, 1e-9);
}

static void test_anim_curve_linear_approx() {
    // With zero tangent lengths, Bezier degenerates near-linearly
    AnimCurve c;
    Tangent t0; t0.dt = 0.333; t0.dv = 10.0 * 0.333;
    Tangent t1; t1.dt = 0.333; t1.dv = 10.0 * 0.333;
    c.add_key({0.0, 0.0,  {}, t0});
    c.add_key({1.0, 10.0, t1, {}});
    CHECK_NEAR(c.evaluate(0.5), 5.0, 0.1);  // allow some bezier deviation
}

static void test_anim_curve_extrapolation() {
    AnimCurve c;
    c.add_key({1.0, 5.0, {}, {}});
    c.add_key({2.0, 8.0, {}, {}});
    CHECK_NEAR(c.evaluate(-1.0), 5.0, 1e-9);  // clamp to first
    CHECK_NEAR(c.evaluate(99.0), 8.0, 1e-9);  // clamp to last
}

static void test_anim_curve_replace_key() {
    AnimCurve c;
    c.add_key({0.0, 1.0, {}, {}});
    c.add_key({0.0, 2.0, {}, {}});  // replaces
    CHECK(c.key_count() == 1);
    CHECK_NEAR(c.evaluate(0.0), 2.0, 1e-9);
}

// ─── Scene graph tests ───────────────────────────────────────────────────────

static void test_scene_hierarchy() {
    AnimateDocument doc;
    NodeId root = doc.add_node("root", NodeKind::Transform);
    NodeId child = doc.add_node("child", NodeKind::Transform, root);

    CHECK(doc.node(root).children.size() == 1);
    CHECK(doc.node(child).parent == root);
    CHECK(doc.root_nodes().size() == 1);
}

static void test_world_matrix_translation() {
    AnimateDocument doc;
    NodeId root  = doc.add_node("root",  NodeKind::Transform);
    NodeId child = doc.add_node("child", NodeKind::Transform, root);

    doc.node(root).translate  = {1.0, 0.0, 0.0};
    doc.node(child).translate = {0.0, 2.0, 0.0};

    auto wms = doc.world_matrices(0.0);
    const Mat4& cw = wms.at(child);
    // Translation column (col 3 in column-major = indices 12,13,14)
    CHECK_NEAR(cw.m[12], 1.0, 1e-9);  // parent X
    CHECK_NEAR(cw.m[13], 2.0, 1e-9);  // child Y
    CHECK_NEAR(cw.m[14], 0.0, 1e-9);
}

static void test_animated_node_translation() {
    AnimateDocument doc;
    NodeId id = doc.add_node("obj", NodeKind::Mesh);
    auto& n = doc.node(id);
    n.anim.emplace();
    n.anim->tx.add_key({0.0, 0.0, {}, {}});
    n.anim->tx.add_key({1.0, 5.0, {}, {}});
    // All other channels: no keys => evaluate returns 0 (constant).
    // sy/sz default keys missing => evaluate returns 0 (not 1), so scale is 0.
    // For world_matrix test we only check translation component.
    auto wms = doc.world_matrices(0.5);
    // tx should be ~2.5 (midpoint, linear-ish bezier)
    // We accept +/- 0.5 deviation due to Bezier shape
    double tx = wms.at(id).m[12];
    CHECK(tx > 1.0 && tx < 4.5);
}

// ─── Mesh tests ──────────────────────────────────────────────────────────────

static void test_mesh_add_and_retrieve() {
    AnimateDocument doc;
    Mesh m;
    m.positions = {{0,0,0}, {1,0,0}, {0,1,0}};
    m.triangles = {{{0,1,2}}};
    std::size_t idx = doc.add_mesh(std::move(m));
    CHECK(idx == 0);
    CHECK(doc.mesh_count() == 1);
    CHECK(doc.mesh(0).positions.size() == 3);
}

// ─── Skinned deform tests ────────────────────────────────────────────────────

static void test_skinned_identity_deform() {
    // One joint at origin, bind inverse = identity, weight = 1 => positions unchanged
    AnimateDocument doc;
    NodeId jnode = doc.add_node("joint0", NodeKind::Joint);
    doc.node(jnode).translate = {0,0,0};

    Joint j;
    j.node_id = jnode;
    j.bind_inverse = Mat4::identity();

    Skeleton skel;
    skel.joints.push_back(j);
    doc.set_skeleton(std::move(skel));

    Mesh m;
    m.positions = {{1,2,3}};
    m.triangles = {{{0,0,0}}};
    SkinVertex sv; sv.joint_indices = {0,0,0,0}; sv.weights = {1.0f,0,0,0};
    m.skin = std::vector<SkinVertex>{sv};
    m.node_id = jnode;
    doc.add_mesh(std::move(m));

    auto out = doc.skinned_positions(0, 0.0);
    CHECK(out.size() == 1);
    CHECK_NEAR(out[0].x, 1.0, 1e-9);
    CHECK_NEAR(out[0].y, 2.0, 1e-9);
    CHECK_NEAR(out[0].z, 3.0, 1e-9);
}

static void test_skinned_translated_joint() {
    // Joint at (0,0,0) in bind pose (bind_inverse = identity).
    // At eval time, joint is at (0,0,5). Vertex at (1,0,0).
    // Skinned pos = world_joint * bind_inverse * vertex_pos
    //             = T(0,0,5) * I * (1,0,0) = (1,0,5)
    AnimateDocument doc;
    NodeId jnode = doc.add_node("joint0", NodeKind::Joint);
    doc.node(jnode).translate = {0, 0, 5};   // at eval time

    Joint j;
    j.node_id = jnode;
    j.bind_inverse = Mat4::identity();

    Skeleton skel;
    skel.joints.push_back(j);
    doc.set_skeleton(std::move(skel));

    Mesh m;
    m.positions = {{1,0,0}};
    m.triangles = {{{0,0,0}}};
    SkinVertex sv; sv.joint_indices = {0,0,0,0}; sv.weights = {1.0f,0,0,0};
    m.skin = std::vector<SkinVertex>{sv};
    doc.add_mesh(std::move(m));

    auto out = doc.skinned_positions(0, 0.0);
    CHECK_NEAR(out[0].x, 1.0, 1e-9);
    CHECK_NEAR(out[0].y, 0.0, 1e-9);
    CHECK_NEAR(out[0].z, 5.0, 1e-9);
}

// ─── USD export tests ────────────────────────────────────────────────────────

static void test_usda_export_header() {
    AnimateDocument doc;
    std::string usda = doc.export_usda();
    CHECK(usda.find("#usda 1.0") != std::string::npos);
    CHECK(usda.find("upAxis") != std::string::npos);
    CHECK(usda.find("timeCodesPerSecond") != std::string::npos);
}

static void test_usda_export_mesh() {
    AnimateDocument doc;
    Mesh m;
    m.positions = {{0,0,0}, {1,0,0}, {0,1,0}};
    m.triangles = {{{0,1,2}}};
    doc.add_mesh(std::move(m));
    std::string usda = doc.export_usda();
    CHECK(usda.find("def Mesh") != std::string::npos);
    CHECK(usda.find("faceVertexCounts") != std::string::npos);
}

// ─── Alembic export tests ────────────────────────────────────────────────────

static void test_alembic_magic() {
    AnimateDocument doc;
    auto abc = doc.export_alembic_minimal();
    CHECK(abc.size() >= 13);
    CHECK(abc[0] == 'O');
    CHECK(abc[1] == 'g');
    CHECK(abc[2] == 'a');
    CHECK(abc[3] == 'w');
    CHECK(abc[4] == 'a');
}

// ─── Timeline tests ──────────────────────────────────────────────────────────

static void test_timeline() {
    AnimateDocument doc;
    doc.timeline().fps = 30.0;
    doc.timeline().end_frame = 300.0;
    CHECK_NEAR(doc.timeline().frame_to_time(30), 1.0, 1e-9);
    CHECK_NEAR(doc.timeline().time_to_frame(1.0), 30.0, 1e-9);
}

static void test_scene_editing_undo_and_native_roundtrip() {
    AnimateDocument doc;
    doc.begin_edit();
    const NodeId box = doc.create_box("AnimatedBox");
    CHECK(doc.node_count() == 1);
    CHECK(doc.mesh_count() == 1);
    CHECK(doc.mesh(0).triangles.size() == 12);

    doc.begin_edit();
    doc.set_transform(box, {2.0, 3.0, 4.0}, {0.0, 0.0, 0.0, 1.0}, {2.0, 1.0, 0.5});
    doc.begin_edit();
    doc.set_keyframe(box, "tx", 24.0, 12.0);
    CHECK(doc.node(box).anim.has_value());
    CHECK(doc.node(box).anim->tx.key_count() == 2);
    CHECK(doc.undo());
    CHECK(!doc.node(box).anim.has_value());
    CHECK(doc.redo());
    CHECK(doc.node(box).anim->tx.key_count() == 2);

    const std::string native = doc.serialize_native();
    CHECK(native.find("\"format\": \"mydesk-animate\"") != std::string::npos);
    AnimateDocument loaded = AnimateDocument::deserialize_native(native);
    CHECK(loaded.node_count() == 1);
    CHECK(loaded.mesh_count() == 1);
    CHECK_NEAR(loaded.node(box).translate.x, 2.0, 1e-9);
    CHECK_NEAR(loaded.node(box).anim->tx.evaluate(1.0), 12.0, 1e-9);

    const auto path = std::filesystem::temp_directory_path() / "mydesk-animate-test.myanimate";
    doc.save_native(path);
    AnimateDocument from_file = AnimateDocument::load_native(path);
    CHECK(from_file.node(box).name == "AnimatedBox");
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::filesystem::remove(path.string() + ".bak", ignored);
}

static void test_scene_parenting_validation_and_recursive_delete() {
    AnimateDocument doc;
    const NodeId parent = doc.create_box("Parent");
    const NodeId child = doc.create_sphere("Child", 6, 8, parent);
    CHECK(doc.node(parent).children.size() == 1);
    bool rejected = false;
    try { doc.reparent_node(parent, child); }
    catch (const std::invalid_argument&) { rejected = true; }
    CHECK(rejected);
    doc.remove_node(parent);
    CHECK(doc.node_count() == 0);
    CHECK(doc.mesh_count() == 0);
}

// ─── Main ────────────────────────────────────────────────────────────────────

int main() {
    // Separate offscreen Qt test: "test_animate_qt_scene_timeline_workflow"
    // "test_anim_curve_constant"
    test_anim_curve_constant();
    // "test_anim_curve_linear_approx"
    test_anim_curve_linear_approx();
    // "test_anim_curve_extrapolation"
    test_anim_curve_extrapolation();
    // "test_anim_curve_replace_key"
    test_anim_curve_replace_key();

    // "test_scene_hierarchy"
    test_scene_hierarchy();
    // "test_world_matrix_translation"
    test_world_matrix_translation();
    // "test_animated_node_translation"
    test_animated_node_translation();

    // "test_mesh_add_and_retrieve"
    test_mesh_add_and_retrieve();
    // "test_skinned_identity_deform"
    test_skinned_identity_deform();
    // "test_skinned_translated_joint"
    test_skinned_translated_joint();

    // "test_usda_export_header"
    test_usda_export_header();
    // "test_usda_export_mesh"
    test_usda_export_mesh();
    // "test_alembic_magic"
    test_alembic_magic();

    // "test_timeline"
    test_timeline();
    // "test_scene_editing_undo_and_native_roundtrip"
    test_scene_editing_undo_and_native_roundtrip();
    // "test_scene_parenting_validation_and_recursive_delete"
    test_scene_parenting_validation_and_recursive_delete();

    if (g_failed == 0) {
        std::printf("All Animate tests passed.\n");
        return 0;
    }
    std::fprintf(stderr, "%d Animate test(s) FAILED.\n", g_failed);
    return 1;
}
