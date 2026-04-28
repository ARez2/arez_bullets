#include "register_types.h"
#include "core/object/class_db.h"

#include "bullet_pooler.h"
#include "projectile.h"

void initialize_arez_bullets_module(ModuleInitializationLevel p_level) {
  if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
    return;
  }
  ClassDB::register_class<BulletPooler>();
  ClassDB::register_class<Projectile>();
}

void uninitialize_arez_bullets_module(ModuleInitializationLevel p_level) {
  if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
    return;
  }
  // Nothing to do here in this example.
}