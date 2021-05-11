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

#ifndef __atl03_reader__
#define __atl03_reader__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <atomic>

#include "List.h"
#include "LuaObject.h"
#include "RecordObject.h"
#include "MsgQ.h"
#include "OsApi.h"

#include "GTArray.h"
#include "lua_parms.h"

/******************************************************************************
 * ATL03 READER
 ******************************************************************************/

class Atl03Reader: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Photon Fields */
        typedef struct {
            double          distance_x; // double[]: dist_ph_along
            double          height_y;   // double[]: h_ph
        } photon_t;

        /* Extent Record */
        typedef struct {
            uint8_t         reference_pair_track; // 1, 2, or 3
            uint8_t         spacecraft_orientation; // sc_orient_t
            uint16_t        reference_ground_track_start;
            uint16_t        reference_ground_track_end;
            uint16_t        cycle_start;
            uint16_t        cycle_end;
            uint32_t        segment_id[PAIR_TRACKS_PER_GROUND_TRACK]; // the id of the first ATL03 segment in range; TODO: just need one per extent
            double          segment_size[PAIR_TRACKS_PER_GROUND_TRACK]; // meters; TODO: just need one per extent
            double          background_rate[PAIR_TRACKS_PER_GROUND_TRACK]; // PE per second
            double          gps_time[PAIR_TRACKS_PER_GROUND_TRACK]; // seconds
            double          latitude[PAIR_TRACKS_PER_GROUND_TRACK];
            double          longitude[PAIR_TRACKS_PER_GROUND_TRACK];
            uint32_t        photon_count[PAIR_TRACKS_PER_GROUND_TRACK];
            uint32_t        photon_offset[PAIR_TRACKS_PER_GROUND_TRACK];
            photon_t        photons[]; // zero length field
        } extent_t;

        /* Statistics */
        typedef struct {
            uint32_t segments_read;
            uint32_t extents_filtered;
            uint32_t extents_sent;
            uint32_t extents_dropped;
            uint32_t extents_retried;
        } stats_t;

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* phRecType;
        static const RecordObject::fieldDef_t phRecDef[];

        static const char* exRecType;
        static const RecordObject::fieldDef_t exRecDef[];

        static const char* OBJECT_TYPE;

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);
        static void init        (void);

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            Atl03Reader*    reader;
            const char*     url;
            int             track;
        } info_t;

        /* Region Subclass */
        class Region
        {
            public:

                Region  (info_t* info, H5Api::context_t* context);
                ~Region (void);
            
                GTArray<double>     segment_lat;
                GTArray<double>     segment_lon;
                GTArray<int32_t>    segment_ph_cnt;
                
                long                first_segment[PAIR_TRACKS_PER_GROUND_TRACK];
                long                num_segments[PAIR_TRACKS_PER_GROUND_TRACK];
                long                first_photon[PAIR_TRACKS_PER_GROUND_TRACK];
                long                num_photons[PAIR_TRACKS_PER_GROUND_TRACK];
        };

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const double ATL03_SEGMENT_LENGTH;
        static const double MAX_ATL06_SEGMENT_LENGTH;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool                active;
        Thread*             readerPid[NUM_TRACKS];
        Mutex               threadMut;
        int                 threadCount;
        int                 numComplete;
        Publisher*          outQ;
        atl06_parms_t*      parms;
        stats_t             stats;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            Atl03Reader         (lua_State* L, const char* url, const char* outq_name, atl06_parms_t* _parms, int track=ALL_TRACKS);
                            ~Atl03Reader        (void);

        static void*        atl06Thread         (void* parm);
        static int          luaParms            (lua_State* L);
        static int          luaStats            (lua_State* L);
};

#endif  /* __atl03_reader__ */
