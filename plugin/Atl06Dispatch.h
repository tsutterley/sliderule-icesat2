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

#ifndef __atl06_dispatch__
#define __atl06_dispatch__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "MsgQ.h"
#include "LuaObject.h"
#include "RecordObject.h"
#include "DispatchObject.h"
#include "OsApi.h"
#include "MsgQ.h"

#include "GTArray.h"
#include "Atl03Reader.h"
#include "lua_parms.h"

/******************************************************************************
 * ATL06 DISPATCH CLASS
 ******************************************************************************/

class Atl06Dispatch: public DispatchObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const double SPEED_OF_LIGHT;
        static const double PULSE_REPITITION_FREQUENCY;
        static const double SPACECRAFT_GROUND_SPEED;
        static const double RDE_SCALE_FACTOR;
        static const double SIGMA_BEAM;
        static const double SIGMA_XMIT;

        static const int BATCH_SIZE = 256;

        static const char* elCompactRecType;
        static const RecordObject::fieldDef_t elCompactRecDef[];

        static const char* atCompactRecType;
        static const RecordObject::fieldDef_t atCompactRecDef[];

        static const char* elRecType;
        static const RecordObject::fieldDef_t elRecDef[];

        static const char* atRecType;
        static const RecordObject::fieldDef_t atRecDef[];

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Statistics --> TODO: NOT THREAD SAFE */
        typedef struct {
            uint32_t            h5atl03_rec_cnt;
            uint32_t            post_success_cnt;
            uint32_t            post_dropped_cnt;
        } stats_t;

        /* Elevation Measurement */
        typedef struct {
            double              gps_time;               // seconds from GPS epoch
            double              latitude;
            double              longitude;
            double              h_mean;                 // meters from ellipsoid
        } elevation_compact_t;

        /* ATL06 Record */
        typedef struct {
            elevation_compact_t elevation[BATCH_SIZE];
        } atl06_compact_t;

        /* Extended Elevation Measurement */
        typedef struct {
            uint32_t            segment_id;
            int32_t             photon_count;           // number of photons used in final elevation calculation
            uint16_t            rgt;                    // reference ground track
            uint16_t            cycle;                  // cycle number
            uint8_t             spot;                   // 1 through 6, or 0 if unknown
            double              gps_time;               // seconds from GPS epoch
            double              latitude;
            double              longitude;
            double              h_mean;                 // meters from ellipsoid
            double              along_track_slope;
            double              across_track_slope;
            double              window_height;
        } elevation_t;

        /* ATL06 Extended Record */
        typedef struct {
            elevation_t         elevation[BATCH_SIZE];
        } atl06_t;

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
            double      intercept;
            double      slope;
            double      x_min;
            double      x_max;
        } lsf_t;

        typedef struct {
            double      x;  // distance
            double      y;  // height
            double      r;  // residual
        } point_t;

       /* Algorithm Result */
        typedef struct {
            bool        provided;
            bool        violated_spread;
            bool        violated_count;
            bool        violated_iterations;
            elevation_t elevation;
            point_t*    photons;
        } result_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        RecordObject*       recObj;
        atl06_compact_t*    recCompactData;
        atl06_t*            recData;
        Publisher*          outQ;

        Mutex               elevationMutex;
        int                 elevationIndex;

        atl06_parms_t       parms;
        stats_t             stats;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        Atl06Dispatch                   (lua_State* L, const char* outq_name, const atl06_parms_t _parms);
                        ~Atl06Dispatch                  (void);

        bool            processRecord                   (RecordObject* record, okey_t key) override;
        bool            processTimeout                  (void) override;
        bool            processTermination              (void) override;

        void            calculateBeam                   (sc_orient_t sc_orient, track_t track, result_t* result);
        void            postResult                      (elevation_t* elevation);

        void            iterativeFitStage               (Atl03Reader::extent_t* extent, result_t* result);

        static int      luaStats                        (lua_State* L);
        static int      luaSelect                       (lua_State* L);

        static lsf_t    lsf                             (point_t* array, int size);
        static void     quicksort                       (point_t* array, int start, int end);
        static int      quicksortpartition              (point_t* array, int start, int end);

        // Unit Tests */
        friend class UT_Atl06Dispatch;
};

#endif  /* __atl06_dispatch__ */
