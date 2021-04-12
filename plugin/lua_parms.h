/*
 * Copyright (c) 2021, University of Washington
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, 
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, 
 *    this list of conditions and the following disclaimer in the documentation 
 *    and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the University of Washington nor the names of its 
 *    contributors may be used to endorse or promote products derived from this 
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF WASHINGTON AND CONTRIBUTORS
 * “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED 
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF WASHINGTON OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __lua_parms__
#define __lua_parms__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <lua.h>
#include "List.h"
#include "MathLib.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_PARM_SURFACE_TYPE                   "srt"
#define LUA_PARM_SIGNAL_CONFIDENCE              "cnf"
#define LUA_PARM_POLYGON                        "poly"
#define LUA_PARM_STAGES                         "stages"
#define LUA_PARM_COMPACT                        "compact"
#define LUA_PARM_LATITUDE                       "lat"
#define LUA_PARM_LONGITUDE                      "lon"
#define LUA_PARM_ALONG_TRACK_SPREAD             "ats"
#define LUA_PARM_MIN_PHOTON_COUNT               "cnt"
#define LUA_PARM_EXTENT_LENGTH                  "len"
#define LUA_PARM_EXTENT_STEP                    "res"
#define LUA_PARM_MAX_ITERATIONS                 "maxi"
#define LUA_PARM_MIN_WINDOW                     "H_min_win"
#define LUA_PARM_MAX_ROBUST_DISPERSION          "sigma_r_max"
#define LUA_PARM_STAGE_LSF                      "LSF"
#define LUA_PARM_STAGE_ERR                      "ERR"
#define LUA_PARM_MAX_COORDS                     32

/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

/* Tracks */
typedef enum {
    ALL_TRACKS = 0,
    RPT_1 = 1,
    RPT_2 = 2,
    RPT_3 = 3,
    NUM_TRACKS = 3
} track_t;

/* Spots */
typedef enum {
    SPOT_1 = 1,
    SPOT_2 = 2,
    SPOT_3 = 3,
    SPOT_4 = 4,
    SPOT_5 = 5,
    SPOT_6 = 6,
    NUM_SPOTS = 6
} spot_t;

/* Spacecraft Orientation */
typedef enum {
    SC_BACKWARD = 0,
    SC_FORWARD = 1,
    SC_TRANSITION = 2
} sc_orient_t;

/* Signal Confidence per Photon */
typedef enum {
    CNF_POSSIBLE_TEP = -2,
    CNF_NOT_CONSIDERED = -1,
    CNF_BACKGROUND = 0,
    CNF_WITHIN_10M = 1,
    CNF_SURFACE_LOW = 2,
    CNF_SURFACE_MEDIUM = 3,
    CNF_SURFACE_HIGH = 4
} signal_conf_t;

/* Surface Types for Signal Confidence */
typedef enum {
    SRT_LAND = 0,
    SRT_OCEAN = 1,
    SRT_SEA_ICE = 2,
    SRT_LAND_ICE = 3,
    SRT_INLAND_WATER = 4
} surface_type_t;

/* Algorithm Stages */
typedef enum {
    STAGE_LSF = 0,  // least squares fit
    STAGE_ERR = 1,  // calculated errors
    NUM_STAGES = 2
} atl06_stages_t;

/* Extraction Parameters */
typedef struct {
    surface_type_t          surface_type;                   // surface reference type (used to select signal confidence column)
    signal_conf_t           signal_confidence;              // minimal allowed signal confidence
    bool                    stages[NUM_STAGES];             // algorithm iterations
    bool                    compact;                        // return compact (only lat,lon,height,time) elevation information
    MathLib::coord_t        polygon[LUA_PARM_MAX_COORDS];   // bounding region
    int                     points_in_polygon;              // 
    int                     max_iterations;                 // least squares fit iterations
    double                  along_track_spread;             // meters
    double                  minimum_photon_count;           // PE
    double                  minimum_window;                 // H_win minimum
    double                  maximum_robust_dispersion;      // sigma_r
    double                  extent_length;                  // length of ATL06 extent (meters)
    double                  extent_step;                    // resolution of the ATL06 extent (meters)
} atl06_parms_t;

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

atl06_parms_t getLuaAtl06Parms (lua_State* L, int index);

#endif  /* __lua_parms__ */
