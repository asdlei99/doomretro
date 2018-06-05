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

#if !defined(__P_LOCAL_H__)
#define __P_LOCAL_H__

#include "d_main.h"
#include "r_local.h"

#define FOOTCLIPSIZE        (10 * FRACUNIT)

#define TOCENTER            -8
#define FLOATSPEED          (4 * FRACUNIT)

#define VIEWHEIGHT          (41 * FRACUNIT)
#define DEADVIEWHEIGHT      (6 * FRACUNIT)
#define JUMPHEIGHT          (9 * FRACUNIT)

#define DEADLOOKDIR         128

// mapblocks are used to check movement
// against lines and things
#define MAPBLOCKUNITS       128
#define MAPBLOCKSIZE        (MAPBLOCKUNITS * FRACUNIT)
#define MAPBLOCKSHIFT       (FRACBITS + 7)
#define MAPBTOFRAC          (MAPBLOCKSHIFT - FRACBITS)

// MAXRADIUS is for precalculated sector block boxes
// the spider demon is larger,
// but we do not have any moving sectors nearby
#define MAXRADIUS           (32 * FRACUNIT)

#define GRAVITY             FRACUNIT
#define MAXMOVE             (30 * FRACUNIT)
#define MAXMOVE_STEP        (8 * FRACUNIT)

#define USERANGE            (64 * FRACUNIT)
#define MELEERANGE          (64 * FRACUNIT)
#define MISSILERANGE        (32 * 64 * FRACUNIT)

// follow a player exclusively for 3 seconds
#define BASETHRESHOLD       100

#define BONUSADD            6

#define MOUSE_LEFTBUTTON    1
#define MOUSE_RIGHTBUTTON   2

#define MOUSE_WHEELUP       MAX_MOUSE_BUTTONS
#define MOUSE_WHEELDOWN     (MAX_MOUSE_BUTTONS + 1)

#define NEEDEDCARDFLASH     8

#define WEAPONBOTTOM        (128 * FRACUNIT)
#define WEAPONTOP           (32 * FRACUNIT)

//
// P_PSPR
//

#define USE_GWND_AMMO_1     1
#define USE_GWND_AMMO_2     1
#define USE_CBOW_AMMO_1     1
#define USE_CBOW_AMMO_2     1
#define USE_BLSR_AMMO_1     1
#define USE_BLSR_AMMO_2     5
#define USE_SKRD_AMMO_1     1
#define USE_SKRD_AMMO_2     5
#define USE_PHRD_AMMO_1     1
#define USE_PHRD_AMMO_2     1
#define USE_MACE_AMMO_1     1
#define USE_MACE_AMMO_2     5

void P_OpenWeapons(void);
void P_CloseWeapons(void);
void P_SetupPsprites(void);
void P_MovePsprites(void);
void P_FireWeapon(void);
void P_DropWeapon(void);
void P_RepositionMace(mobj_t *mo);
void P_SetPsprite(size_t position, statenum_t stnum);
void P_PostChickenWeapon(weapontype_t weapon);
void P_UpdateBeak(mobj_t *actor, player_t *player, pspdef_t *psp);
void P_ActivateBeak(void);
void P_AddMaceSpot(mapthing_t *mthing);

//
// P_USER
//
// 16 pixels of bob
#define MAXBOB              0x100000
#define MLOOKUNIT           8

void P_CalcHeight(void);
void P_MovePlayer(void);
void P_PlayerThink(void);
void P_ResurrectPlayer(int health);
void P_ChangeWeapon(weapontype_t newweapon);
void P_PlayerRemoveArtifact(int slot);
void P_PlayerUseArtifact(artitype_t arti);
void P_PlayerNextArtifact(void);
dboolean P_UseArtifact(artitype_t arti);

//
// P_MOBJ
//
#define FLOOR_SOLID         0
#define FLOOR_WATER         1
#define FLOOR_LAVA          2
#define FLOOR_SLUDGE        3

#define ONFLOORZ            INT_MIN
#define ONCEILINGZ          INT_MAX
#define FLOATRANDZ          (INT_MAX - 1)

// Time interval for item respawning.
#define ITEMQUEUESIZE       512

#define CARDNOTFOUNDYET     -1
#define CARDNOTINMAP        0

extern mapthing_t   itemrespawnque[ITEMQUEUESIZE];
extern int          itemrespawntime[ITEMQUEUESIZE];
extern int          iquehead;
extern int          iquetail;
extern mobjtype_t   pufftype;
extern mobj_t       *missilemobj;
extern mapthing_t   playerstart;

void P_RespawnSpecials(void);

void P_InitCards(void);

mobj_t *P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type);
void P_SetShadowColumnFunction(mobj_t *mobj);
mobjtype_t P_FindDoomedNum(unsigned int type);

void P_RemoveMobj(mobj_t *mobj);
dboolean P_SetMobjState(mobj_t *mobj, statenum_t state);
dboolean P_SetMobjStateNF(mobj_t *mobj, statenum_t state);
void P_MobjThinker(mobj_t *mobj);
void P_BlasterMobjThinker(mobj_t *mobj);

int P_HitFloor(mobj_t *thing);
int P_GetThingFloorType(mobj_t *thing);

void P_SpawnPuff(fixed_t x, fixed_t y, fixed_t z, angle_t angle);
void P_SpawnSmokeTrail(fixed_t x, fixed_t y, fixed_t z, angle_t angle);
void P_SpawnBlood(fixed_t x, fixed_t y, fixed_t z, angle_t angle, int damage, mobj_t *target);
void P_SpawnBloodSplat(fixed_t x, fixed_t y, int blood, int maxheight, mobj_t *target);
void P_BloodSplatter(fixed_t x, fixed_t y, fixed_t z, mobj_t *originator);
void P_RipperBlood(mobj_t *mo);
dboolean P_CheckMissileSpawn(mobj_t *th);
mobj_t *P_SpawnMissile(mobj_t *source, mobj_t *dest, mobjtype_t type);
mobj_t *P_SpawnMissileAngle(mobj_t *source, mobjtype_t type, angle_t angle, fixed_t momz);
mobj_t *P_SpawnPlayerMissile(mobj_t *source, mobjtype_t type);
mobj_t *P_SPMAngle(mobj_t * source, mobjtype_t type, angle_t angle);
dboolean P_SeekerMissile(mobj_t *actor, angle_t thresh, angle_t turnmax);
void P_ThrustMobj(mobj_t *mo, angle_t angle, fixed_t move);
int P_FaceMobj(mobj_t *source, mobj_t *target, angle_t *delta);
void P_ExplodeMissile(mobj_t *mo);
void P_InitExtraMobjs(void);
void P_InitHereticMobjs(void);

//
// P_ENEMY
//
#define BARRELMS    1500

void P_AddBossSpot(fixed_t x, fixed_t y, angle_t angle);
void P_DSparilTeleport(mobj_t *actor);
void P_InitMonsters(void);
void P_NoiseAlert(mobj_t *target);

//
// P_MAPUTL
//
typedef struct
{
    fixed_t     x, y;
    fixed_t     dx, dy;
} divline_t;

typedef struct
{
    fixed_t     frac;           // along trace line
    dboolean    isaline;

    union
    {
        mobj_t  *thing;
        line_t  *line;
    } d;
} intercept_t;

typedef dboolean (*traverser_t)(intercept_t *in);

fixed_t P_ApproxDistance(fixed_t dx, fixed_t dy);
int P_PointOnLineSide(fixed_t x, fixed_t y, line_t *line);
int P_BoxOnLineSide(fixed_t *tmbox, line_t *ld);
void P_MakeDivline(line_t *li, divline_t *dl);
fixed_t P_InterceptVector(divline_t *v2, divline_t *v1);

extern fixed_t  opentop;
extern fixed_t  openbottom;
extern fixed_t  openrange;
extern fixed_t  lowfloor;

void P_LineOpening(line_t *linedef);

dboolean P_BlockLinesIterator(int x, int y, dboolean func(line_t *));
dboolean P_BlockThingsIterator(int x, int y, dboolean func(mobj_t *));

#define PT_ADDLINES     1
#define PT_ADDTHINGS    2

extern divline_t    dlTrace;

dboolean P_PathTraverse(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2, int flags, dboolean (*trav)(intercept_t *));

void P_UnsetThingPosition(mobj_t *thing);
void P_UnsetBloodSplatPosition(bloodsplat_t *splat);
void P_SetThingPosition(mobj_t *thing);
void P_SetBloodSplatPosition(bloodsplat_t *splat);

//
// P_MAP
//

// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".
extern dboolean floatok;
extern dboolean felldown;       // killough 11/98: indicates object pushed off ledge
extern fixed_t  tmfloorz;
extern fixed_t  tmceilingz;
extern fixed_t  tmbbox[4];      // phares 3/20/98

extern line_t   *ceilingline;
extern line_t   *blockline;

extern dboolean infight;

dboolean P_TestMobjLocation(mobj_t *mobj);
dboolean P_CheckPosition(mobj_t *thing, fixed_t x, fixed_t y);
mobj_t *P_CheckOnmobj(mobj_t *thing);
void P_FakeZMovement(mobj_t *mo);
dboolean P_TryMove(mobj_t *thing, fixed_t x, fixed_t y, dboolean dropoff);
dboolean P_CheckLineSide(mobj_t *actor, fixed_t x, fixed_t y);
dboolean P_TeleportMove(mobj_t *thing, fixed_t x, fixed_t y, fixed_t z, dboolean boss);
void P_SlideMove(mobj_t *mo);
dboolean P_CheckSight(mobj_t *t1, mobj_t *t2);
void P_UseLines(void);

dboolean P_ChangeSector(sector_t *sector, dboolean crunch);
void P_FreeSecNodeList(void);

extern mobj_t   *linetarget;    // who got hit (or NULL)

fixed_t P_AimLineAttack(mobj_t *t1, angle_t angle, fixed_t distance);

void P_LineAttack(mobj_t *t1, angle_t angle, fixed_t distance, fixed_t slope, int damage);

void P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage);

int P_GetMoveFactor(const mobj_t *mo, int *frictionp);      // killough 8/28/98
int P_GetFriction(const mobj_t *mo, int *frictionfactor);   // killough 8/28/98
void P_ApplyTorque(mobj_t *mo);                             // killough 9/12/98

void P_MapEnd(void);

//
// P_SETUP
//
extern const byte   *rejectmatrix;  // for fast sight rejection
extern int          *blockmaplump;
extern int          *blockmap;
extern int          bmapwidth;
extern int          bmapheight;     // in mapblocks
extern fixed_t      bmaporgx;
extern fixed_t      bmaporgy;       // origin of block map
extern mobj_t       **blocklinks;   // for thing chains

//
// P_INTER
//
#define MAXHEALTH           100
#define MAXCHICKENHEALTH    30

void P_TouchSpecialThing(mobj_t *special, mobj_t *toucher, dboolean message, dboolean stat);

void P_DamageMobj(mobj_t *target, mobj_t *inflicter, mobj_t *source, int damage, dboolean adjust);

extern int      god_health;
extern int      idfa_armor;
extern int      idfa_armor_class;
extern int      idkfa_armor;
extern int      idkfa_armor_class;
extern int      initial_health;
extern int      initial_bullets;
extern int      maxhealth;
extern int      max_armor;
extern int      green_armor_class;
extern int      blue_armor_class;
extern int      max_soul;
extern int      soul_health;
extern int      mega_health;
extern int      bfgcells;
extern dboolean species_infighting;
extern int      maxammo[];
extern int      clipammo[];

//
// P_SPEC
//
#include "p_spec.h"

#endif
