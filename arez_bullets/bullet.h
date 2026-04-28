#pragma once

#include "core/math/transform_3d.h"
#include "core/math/vector3.h"
#include "projectile.h"

struct Bullet {
  Transform3D transform;
  Vector3 velocity;
  // index inside the active_bullets vector
  int active_idx;
  bool should_be_drawn;
  HashSet<RID> exclude;
  Ref<Projectile> projectile;
};