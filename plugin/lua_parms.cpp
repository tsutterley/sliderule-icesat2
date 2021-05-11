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

/******************************************************************************
 * INCLUDE
 ******************************************************************************/

#include "core.h"
#include "lua_parms.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define ATL06_DEFAULT_SURFACE_TYPE              SRT_LAND_ICE
#define ATL06_DEFAULT_SIGNAL_CONFIDENCE         CNF_SURFACE_HIGH
#define ATL06_DEFAULT_ALONG_TRACK_SPREAD        20.0 // meters
#define ATL06_DEFAULT_MIN_PHOTON_COUNT          10
#define ATL06_DEFAULT_EXTENT_LENGTH             40.0 // meters
#define ATL06_DEFAULT_EXTENT_STEP               20.0 // meters
#define ATL06_DEFAULT_MAX_ITERATIONS            20
#define ATL06_DEFAULT_MIN_WINDOW                3.0 // meters
#define ATL06_DEFAULT_MAX_ROBUST_DISPERSION     5.0 // meters
#define ATL06_DEFAULT_COMPACT                   false

/******************************************************************************
 * FILE DATA
 ******************************************************************************/

const atl06_parms_t DefaultParms = {
    .surface_type               = ATL06_DEFAULT_SURFACE_TYPE,
    .signal_confidence          = ATL06_DEFAULT_SIGNAL_CONFIDENCE,
    .stages                     = { true },
    .compact                    = ATL06_DEFAULT_COMPACT,
    .points_in_polygon          = 0,
    .max_iterations             = ATL06_DEFAULT_MAX_ITERATIONS,
    .along_track_spread         = ATL06_DEFAULT_ALONG_TRACK_SPREAD,
    .minimum_photon_count       = ATL06_DEFAULT_MIN_PHOTON_COUNT,
    .minimum_window             = ATL06_DEFAULT_MIN_WINDOW,
    .maximum_robust_dispersion  = ATL06_DEFAULT_MAX_ROBUST_DISPERSION,
    .extent_length              = ATL06_DEFAULT_EXTENT_LENGTH,
    .extent_step                = ATL06_DEFAULT_EXTENT_STEP
};

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

static void get_lua_polygon (lua_State* L, int index, atl06_parms_t* parms, bool* provided)
{
    /* Must be table of coordinates */
    if(lua_istable(L, index))
    {
        /* Get Number of Points in Polygon */
        int num_points = lua_rawlen(L, index);
        if(num_points > LUA_PARM_MAX_COORDS)
        {
            mlog(CRITICAL, "Points in polygon [%d] exceed maximum: %d", num_points, LUA_PARM_MAX_COORDS);
            num_points = LUA_PARM_MAX_COORDS;
        }
        parms->points_in_polygon = num_points;

        /* Iterate through each coordinate */
        for(int i = 0; i < num_points; i++)
        {
            /* Get coordinate table */
            lua_rawgeti(L, index, i+1);
            if(lua_istable(L, index))
            {
                MathLib::coord_t coord;

                /* Get latitude entry */
                lua_getfield(L, index, LUA_PARM_LATITUDE);
                coord.lat = LuaObject::getLuaFloat(L, -1);
                lua_pop(L, 1);

                /* Get longitude entry */
                lua_getfield(L, index, LUA_PARM_LONGITUDE);
                coord.lon = LuaObject::getLuaFloat(L, -1);
                lua_pop(L, 1);

                /* Add Coordinate */
                parms->polygon.add(coord);
                if(provided) *provided = true;
            }
            lua_pop(L, 1);
        }
    }
}

static void get_lua_stages (lua_State* L, int index, atl06_parms_t* parms, bool* provided)
{
    /* Must be table of stages */
    if(lua_istable(L, index))
    {
        /* Clear stages table (sets all to false) */
        LocalLib::set(parms->stages, 0, sizeof(parms->stages));

        /* Get number of stages in table */
        int num_stages = lua_rawlen(L, index);
        if(num_stages > 0 && provided) *provided = true;

        /* Iterate through each stage in table */
        for(int i = 0; i < num_stages; i++)
        {
            /* Get stage */
            lua_rawgeti(L, index, i+1);

            /* Set stage */
            if(lua_isinteger(L, -1))
            {
                int stage = LuaObject::getLuaInteger(L, -1);
                if(stage >= 0 && stage < NUM_STAGES)
                {
                    parms->stages[stage] = true;
                }
            }
            else if(lua_isstring(L, -1))
            {
                const char* stage_str = LuaObject::getLuaString(L, -1);
                if(StringLib::match(stage_str, LUA_PARM_STAGE_LSF))
                {
                    parms->stages[STAGE_LSF] = true;
                    mlog(INFO, "Enabling %s stage", LUA_PARM_STAGE_LSF);
                }
            }

            /* Clean up stack */
            lua_pop(L, 1);
        }
    }
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

atl06_parms_t* getLuaAtl06Parms (lua_State* L, int index)
{
    atl06_parms_t* parms = new atl06_parms_t; // freed in ATL03Reader and ATL06Dispatch destructor
    *parms = DefaultParms; // initialize with defaults

    if(lua_type(L, index) == LUA_TTABLE)
    {
        try
        {
            bool provided = false;

            lua_getfield(L, index, LUA_PARM_SURFACE_TYPE);
            parms->surface_type = (surface_type_t)LuaObject::getLuaInteger(L, -1, true, parms->surface_type, &provided);
            if(provided) mlog(INFO, "Setting %s to %d", LUA_PARM_SURFACE_TYPE, (int)parms->surface_type);
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_SIGNAL_CONFIDENCE);
            parms->signal_confidence = (signal_conf_t)LuaObject::getLuaInteger(L, -1, true, parms->signal_confidence, &provided);
            if(provided) mlog(INFO, "Setting %s to %d", LUA_PARM_SIGNAL_CONFIDENCE, (int)parms->signal_confidence);
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_POLYGON);
            get_lua_polygon(L, -1, parms, &provided);
            if(provided) mlog(INFO, "Setting %s to %d points", LUA_PARM_POLYGON, (int)parms->points_in_polygon);
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_STAGES);
            get_lua_stages(L, -1, parms, &provided);
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_COMPACT);
            parms->compact = LuaObject::getLuaBoolean(L, -1, true, parms->compact, &provided);
            if(provided) mlog(INFO, "Setting %s to %s", LUA_PARM_COMPACT, parms->compact ? "true" : "false");
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_MAX_ITERATIONS);
            parms->max_iterations = LuaObject::getLuaInteger(L, -1, true, parms->max_iterations, &provided);
            if(provided) mlog(INFO, "Setting %s to %d", LUA_PARM_MAX_ITERATIONS, (int)parms->max_iterations);
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_ALONG_TRACK_SPREAD);
            parms->along_track_spread = LuaObject::getLuaFloat(L, -1, true, parms->along_track_spread, &provided);
            if(provided) mlog(INFO, "Setting %s to %lf", LUA_PARM_ALONG_TRACK_SPREAD, parms->along_track_spread);
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_MIN_PHOTON_COUNT);
            parms->minimum_photon_count = LuaObject::getLuaInteger(L, -1, true, parms->minimum_photon_count, &provided);
            if(provided) mlog(INFO, "Setting %s to %lf", LUA_PARM_MIN_PHOTON_COUNT, parms->minimum_photon_count);
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_MIN_WINDOW);
            parms->minimum_window = LuaObject::getLuaFloat(L, -1, true, parms->minimum_window, &provided);
            if(provided) mlog(INFO, "Setting %s to %lf", LUA_PARM_MIN_WINDOW, parms->minimum_window);
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_MAX_ROBUST_DISPERSION);
            parms->maximum_robust_dispersion = LuaObject::getLuaFloat(L, -1, true, parms->maximum_robust_dispersion, &provided);
            if(provided) mlog(INFO, "Setting %s to %lf", LUA_PARM_MAX_ROBUST_DISPERSION, parms->maximum_robust_dispersion);
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_EXTENT_LENGTH);
            parms->extent_length = LuaObject::getLuaFloat(L, -1, true, parms->extent_length, &provided);
            if(provided) mlog(INFO, "Setting %s to %lf", LUA_PARM_EXTENT_LENGTH, parms->extent_length);
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_EXTENT_STEP);
            parms->extent_step = LuaObject::getLuaFloat(L, -1, true, parms->extent_step, &provided);
            if(provided) mlog(INFO, "Setting %s to %lf", LUA_PARM_EXTENT_STEP, parms->extent_step);
            lua_pop(L, 1);
        }
        catch(const RunTimeException& e)
        {
            delete parms; // free allocated parms since it won't be owned by anything else
            throw; // rethrow exception
        }
    }

    return parms;
}
