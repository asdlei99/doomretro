/*
========================================================================

                           D O O M  R e t r o
         The classic, refined DOOM source port. For Windows PC.

========================================================================

  Copyright © 1993-2012 by id Software LLC, a ZeniMax Media company.
  Copyright © 2013-2019 by Brad Harding.

  DOOM Retro is a fork of Chocolate DOOM. For a list of credits, see
  <https://github.com/bradharding/doomretro/wiki/CREDITS>.

  This file is a part of DOOM Retro.

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
  company, in the US and/or other countries, and is used without
  permission. All other trademarks are the property of their respective
  holders. DOOM Retro is in no way affiliated with nor endorsed by
  id Software.

========================================================================
*/

#if !defined(__I_SYSTEM_H__)
#define __I_SYSTEM_H__

#include "d_event.h"

#if defined(_WIN32)
#define OPERATINGSYSTEM "Windows"
#elif defined(__APPLE__)
#define OPERATINGSYSTEM "macOS"
#else
#define OPERATINGSYSTEM "Linux"
#endif

//
// Called by D_DoomLoop,
// called before processing each tic in a frame.
// Quick synchronous operations are performed here.
// Can call D_PostEvent.
void I_StartTic(void);

// Called by M_Responder when quit is selected.
// Clean exit, displays sell blurb.
void I_Quit(dboolean shutdown);

void I_Error(const char *error, ...);

void I_PrintWindowsVersion(void);
void I_PrintSystemInfo(void);

void *I_Realloc(void *ptr, size_t size);

#endif
