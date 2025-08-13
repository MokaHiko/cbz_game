--- Declare Level assets --- // Will be used later to determine what needs to be in memory during level
local helmet_gltf
local ground_gltf
local ground_detail_gltf
local soldier_gltf
local cube_gltf 
local grass_blade_gltf

local pbr_material

--- Spawn level entities ---
local main_camera
local my_helmet
local sun

--- Character Info ---
local movespeed = 5.0

local time = 0.0

function init()
    --- Load level assets ---
    pbr_material = material.load("assets/materials/pbr.material.lua")

    helmet_gltf = gltf.load(
        "/Users/christianmarkg.solon/dev/mx_app/build/_deps/gltf_sample_models-src/2.0/DamagedHelmet/glTF/DamagedHelmet.gltf")
    cube_gltf = gltf.load(
        "/Users/christianmarkg.solon/dev/Vulkan-Samples-Assets/scenes/cube.gltf")

    ground_gltf = gltf.load("/Users/christianmarkg.solon/Downloads/kenney_mini-arena/Models/GLB format/floor.glb")
    ground_detail_gltf = gltf.load("/Users/christianmarkg.solon/Downloads/kenney_mini-arena/Models/GLB format/floor-detail.glb")
 	grass_blade_gltf = gltf.load("/Users/christianmarkg.solon/Downloads/source/archive/sketch.gltf")

    soldier_gltf = gltf.load(
        "/Users/christianmarkg.solon/Downloads/kenney_mini-arena/Models/GLB format/character-soldier.glb")
	-- soldier_gltf = gltf.load("/Users/christianmarkg.solon/dev/cubozoa_game/assets/models/twurdle/ImmortalJelly_Loop_GameJam.gltf")


    --- Spawn level entities ---
    main_camera = camera.spawn("Camera")

	-- Close View
    position.set(main_camera, 0.0, 5.0, 10.0)
    rotation.set_euler(main_camera, math.rad(-20), 0, 0)

	-- Rts View
    -- position.set(main_camera, 0.0, 10.0, 7.0)
    -- rotation.set_euler(main_camera, math.rad(-60), 0, 0)

    sun = lights.spawn_directional("Sun")
    rotation.set_euler(sun, math.rad(-34), math.rad(-76), 0);

    for x = -5, 5 do
        for z = -5, 5 do
			groundType = math.random(1, 3)
			if(groundType > 1)  then
            ground = gltf.spawn(ground_gltf)
            position.set(ground, x, 0, z)
			else
            ground = gltf.spawn(ground_detail_gltf)
            position.set(ground, x, 0, z)
			end
        end
    end

            -- leaf = gltf.spawn(grass_blade_gltf)
            -- position.set(leaf, x, 0, z)
    for i = -5, 5 do
        soldier = gltf.spawn(soldier_gltf)
        position.set(soldier, i, 0.0, 0.0)

		-- if(i % 2 == 0) then
        -- soldier = gltf.spawn(soldier_gltf)
        -- position.set(soldier, i, 0.0, 0.0)
        -- position.set(soldier, i, 0.5, 0.0)
        -- scale.set(soldier, 0.1, 0.1, 0.1)
        -- rotation.set_euler(soldier, 0.0, math.rad(-90.0), 0.0)
		-- end
    end

    my_helmet = gltf.spawn(helmet_gltf)
    position.set(my_helmet, 1.0, 5.0, 8.0)

    some_cube = gltf.spawn(cube_gltf)
    position.set(some_cube, -1.0, 5.0, 8.0)
    scale.set(some_cube, 0.1, 0.1, 0.1)
end

function update(dt)
	time = time + dt
    -- local pos = position.get(my_helmet)
    -- position.set(my_helmet, pos.x, pos.y, pos.z - movespeed * dt)
	rs = math.rad(250 * time)
    rotation.set_euler(my_helmet, rs, rs, rs)
end

