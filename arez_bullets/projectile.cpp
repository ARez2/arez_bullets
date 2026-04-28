#include "projectile.h"

int Projectile::get_damage() { return this->base_damage; }
void Projectile::set_damage(int new_dmg) { this->base_damage = new_dmg; }

float Projectile::get_penetration() { return this->penetration; }
void Projectile::set_penetration(float new_val) {
  this->penetration = new_val;
}

float Projectile::get_speed() { return this->speed; }
float Projectile::get_speed_sq() { return this->speed_sq; }
void Projectile::set_speed(float new_val) { this->speed = new_val; this->speed_sq = this->speed*this->speed; }

Color Projectile::get_color() { return this->color; }
void Projectile::set_color(Color new_val) { this->color = new_val; }

float Projectile::get_max_projectile_speed() { return 5000.0; }

void Projectile::_bind_methods() {
  ClassDB::bind_method(D_METHOD("get_damage"), &Projectile::get_damage);
  ClassDB::bind_method(D_METHOD("set_damage", "new_dmg"),
                       &Projectile::set_damage);
  ClassDB::bind_method(D_METHOD("get_penetration"),
                       &Projectile::get_penetration);
  ClassDB::bind_method(D_METHOD("set_penetration", "new_val"),
                       &Projectile::set_penetration);
  ClassDB::bind_method(D_METHOD("get_speed"), &Projectile::get_speed);
  ClassDB::bind_method(D_METHOD("set_speed", "new_val"),
                       &Projectile::set_speed);
  ClassDB::bind_method(D_METHOD("get_color"), &Projectile::get_color);
  ClassDB::bind_method(D_METHOD("set_color", "new_val"),
                       &Projectile::set_color);
  ADD_PROPERTY(PropertyInfo(Variant::INT, "base_damage"), "set_damage",
               "get_damage");
  ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "penetration"),
               "set_penetration", "get_penetration");
  ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed"), "set_speed", "get_speed");
  ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "set_color", "get_color");

  //   ADD_PROPERTY_DEFAULT("base_damage", 10);
  //   ADD_PROPERTY_DEFAULT("armor_piercing", false);
  //   ADD_PROPERTY_DEFAULT("speed", 2050.0);
  //   ADD_PROPERTY_DEFAULT("color", Color(2.0, 1.474, 0.659));
}