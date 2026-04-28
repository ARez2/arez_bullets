#include "core/math/transform_3d.h"
#include "core/math/vector3.h"
#include "projectile.h"
#include "scene/resources/3d/world_3d.h"
#include "servers/physics_3d/physics_server_3d.h"
#include "servers/rendering/rendering_server.h"

#include "bullet_pooler.h"

BulletPooler::BulletPooler() {
  // set_process(true);
  set_physics_process(true);
  this->initialized = false;
}

BulletPooler::~BulletPooler() {
  RenderingServer *rs = RenderingServer::get_singleton();
  for (KeyValue<Ref<Projectile>, ProjectileStorage> &proj :
       this->projectile_storage) {
    rs->free_rid(proj.value.multimesh_visual_instance_rid);
    rs->free_rid(proj.value.multimesh_rid);
  }
}

// Main init function
void BulletPooler::init(int init_pool_size,
                        const TypedArray<Ref<Projectile>> &projectiles,
                        const TypedArray<RID> &projectile_meshes,
                        float _world_radius, float _max_shipspeed,
                        float _max_torpedospeed, float _max_bulletspeed) {
  this->initialized = true;
  this->initial_pool_size = init_pool_size;
  this->world = get_world_3d();
  this->world_radius = _world_radius;
  this->max_shipspeed = _max_shipspeed;
  this->max_torpedospeed = _max_torpedospeed;
  this->max_bulletspeed = _max_bulletspeed;
  this->scenario = this->world->get_scenario();

  RenderingServer *rs = RenderingServer::get_singleton();

  this->projectile_storage.clear();
  for (int i = 0; i < projectile_meshes.size(); i++) {
    Ref<Projectile> proj = projectiles[i];
    ProjectileStorage storage;
    storage.multimesh_rid = rs->multimesh_create();
    storage.visible_instances = 1;
    // Set up bullet multimesh
    rs->multimesh_allocate_data(
        storage.multimesh_rid,
        init_pool_size, // FIXME adapt for each projectile type
        RenderingServer::MultimeshTransformFormat::MULTIMESH_TRANSFORM_3D,
        false, false);
    rs->multimesh_set_mesh(storage.multimesh_rid, projectile_meshes[i]);
    rs->multimesh_set_visible_instances(storage.multimesh_rid, 1);
    storage.multimesh_visual_instance_rid = rs->instance_create();
    rs->instance_set_scenario(storage.multimesh_visual_instance_rid,
                              this->scenario);
    rs->instance_set_base(storage.multimesh_visual_instance_rid,
                          storage.multimesh_rid);

    this->projectile_storage.insert(proj, storage);
  }

  // Create and allocate the vector for the main bullet pool
  this->bulletpool = LocalVector<Bullet>();
  this->bulletpool.reserve(init_pool_size);
  // Create and allocate the vector for the bullets that are ready to be used
  this->available_bullets = LocalVector<int>();
  this->available_bullets.reserve(init_pool_size);
  // Pre-allocate the active bullet list
  this->active_bullets.reserve(init_pool_size);

  for (int i = 0; i < init_pool_size; i++) {
    Bullet bullet;
    bullet.transform = Transform3D(Basis(), Vector3(0, 0, 0));
    bullet.velocity = Vector3(0, 0, 0);
    this->bulletpool.push_back(bullet);
    this->available_bullets.push_back(i);
  }

  this->raycast_params = PhysicsDirectSpaceState3D::RayParameters();
  this->raycast_params.collide_with_areas = true;
  this->raycast_params.hit_back_faces = true;
  this->raycast_params.collision_mask = 32; // collide only with hitboxes
}

void bullet_look_at_vel(Bullet &bullet) {
  bullet.transform = bullet.transform.looking_at(
      bullet.transform.origin + bullet.velocity, Vector3::UP, true);
  bullet.transform =
      bullet.transform.rotated_local(Vector3::RIGHT, Math::PI * 0.5);
}

// Spawns a single bullet while extending the pool if needed
void BulletPooler::spawn_bullet(Vector3 position, Vector3 velocity,
                                const Ref<Projectile> &projectile, bool bigger,
                                const TypedArray<RID> &excluded_hurtboxes) {
  if (!this->initialized) {
    return;
  }
  // Bullet index into the main bulletpool
  int bullet_idx;
  if (this->available_bullets.is_empty()) {
    // add a new bullet to the pool
    Bullet new_bullet;
    // TODO
    new_bullet.transform = Transform3D();
    new_bullet.velocity = Vector3();
    // push the bullet into the bulletpool but not into available_bullets
    // (since it will be used instantly)
    this->bulletpool.push_back(new_bullet);
    bullet_idx = this->bulletpool.size() - 1;
  } else {
    bullet_idx = this->available_bullets[0];
    this->available_bullets.remove_at_unordered(0);
  }

  // Okay so now we grab the right bullet from the pool...
  Bullet &bullet = this->bulletpool[bullet_idx];
  // ... and modify its values to fit the request
  bullet.transform.origin = position;
  bullet.transform.basis.orthonormalize();
  if (bigger) {
    bullet.transform.scale_basis(Vector3(3, 3, 3));
  }
  bullet.velocity = velocity;
  if (!bullet.velocity.is_zero_approx()) {
    bullet_look_at_vel(bullet);
  }

  for (RID rid : excluded_hurtboxes) {
    bullet.exclude.insert(rid);
  }
  bullet.projectile = projectile;
  this->active_bullets.push_back(bullet_idx);
  bullet.active_idx = this->active_bullets.size() - 1;
}

void BulletPooler::remove_bullet(int bullet_idx) {
  if (!this->initialized) {
    return;
  }
  // Index of the "to be removed" bullet inside the active_bullets vector
  int idx_in_active_bullets = this->bulletpool[bullet_idx].active_idx;
  // the index into the bulletpool of the last element in active_bullets
  int last_active_bullets_idx =
      this->active_bullets[this->active_bullets.size() - 1];
  if (idx_in_active_bullets == -1) {
    idx_in_active_bullets = this->active_bullets.find(bullet_idx);
  }
  if (idx_in_active_bullets >= 0 &&
      idx_in_active_bullets < this->active_bullets.size()) {
    if (idx_in_active_bullets != last_active_bullets_idx) {
      // swap indices
      this->active_bullets[idx_in_active_bullets] = last_active_bullets_idx;
      this->bulletpool[last_active_bullets_idx].active_idx =
          idx_in_active_bullets;
    }
    // Remove the last element (which is now our bullet after the swap) from
    // active_bullets
    this->active_bullets.remove_at(this->active_bullets.size() - 1);
  }
  // Just for sanity: Set the bullets active_idx to be -1 since its now
  // inactive
  this->bulletpool[bullet_idx].active_idx = -1;
  this->bulletpool[bullet_idx].exclude.clear();
  //   RenderingServer::get_singleton()->instance_set_visible(
  //       this->bulletpool[bullet_idx].visual_instance, false);
  // Mark the bullet that's been removed as ready to be used
  this->available_bullets.push_back(bullet_idx);
}

void BulletPooler::process_bullets(double delta) {
  if (!this->initialized) {
    return;
  }
  this->space_state = this->world->get_direct_space_state();
  Vector3 cam_pos = this->active_camera_pos;
  for (int bullet_idx : this->active_bullets) {
    // if it hits something -> return to pool
    Bullet *bullet = &this->bulletpool[bullet_idx];

    float dist_sq_from_center = bullet->transform.origin.length_squared();
    if (dist_sq_from_center > this->world_radius * this->world_radius) {
      this->remove_bullet(bullet_idx);
      continue;
    }

    Vector3 new_position = bullet->transform.origin + bullet->velocity * delta;

    bool cast_ray = false;
    for (Node3D *collider : this->bullet_colliders) {
      float dist_sq_to_collider = bullet->transform.origin.distance_squared_to(
          collider->get_global_position());
      // ray could be colliding if: dist < max_bullet + max(max_ship, max_torp)
      if (dist_sq_to_collider <
          powf(this->max_bulletspeed * delta +
                   fmax(this->max_shipspeed, this->max_torpedospeed) * delta,
               2.0)) {
        cast_ray = true;
        break;
      }
    }

    if (cast_ray) {
      PhysicsDirectSpaceState3D::RayResult result;
      Vector3 ray_start = bullet->transform.origin;
      Vector3 ray_end = new_position;
      this->raycast_params.from = ray_start;
      this->raycast_params.to = ray_end;
      // By assigning here, we overwrite any additions made by the previous
      // bullet
      this->raycast_params.exclude = bullet->exclude;
      bool collided_once =
          this->space_state->intersect_ray(this->raycast_params, result);

      // this is written a bit awkard to only do additional calculations if the
      // first ray hits
      if (collided_once) {
        Vector3 ray_diff = ray_end - ray_start;
        float ray_length = ray_diff.length();
        Vector3 ray_dir = ray_diff.normalized();

        // do hit logic for first hit
        bool remove_bullet = bullet_hit_and_maybe_remove(
            1, result, bullet, &ray_start, &ray_end, &ray_dir, delta);
        if (remove_bullet) {
          this->remove_bullet(bullet_idx);
          continue;
        }
        // print_line("Bullet ", bullet_idx, " hit ", 1, "/", max_hits, " at ",
        //            result.position, ". Next end: ", ray_end);

        // i = 1 because we already collided once
        for (int i = 1; i <= max_hits; i++) {
          this->raycast_params.from = ray_start;
          this->raycast_params.to = ray_end;
          bool collided =
              this->space_state->intersect_ray(this->raycast_params, result);
          if (collided) {
            remove_bullet = bullet_hit_and_maybe_remove(
                i + 1, result, bullet, &ray_start, &ray_end, &ray_dir, delta);
            if (remove_bullet) {
              break;
            }
          } else {
            break;
          }
        }

        if (remove_bullet) {
          this->remove_bullet(bullet_idx);
          continue;
        }
      }
    }

    bullet->transform.origin = new_position;
    bullet->should_be_drawn = bullet->transform.origin.distance_squared_to(
                                  cam_pos) < bullet_drawing_radius_sq;
    // rs->instance_set_transform(bullet.visual_instance, bullet.transform);
  }
}

// Does the bullet hit registration, penetration and ricochet logic.
// Returns true if the bullet should be removed, false otherwise
bool BulletPooler::bullet_hit_and_maybe_remove(
    int hit_nr, const PhysicsDirectSpaceState3D::RayResult &result,
    Bullet *bullet, Vector3 *ray_start, Vector3 *ray_end, Vector3 *ray_dir,
    double delta) {
  Object *collider = result.collider;

  // Should be something like 0.8, 1.0, 1.7 etc.
  float hurtbox_resistance = collider->call("get_resistance");
  float penetration_resistance_factor = CLAMP(
      bullet->projectile->get_penetration() - hurtbox_resistance, 0.0, 1.0);
  // exit early because speed will be ~0 therefore damage 0 and
  // it will be removed at the end because of low velocity anyways
  if (penetration_resistance_factor == 0.0) {
    return true; // remove bullet
  }
  // its fine to skip bullet_look_at_vel here because we're only scaling
  // slow down the bullet based on bullet penetration and hurtbox resistance
  bullet->velocity *= penetration_resistance_factor;
  float new_speed_after_col = bullet->velocity.length();
  float bullet_speed_ratio_after_col =
      new_speed_after_col / bullet->projectile->get_speed();
  // Bullet damage = Base Damage * speed_ratio
  float bullet_damage =
      bullet->projectile->get_damage() * bullet_speed_ratio_after_col;

  // Angle of impact
  float angle = (*ray_dir).angle_to(-result.normal);
  // angle in range 0-1 but only going 0°-90°
  float angle_norm = CLAMP(angle / (Math::PI * 0.5), 0.0, 1.0);
  // weighing the normalized angle
  float attenuated_angle_norm = powf(angle_norm, 4.0);
  // Using the weighted angle and the penetration_resistance_factor
  // (so bullets with 200% PEN vs. 100% RES armor will penetrate more 20%
  // likely)
  float ricoshet_chance =
      attenuated_angle_norm - penetration_resistance_factor * 0.2;
  float r = Math::randf();
  bool ricochet = r < ricoshet_chance;
  if (ricochet) {
    *ray_dir = ray_dir->bounce(result.normal);
    bullet->velocity = bullet->velocity.bounce(result.normal);
    bullet_look_at_vel(*bullet);
    // if ricoshet: reduced damage
    bullet_damage *= ricoshet_dmg_reduction_factor;
  }

  collider->call("get_hit", bullet_damage);
  this->raycast_params.exclude.insert(result.rid);

  *ray_start = result.position + *ray_dir * step_after_col;
  *ray_end = *ray_start + *ray_dir * new_speed_after_col * delta;
  // print_line("Bullet ", bullet->active_idx, " hit ", hit_nr, " at ",
  //            result.position, " with angle:", Math::rad_to_deg(angle),
  //            ricoshet_chance, " and PEN to RES ratio of: ",
  //            penetration_resistance_factor, "and damage: ", bullet_damage, ".
  //            Riccochet? ", ricochet);

  // remove if not riccoshet or if bullet velocity is too low
  return (!ricochet && hit_nr >= max_hits) ||
         (bullet->velocity.length_squared() <
          powf(remove_bullet_if_speed_lower_than, 2.0));
}

void BulletPooler::draw_bullets() {
  if (!this->initialized) {
    return;
  }
  // resetting of visible instances for each projectile happens below this loop
  // to save 1 projectile loop
  RenderingServer *rs = RenderingServer::get_singleton();

  // Iterate backwards so just in case we reach the maximum drawing amount we
  // prioritize recently fired bullets
  for (int i = active_bullets.size() - 1; i >= 0; --i) {
    int bullet_idx = active_bullets[i];
    Bullet &bullet = this->bulletpool[bullet_idx];
    if (!bullet.should_be_drawn) {
      continue;
    }
    ProjectileStorage &proj_storage =
        this->projectile_storage.get(bullet.projectile);
    // Only draw as many bullets as there were in the initial pool
    if (proj_storage.visible_instances >= this->initial_pool_size - 10) {
      break;
    }
    RID proj_multimesh_rid = proj_storage.multimesh_rid;
    rs->multimesh_instance_set_transform(
        proj_multimesh_rid, proj_storage.visible_instances, bullet.transform);
    this->projectile_storage[bullet.projectile].visible_instances += 1;
    // RenderingServer::get_singleton()->instance_set_transform(
    //     bullet.visual_instance, bullet.transform);
  }

  for (const KeyValue<Ref<Projectile>, ProjectileStorage> &proj :
       this->projectile_storage) {
    rs->multimesh_set_visible_instances(proj.value.multimesh_rid,
                                        proj.value.visible_instances);
    this->projectile_storage[proj.key].visible_instances = 0;
  }
}

int BulletPooler::get_available_count() {
  return this->available_bullets.size();
}

int BulletPooler::get_active_count() { return this->active_bullets.size(); }

int BulletPooler::get_pool_size() { return this->bulletpool.size(); }

void BulletPooler::register_bullet_collider(Node3D *collider) {
  if (!this->initialized) {
    return;
  }
  this->bullet_colliders.push_back(collider);
}

void BulletPooler::deregister_bullet_collider(Node3D *collider) {
  if (!this->initialized) {
    return;
  }
  this->bullet_colliders.erase(collider);
}

void BulletPooler::update_active_camera_pos(Vector3 new_pos) {
  this->active_camera_pos = new_pos;
};

void BulletPooler::_bind_methods() {
  ClassDB::bind_method(D_METHOD("init", "init_pool_size", "projectiles",
                                "projectile_meshes", "world_radius",
                                "max_shipspeed", "max_torpedospeed",
                                "max_bulletspeed"),
                       &BulletPooler::init);
  ClassDB::bind_method(D_METHOD("spawn_bullet", "position", "velocity",
                                "projectile", "bigger", "excluded_hurtboxes"),
                       &BulletPooler::spawn_bullet);
  ClassDB::bind_method(D_METHOD("get_available_count"),
                       &BulletPooler::get_available_count);
  ClassDB::bind_method(D_METHOD("get_active_count"),
                       &BulletPooler::get_active_count);
  ClassDB::bind_method(D_METHOD("get_pool_size"), &BulletPooler::get_pool_size);
  ClassDB::bind_method(D_METHOD("register_bullet_collider", "collider"),
                       &BulletPooler::register_bullet_collider);
  ClassDB::bind_method(D_METHOD("deregister_bullet_collider", "collider"),
                       &BulletPooler::deregister_bullet_collider);
  ClassDB::bind_method(D_METHOD("update_active_camera_pos", "new_pos"),
                       &BulletPooler::update_active_camera_pos);
}

void BulletPooler::_notification(int p_what) {
  switch (p_what) {

  case NOTIFICATION_READY: {
    if (Engine::get_singleton()->is_editor_hint()) {
      return;
    }
    set_process(true);
    set_physics_process(true);
    // _init_bullets();
  } break;

  case NOTIFICATION_PROCESS: {
    draw_bullets();
  } break;

  case NOTIFICATION_PHYSICS_PROCESS: {
    if (Engine::get_singleton()->is_editor_hint()) {
      return;
    }
    process_bullets(get_physics_process_delta_time());
  } break;

  default:
    break;
  }
}
