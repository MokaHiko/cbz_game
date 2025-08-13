-- butcher.lua
return {
    type = "unit",

    description = {
        name = "butcher",
        text = "He creepy and smelly"
    },

    stats = {
        health = 100,
        mana = 100,
        damage = 10,
        armor = 100,
        movement_speed = 5
    },

    -- Abilities -- 
    attack = function(self, target)
        combat.attack(damage_type.slashing, target, damage)
        animation.play("Cleave")

        combat.inc_status(target, combat.status.bleed)
        if (combat.is_status(target, combat.status.bleed)) then
            combat.attack(damage_type.bleed, target, 1)
        end
    end,

    attack_desc = {
        name = "Cleave",
        text = "Slices into the target applying a stack bleed. Cleave does more damage each stack of bleed"
    },

    passive = function(self, dt)
        combat.attack_circle(get_position(), 10.0, damage_type.rot, damage)
        animation.play("Rot")
    end,

    passive_desc = {
        name = "Decay",
        text = "While moving you deal rot damage"
    },

    active = function(self)
        -- cast a special ability
    end,

    -- Callbacks -- 
    on_spawn = function(self, target)
        combat.attack(damage_type.fire, target, damage)
    end,

    on_hit = function(self, damage)
        -- maybe trigger a bleed effect or sound
    end,

    on_level = function(self, level)
        -- maybe trigger a bleed effect or sound
    end
}
