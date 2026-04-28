#pragma once

#include "core/math/vector3.h"
#include "core/object/ref_counted.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/3d/world_3d.h"
#include "servers/physics_3d/physics_server_3d.h"

#include "bullet.h"

struct ProjectileStorage {
  RID mesh;
  RID multimesh_rid;
  RID multimesh_visual_instance_rid;
  int visible_instances;
};

class BulletPooler : public Node3D {
  GDCLASS(BulletPooler, Node3D);

  const float step_after_col = 0.0;
  const int max_hits = 5;
  const float penetration_depth_falloff = 0.5;
  const float remove_bullet_if_speed_lower_than = 50.0;
  const float ricoshet_dmg_reduction_factor = 0.25;
  const float bullet_drawing_radius_sq = powf(5000.0, 2.0);

private:
  bool initialized;
  int initial_pool_size;
  float world_radius;
  float max_shipspeed;
  float max_torpedospeed;
  float max_bulletspeed;
  Ref<World3D> world;
  RID scenario;
  PhysicsDirectSpaceState3D *space_state;
  PhysicsDirectSpaceState3D::RayParameters raycast_params;
  LocalVector<Node3D *> bullet_colliders;
  HashMap<Ref<Projectile>, ProjectileStorage> projectile_storage;
  LocalVector<Bullet> bulletpool;
  // Indices into the bulletpool of all bullets which are currently available to
  // use
  LocalVector<int> available_bullets;
  // Indices into the bulletpool of all bullets which are currently active
  LocalVector<int> active_bullets;
  Vector3 active_camera_pos;

  void process_bullets(double delta);
  bool bullet_hit_and_maybe_remove(
      int hit_nr, const PhysicsDirectSpaceState3D::RayResult &result,
      Bullet *bullet, Vector3 *ray_start, Vector3 *ray_end, Vector3 *ray_dir,
      double delta);
  void draw_bullets();
  void remove_bullet(int bullet_idx);

protected:
  static void _bind_methods();
  void _notification(int p_what);

public:
  void init(int init_pool_size, const TypedArray<Ref<Projectile>> &projectiles,
            const TypedArray<RID> &projectile_meshes, float _world_radius,
            float _max_shipspeed, float _max_torpedospeed,
            float _max_bulletspeed);
  void register_bullet_collider(Node3D *collider);
  void deregister_bullet_collider(Node3D *collider);
  void spawn_bullet(Vector3 position, Vector3 velocity,
                    const Ref<Projectile> &projectile, bool bigger,
                    const TypedArray<RID> &excluded_hurtboxes);
  int get_available_count();
  int get_active_count();
  int get_pool_size();
  void update_active_camera_pos(Vector3 new_pos);

  BulletPooler();
  ~BulletPooler();
};
