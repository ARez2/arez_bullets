# ARez Bullets
A high performance projectile system for Godot written as C++ engine module.
This was written for a personal project of mine and might not fit every project. But it can serve as a baseline to adapt.

Allows spawning and rendering thousands of projectiles:

https://github.com/user-attachments/assets/d2ce3c26-ab94-492a-8b01-a8cad14c76af

Each bullet can penetrate hurtboxes (with the right properties) and also ricoshet off of them.

## How it works
The `Bullet` struct represents one instance of a bullet. It stores transform, velocity and type of projectile. You can define different `Projectile` types which have the following properties:
- Base damage
- Penetration value
- Speed
- Color

The `BulletPooler` is responsible for spawning and managing the bullets. Internally, it uses object pooling to support thousands of projectiles at once. It uses Godot's `RenderingServer` and multimeshes (GPU instancing) to render the bullets and Godot's `PhysicsServer` to do raycasts for intersection tests (but it saves performance by doing a square distance check to each collider before casting a ray).
It uses the camera position to cull bullets which are far away (not render them).

The penetration/ ricoshet logic works as following:
- Each hurtbox has a resistance value (for example 0 or 1.0 or 1.7 etc.)
- Each `Projectile` has a penetration value
- When a bullet collides with a hurtbox, it calculates `factor = clamp(projectile.penetration - hurtbox.resistance, 0, 1)`
- If factor == 0 or "nr. of hits == `max_hits`", remove the bullet. This basically means the bullet was stopped by the armor of the hurtbox
- Otherwise, slow down the bullet by the factor and also reduce damage depending on how much we slowed down the bullet
- If the bullet speed is less than `remove_bullet_if_speed_lower_than` also remove the bullet
- Next, check angle of impact
- Use a weighting function to weigh the angle and calculate the ricoshet chance (also using the penetration factor we calculated earlier)
- Roll a RNG, if it hits, let the bullet ricoshet (bounce) on the surface (This means the armor was hard enough to not let the bullet penetrate)
- If it ricoshets, reduce multiply damage by `ricoshet_dmg_reduction_factor` we are done (the bullet just flies in that new direction now)
- If not, it means the bullet penetrates the armor. So we let it continue on its current path (if `step_after_col` is greater than zero, we skip forward a by this distance)
- On the next hit, do all of this over again


There are some settings in `bullet_pooler.h`:
```h
const float step_after_col = 0.0;
const int max_hits = 5;
const float penetration_depth_falloff = 0.5;
const float remove_bullet_if_speed_lower_than = 50.0;
const float ricoshet_dmg_reduction_factor = 0.25;
const float bullet_drawing_radius_sq = powf(5000.0, 2.0);
```


## How to use
Copy the contents of this repo into your local Godot clone (into `modules/arez_bullets`). Compile the engine.

From GDScript, create a new script which extends `BulletPooler` and add it as autoload. Example:
```swift
extends BulletPooler

func _ready() -> void:
	var projectiles: Array[Projectile] = []
	var projectile_meshes: Array[Mesh] = []
	# fill the arrays here...

  # call BulletPooler.init
	init(
		10_000,
		projectiles,
		projectile_meshes,
		WorldRadius,
		ShipMaxSpeed,
		TorpedoMaxSpeed,
		BulletMaxSpeed)

func _physics_process(_delta: float) -> void:
  # call to enable bullet culling
	update_active_camera_pos(get_viewport().get_camera_3d().global_position)
```

For each projectile type you want, create a new resource which extends `Projectile` and set the desired properties.

On your hurtboxes, add this:
```swift
func _enter_tree() -> void:
	BulletPoolerAutoload.register_bullet_collider(self)

func _exit_tree() -> void:
	BulletPoolerAutoload.deregister_bullet_collider(self)

func get_resistance() -> float:
  return <your hitboxes resistance value>
```


Then to spawn a bullet:
```swift
BulletPoolerAutoload.spawn_bullet(spawn_position, bullet_velocity, SomeProjectile, has_bigger_mesh, excluded_hurtboxes_rids)
```

