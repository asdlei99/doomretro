/*
========================================================================

                           D O O M  R e t r o
         The classic, refined DOOM source port. For Windows PC.

========================================================================

  Copyright © 1993-2012 id Software LLC, a ZeniMax Media company.
  Copyright © 2013-2018 Brad Harding.

  DOOM Retro is a fork of Chocolate DOOM. For a list of credits, see
  <https://github.com/bradharding/doomretro/wiki/CREDITS>.

  This file is part of DOOM Retro.

  DOOM Retro is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation, either version 3 of the License, or (at your
  option) any later version.

  DOOM Retro is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with DOOM Retro. If not, see <https://www.gnu.org/licenses/>.

  DOOM is a registered trademark of id Software LLC, a ZeniMax Media
  company, in the US and/or other countries and is used without
  permission. All other trademarks are the property of their respective
  holders. DOOM Retro is in no way affiliated with nor endorsed by
  id Software.

========================================================================
*/

#include "c_console.h"
#include "doomstat.h"
#include "hu_stuff.h"
#include "i_gamepad.h"
#include "i_system.h"
#include "m_config.h"
#include "m_misc.h"
#include "m_random.h"
#include "p_local.h"
#include "p_tick.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "w_wad.h"
#include "z_zone.h"

int         r_blood = r_blood_default;
int         r_bloodsplats_max = r_bloodsplats_max_default;
int         r_bloodsplats_total;
dboolean    r_corpses_color = r_corpses_color_default;
dboolean    r_corpses_mirrored = r_corpses_mirrored_default;
dboolean    r_corpses_moreblood = r_corpses_moreblood_default;
dboolean    r_corpses_nudge = r_corpses_nudge_default;
dboolean    r_corpses_slide = r_corpses_slide_default;
dboolean    r_corpses_smearblood = r_corpses_smearblood_default;
dboolean    r_floatbob = r_floatbob_default;
dboolean    r_rockettrails = r_rockettrails_default;
dboolean    r_shadows = r_shadows_default;

mobjtype_t  pufftype = MT_PUFF;
mobj_t      *missilemobj;
mapthing_t  playerstart;

static fixed_t floatbobdiffs[64] =
{
     25695,  25695,  25447,  24955,  24222,  23256,  22066,  20663,
     19062,  17277,  15325,  13226,  10999,   8667,   6251,   3775,
      1262,  -1262,  -3775,  -6251,  -8667, -10999, -13226, -15325,
    -17277, -19062, -20663, -22066, -23256, -24222, -24955, -25447,
    -25695, -25695, -25447, -24955, -24222, -23256, -22066, -20663,
    -19062, -17277, -15325, -13226, -11000,  -8667,  -6251,  -3775,
     -1262,   1262,   3775,   6251,   8667,  10999,  13226,  15325,
     17277,  19062,  20663,  22066,  23256,  24222,  24955,  25447
};

extern fixed_t      animatedliquiddiffs[64];
extern int          deadlookdir;
extern int          deathcount;
extern msecnode_t   *sector_list;   // phares 3/16/98
extern dboolean     canmodify;
extern dboolean     usemouselook;

void A_Recoil(weapontype_t weapon);
void G_PlayerReborn(void);
void P_DelSeclist(msecnode_t *node);

//
//
// P_SetMobjState
// Returns true if the mobj is still present.
//
dboolean P_SetMobjState(mobj_t *mobj, statenum_t state)
{
    do
    {
        state_t *st;

        if (state == S_NULL)
        {
            mobj->state = (state_t *)S_NULL;
            P_RemoveMobj(mobj);
            return false;
        }

        st = &states[state];
        mobj->state = st;
        mobj->tics = st->tics;
        mobj->sprite = st->sprite;
        mobj->frame = st->frame;

        // Modified handling.
        // Call action functions when the state is set
        if (st->action)
            st->action(mobj, NULL, NULL);

        state = st->nextstate;
    } while (!mobj->tics);

    return true;
}

dboolean P_SetMobjStateNF(mobj_t *mobj, statenum_t state)
{
    state_t *st;

    if (state == HS_NULL)
    {
        mobj->state = (state_t *)HS_NULL;
        P_RemoveMobj(mobj);
        return false;
    }

    st = &states[state];
    mobj->state = st;
    mobj->tics = st->tics;
    mobj->sprite = st->sprite;
    mobj->frame = st->frame;
    return true;
}

//
// P_ExplodeMissile
//
void P_ExplodeMissile(mobj_t *mo)
{
    if (mo->type == HMT_WHIRLWIND && gamemission == heretic)
        if (++mo->special2.i < 60)
            return;

    mo->momx = 0;
    mo->momy = 0;
    mo->momz = 0;

    P_SetMobjState(mo, mo->info->deathstate);

    mo->flags &= ~MF_MISSILE;

    if (gamemission != heretic)
    {
        mo->tics = MAX(1, mo->tics - (M_Random() & 3));

        // [BH] make explosion translucent
        if (mo->type == MT_ROCKET)
        {
            mo->colfunc = tlcolfunc;
            mo->flags2 &= ~MF2_CASTSHADOW;
        }
    }

    if (mo->info->deathsound)
        S_StartSound(mo, mo->info->deathsound);
}

void P_FloorBounceMissile(mobj_t *mo)
{
    mo->momz = -mo->momz;
    P_SetMobjState(mo, mobjinfo[mo->type].deathstate);
}

void P_ThrustMobj(mobj_t *mo, angle_t angle, fixed_t move)
{
    angle >>= ANGLETOFINESHIFT;
    mo->momx += FixedMul(move, finecosine[angle]);
    mo->momy += FixedMul(move, finesine[angle]);
}

int P_FaceMobj(mobj_t *source, mobj_t *target, angle_t *delta)
{
    angle_t diff;
    angle_t angle1 = source->angle;
    angle_t angle2 = R_PointToAngle2(source->x, source->y, target->x, target->y);

    if (angle2 > angle1)
    {
        diff = angle2 - angle1;

        if (diff > ANG180)
        {
            *delta = ANG_MAX - diff;
            return 0;
        }
        else
        {
            *delta = diff;
            return 1;
        }
    }
    else
    {
        diff = angle1 - angle2;

        if (diff > ANG180)
        {
            *delta = ANG_MAX - diff;
            return 1;
        }
        else
        {
            *delta = diff;
            return 0;
        }
    }
}

dboolean P_SeekerMissile(mobj_t *actor, angle_t thresh, angle_t turnmax)
{
    int     dir;
    angle_t delta;
    angle_t angle;
    mobj_t  *target = (mobj_t *)actor->special1.m;

    if (target == NULL)
        return false;

    if (!(target->flags & MF_SHOOTABLE))
    {
        // Target died
        actor->special1.m = NULL;
        return false;
    }

    dir = P_FaceMobj(actor, target, &delta);

    if (delta > thresh)
    {
        delta >>= 1;

        if (delta > turnmax)
            delta = turnmax;
    }

    if (dir)
        // Turn clockwise
        actor->angle += delta;
    else
        // Turn counter clockwise
        actor->angle -= delta;

    angle = actor->angle >> ANGLETOFINESHIFT;
    actor->momx = FixedMul(actor->info->speed, finecosine[angle]);
    actor->momy = FixedMul(actor->info->speed, finesine[angle]);

    if (actor->z + actor->height < target->z || target->z + target->height < actor->z)
        // Need to seek vertically
        actor->momz = (target->z - actor->z) / MAX(1, P_ApproxDistance(target->x - actor->x, target->y - actor->y) / actor->info->speed);

    return true;
}

//
// P_XYMovement
//
#define STOPSPEED       0x1000
#define WATERFRICTION   0xFB00
#define LOWFRICTION     0xF900
#define FLYFRICTION     0xEB00

static int  puffcount;

static void P_XYMovement(mobj_t *mo)
{
    player_t    *player;
    fixed_t     xmove, ymove;
    mobjtype_t  type = mo->type;
    int         flags = mo->flags;
    int         flags2 = mo->flags2;
    int         flags3 = mo->flags3;
    dboolean    corpse = ((flags & MF_CORPSE) && (type != MT_BARREL || gamemission == heretic));
    int         stepdir = 0;
    int         special = mo->subsector->sector->special;

    if (!(mo->momx | mo->momy))
    {
        if (flags & MF_SKULLFLY)
        {
            // the skull slammed into something
            mo->flags &= ~MF_SKULLFLY;
            mo->momz = 0;

            if (gamemission == heretic)
            {
                mo->momx = 0;
                mo->momy = 0;
                P_SetMobjState(mo, mo->info->seestate);
            }
            else
                P_SetMobjState(mo, mo->info->spawnstate);
        }

        return;
    }

    if (mo->flags3 & MF3_WINDTHRUST)
    {
        static int  windtab[3] = { 2048 * 5, 2048 * 10, 2048 * 25 };

        switch (special)
        {
            case 40:
            case 41:
            case 42:
                P_ThrustMobj(mo, 0, windtab[special - 40]);
                break;

            case 43:
            case 44:
            case 45:
                P_ThrustMobj(mo, ANG90, windtab[special - 43]);
                break;

            case 46:
            case 47:
            case 48:
                P_ThrustMobj(mo, ANG270, windtab[special - 46]);
                break;

            case 49:
            case 50:
            case 51:
                P_ThrustMobj(mo, ANG180, windtab[special - 49]);
                break;
        }
    }

    player = mo->player;

    // [BH] give smoke trails to rockets
    if (flags2 & MF2_SMOKETRAIL)
        if (puffcount++ > 1)
            P_SpawnSmokeTrail(mo->x, mo->y, mo->z, mo->angle);

    mo->momx = BETWEEN(-MAXMOVE, mo->momx, MAXMOVE);
    mo->momy = BETWEEN(-MAXMOVE, mo->momy, MAXMOVE);

    xmove = mo->momx;
    ymove = mo->momy;

    if (xmove < 0)
    {
        xmove = -xmove;
        stepdir = 1;
    }

    if (ymove < 0)
    {
        ymove = -ymove;
        stepdir |= 2;
    }

    do
    {
        fixed_t stepx = MIN(xmove, MAXMOVE_STEP);
        fixed_t stepy = MIN(ymove, MAXMOVE_STEP);
        fixed_t ptryx = mo->x + ((stepdir & 1) ? -stepx : stepx);
        fixed_t ptryy = mo->y + ((stepdir & 2) ? -stepy : stepy);

        xmove -= stepx;
        ymove -= stepy;

        // killough 3/15/98: Allow objects to drop off
        if (!P_TryMove(mo, ptryx, ptryy, true))
        {
            // blocked move
            // killough 8/11/98: bouncing off walls
            // killough 10/98:
            // Add ability for objects other than players to bounce on ice
            if (!(flags & MF_MISSILE) && !player && blockline && mo->z <= mo->floorz
                && P_GetFriction(mo, NULL) > ORIG_FRICTION)
            {
                fixed_t r = ((blockline->dx >> FRACBITS) * mo->momx
                            + (blockline->dy >> FRACBITS) * mo->momy)
                            / ((blockline->dx >> FRACBITS) * (blockline->dx >> FRACBITS)
                            + (blockline->dy >> FRACBITS) * (blockline->dy >> FRACBITS));
                fixed_t x = FixedMul(r, blockline->dx);
                fixed_t y = FixedMul(r, blockline->dy);

                // reflect momentum away from wall
                mo->momx = x * 2 - mo->momx;
                mo->momy = y * 2 - mo->momy;

                // if under gravity, slow down in
                // direction perpendicular to wall.
                if (!(flags & MF_NOGRAVITY))
                {
                    mo->momx = (mo->momx + x) / 2;
                    mo->momy = (mo->momy + y) / 2;
                }
            }
            else if (flags3 & MF3_SLIDE)
            {
                // try to slide along it
                P_SlideMove(mo);
                break;
            }
            else if (flags & MF_MISSILE)
            {
                // explode a missile
                if (ceilingline && ceilingline->backsector
                    && ceilingline->backsector->ceilingpic == skyflatnum
                    && mo->z > ceilingline->backsector->ceilingheight)
                {
                    // Hack to prevent missiles exploding
                    // against the sky.
                    // Does not handle sky floors.

                    if (type == HMT_BLOODYSKULL && gamemission == heretic)
                    {
                        mo->momx = 0;
                        mo->momy = 0;
                        mo->momz = -FRACUNIT;
                        return;
                    }

                    // [BH] still play sound when firing BFG into sky
                    if (type == MT_BFG && gamemission != heretic)
                        S_StartSound(mo, mo->info->deathsound);

                    P_RemoveMobj(mo);
                    return;
                }

                P_ExplodeMissile(mo);
            }
            else
            {
                mo->momx = 0;
                mo->momy = 0;
            }
        }
    } while (xmove || ymove);

    if (flags & (MF_MISSILE | MF_SKULLFLY))
        return;         // no friction for missiles or lost souls ever

    if (mo->z > mo->floorz && !(flags2 & MF2_ONMOBJ) && !(flags3 & MF3_FLY))
        return;         // no friction when airborne

    // [BH] spawn random blood splats on floor as corpses slide
    if (corpse && !(flags & MF_NOBLOOD) && mo->blood && r_corpses_slide && r_corpses_smearblood
        && (mo->momx || mo->momy) && mo->bloodsplats && r_bloodsplats_max && !mo->nudge)
    {
        int blood = mobjinfo[mo->blood].blood;

        if (blood)
        {
            int radius = (spritewidth[sprites[mo->sprite].spriteframes[mo->frame & FF_FRAMEMASK].lump[0]] >> FRACBITS) >> 1;
            int max = MIN((ABS(mo->momx) + ABS(mo->momy)) >> (FRACBITS - 2), 8);
            int floorz = mo->floorz;

            for (int i = 0, x, y; i < max; i++)
            {
                if (!mo->bloodsplats)
                    break;

                x = mo->x + (M_RandomInt(-radius, radius) << FRACBITS);
                y = mo->y + (M_RandomInt(-radius, radius) << FRACBITS);

                if (R_PointInSubsector(x, y)->sector->floorheight <= floorz)
                    P_SpawnBloodSplat(x, y, blood, floorz, mo);
            }
        }
    }

    if ((corpse || (flags2 & MF2_FALLING))
        && (mo->momx > FRACUNIT / 4 || mo->momx < -FRACUNIT / 4
            || mo->momy > FRACUNIT / 4 || mo->momy < -FRACUNIT / 4)
        && mo->floorz != mo->subsector->sector->floorheight)
        return;         // do not stop sliding if halfway off a step with some momentum

    if (mo->momx > -STOPSPEED && mo->momx < STOPSPEED && mo->momy > -STOPSPEED && mo->momy < STOPSPEED
        && (!player || (!player->cmd.forwardmove && !player->cmd.sidemove) || player->mo != mo))
    {
        mo->momx = 0;
        mo->momy = 0;

        // killough 10/98: kill any bobbing momentum too (except in voodoo dolls)
        if (player && player->mo == mo)
        {
            if (player->chickentics)
                if ((unsigned int)((player->mo->state - states) - HS_CHICPLAY_RUN1) < 4)
                    P_SetMobjState(mo, HS_CHICPLAY);

            player->momx = 0;
            player->momy = 0;
        }
    }
    else if ((flags2 & MF2_FEETARECLIPPED) && corpse && !player)
    {
        // [BH] reduce friction for corpses in water
        mo->momx = FixedMul(mo->momx, WATERFRICTION);
        mo->momy = FixedMul(mo->momy, WATERFRICTION);
    }
    else if ((flags3 & MF3_FLY) && mo->z > mo->floorz && !(flags2 & MF2_ONMOBJ))
    {
        mo->momx = FixedMul(mo->momx, FLYFRICTION);
        mo->momy = FixedMul(mo->momy, FLYFRICTION);
    }
    else if (special == 15)
    {
        mo->momx = FixedMul(mo->momx, LOWFRICTION);
        mo->momy = FixedMul(mo->momy, LOWFRICTION);
    }
    else
    {
        // phares 3/17/98
        //
        // Friction will have been adjusted by friction thinkers for
        // icy or muddy floors. Otherwise it was never touched and
        // remained set at ORIG_FRICTION
        //
        // killough 8/28/98: removed inefficient thinker algorithm,
        // instead using touching_sectorlist in P_GetFriction() to
        // determine friction (and thus only when it is needed).
        //
        // killough 10/98: changed to work with new bobbing method.
        // Reducing player momentum is no longer needed to reduce
        // bobbing, so ice works much better now.
        fixed_t friction = P_GetFriction(mo, NULL);

        mo->momx = FixedMul(mo->momx, friction);
        mo->momy = FixedMul(mo->momy, friction);

        // killough 10/98: Always decrease player bobbing by ORIG_FRICTION.
        // This prevents problems with bobbing on ice, where it was not being
        // reduced fast enough, leading to all sorts of kludges being developed.
        if (player && player->mo == mo)     //  Not voodoo dolls
        {
            player->momx = FixedMul(player->momx, ORIG_FRICTION);
            player->momy = FixedMul(player->momy, ORIG_FRICTION);
        }
    }
}

//
// P_ZMovement
//
static void P_ZMovement(mobj_t *mo)
{
    player_t    *player = mo->player;
    int         flags = mo->flags;

    // check for smooth step up
    if (player && player->mo == mo && mo->z < mo->floorz)
    {
        player->viewheight -= mo->floorz - mo->z;
        player->deltaviewheight = (VIEWHEIGHT - player->viewheight) >> 3;
    }

    // adjust height
    mo->z += mo->momz;

    // float down towards target if too close
    if (!((flags ^ MF_FLOAT) & (MF_FLOAT | MF_SKULLFLY | MF_INFLOAT)) && mo->target)
    {
        fixed_t delta = (mo->target->z + (mo->height >> 1) - mo->z) * 3;

        if (P_ApproxDistance(mo->x - mo->target->x, mo->y - mo->target->y) < ABS(delta))
            mo->z += (delta < 0 ? -FLOATSPEED : FLOATSPEED);
    }

    if (player && (mo->flags3 & MF3_FLY) && mo->z > mo->floorz && (leveltime & 2))
        mo->z += finesine[(FINEANGLES / 160 * gametic) & FINEMASK] / 16;  // [JN] Smooth floating

    // clip movement
    if (mo->z <= mo->floorz)
    {
        // [BH] remove blood the moment it hits the ground and spawn a blood splat in its place
        if ((mo->flags2 & MF2_BLOOD) && mo->blood)
        {
            P_RemoveMobj(mo);

            if (r_bloodsplats_max)
                P_SpawnBloodSplat(mo->x, mo->y, mo->blood, mo->floorz, NULL);

            return;
        }

        // hit the floor
        if (flags & MF_SKULLFLY)
            mo->momz = -mo->momz;       // the skull slammed into something

        if (mo->momz < 0)
        {
            if (player && player->mo == mo)
            {
                player->jumptics = 7;

                if (mo->momz < -GRAVITY * 8 && !(mo->flags3 & MF3_FLY))
                {
                    // Squat down.
                    // Decrease viewheight for a moment
                    // after hitting the ground (hard),
                    // and utter appropriate sound.
                    player->deltaviewheight = mo->momz >> 3;

                    if (mo->health > 0)
                        S_StartSound(mo, SFX_OOF);
                }
            }

            mo->momz = 0;
        }

        if (gamemission == heretic)
            if (mo->z - mo->momz > mo->floorz)
                P_HitFloor(mo);

        mo->z = mo->floorz;

        if (mo->info->crashstate && (flags & MF_CORPSE))
        {
            P_SetMobjState(mo, mo->info->crashstate);
            return;
        }

        if (!((flags ^ MF_MISSILE) & (MF_MISSILE | MF_NOCLIP)))
        {
            if (gamemission == heretic)
            {
                mo->z = mo->floorz;

                if (mo->flags3 & MF3_FLOORBOUNCE)
                    P_FloorBounceMissile(mo);
                else if (mo->type != HMT_MNTRFX2)
                    P_ExplodeMissile(mo);
            }
            else
                P_ExplodeMissile(mo);

            return;
        }

    }
    else if (mo->flags3 & MF3_LOGRAV)
    {
        if (!mo->momz)
            mo->momz = -(GRAVITY >> 3) * 2;
        else
            mo->momz -= GRAVITY >> 3;
    }
    else if (!(flags & MF_NOGRAVITY))
    {
        if (!mo->momz)
            mo->momz = -GRAVITY;

        mo->momz -= GRAVITY;
    }

    if (mo->z + mo->height > mo->ceilingz)
    {
        if (flags & MF_SKULLFLY)
            mo->momz = -mo->momz;       // the skull slammed into something

        // hit the ceiling
        if (mo->momz > 0)
            mo->momz = 0;

        mo->z = mo->ceilingz - mo->height;

        if (!((flags ^ MF_MISSILE) & (MF_MISSILE | MF_NOCLIP)))
        {
            if (mo->subsector->sector->ceilingpic == skyflatnum)
            {
                if (mo->type == HMT_BLOODYSKULL && gamemission == heretic)
                {
                    mo->momx = 0;
                    mo->momy = 0;
                    mo->momz = -FRACUNIT;
                }
                else
                    P_RemoveMobj(mo);
            }
            else
                P_ExplodeMissile(mo);
        }
    }
}

//
// P_NightmareRespawn
//
static void P_NightmareRespawn(mobj_t *mobj)
{
    fixed_t     x = mobj->spawnpoint.x << FRACBITS;
    fixed_t     y = mobj->spawnpoint.y << FRACBITS;
    fixed_t     z = ((mobj->flags & MF_SPAWNCEILING) ? ONCEILINGZ : ONFLOORZ);
    mobj_t      *mo;
    mapthing_t  *mthing = &mobj->spawnpoint;

    // [BH] Fix <https://doomwiki.org/wiki/(0,0)_respawning_bug>.
    if (!x && !y)
    {
        x = mobj->x;
        y = mobj->y;
    }

    // something is occupying it's position?
    if (!P_CheckPosition(mobj, x, y))
        return;         // no respawn

    // spawn a teleport fog at old spot
    //  because of removal of the body?
    mo = P_SpawnMobj(mobj->x, mobj->y, z, (gamemission == heretic ? HMT_TFOG : MT_TFOG));
    mo->angle = mobj->angle;

    // initiate teleport sound
    S_StartSound(mo, SFX_TELEPT);

    // spawn a teleport fog at the new spot
    if (x != mobj->x || y != mobj->y)
    {
        mo = P_SpawnMobj(x, y, z, (gamemission == heretic ? HMT_TFOG : MT_TFOG));
        mo->angle = ANG45 * (mthing->angle / 45);
        S_StartSound(mo, SFX_TELEPT);
    }

    // spawn the new monster
    // inherit attributes from deceased one
    mo = P_SpawnMobj(x, y, z, mobj->type);
    mo->spawnpoint = mobj->spawnpoint;
    mo->angle = ANG45 * (mthing->angle / 45);

    mo->flags &= ~MF_COUNTKILL;

    if (mthing->options & MTF_AMBUSH)
        mo->flags |= MF_AMBUSH;

    mo->reactiontime = 18;

    // remove the old monster
    P_RemoveMobj(mobj);
}

//
// P_MobjThinker
//
void P_MobjThinker(mobj_t *mobj)
{
    int         flags = mobj->flags;
    int         flags2 = mobj->flags2;
    player_t    *player = mobj->player;
    sector_t    *sector = mobj->subsector->sector;

    // [AM] Handle interpolation unless we're an active player.
    if (mobj->interpolate == -1)
        mobj->interpolate = false;
    else if (!(player && mobj == player->mo))
    {
        // Assume we can interpolate at the beginning of the tic.
        mobj->interpolate = true;

        // Store starting position for mobj interpolation.
        mobj->oldx = mobj->x;
        mobj->oldy = mobj->y;
        mobj->oldz = mobj->z;
        mobj->oldangle = mobj->angle;
    }

    if (freeze && !player)
        return;

    if (mobj->nudge > 0)
        mobj->nudge--;

    // momentum movement
    if (mobj->momx || mobj->momy || (flags & MF_SKULLFLY))
    {
        P_XYMovement(mobj);

        if (mobj->thinker.function == P_RemoveThinkerDelayed)   // killough
            return;             // mobj was removed
    }

    // [BH] don't clip sprite if no longer in liquid
    if (!sector->isliquid)
        mobj->flags2 &= ~MF2_FEETARECLIPPED;

    // [BH] bob objects in liquid
    if ((flags2 & MF2_FEETARECLIPPED) && !(flags2 & MF2_NOLIQUIDBOB) && mobj->z <= sector->floorheight
        && !mobj->momz && !sector->heightsec && r_liquid_bob)
        mobj->z += animatedliquiddiffs[(mobj->floatbob + leveltime) & 63];

    // [BH] otherwise bob certain power-ups
    else if ((flags2 & MF2_FLOATBOB) && r_floatbob)
        mobj->z = BETWEEN(mobj->floorz, mobj->z + floatbobdiffs[(mobj->floatbob + leveltime) & 63], mobj->ceilingz);

    else if (mobj->z != mobj->floorz || mobj->momz)
    {
        if ((flags2 & MF2_PASSMOBJ) && !infiniteheight)
        {
            mobj_t  *onmo = P_CheckOnmobj(mobj);

            if (!onmo)
            {
                P_ZMovement(mobj);
                mobj->flags2 &= ~MF2_ONMOBJ;
            }
            else if (player && (onmo->player || (onmo->type == HMT_POD && gamemission == heretic)))
            {
                if (mobj->momz < -GRAVITY * 8)
                {
                    player->deltaviewheight = mobj->momz >> 3;

                    if (mobj->momz < -23 * FRACUNIT)
                        P_NoiseAlert(mobj);
                }

                if (onmo->z + onmo->height - mobj->z <= 24 * FRACUNIT)
                {
                    player->viewheight -= onmo->z + onmo->height - mobj->z;
                    player->deltaviewheight = (VIEWHEIGHT - player->viewheight) >> 3;
                    mobj->z = onmo->z + onmo->height;
                    mobj->flags2 |= MF2_ONMOBJ;
                }

                mobj->momz = 0;
            }
        }
        else
            P_ZMovement(mobj);

        if (mobj->thinker.function == P_RemoveThinkerDelayed)   // killough
            return;             // mobj was removed
    }
    else if (!(mobj->momx | mobj->momy) && !sentient(mobj))
    {
        // killough 9/12/98: objects fall off ledges if they are hanging off
        // slightly push off of ledge if hanging more than halfway off
        if (((flags & MF_CORPSE) || (flags & MF_DROPPED) || (mobj->type == MT_BARREL && gamemission != heretic))
            && mobj->z - mobj->dropoffz > 2 * FRACUNIT)
            P_ApplyTorque(mobj);
        else
        {
            // Reset torque
            mobj->flags2 &= ~MF2_FALLING;
            mobj->gear = 0;
        }
    }

    // cycle through states,
    //  calling action functions at transitions
    if (mobj->tics != -1)
    {
        mobj->tics--;

        // you can cycle through multiple states in a tic
        while (!mobj->tics)
            if (!P_SetMobjState(mobj, mobj->state->nextstate))
                return;
    }
    else
    {
        // check for nightmare respawn
        if ((flags & MF_COUNTKILL) && (gameskill == sk_nightmare || respawnmonsters))
        {
            mobj->movecount++;

            if (mobj->movecount >= 12 * TICRATE && !(leveltime & 31) && M_Random() <= 4)
                P_NightmareRespawn(mobj);
        }
    }
}

void P_BlasterMobjThinker(mobj_t *mobj)
{
    // Handle movement
    if (mobj->momx || mobj->momy || (mobj->z != mobj->floorz) || mobj->momz)
    {
        fixed_t     xfrac = mobj->momx >> 3;
        fixed_t     yfrac = mobj->momy >> 3;
        fixed_t     zfrac = mobj->momz >> 3;
        dboolean    changexy = (xfrac || yfrac);

        for (int i = 0; i < 8; i++)
        {
            if (changexy)
                if (!P_TryMove(mobj, mobj->x + xfrac, mobj->y + yfrac, true))
                {
                    // Blocked move
                    P_ExplodeMissile(mobj);
                    return;
                }

            mobj->z += zfrac;

            if (mobj->z <= mobj->floorz)
            {
                // Hit the floor
                mobj->z = mobj->floorz;
                P_HitFloor(mobj);
                P_ExplodeMissile(mobj);
                return;
            }

            if (mobj->z + mobj->height > mobj->ceilingz)
            {
                // Hit the ceiling
                mobj->z = mobj->ceilingz - mobj->height;
                P_ExplodeMissile(mobj);
                return;
            }

            if (changexy && (M_Random() < 64))
            {
                fixed_t z = mobj->z - 8 * FRACUNIT;

                if (z < mobj->floorz)
                    z = mobj->floorz;

                P_SpawnMobj(mobj->x, mobj->y, z, HMT_BLASTERSMOKE);
            }
        }
    }

    // Advance the state
    if (mobj->tics != -1)
    {
        mobj->tics--;

        while (!mobj->tics)
            if (!P_SetMobjState(mobj, mobj->state->nextstate))
                return;
    }
}

//
// P_SetShadowColumnFunction
//
void P_SetShadowColumnFunction(mobj_t *mobj)
{
    if (r_shadows_translucency)
        mobj->shadowcolfunc = ((mobj->flags & MF_FUZZ) && r_textures ? R_DrawFuzzyShadowColumn : R_DrawShadowColumn);
    else
        mobj->shadowcolfunc = ((mobj->flags & MF_FUZZ) && r_textures ? R_DrawSolidFuzzyShadowColumn : R_DrawSolidShadowColumn);
}

//
// P_SpawnMobj
//
mobj_t *P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type)
{
    mobj_t      *mobj = Z_Calloc(1, sizeof(*mobj), PU_LEVEL, NULL);
    state_t     *st;
    mobjinfo_t  *info = &mobjinfo[type];
    sector_t    *sector;
    static int  prevx, prevy;
    static int  prevbob;
    int         height = (z == ONCEILINGZ && (type != MT_KEEN || gamemission == heretic) && info->projectilepassheight ?
                    info->projectilepassheight : info->height);

    mobj->type = type;
    mobj->info = info;
    mobj->x = x;
    mobj->y = y;
    mobj->radius = info->radius;
    mobj->height = height;
    mobj->flags = info->flags;
    mobj->flags2 = info->flags2;
    mobj->flags3 = info->flags3;
    mobj->health = info->spawnhealth;

    if (gameskill != sk_nightmare)
        mobj->reactiontime = info->reactiontime;

    // do not set the state with P_SetMobjState,
    // because action routines cannot be called yet
    st = &states[info->spawnstate];

    // [BH] initialize certain mobj's animations to random start frame
    // so groups of same mobjs are deliberately out of sync
    if (info->frames > 1)
    {
        int frames = M_RandomInt(0, info->frames);
        int i = 0;

        while (i++ < frames && st->nextstate != S_NULL)
            st = &states[st->nextstate];
    }

    mobj->state = st;
    mobj->tics = st->tics;
    mobj->sprite = st->sprite;
    mobj->frame = st->frame;
    mobj->colfunc = info->colfunc;
    mobj->altcolfunc = info->altcolfunc;

    P_SetShadowColumnFunction(mobj);

    mobj->shadowoffset = info->shadowoffset;

    mobj->blood = info->blood;

    // [BH] set random pitch for monster sounds when spawned
    mobj->pitch = NORM_PITCH;

    if ((mobj->flags & MF_SHOOTABLE) && type != playermobjtype && (type != MT_BARREL || gamemission == heretic))
        mobj->pitch += M_RandomInt(-16, 16);

    // set subsector and/or block links
    P_SetThingPosition(mobj);

    sector = mobj->subsector->sector;
    mobj->dropoffz =           // killough 11/98: for tracking dropoffs
    mobj->floorz = sector->floorheight;
    mobj->ceilingz = sector->ceilingheight;

    // [BH] initialize bobbing things
    mobj->floatbob = prevbob = (x == prevx && y == prevy ? prevbob : M_Random());

    if (z == ONFLOORZ)
        mobj->z = mobj->floorz;
    else if (z == ONCEILINGZ)
        mobj->z = mobj->ceilingz - mobj->height;
    else if (z == FLOATRANDZ)
    {
        fixed_t space = mobj->ceilingz - info->height - mobj->floorz;

        mobj->z = (space > 48 * FRACUNIT ? (((space - 40 * FRACUNIT) * M_Random()) >> 8) + mobj->floorz + 40 * FRACUNIT : mobj->floorz);
    }
    else
        mobj->z = z;

    mobj->oldx = mobj->x;
    mobj->oldy = mobj->y;
    mobj->oldz = mobj->z;
    mobj->oldangle = mobj->angle;

    mobj->thinker.function = (type == MT_MUSICSOURCE && gamemission != heretic ? MusInfoThinker : P_MobjThinker);
    P_AddThinker(&mobj->thinker);

    if (!(mobj->flags & MF_SPAWNCEILING) && (mobj->flags2 & MF2_FOOTCLIP) && sector->isliquid && !sector->heightsec)
        mobj->flags2 |= MF2_FEETARECLIPPED;

    prevx = x;
    prevy = y;

    return mobj;
}

mapthing_t  itemrespawnque[ITEMQUEUESIZE];
int         itemrespawntime[ITEMQUEUESIZE];
int         iquehead;
int         iquetail;

//
// P_RemoveMobj
//
void P_RemoveMobj(mobj_t *mobj)
{
    int         flags = mobj->flags;
    mobjtype_t  type = mobj->type;

    if ((flags & MF_SPECIAL) && !(flags & MF_DROPPED) && type != MT_INV && type != MT_INS)
    {
        itemrespawnque[iquehead] = mobj->spawnpoint;
        itemrespawntime[iquehead] = leveltime;
        iquehead = (iquehead + 1) & (ITEMQUEUESIZE - 1);

        // lose one off the end?
        if (iquehead == iquetail)
            iquetail = (iquetail + 1) & (ITEMQUEUESIZE - 1);
    }

    // unlink from sector and block lists
    P_UnsetThingPosition(mobj);

    // [crispy] removed map objects may finish their sounds
    S_UnlinkSound(mobj);

    // Delete all nodes on the current sector_list
    if (sector_list)
    {
        P_DelSeclist(sector_list);
        sector_list = NULL;
    }

    mobj->flags |= (MF_NOSECTOR | MF_NOBLOCKMAP);

    P_SetTarget(&mobj->target, NULL);
    P_SetTarget(&mobj->tracer, NULL);
    P_SetTarget(&mobj->lastenemy, NULL);

    // free block
    P_RemoveThinker((thinker_t *)mobj);
}

//
// P_FindDoomedNum
// Finds a mobj type with a matching doomednum
// killough 8/24/98: rewrote to use hashing
//
mobjtype_t P_FindDoomedNum(unsigned int type)
{
    static struct
    {
        int first;
        int next;
    } *hash;

    mobjtype_t  i;
    int         nummobjtypes = (gamemission == heretic ? NUMMOBJTYPES : NUMHMOBJTYPES);

    if (!hash)
    {
        hash = Z_Malloc(sizeof(*hash) * nummobjtypes, PU_CACHE, (void **)&hash);

        for (i = 0; i < nummobjtypes; i++)
            hash[i].first = nummobjtypes;

        for (i = 0; i < nummobjtypes; i++)
            if (mobjinfo[i].doomednum != -1)
            {
                unsigned int    h = (unsigned int)mobjinfo[i].doomednum % nummobjtypes;

                hash[i].next = hash[h].first;
                hash[h].first = i;
            }
    }

    i = hash[type % nummobjtypes].first;

    while (i < nummobjtypes && (unsigned int)mobjinfo[i].doomednum != type)
        i = hash[i].next;

    return i;
}

//
// P_RespawnSpecials
//
void P_RespawnSpecials(void)
{
    fixed_t     x, y, z;
    mobj_t      *mo;
    mapthing_t  *mthing;
    int         i;

    if (!respawnitems)
        return;

    // nothing left to respawn?
    if (iquehead == iquetail)
        return;

    // wait at least 30 seconds
    if (leveltime - itemrespawntime[iquetail] < 30 * TICRATE)
        return;

    mthing = &itemrespawnque[iquetail];

    // find which type to spawn
    // killough 8/23/98: use table for faster lookup
    i = P_FindDoomedNum(mthing->type);

    x = mthing->x << FRACBITS;
    y = mthing->y << FRACBITS;
    z = ((mobjinfo[i].flags & MF_SPAWNCEILING) ? ONCEILINGZ : ONFLOORZ);

    // spawn a teleport fog at the new spot
    mo = P_SpawnMobj(x, y, z, (gamemission == heretic ? HMT_TFOG : MT_IFOG));
    S_StartSound(mo, sfx_itmbk);

    // spawn it
    mo = P_SpawnMobj(x, y, z, i);
    mo->spawnpoint = *mthing;
    mo->angle = ANG45 * (mthing->angle / 45);

    // pull it from the queue
    iquetail = (iquetail + 1) & (ITEMQUEUESIZE - 1);
}

//
// P_SpawnPlayer
// Called when a player is spawned on the level.
// Most of the player structure stays unchanged
//  between levels.
//
extern int lastlevel;
extern int lastepisode;

static void P_SpawnPlayer(const mapthing_t *mthing)
{
    mobj_t  *mobj;

    if (viewplayer->playerstate == PST_REBORN)
        G_PlayerReborn();

    mobj = P_SpawnMobj(mthing->x << FRACBITS, mthing->y << FRACBITS, ONFLOORZ, playermobjtype);

    mobj->angle = ((mthing->angle % 45) ? mthing->angle * (ANG45 / 45) : ANG45 * (mthing->angle / 45));
    mobj->player = viewplayer;
    mobj->health = viewplayer->health;

    viewplayer->mo = mobj;
    viewplayer->playerstate = PST_LIVE;
    viewplayer->refire = 0;
    viewplayer->message = NULL;
    viewplayer->damagecount = 0;
    viewplayer->bonuscount = 0;
    viewplayer->chickentics = 0;
    viewplayer->rain1 = NULL;
    viewplayer->rain2 = NULL;
    viewplayer->extralight = 0;
    viewplayer->fixedcolormap = 0;
    viewplayer->viewheight = VIEWHEIGHT;

    viewplayer->viewz = viewplayer->mo->z + viewplayer->viewheight;
    viewplayer->psprites[ps_weapon].sx = 0;
    viewplayer->mo->momx = 0;
    viewplayer->mo->momy = 0;
    viewplayer->momx = 0;
    viewplayer->momy = 0;
    viewplayer->lookdir = 0;
    viewplayer->recoil = 0;

    deathcount = 0;
    deadlookdir = -1;

    // setup gun psprite
    P_SetupPsprites();

    lastlevel = -1;
    lastepisode = -1;

    ST_Start(); // wake up the status bar
    HU_Start(); // wake up the heads up text
}

//
// P_SpawnMoreBlood
// [BH] Spawn blood splats around corpses
//
static void P_SpawnMoreBlood(mobj_t *mobj)
{
    int blood = mobjinfo[mobj->blood].blood;

    if (blood)
    {
        int radius = ((spritewidth[sprites[mobj->sprite].spriteframes[0].lump[0]] >> FRACBITS) >> 1) + 12;
        int max = M_RandomInt(50, 100) + radius;
        int x = mobj->x;
        int y = mobj->y;
        int floorz = mobj->floorz;

        if (!(mobj->flags & MF_SPAWNCEILING))
        {
            x += M_RandomInt(-radius / 3, radius / 3) << FRACBITS;
            y += M_RandomInt(-radius / 3, radius / 3) << FRACBITS;
        }

        for (int i = 0; i < max; i++)
        {
            int angle;
            int fx, fy;

            if (!mobj->bloodsplats)
                break;

            angle = M_RandomInt(0, FINEANGLES - 1);
            fx = x + FixedMul(M_RandomInt(0, radius) << FRACBITS, finecosine[angle]);
            fy = y + FixedMul(M_RandomInt(0, radius) << FRACBITS, finesine[angle]);

            P_SpawnBloodSplat(fx, fy, blood, floorz, mobj);
        }
    }
}

//
// P_SpawnMapThing
// The fields of the mapthing should
//  already be in host byte order.
//
mobj_t *P_SpawnMapThing(mapthing_t *mthing, int index, dboolean nomonsters)
{
    int     i;
    int     bit;
    mobj_t  *mobj;
    fixed_t x, y, z;
    short   type = mthing->type;
    int     flags;
    int     id = 0;

    // check for players specially
    if (type == Player1Start)
    {
        playerstart = *mthing;
        P_SpawnPlayer(mthing);
        return NULL;
    }
    else if ((type >= Player2Start && type <= Player4Start) || type == PlayerDeathmatchStart)
        return NULL;

    // check for appropriate skill level
    if (mthing->options & 16)
        return NULL;

    if (gameskill == sk_baby)
        bit = 1;
    else if (gameskill == sk_nightmare)
        bit = 4;
    else
        bit = 1 << (gameskill - 1);

    // Ambient sound sequences
    if (mthing->type >= 1200 && mthing->type < 1300)
    {
        P_AddAmbientSfx(mthing->type - 1200);
        return NULL;
    }

    // Check for boss spots
    if (mthing->type == 56 && gamemission == heretic)
    {
        P_AddBossSpot(mthing->x << FRACBITS, mthing->y << FRACBITS, ANG45 * (mthing->angle / 45));
        return NULL;
    }

    // killough 8/23/98: use table for faster lookup
    i = P_FindDoomedNum(type);

    if (i == NUMMOBJTYPES)
    {
        // [BH] make unknown thing type non-fatal and show console warning instead
        if (type != VisualModeCamera)
            C_Warning("Thing %s at (%i,%i) didn't spawn because it has an unknown type.",
                commify(index), mthing->x, mthing->y);

        return NULL;
    }

    if (!(mthing->options & (MTF_EASY | MTF_NORMAL | MTF_HARD)) && (!canmodify || !r_fixmaperrors) && type != VisualModeCamera)
    {
        if (mobjinfo[i].name1[0] != '\0')
            C_Warning("The %s at (%i,%i) didn't spawn because it has no skill flags.", mobjinfo[i].name1, mthing->x, mthing->y);
        else
            C_Warning("Thing %s at (%i,%i) didn't spawn because it has no skill flags.", commify(index), mthing->x, mthing->y);
    }

    if (!(mthing->options & bit))
        return NULL;

    if (type >= 14100 && type <= 14164)
    {
        // Use the ambient number
        id = type - 14100;              // Mus change
        type = MusicSource;             // MT_MUSICSOURCE
    }

    // find which type to spawn

    if (mobjinfo[i].flags & MF_COUNTKILL)
    {
        // don't spawn any monsters if -nomonsters
        if (nomonsters && (i != MT_KEEN || gamemission == heretic))
            return NULL;

        totalkills++;
        monstercount[i]++;
    }
    else if (i == MT_BARREL && gamemission != heretic)
        barrelcount++;

    // [BH] don't spawn any monster corpses if -nomonsters
    if ((mobjinfo[i].flags & MF_CORPSE) && nomonsters && (i != MT_MISC62 || gamemission == heretic))
        return NULL;

    // spawn it
    x = mthing->x << FRACBITS;
    y = mthing->y << FRACBITS;
    z = ((mobjinfo[i].flags & MF_SPAWNCEILING) ? ONCEILINGZ : ((mobjinfo[i].flags3 & MF3_SPAWNFLOAT) ? FLOATRANDZ : ONFLOORZ));

    mobj = P_SpawnMobj(x, y, z, (mobjtype_t)i);
    mobj->spawnpoint = *mthing;
    mobj->id = id;

    if (mthing->options & MTF_AMBUSH)
        mobj->flags |= MF_AMBUSH;

    flags = mobj->flags;

    if (mobj->tics > 0)
        mobj->tics = 1 + (M_Random() % mobj->tics);

    if (flags & MF_COUNTITEM)
        totalitems++;

    if (flags & MF_SPECIAL)
        totalpickups++;

    mobj->angle = ((mthing->angle % 45) ? mthing->angle * (ANG45 / 45) : ANG45 * (mthing->angle / 45));

    // [BH] randomly mirror corpses
    if ((flags & MF_CORPSE) && r_corpses_mirrored)
    {
        static int  prev;
        int         r = M_RandomInt(1, 10);

        if (r <= 5 + prev)
        {
            prev--;
            mobj->flags2 |= MF2_MIRRORED;
        }
        else
            prev++;
    }

    // [BH] randomly mirror weapons
    if (r_mirroredweapons && (type == SuperShotgun || (type >= Shotgun && type <= BFG9000)) && (M_Random() & 1))
        mobj->flags2 |= MF2_MIRRORED;

    // [BH] spawn blood splats around corpses
    if (!(flags & (MF_SHOOTABLE | MF_NOBLOOD | MF_SPECIAL)) && mobj->blood && !chex
        && (!hacx || !(mobj->flags2 & MF2_DECORATION)) && r_bloodsplats_max
        && (BTSX || lumpinfo[firstspritelump + sprites[mobj->sprite].spriteframes[0].lump[0]]->wadfile->type != PWAD))
    {
        mobj->bloodsplats = CORPSEBLOODSPLATS;

        if (r_corpses_moreblood && !mobj->subsector->sector->isliquid)
            P_SpawnMoreBlood(mobj);
    }

    // [crispy] randomly colorize space marine corpse objects
    if (mobj->info->spawnstate == S_PLAY_DIE7 || mobj->info->spawnstate == S_PLAY_XDIE9)
        mobj->flags |= (M_RandomInt(0, 3) << MF_TRANSSHIFT);

    if (mobj->flags2 & MF2_DECORATION)
        numdecorations++;

    return mobj;
}

//
// GAME SPAWN FUNCTIONS
//

//
// P_SpawnPuff
//
extern fixed_t  attackrange;

void P_SpawnPuff(fixed_t x, fixed_t y, fixed_t z, angle_t angle)
{
    mobj_t      *th = Z_Calloc(1, sizeof(*th), PU_LEVEL, NULL);
    mobjinfo_t  *info = &mobjinfo[pufftype];
    state_t     *st = &states[info->spawnstate];
    sector_t    *sector;

    th->type = pufftype;
    th->info = info;
    th->x = x;
    th->y = y;
    th->momz = FRACUNIT;
    th->angle = angle;
    th->flags = info->flags;
    th->flags2 = (info->flags2 | ((M_Random() & 1) * MF2_MIRRORED));

    th->state = st;
    th->tics = MAX(1, st->tics - (M_Random() & 3));
    th->sprite = st->sprite;
    th->frame = st->frame;
    th->interpolate = true;

    th->colfunc = info->colfunc;
    th->altcolfunc = info->altcolfunc;

    P_SetThingPosition(th);

    if (th->info->attacksound)
        S_StartSound(th, th->info->attacksound);

    switch (pufftype)
    {
        case HMT_BEAKPUFF:
        case HMT_STAFFPUFF:
            th->momz = FRACUNIT;
            break;

        case HMT_GAUNTLETPUFF1:
        case HMT_GAUNTLETPUFF2:
            th->momz = (fixed_t)(0.8 * FRACUNIT);

        default:
            break;
    }

    sector = th->subsector->sector;
    th->floorz = sector->interpfloorheight;
    th->ceilingz = sector->interpceilingheight;

    th->z = z + (M_NegRandom() << 10);

    th->thinker.function = P_MobjThinker;
    P_AddThinker(&th->thinker);

    // don't make punches spark on the wall
    if (attackrange == MELEERANGE && gamemission != heretic)
    {
        P_SetMobjState(th, S_PUFF3);

        // [BH] vibrate XInput gamepads
        if (gp_vibrate_damage && vibrate)
        {
            int motorspeed = weaponinfo[wp_fist].motorspeed * gp_vibrate_damage / 100;

            if (viewplayer->powers[pw_strength])
                motorspeed *= 2;

            XInputVibration(motorspeed);
            weaponvibrationtics = weaponinfo[wp_fist].tics;
        }
    }
}

//
// P_SpawnSmokeTrail
//
void P_SpawnSmokeTrail(fixed_t x, fixed_t y, fixed_t z, angle_t angle)
{
    mobj_t  *th = P_SpawnMobj(x, y, z + (M_NegRandom() << 10), MT_TRAIL);

    th->momz = FRACUNIT / 2;
    th->tics -= M_Random() & 3;

    th->angle = angle;

    th->flags2 |= (M_Random() & 1) * MF2_MIRRORED;
}

//
// P_SpawnBlood
// [BH] spawn much more blood than Vanilla DOOM
//
void P_SpawnBlood(fixed_t x, fixed_t y, fixed_t z, angle_t angle, int damage, mobj_t *target)
{
    int         minz = target->z;
    int         maxz = minz + spriteheight[sprites[target->sprite].spriteframes[0].lump[0]];
    dboolean    fuzz = (target->flags & MF_FUZZ);
    int         type = (r_blood == r_blood_all ? (fuzz ? MT_FUZZYBLOOD : (target->blood ? target->blood :
                    MT_BLOOD)) : MT_BLOOD);
    mobjinfo_t  *info = &mobjinfo[type];
    int         blood = (fuzz ? FUZZYBLOOD : info->blood);
    sector_t    *sector;

    angle += ANG180;

    for (int i = (damage >> 2) + 1; i > 0; i--)
    {
        mobj_t  *th = Z_Calloc(1, sizeof(*th), PU_LEVEL, NULL);
        state_t *st = &states[info->spawnstate];

        th->type = type;
        th->info = info;
        th->x = x;
        th->y = y;
        th->flags = info->flags;
        th->flags2 = (info->flags2 | ((M_Random() & 1) * MF2_MIRRORED));

        th->state = st;
        th->tics = MAX(1, st->tics - (M_Random() & 3));
        th->sprite = st->sprite;
        th->frame = st->frame;
        th->interpolate = true;

        th->colfunc = info->colfunc;
        th->altcolfunc = info->altcolfunc;
        th->blood = blood;

        P_SetThingPosition(th);

        sector = th->subsector->sector;
        th->floorz = sector->interpfloorheight;
        th->ceilingz = sector->interpceilingheight;

        th->z = BETWEEN(minz, z + (M_NegRandom() << 10), maxz);

        th->thinker.function = P_MobjThinker;
        P_AddThinker(&th->thinker);

        th->momx = FixedMul(i * FRACUNIT / 4, finecosine[angle >> ANGLETOFINESHIFT]);
        th->momy = FixedMul(i * FRACUNIT / 4, finesine[angle >> ANGLETOFINESHIFT]);
        th->momz = FRACUNIT * (2 + i / 6);

        th->angle = angle;
        angle += M_NegRandom() * 0xB60B60;

        if (damage <= 12 && th->state->nextstate)
            P_SetMobjState(th, th->state->nextstate);

        if (damage < 9 && th->state->nextstate)
            P_SetMobjState(th, th->state->nextstate);
    }
}

//
// P_SpawnBloodSplat
//
extern short    firstbloodsplatlump;

void P_SpawnBloodSplat(fixed_t x, fixed_t y, int blood, int maxheight, mobj_t *target)
{
    if (r_bloodsplats_total >= r_bloodsplats_max)
        return;
    else
    {
        sector_t    *sec = R_PointInSubsector(x, y)->sector;

        if (!sec->isliquid && sec->interpfloorheight <= maxheight && sec->floorpic != skyflatnum)
        {
            bloodsplat_t    *splat = malloc(sizeof(*splat));
            int             patch = firstbloodsplatlump + (M_Random() & 7);

            splat->patch = patch;
            splat->flip = M_Random() & 1;
            splat->colfunc = (blood == FUZZYBLOOD ? fuzzcolfunc : bloodsplatcolfunc);
            splat->blood = blood;
            splat->x = x;
            splat->y = y;
            splat->width = spritewidth[patch];
            splat->sector = sec;
            P_SetBloodSplatPosition(splat);
            r_bloodsplats_total++;

            if (target && target->bloodsplats)
                target->bloodsplats--;
        }
    }
}

void P_BloodSplatter(fixed_t x, fixed_t y, fixed_t z, mobj_t * originator)
{
    mobj_t  *mo = P_SpawnMobj(x, y, z, HMT_BLOODSPLATTER);

    mo->target = originator;
    mo->momx = M_NegRandom() << 9;
    mo->momy = M_NegRandom() << 9;
    mo->momz = FRACUNIT * 2;
}

void P_RipperBlood(mobj_t *mo)
{
    mobj_t  *th;
    fixed_t x = mo->x + (M_NegRandom() << 12);
    fixed_t y = mo->y + (M_NegRandom() << 12);
    fixed_t z = mo->z + (M_NegRandom() << 12);

    th = P_SpawnMobj(x, y, z, HMT_BLOOD);
    th->flags |= MF_NOGRAVITY;
    th->momx = mo->momx >> 1;
    th->momy = mo->momy >> 1;
    th->tics += M_Random() & 3;
}

int P_GetThingFloorType(mobj_t *thing)
{
    return terraintypes[thing->subsector->sector->floorpic];
}

int P_HitFloor(mobj_t *thing)
{
    mobj_t  *mo;

    if (thing->floorz != thing->subsector->sector->floorheight)
    {
        // don't splash if landing on the edge above water/lava/etc....
        return FLOOR_SOLID;
    }

    switch (P_GetThingFloorType(thing))
    {
        case FLOOR_WATER:
            P_SpawnMobj(thing->x, thing->y, ONFLOORZ, HMT_SPLASHBASE);
            mo = P_SpawnMobj(thing->x, thing->y, ONFLOORZ, HMT_SPLASH);
            mo->target = thing;
            mo->momx = M_NegRandom() << 8;
            mo->momy = M_NegRandom() << 8;
            mo->momz = 2 * FRACUNIT + (M_Random() << 8);
            S_StartSound(mo, hsfx_gloop);
            return FLOOR_WATER;

        case FLOOR_LAVA:
            P_SpawnMobj(thing->x, thing->y, ONFLOORZ, HMT_LAVASPLASH);
            mo = P_SpawnMobj(thing->x, thing->y, ONFLOORZ, HMT_LAVASMOKE);
            mo->momz = FRACUNIT + (M_Random() << 7);
            S_StartSound(mo, hsfx_burn);
            return FLOOR_LAVA;

        case FLOOR_SLUDGE:
            P_SpawnMobj(thing->x, thing->y, ONFLOORZ, HMT_SLUDGESPLASH);
            mo = P_SpawnMobj(thing->x, thing->y, ONFLOORZ, HMT_SLUDGECHUNK);
            mo->target = thing;
            mo->momx = M_NegRandom() << 8;
            mo->momy = M_NegRandom() << 8;
            mo->momz = FRACUNIT + (M_Random() << 8);
            return FLOOR_SLUDGE;
    }

    return FLOOR_SOLID;
}

//
// P_CheckMissileSpawn
// Moves the missile forward a bit
//  and possibly explodes it right there.
//
dboolean P_CheckMissileSpawn(mobj_t *th)
{
    if (gamemission != heretic)
        th->tics = MAX(1, th->tics - (M_Random() & 3));

    // move a little forward so an angle can
    // be computed if it immediately explodes
    th->x += (th->momx >> 1);
    th->y += (th->momy >> 1);
    th->z += (th->momz >> 1);

    if (!P_TryMove(th, th->x, th->y, false))
    {
        P_ExplodeMissile(th);
        return false;
    }

    return true;
}

//
// P_SpawnMissile
//
mobj_t *P_SpawnMissile(mobj_t *source, mobj_t *dest, mobjtype_t type)
{
    fixed_t z = source->z + 32 * FRACUNIT;
    mobj_t  *th;
    angle_t an;
    int     dist;
    int     speed;

    if (gamemission == heretic)
        switch (type)
        {
            case HMT_MNTRFX1:       // Minotaur swing attack missile
                z = source->z + 40 * FRACUNIT;
                break;

            case HMT_MNTRFX2:       // Minotaur floor fire missile
                z = ONFLOORZ;
                break;

            case HMT_SRCRFX1:       // Sorcerer Demon fireball
                z = source->z + 48 * FRACUNIT;
                break;

            case HMT_KNIGHTAXE:     // Knight normal axe
            case HMT_REDAXE:        // Knight red power axe
                z = source->z + 36 * FRACUNIT;
                break;

            default:
                z = source->z + 32 * FRACUNIT;
                break;
        }

    if ((source->flags2 & MF2_FEETARECLIPPED) && !source->subsector->sector->heightsec && r_liquid_clipsprites)
        z -= FOOTCLIPSIZE;

    th = P_SpawnMobj(source->x, source->y, z, type);

    if (th->info->seesound)
        S_StartSound(th, th->info->seesound);

    P_SetTarget(&th->target, source);   // where it came from
    an = R_PointToAngle2(source->x, source->y, dest->x, dest->y);

    // fuzzy player
    if (dest->flags & MF_FUZZ)
        an += M_NegRandom() << (gamemission == heretic ? 21 : 20);

    th->angle = an;
    an >>= ANGLETOFINESHIFT;
    speed = th->info->speed;
    th->momx = FixedMul(speed, finecosine[an]);
    th->momy = FixedMul(speed, finesine[an]);
    dist = MAX(1, P_ApproxDistance(dest->x - source->x, dest->y - source->y) / speed);
    th->momz = (dest->z - source->z) / dist;
    th->flags2 |= MF2_MONSTERMISSILE;

    return (P_CheckMissileSpawn(th) ? th : NULL);
}

mobj_t *P_SpawnMissileAngle(mobj_t *source, mobjtype_t type, angle_t angle, fixed_t momz)
{
    fixed_t z;
    mobj_t  *mo;

    switch (type)
    {
        case HMT_MNTRFX1:       // Minotaur swing attack missile
            z = source->z + 40 * FRACUNIT;
            break;

        case HMT_MNTRFX2:       // Minotaur floor fire missile
            z = ONFLOORZ;
            break;

        case HMT_SRCRFX1:       // Sorcerer Demon fireball
            z = source->z + 48 * FRACUNIT;
            break;

        default:
            z = source->z + 32 * FRACUNIT;
            break;
    }

    if (source->flags2 & MF2_FEETARECLIPPED)
        z -= FOOTCLIPSIZE;

    mo = P_SpawnMobj(source->x, source->y, z, type);

    if (mo->info->seesound)
        S_StartSound(mo, mo->info->seesound);

    mo->target = source;        // Originator
    mo->angle = angle;
    angle >>= ANGLETOFINESHIFT;
    mo->momx = FixedMul(mo->info->speed, finecosine[angle]);
    mo->momy = FixedMul(mo->info->speed, finesine[angle]);
    mo->momz = momz;
    return (P_CheckMissileSpawn(mo) ? mo : NULL);
}

//
// P_SpawnPlayerMissile
// Tries to aim at a nearby monster.
//
mobj_t *P_SpawnPlayerMissile(mobj_t *source, mobjtype_t type)
{
    mobj_t  *th;
    angle_t an = source->angle;
    fixed_t x, y, z;
    fixed_t slope;

    if (usemouselook && !autoaim)
        slope = ((source->player->lookdir / MLOOKUNIT) << FRACBITS) / 173;
    else
    {
        // see which target is to be aimed at
        slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);

        if (!linetarget)
        {
            slope = P_AimLineAttack(source, (an += 1 << 26), 16 * 64 * FRACUNIT);

            if (!linetarget)
                slope = P_AimLineAttack(source, (an -= 2 << 26), 16 * 64 * FRACUNIT);

            if (!linetarget)
            {
                an = source->angle;
                slope = (usemouselook ? ((source->player->lookdir / MLOOKUNIT) << FRACBITS) / 173 : 0);
            }
        }
    }

    x = source->x;
    y = source->y;
    z = source->z + 4 * 8 * FRACUNIT;

    if ((source->flags2 & MF2_FEETARECLIPPED) && !source->subsector->sector->heightsec && r_liquid_lowerview)
        z -= FOOTCLIPSIZE;

    th = P_SpawnMobj(x, y, z, type);

    if (th->info->seesound)
        S_StartSound(th, th->info->seesound);

    P_SetTarget(&th->target, source);
    th->angle = an;
    an >>= ANGLETOFINESHIFT;
    th->momx = FixedMul(th->info->speed, finecosine[an]);
    th->momy = FixedMul(th->info->speed, finesine[an]);
    th->momz = FixedMul(th->info->speed, slope);
    th->interpolate = -1;

    if (gamemission == heretic)
    {
        if (th->type == HMT_BLASTERFX1)
        {
            // Ultra-fast ripper spawning missile
            th->x += (th->momx >> 3);
            th->y += (th->momy >> 3);
            th->z += (th->momz >> 3);
        }
        else
        {
            // Normal missile
            th->x += (th->momx >> 1);
            th->y += (th->momy >> 1);
            th->z += (th->momz >> 1);
        }

        if (!P_TryMove(th, th->x, th->y, false))
        {
            // Exploded immediately
            P_ExplodeMissile(th);
            return NULL;
        }
    }
    else
    {
        P_NoiseAlert(source);

        if (type == MT_ROCKET && r_rockettrails && !hacx)
        {
            th->flags2 |= MF2_SMOKETRAIL;
            puffcount = 0;
            th->nudge = 1;
        }

        P_CheckMissileSpawn(th);

        A_Recoil(source->player->readyweapon);
    }

    return th;
}

mobj_t *P_SPMAngle(mobj_t * source, mobjtype_t type, angle_t angle)
{
    mobj_t  *th;
    angle_t an = angle;
    fixed_t x, y, z;
    fixed_t slope;

    if (usemouselook && !autoaim)
        slope = ((source->player->lookdir / MLOOKUNIT) << FRACBITS) / 173;
    else
    {
        // see which target is to be aimed at
        slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);

        if (!linetarget)
        {
            slope = P_AimLineAttack(source, (an += 1 << 26), 16 * 64 * FRACUNIT);

            if (!linetarget)
                slope = P_AimLineAttack(source, (an -= 2 << 26), 16 * 64 * FRACUNIT);

            if (!linetarget)
            {
                an = angle;
                slope = (usemouselook ? ((source->player->lookdir / MLOOKUNIT) << FRACBITS) / 173 : 0);
            }
        }
    }

    x = source->x;
    y = source->y;
    z = source->z + 4 * 8 * FRACUNIT;

    if ((source->flags2 & MF2_FEETARECLIPPED) && !source->subsector->sector->heightsec && r_liquid_lowerview)
        z -= FOOTCLIPSIZE;

    th = P_SpawnMobj(x, y, z, type);

    if (th->info->seesound)
        S_StartSound(th, th->info->seesound);

    P_SetTarget(&th->target, source);
    th->angle = an;
    an >>= ANGLETOFINESHIFT;
    th->momx = FixedMul(th->info->speed, finecosine[an]);
    th->momy = FixedMul(th->info->speed, finesine[an]);
    th->momz = FixedMul(th->info->speed, slope);
    th->interpolate = -1;
    return (P_CheckMissileSpawn(th) ? th : NULL);
}

void P_InitExtraMobjs(void)
{
    for (int i = MT_EXTRA00; i <= MT_EXTRA99; i++)
    {
        memset(&mobjinfo[i], 0, sizeof(mobjinfo_t));
        mobjinfo[i].doomednum = -1;
    }
}

void P_InitHereticMobjs(void)
{
    for (int i = 0; i < NUMHSTATES; i++)
        memcpy(&states[i], &hereticstates[i], sizeof(state_t));

    for (int i = 0; i < NUMHMOBJTYPES; i++)
        memcpy(&mobjinfo[i], &hereticmobjinfo[i], sizeof(mobjinfo_t));

    playermobjtype = HMT_PLAYER;
}

void A_ContMobjSound(mobj_t *actor, player_t *player, pspdef_t *psp)
{
    switch (actor->type)
    {
        case HMT_KNIGHTAXE:
            S_StartSound(actor, hsfx_kgtatk);
            break;

        case HMT_MUMMYFX1:
            S_StartSound(actor, hsfx_mumhed);
            break;

        default:
            break;
    }
}
