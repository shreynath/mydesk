#include "test_assert.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

using TestFn = void (*)();

struct TestCase {
    const char* name;
    TestFn fn;
};

void test_vec2_distance();
void test_line_bbox();
void test_document_line_undo_redo();
void test_circle_reject_nonpositive();
void test_command_line_alias();
void test_command_circle_radius();
void test_snap_endpoint();
void test_native_roundtrip();
void test_dxf_export_smoke();
void test_rectangle_four_lines();
void test_move_copy_rotate_scale();
void test_ortho_mode();
void test_pline_and_trim();
void test_extend_offset_fillet_chamfer();
void test_selection_window_crossing();
void test_dxf_line_circle_pline_roundtrip();
void test_layer_command_new_set_off();
void test_layer_duplicate_new_rejected();
void test_chprop_and_dimlinear();
void test_osnap_intersection_perpendicular();
void test_dim_native_roundtrip();
void test_block_and_insert();
void test_text_and_hatch();
void test_relative_and_polar_coordinates();
void test_explode_insert_and_polyline();
void test_layer_freeze_thaw_plot_roundtrip();
void test_linetype_lineweight_commands_and_roundtrip();

int main() {
    const std::vector<TestCase> tests = {
        {"test_vec2_distance", &test_vec2_distance},
        {"test_line_bbox", &test_line_bbox},
        {"test_document_line_undo_redo", &test_document_line_undo_redo},
        {"test_circle_reject_nonpositive", &test_circle_reject_nonpositive},
        {"test_command_line_alias", &test_command_line_alias},
        {"test_command_circle_radius", &test_command_circle_radius},
        {"test_snap_endpoint", &test_snap_endpoint},
        {"test_native_roundtrip", &test_native_roundtrip},
        {"test_dxf_export_smoke", &test_dxf_export_smoke},
        {"test_rectangle_four_lines", &test_rectangle_four_lines},
        {"test_move_copy_rotate_scale", &test_move_copy_rotate_scale},
        {"test_ortho_mode", &test_ortho_mode},
        {"test_pline_and_trim", &test_pline_and_trim},
        {"test_extend_offset_fillet_chamfer", &test_extend_offset_fillet_chamfer},
        {"test_selection_window_crossing", &test_selection_window_crossing},
        {"test_dxf_line_circle_pline_roundtrip", &test_dxf_line_circle_pline_roundtrip},
        {"test_layer_command_new_set_off", &test_layer_command_new_set_off},
        {"test_layer_duplicate_new_rejected", &test_layer_duplicate_new_rejected},
        {"test_chprop_and_dimlinear", &test_chprop_and_dimlinear},
        {"test_osnap_intersection_perpendicular", &test_osnap_intersection_perpendicular},
        {"test_dim_native_roundtrip", &test_dim_native_roundtrip},
        {"test_block_and_insert", &test_block_and_insert},
        {"test_text_and_hatch", &test_text_and_hatch},
        {"test_relative_and_polar_coordinates", &test_relative_and_polar_coordinates},
        {"test_explode_insert_and_polyline", &test_explode_insert_and_polyline},
        {"test_layer_freeze_thaw_plot_roundtrip", &test_layer_freeze_thaw_plot_roundtrip},
        {"test_linetype_lineweight_commands_and_roundtrip",
         &test_linetype_lineweight_commands_and_roundtrip},
    };

    int failed = 0;
    for (const auto& test : tests) {
        try {
            test.fn();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& ex) {
            std::cout << "[FAIL] " << test.name << " — " << ex.what() << '\n';
            ++failed;
        } catch (...) {
            std::cout << "[FAIL] " << test.name << " — unknown exception\n";
            ++failed;
        }
    }
    std::cout << (tests.size() - static_cast<std::size_t>(failed)) << " passed, " << failed
              << " failed\n";
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
