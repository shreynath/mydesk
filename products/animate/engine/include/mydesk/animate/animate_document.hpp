// SPDX-License-Identifier: MPL-2.0
// MyDesk Animate — Maya-class animation / DCC engine
// Provides: scene graph, transform hierarchy, keyframe curves (Bezier),
//           skeletal mesh with skin weights, USD and Alembic interchange.
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace mydesk::animate {

// ────────────────────────────────────────────────────────────────────────────
// Math primitives
// ────────────────────────────────────────────────────────────────────────────
struct Vec3  { double x{}, y{}, z{}; };
struct Vec4  { double x{}, y{}, z{}, w{1.0}; };
struct Quat  { double x{}, y{}, z{}, w{1.0}; };

/// Column-major 4x4 matrix (OpenGL convention)
struct Mat4 {
    std::array<double, 16> m{};

    static Mat4 identity() noexcept {
        Mat4 r;
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0;
        return r;
    }

    static Mat4 translation(double tx, double ty, double tz) noexcept {
        auto r = identity();
        r.m[12] = tx; r.m[13] = ty; r.m[14] = tz;
        return r;
    }

    static Mat4 from_quat(const Quat& q) noexcept;
    Mat4 operator*(const Mat4& o) const noexcept;
};

inline Mat4 Mat4::from_quat(const Quat& q) noexcept {
    Mat4 r = identity();
    double xx = q.x*q.x, yy = q.y*q.y, zz = q.z*q.z;
    double xy = q.x*q.y, xz = q.x*q.z, yz = q.y*q.z;
    double wx = q.w*q.x, wy = q.w*q.y, wz = q.w*q.z;
    r.m[0]  = 1-2*(yy+zz); r.m[4]  = 2*(xy-wz);   r.m[8]  = 2*(xz+wy);
    r.m[1]  = 2*(xy+wz);   r.m[5]  = 1-2*(xx+zz); r.m[9]  = 2*(yz-wx);
    r.m[2]  = 2*(xz-wy);   r.m[6]  = 2*(yz+wx);   r.m[10] = 1-2*(xx+yy);
    return r;
}

inline Mat4 Mat4::operator*(const Mat4& o) const noexcept {
    Mat4 r;
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            for (int k = 0; k < 4; ++k)
                r.m[col*4+row] += m[k*4+row] * o.m[col*4+k];
    return r;
}

// ────────────────────────────────────────────────────────────────────────────
// Keyframe animation — cubic Bezier curve per channel
// ────────────────────────────────────────────────────────────────────────────
struct Tangent { double dt{0.333}; double dv{0.0}; };

struct Keyframe {
    double time{};
    double value{};
    Tangent in{};
    Tangent out{};
};

/// Single-channel float animation curve (sorted keyframes)
class AnimCurve {
public:
    void add_key(const Keyframe& k);
    double evaluate(double t) const noexcept;
    std::size_t key_count() const noexcept { return keys_.size(); }
    const Keyframe& key_at(std::size_t i) const { return keys_.at(i); }

private:
    static double bezier_solve(double t,
                               double p0, double p1, double p2, double p3) noexcept;
    std::vector<Keyframe> keys_;
};

// ────────────────────────────────────────────────────────────────────────────
// Scene graph node
// ────────────────────────────────────────────────────────────────────────────
using NodeId = std::uint32_t;
constexpr NodeId kInvalidNode = 0xFFFFFFFF;

enum class NodeKind : std::uint8_t {
    Transform, Mesh, Camera, Light, Joint, Locator
};

struct TransformChannels {
    AnimCurve tx, ty, tz;
    AnimCurve rx, ry, rz;
    AnimCurve sx, sy, sz;
};

struct SceneNode {
    NodeId       id{kInvalidNode};
    std::string  name;
    NodeKind     kind{NodeKind::Transform};
    NodeId       parent{kInvalidNode};
    std::vector<NodeId> children;

    Vec3  translate{};
    Quat  rotate{0,0,0,1};
    Vec3  scale{1,1,1};

    std::optional<TransformChannels> anim;

    Mat4 local_matrix(double t) const noexcept;
};

// ────────────────────────────────────────────────────────────────────────────
// Skeletal mesh
// ────────────────────────────────────────────────────────────────────────────
struct SkinVertex {
    std::array<std::uint32_t, 4> joint_indices{};
    std::array<float, 4>         weights{};
};

struct Mesh {
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec3> uvs;
    std::vector<std::array<std::uint32_t, 3>> triangles;
    std::optional<std::vector<SkinVertex>> skin;
    NodeId node_id{kInvalidNode};
};

struct Joint {
    NodeId node_id{kInvalidNode};
    Mat4   bind_inverse;
};

struct Skeleton {
    std::vector<Joint> joints;
};

// ────────────────────────────────────────────────────────────────────────────
// Camera / Light
// ────────────────────────────────────────────────────────────────────────────
struct Camera {
    NodeId node_id{kInvalidNode};
    double fov_y_deg{54.0};
    double near_clip{0.01};
    double far_clip{10000.0};
    bool   orthographic{false};
    double ortho_width{5.0};
};

enum class LightKind : std::uint8_t { Point, Directional, Spot, Area };

struct Light {
    NodeId    node_id{kInvalidNode};
    LightKind kind{LightKind::Point};
    Vec3      color{1,1,1};
    double    intensity{1.0};
    double    spot_cone_angle_deg{40.0};
};

// ────────────────────────────────────────────────────────────────────────────
// Timeline
// ────────────────────────────────────────────────────────────────────────────
struct Timeline {
    double start_frame{0};
    double end_frame{240};
    double fps{24.0};
    double frame_to_time(double frame) const noexcept { return frame / fps; }
    double time_to_frame(double t)     const noexcept { return t * fps; }
};

// ────────────────────────────────────────────────────────────────────────────
// Document
// ────────────────────────────────────────────────────────────────────────────
class AnimateDocument {
public:
    AnimateDocument();

    NodeId add_node(const std::string& name, NodeKind kind,
                    NodeId parent = kInvalidNode);
    SceneNode&       node(NodeId id);
    const SceneNode& node(NodeId id) const;
    std::size_t      node_count() const noexcept { return nodes_.size(); }
    std::vector<NodeId> root_nodes() const;

    std::size_t add_mesh(Mesh mesh);
    Mesh&       mesh(std::size_t i) { return meshes_.at(i); }
    std::size_t mesh_count() const noexcept { return meshes_.size(); }

    void set_skeleton(Skeleton skel) { skeleton_ = std::move(skel); }
    bool has_skeleton() const noexcept { return skeleton_.has_value(); }
    const Skeleton& skeleton() const { return skeleton_.value(); }

    std::size_t add_camera(Camera cam);
    std::size_t add_light(Light light);

    Timeline&       timeline()       noexcept { return timeline_; }
    const Timeline& timeline() const noexcept { return timeline_; }

    std::unordered_map<NodeId, Mat4> world_matrices(double time_seconds) const;
    std::vector<Vec3> skinned_positions(std::size_t mesh_idx,
                                        double time_seconds) const;

    std::string export_usda() const;
    std::vector<std::uint8_t> export_alembic_minimal() const;

private:
    std::unordered_map<NodeId, SceneNode> nodes_;
    NodeId next_id_{0};
    std::vector<Mesh>   meshes_;
    std::optional<Skeleton> skeleton_;
    std::vector<Camera> cameras_;
    std::vector<Light>  lights_;
    Timeline timeline_;

    Mat4 compute_world(NodeId id, double t,
                       std::unordered_map<NodeId, Mat4>& cache) const;
};

} // namespace mydesk::animate
