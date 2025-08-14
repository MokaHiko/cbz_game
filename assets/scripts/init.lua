--- Declare Level assets --- // Will be used later to determine what needs to be in memory during level
local helmet_gltf
local ground_gltf
local ground_detail_gltf
local soldier_gltf

--- Spawn level entities ---
local main_camera
local my_helmet
local sun

--- Character Info ---
-- local movespeed = 5.0
local time = 0.0

function Init()
	--- Load level assets ---
	helmet_gltf = gltf.load("models/DamagedHelmet/DamagedHelmet.gltf")

	ground_gltf = gltf.load("models/Kenney/floor.glb")
	ground_detail_gltf = gltf.load("models/Kenney/floor-detail.glb")

	soldier_gltf = gltf.load("models/Kenney/character-soldier.glb")

	--- Spawn level entities ---
	main_camera = camera.spawn("Camera")

	-- Close View
	position.set(main_camera, 0.0, 5.0, 10.0)
	rotation.set_euler(main_camera, math.rad(-20), 0, 0)

	-- Rts View
	-- position.set(main_camera, 0.0, 10.0, 7.0)
	-- rotation.set_euler(main_camera, math.rad(-60), 0, 0)
 
	sun = lights.spawn_directional("Sun")
	rotation.set_euler(sun, math.rad(-34), math.rad(-76), 0)

	for x = -5, 5 do
		for z = -5, 5 do
			local groundType = math.random(1, 3)
			if groundType > 1 then
				local ground = gltf.spawn(ground_gltf)
				position.set(ground, x, 0, z)
			else
				local ground = gltf.spawn(ground_detail_gltf)
				position.set(ground, x, 0, z)
			end
		end
	end

	for i = -5, 5 do
		local soldier = gltf.spawn(soldier_gltf)
		position.set(soldier, i, 0.0, 0.0)
	end

	my_helmet = gltf.spawn(helmet_gltf)
	position.set(my_helmet, 1.0, 5.0, 8.0)
end

function Update(dt)
	time = time + dt

	local rs = math.rad(250 * time)
	rotation.set_euler(my_helmet, rs, rs, rs)
end
