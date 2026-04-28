#pragma once

#include "core/io/resource.h"
#include "core/math/color.h"

class Projectile : public Resource {
  GDCLASS(Projectile, Resource);

  int base_damage;
  float penetration;
  float speed;
  float speed_sq;
  Color color;

protected:
  static void _bind_methods();

public:
  void set_damage(int new_dmg);
  int get_damage();
  void set_penetration(float new_val);
  float get_penetration();
  void set_speed(float new_val);
  float get_speed();
  float get_speed_sq();
  void set_color(Color new_val);
  Color get_color();

  static float get_max_projectile_speed();
};
