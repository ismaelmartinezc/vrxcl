#include "g_local.h"

void lightningstorm_think (edict_t *self) {
    edict_t *e = NULL;
    vec3_t start, end, dir;
    trace_t tr;

    // owner must be alive
    if (!G_EntIsAlive(self->owner) || vrx_has_flag(self->owner) || (self->delay < level.time)) {
        G_FreeEdict(self);
        return;
    }

    // calculate randomized starting position
    VectorCopy(self->s.origin, start);
    start[0] += GetRandom(0, (int) self->dmg_radius) * crandom();
    start[1] += GetRandom(0, (int) self->dmg_radius) * crandom();
    tr = gi.trace(self->s.origin, NULL, NULL, start, NULL, MASK_SOLID);
	VectorCopy(tr.endpos, start);
	VectorCopy(start, end);
	end[2] += 8192;
	tr = gi.trace(start, NULL, NULL, end, NULL, MASK_SOLID);
	VectorCopy(tr.endpos, start);

	// calculate ending position
	VectorCopy(start, end);
	end[2] -= 8192;
	tr = gi.trace(start, NULL, NULL, end, NULL, MASK_SHOT);
	VectorCopy(tr.endpos, end);

	while ((e = findradius(e, tr.endpos, LIGHTNING_STRIKE_RADIUS)) != NULL)
	{
		//FIXME: make a noise when we hit something?
		if (e && e->inuse && e->takedamage)
		{
			tr = gi.trace(end, NULL, NULL, e->s.origin, NULL, MASK_SHOT);
			VectorSubtract(tr.endpos, end, dir);
			VectorNormalize(dir);
			T_Damage(e, self, self->owner, dir, tr.endpos, tr.plane.normal, self->dmg, 0, DAMAGE_ENERGY, MOD_LIGHTNING_STORM);
		}
	}
	
	gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_HEATBEAM);
	gi.WriteShort (self-g_edicts);
	gi.WritePosition (start);
	gi.WritePosition (end);
	gi.multicast (end, MULTICAST_PVS);

//	gi.sound (self, CHAN_WEAPON, gi.soundindex("abilities/chargedbolt1.wav"), 1, ATTN_NORM, 0);

	self->nextthink = level.time + GetRandom(LIGHTNING_MIN_DELAY, LIGHTNING_MAX_DELAY) * FRAMETIME;
}

void SpawnLightningStorm (edict_t *ent, vec3_t start, float radius, int duration, int damage)
{
	edict_t *storm;

	storm = G_Spawn();
	storm->solid = SOLID_NOT;
	storm->svflags |= SVF_NOCLIENT;
	storm->owner = ent;
	storm->delay = level.time + duration;
	storm->nextthink = level.time + FRAMETIME;
	storm->think = lightningstorm_think;
	VectorCopy(start, storm->s.origin);
	VectorCopy(start, storm->s.old_origin);
	storm->dmg_radius = radius;
	storm->dmg = damage;
	gi.linkentity(storm);
}

void Cmd_LightningStorm_f (edict_t *ent, float skill_mult, float cost_mult)
{
	int		slvl = ent->myskills.abilities[LIGHTNING_STORM].current_level;
	int		damage, duration, cost=LIGHTNING_COST*cost_mult;
	float	radius;
	vec3_t	forward, offset, right, start, end;
	trace_t	tr;

	if (!V_CanUseAbilities(ent, LIGHTNING_STORM, cost, true))
		return;

	damage = LIGHTNING_INITIAL_DAMAGE + LIGHTNING_ADDON_DAMAGE * slvl * skill_mult;
	duration = LIGHTNING_INITIAL_DURATION + LIGHTNING_ADDON_DURATION * slvl;
	radius = LIGHTNING_INITIAL_RADIUS + LIGHTNING_ADDON_DURATION * slvl;

	// randomize damage
	damage = GetRandom((int)(0.5*damage), damage);

	// get starting position and forward vector
	AngleVectors (ent->client->v_angle, forward, right, NULL);
	VectorSet(offset, 0, 8,  ent->viewheight-8);
	P_ProjectSource(ent->client, ent->s.origin, offset, forward, right, start);

	// get ending position
	VectorCopy(start, end);
	VectorMA(start, 8192, forward, end);

	tr = gi.trace(start, NULL, NULL, end, ent, MASK_SHOT);

	SpawnLightningStorm(ent, tr.endpos, radius, duration, damage);

	ent->client->ability_delay = level.time + LIGHTNING_ABILITY_DELAY/* * cost_mult*/;
	ent->client->pers.inventory[power_cube_index] -= cost;

	// write a nice effect so everyone knows we've cast a spell
	gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_TELEPORT_EFFECT);
	gi.WritePosition (ent->s.origin);
	gi.multicast (ent->s.origin, MULTICAST_PVS);

	//  entity made a sound, used to alert monsters
	ent->lastsound = level.framenum;

    gi.sound(ent, CHAN_WEAPON, gi.soundindex("abilities/eleccast.wav"), 1, ATTN_NORM, 0);
}