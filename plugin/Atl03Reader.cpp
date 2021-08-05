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
 * INCLUDES
 ******************************************************************************/

#include <math.h>
#include "core.h"
#include "icesat2.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_STAT_SEGMENTS_READ          "read"
#define LUA_STAT_EXTENTS_FILTERED       "filtered"
#define LUA_STAT_EXTENTS_SENT           "sent"
#define LUA_STAT_EXTENTS_DROPPED        "dropped"
#define LUA_STAT_EXTENTS_RETRIED        "retried"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl03Reader::phRecType = "atl03rec.photons";
const RecordObject::fieldDef_t Atl03Reader::phRecDef[] = {
    {"delta_time",  RecordObject::DOUBLE,   offsetof(photon_t, delta_time), 1,  NULL, NATIVE_FLAGS},
    {"latitude",    RecordObject::DOUBLE,   offsetof(photon_t, latitude),   1,  NULL, NATIVE_FLAGS},
    {"longitude",   RecordObject::DOUBLE,   offsetof(photon_t, longitude),  1,  NULL, NATIVE_FLAGS},
    {"distance",    RecordObject::DOUBLE,   offsetof(photon_t, distance),   1,  NULL, NATIVE_FLAGS},
    {"height",      RecordObject::FLOAT,    offsetof(photon_t, height),     1,  NULL, NATIVE_FLAGS},
    {"info",        RecordObject::UINT32,   offsetof(photon_t, info),       1,  NULL, NATIVE_FLAGS}
};

const char* Atl03Reader::exRecType = "atl03rec";
const RecordObject::fieldDef_t Atl03Reader::exRecDef[] = {
    {"track",       RecordObject::UINT8,    offsetof(extent_t, reference_pair_track),           1,  NULL, NATIVE_FLAGS},
    {"sc_orient",   RecordObject::UINT8,    offsetof(extent_t, spacecraft_orientation),         1,  NULL, NATIVE_FLAGS},
    {"rgt",         RecordObject::UINT16,   offsetof(extent_t, reference_ground_track_start),   1,  NULL, NATIVE_FLAGS},
    {"cycle",       RecordObject::UINT16,   offsetof(extent_t, cycle_start),                    1,  NULL, NATIVE_FLAGS},
    {"segment_id",  RecordObject::UINT32,   offsetof(extent_t, segment_id[0]),                  2,  NULL, NATIVE_FLAGS},
    {"extent_len",  RecordObject::DOUBLE,   offsetof(extent_t, extent_length[0]),               2,  NULL, NATIVE_FLAGS},
    {"count",       RecordObject::UINT32,   offsetof(extent_t, photon_count[0]),                2,  NULL, NATIVE_FLAGS},
    {"photons",     RecordObject::USER,     offsetof(extent_t, photon_offset[0]),               2,  phRecType, NATIVE_FLAGS | RecordObject::POINTER},
    {"data",        RecordObject::USER,     sizeof(extent_t),                                   0,  phRecType, NATIVE_FLAGS} // variable length
};

const double Atl03Reader::ATL03_SEGMENT_LENGTH = 20.0; // meters

const char* Atl03Reader::OBJECT_TYPE = "Atl03Reader";
const char* Atl03Reader::LuaMetaName = "Atl03Reader";
const struct luaL_Reg Atl03Reader::LuaMetaTable[] = {
    {"parms",       luaParms},
    {"stats",       luaStats},
    {NULL,          NULL}
};

/******************************************************************************
 * HDF5 DATASET HANDLE CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<asset>, <resource>, <outq_name>, [<parms>], [<track>])
 *----------------------------------------------------------------------------*/
int Atl03Reader::luaCreate (lua_State* L)
{
    try
    {
        /* Get URL */
        const Asset* asset = (const Asset*)getLuaObject(L, 1, Asset::OBJECT_TYPE);
        const char* resource = getLuaString(L, 2);
        const char* outq_name = getLuaString(L, 3);
        atl06_parms_t* parms = getLuaAtl06Parms(L, 4);
        int track = getLuaInteger(L, 5, true, ALL_TRACKS);

        /* Return Reader Object */
        return createLuaObject(L, new Atl03Reader(L, asset, resource, outq_name, parms, track));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating Atl03Reader: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void Atl03Reader::init (void)
{
    RecordObject::recordDefErr_t ex_rc = RecordObject::defineRecord(exRecType, "track", sizeof(extent_t), exRecDef, sizeof(exRecDef) / sizeof(RecordObject::fieldDef_t), 16);
    if(ex_rc != RecordObject::SUCCESS_DEF)
    {
        mlog(CRITICAL, "Failed to define %s: %d", exRecType, ex_rc);
    }

    RecordObject::recordDefErr_t ph_rc = RecordObject::defineRecord(phRecType, NULL, sizeof(extent_t), phRecDef, sizeof(phRecDef) / sizeof(RecordObject::fieldDef_t), 16);
    if(ph_rc != RecordObject::SUCCESS_DEF)
    {
        mlog(CRITICAL, "Failed to define %s: %d", phRecType, ph_rc);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl03Reader::Atl03Reader (lua_State* L, const Asset* asset, const char* resource, const char* outq_name, atl06_parms_t* _parms, int track):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    assert(asset);
    assert(resource);
    assert(outq_name);
    assert(_parms);

    /* Create Publisher */
    outQ = new Publisher(outq_name);

    /* Set Parameters */
    parms = _parms;

    /* Clear Statistics */
    stats.segments_read     = 0;
    stats.extents_filtered  = 0;
    stats.extents_sent      = 0;
    stats.extents_dropped   = 0;
    stats.extents_retried   = 0;

    /* Initialize Readers */
    active = true;
    numComplete = 0;
    LocalLib::set(readerPid, 0, sizeof(readerPid));

    /* Initialize Global Information to Null */
    sc_orient       = NULL;
    start_rgt       = NULL;
    start_cycle     = NULL;

    /* Read Global Resource Information */
    try
    {
        /* Read ATL03 Global Data */
        sc_orient       = new H5Array<int8_t> (asset, resource, "/orbit_info/sc_orient", &context);
        start_rgt       = new H5Array<int32_t>(asset, resource, "/ancillary_data/start_rgt", &context);
        start_cycle     = new H5Array<int32_t>(asset, resource, "/ancillary_data/start_cycle", &context);

        /* Read ATL03 Track Data */
        if(track == ALL_TRACKS)
        {
            threadCount = NUM_TRACKS;

            /* Create Readers */
            for(int t = 0; t < NUM_TRACKS; t++)
            {
                info_t* info = new info_t;
                info->reader = this;
                info->asset = asset;
                info->resource = StringLib::duplicate(resource);
                info->track = t + 1;
                readerPid[t] = new Thread(atl06Thread, info);
            }
        }
        else if(track >= 1 && track <= 3)
        {
            /* Execute Reader */
            threadCount = 1;
            info_t* info = new info_t;
            info->reader = this;
            info->asset = asset;
            info->resource = StringLib::duplicate(resource);
            info->track = track;
            atl06Thread(info);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Failed to read global information in resource %s: %s\n", resource, e.what());

        /* Indicate End of Data */
        outQ->postCopy("", 0);
        signalComplete();
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Atl03Reader::~Atl03Reader (void)
{
    active = false;

    for(int t = 0; t < NUM_TRACKS; t++)
    {
        if(readerPid[t]) delete readerPid[t];
    }

    delete outQ;
    delete parms;

    if(sc_orient)       delete sc_orient;
    if(start_rgt)       delete start_rgt;
    if(start_cycle)     delete start_cycle;
}

/*----------------------------------------------------------------------------
 * Region::Constructor
 *----------------------------------------------------------------------------*/
Atl03Reader::Region::Region (info_t* info, H5Api::context_t* context):
    segment_lat    (info->asset, info->resource, info->track, "geolocation/reference_photon_lat", context),
    segment_lon    (info->asset, info->resource, info->track, "geolocation/reference_photon_lon", context),
    segment_ph_cnt (info->asset, info->resource, info->track, "geolocation/segment_ph_cnt",       context)
{
    /* Initialize Region */
    for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
    {
        first_segment[t] = 0;
        num_segments[t] = H5Api::ALL_ROWS;
        first_photon[t] = 0;
        num_photons[t] = H5Api::ALL_ROWS;
    }

    /* Determine Spatial Extent */
    if(info->reader->parms->points_in_polygon > 0)
    {
        /* Determine Best Projection To Use */
        MathLib::proj_t projection = MathLib::PLATE_CARREE;
        if(segment_lat.gt[PRT_LEFT][0] > 60.0) projection = MathLib::NORTH_POLAR;
        else if(segment_lat.gt[PRT_LEFT][0] < -60.0) projection = MathLib::SOUTH_POLAR;

        /* Project Polygon */
        List<MathLib::coord_t>::Iterator poly_iterator(info->reader->parms->polygon);
        MathLib::point_t* projected_poly = new MathLib::point_t [info->reader->parms->points_in_polygon];
        for(int i = 0; i < info->reader->parms->points_in_polygon; i++)
        {
            projected_poly[i] = MathLib::coord2point(poly_iterator[i], projection);
        }

        /* Find First Segment In Polygon */
        bool first_segment_found[PAIR_TRACKS_PER_GROUND_TRACK] = {false, false};
        bool last_segment_found[PAIR_TRACKS_PER_GROUND_TRACK] = {false, false};
        for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
        {
            int segment = 0;
            while(segment < segment_ph_cnt.gt[t].size)
            {
                bool inclusion = false;

                /* Project Segment Coordinate */
                MathLib::coord_t segment_coord = {segment_lat.gt[t][segment], segment_lon.gt[t][segment]};
                MathLib::point_t segment_point = MathLib::coord2point(segment_coord, projection);

                /* Test Inclusion */
                if(MathLib::inpoly(projected_poly, info->reader->parms->points_in_polygon, segment_point))
                {
                    inclusion = true;
                }

                /* Check First Segment */
                if(!first_segment_found[t])
                {
                    /* If Coordinate Is In Polygon */
                    if(inclusion && segment_ph_cnt.gt[t][segment] != 0)
                    {
                        /* Set First Segment */
                        first_segment_found[t] = true;
                        first_segment[t] = segment;

                        /* Include Photons From First Segment */
                        num_photons[t] = segment_ph_cnt.gt[t][segment];
                    }
                    else
                    {
                        /* Update Photon Index */
                        first_photon[t] += segment_ph_cnt.gt[t][segment];
                    }
                }
                else if(!last_segment_found[t])
                {
                    /* If Coordinate Is NOT In Polygon */
                    if(!inclusion && segment_ph_cnt.gt[t][segment] != 0)
                    {
                        /* Set Last Segment */
                        last_segment_found[t] = true;
                        break; // full extent found!
                    }
                    else
                    {
                        /* Update Photon Index */
                        num_photons[t] += segment_ph_cnt.gt[t][segment];
                    }
                }

                /* Bump Segment */
                segment++;
            }

            /* Set Number of Segments */
            if(first_segment_found[t])
            {
                num_segments[t] = segment - first_segment[t];
            }
        }

        /* Delete Projected Polygon */
        delete [] projected_poly;

        /* Check If Anything to Process */
        if(num_photons[PRT_LEFT] < 0 || num_photons[PRT_RIGHT] < 0)
        {
            throw RunTimeException(INFO, "empty spatial region");
        }
    }

    /* Trim Geospatial Extent Datasets Read from HDF5 File */
    segment_lat.trim(first_segment);
    segment_lon.trim(first_segment);
    segment_ph_cnt.trim(first_segment);
}

/*----------------------------------------------------------------------------
 * Region::Destructor
 *----------------------------------------------------------------------------*/
Atl03Reader::Region::~Region (void)
{
}

/*----------------------------------------------------------------------------
 * atl06Thread
 *----------------------------------------------------------------------------*/
void* Atl03Reader::atl06Thread (void* parm)
{
    /* Get Thread Info */
    info_t* info = (info_t*)parm;
    Atl03Reader* reader = info->reader;
    const Asset* asset = info->asset;
    const char* resource = info->resource;
    int track = info->track;
    stats_t local_stats = {0, 0, 0, 0, 0};

    /* Start Trace */
    uint32_t trace_id = start_trace(INFO, reader->traceId, "atl03_reader", "{\"asset\":\"%s\", \"resource\":\"%s\", \"track\":%d}", info->asset->getName(), resource, track);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Api

    try
    {
        /* Subset to Region of Interest */
        Region region(info, &reader->context);

        /* Read ATL03 Data from HDF5 File */
        GTArray<float>      velocity_sc         (asset, resource, track, "geolocation/velocity_sc", &reader->context, H5Api::ALL_COLS, region.first_segment, region.num_segments);
        GTArray<double>     segment_delta_time  (asset, resource, track, "geolocation/delta_time", &reader->context, 0, region.first_segment, region.num_segments);
        GTArray<int32_t>    segment_id          (asset, resource, track, "geolocation/segment_id", &reader->context, 0, region.first_segment, region.num_segments);
        GTArray<double>     segment_dist_x      (asset, resource, track, "geolocation/segment_dist_x", &reader->context, 0, region.first_segment, region.num_segments);
        GTArray<float>      dist_ph_along       (asset, resource, track, "heights/dist_ph_along", &reader->context, 0, region.first_photon, region.num_photons);
        GTArray<float>      h_ph                (asset, resource, track, "heights/h_ph", &reader->context, 0, region.first_photon, region.num_photons);
        GTArray<int8_t>     signal_conf_ph      (asset, resource, track, "heights/signal_conf_ph", &reader->context, reader->parms->surface_type, region.first_photon, region.num_photons);
        GTArray<double>     lat_ph              (asset, resource, track, "heights/lat_ph", &reader->context, 0, region.first_photon, region.num_photons);
        GTArray<double>     lon_ph              (asset, resource, track, "heights/lon_ph", &reader->context, 0, region.first_photon, region.num_photons);
        GTArray<double>     delta_time          (asset, resource, track, "heights/delta_time", &reader->context, 0, region.first_photon, region.num_photons);
        GTArray<double>     bckgrd_delta_time   (asset, resource, track, "bckgrd_atlas/delta_time", &reader->context);
        GTArray<float>      bckgrd_rate         (asset, resource, track, "bckgrd_atlas/bckgrd_rate", &reader->context);

        /* Read ATL08 Data from HDF5 File */
        GTArray<int32_t>* atl08_ph_segment_id   = NULL;
        GTArray<int32_t>* atl08_classed_pc_indx = NULL;
        GTArray<int8_t>*  atl08_classed_pc_flag = NULL;
        if(reader->parms->use_atl08_classification)
        {
            atl08_ph_segment_id     = new GTArray<int32_t>(asset, resource, track, "signal_photons/ph_segment_id", &reader->context);
            atl08_classed_pc_indx   = new GTArray<int32_t>(asset, resource, track, "signal_photons/classed_pc_indx", &reader->context);
            atl08_classed_pc_flag   = new GTArray<int8_t>(asset, resource, track, "signal_photons/classed_pc_flag", &reader->context);
        }

        /* Early Tear Down of Context */
        mlog(INFO, "I/O context for %s: %lu reads, %lu bytes", resource, (unsigned long)reader->context.read_rqsts, (unsigned long)reader->context.bytes_read);

        /* Initialize Dataset Scope Variables */
        int32_t ph_in[PAIR_TRACKS_PER_GROUND_TRACK] = { 0, 0 }; // photon index
        int32_t seg_in[PAIR_TRACKS_PER_GROUND_TRACK] = { 0, 0 }; // segment index
        int32_t seg_ph[PAIR_TRACKS_PER_GROUND_TRACK] = { 0, 0 }; // current photon index in segment
        int32_t start_segment[PAIR_TRACKS_PER_GROUND_TRACK] = { 0, 0 };
        double  start_distance[PAIR_TRACKS_PER_GROUND_TRACK] = { segment_dist_x.gt[PRT_LEFT][0], segment_dist_x.gt[PRT_RIGHT][0] };
        double  start_seg_portion[PAIR_TRACKS_PER_GROUND_TRACK] = { 0.0, 0.0 };
        bool    track_complete[PAIR_TRACKS_PER_GROUND_TRACK] = { false, false };
        int32_t bckgrd_in[PAIR_TRACKS_PER_GROUND_TRACK] = { 0, 0 }; // bckgrd index
        int32_t atl08_in[PAIR_TRACKS_PER_GROUND_TRACK] = { 0, 0 }; // ATL08 datasets index

        /* Set Number of Photons to Process (if not already set by subsetter) */
        if(region.num_photons[PRT_LEFT] == H5Api::ALL_ROWS) region.num_photons[PRT_LEFT] = dist_ph_along.gt[PRT_LEFT].size;
        if(region.num_photons[PRT_RIGHT] == H5Api::ALL_ROWS) region.num_photons[PRT_RIGHT] = dist_ph_along.gt[PRT_RIGHT].size;

        /* Increment Read Statistics */
        local_stats.segments_read = (region.segment_ph_cnt.gt[PRT_LEFT].size + region.segment_ph_cnt.gt[PRT_RIGHT].size);

        /* Traverse All Photons In Dataset */
        while( reader->active && (!track_complete[PRT_LEFT] || !track_complete[PRT_RIGHT]) )
        {
            List<photon_t> extent_photons[PAIR_TRACKS_PER_GROUND_TRACK];
            int32_t extent_segment[PAIR_TRACKS_PER_GROUND_TRACK];
            bool extent_valid[PAIR_TRACKS_PER_GROUND_TRACK] = { true, true };

            /* Select Photons for Extent from each Track */
            for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
            {
                /* Skip Completed Tracks */
                if(track_complete[t])
                {
                    extent_valid[t] = false;
                    continue;
                }

                /* Setup Variables for Extent */
                int32_t current_photon = ph_in[t];
                int32_t current_segment = seg_in[t];
                int32_t current_count = seg_ph[t]; // number of photons in current segment already accounted for
                bool extent_complete = false;
                bool step_complete = false;

                /* Set Extent Segment */
                extent_segment[t] = seg_in[t];
                start_seg_portion[t] = dist_ph_along.gt[t][current_photon] / ATL03_SEGMENT_LENGTH;

                /* Traverse Photons Until Desired Along Track Distance Reached */
                while(!extent_complete || !step_complete)
                {
                    /* Go to Photon's Segment */
                    current_count++;
                    while((current_count > region.segment_ph_cnt.gt[t][current_segment]) &&
                          (current_segment < segment_dist_x.gt[t].size) )
                    {
                        current_count = 1; // reset photons in segment
                        current_segment++; // go to next segment
                    }

                    /* Check Current Segment */
                    if(current_segment >= segment_dist_x.gt[t].size)
                    {
                        mlog(ERROR, "Photons with no segments are detected is %s!\n", resource);
                        track_complete[t] = true;
                        break;
                    }

                    /* Update Along Track Distance */
                    double delta_distance = segment_dist_x.gt[t][current_segment] - start_distance[t];
                    double along_track_distance = delta_distance + dist_ph_along.gt[t][current_photon];

                    /* Set Next Extent's First Photon */
                    if(!step_complete && along_track_distance >= reader->parms->extent_step)
                    {
                        ph_in[t] = current_photon;
                        seg_in[t] = current_segment;
                        seg_ph[t] = current_count - 1;
                        step_complete = true;
                    }

                    /* Check if Photon within Extent's Length */
                    if(along_track_distance < reader->parms->extent_length)
                    {
                        /* Find ATL08 Classification */
                        atl08_classification_t classification = ATL08_UNCLASSIFIED;
                        bool acceptable_classification = true;
                        if(reader->parms->use_atl08_classification)
                        {
                            /* Go To Segment */
                            while(atl08_ph_segment_id->gt[t][atl08_in[t]] < segment_id.gt[t][current_segment])
                            {
                                atl08_in[t]++;
                            }

                            /* Go To Photon */
                            while( (atl08_ph_segment_id->gt[t][atl08_in[t]] == segment_id.gt[t][current_segment]) &&
                                   (atl08_classed_pc_indx->gt[t][atl08_in[t]] < current_count) )
                            {
                                atl08_in[t]++;
                            }

                            /* Check Match */
                            if( (atl08_ph_segment_id->gt[t][atl08_in[t]] == segment_id.gt[t][current_segment]) &&
                                (atl08_classed_pc_indx->gt[t][atl08_in[t]] == current_count) )
                            {
                                /* Assign Classification */
                                classification = (atl08_classification_t)atl08_classed_pc_flag->gt[t][atl08_in[t]];

                                /* Check Classification */
                                if(classification >= 0 && classification < NUM_ATL08_CLASSES)
                                {
                                    acceptable_classification = reader->parms->atl08_class[classification];
                                }
                                else
                                {
                                    throw RunTimeException(CRITICAL, "invalid atl08 classification: %d", classification);
                                }

                                /* Got To Next Photon */
                                atl08_in[t]++;
                            }
                        }

                        /* Check Photon Signal Confidence Level and Classification */
                        if(acceptable_classification && (signal_conf_ph.gt[t][current_photon] >= reader->parms->signal_confidence))
                        {
                            photon_t ph = {
                                .delta_time = delta_time.gt[t][current_photon],
                                .latitude = lat_ph.gt[t][current_photon],
                                .longitude = lon_ph.gt[t][current_photon],
                                .distance = along_track_distance - (reader->parms->extent_length / 2.0),
                                .height = h_ph.gt[t][current_photon],
                                .info = (uint32_t)classification & 0x00000007
                            };
                            extent_photons[t].add(ph);
                        }
                    }
                    else
                    {
                        extent_complete = true;
                    }

                    /* Go to Next Photon */
                    current_photon++;

                    /* Check Current Photon */
                    if(current_photon >= dist_ph_along.gt[t].size)
                    {
                        track_complete[t] = true;
                        break;
                    }
                }

                /* Add Step to Start Distance */
                start_distance[t] += reader->parms->extent_step;

                /* Apply Segment Distance Correction and Update Start Segment */
                while( ((start_segment[t] + 1) < segment_dist_x.gt[t].size) &&
                        (start_distance[t] >= segment_dist_x.gt[t][start_segment[t] + 1]) )
                {
                    start_distance[t] += segment_dist_x.gt[t][start_segment[t] + 1] - segment_dist_x.gt[t][start_segment[t]];
                    start_distance[t] -= ATL03_SEGMENT_LENGTH;
                    start_segment[t]++;
                }

                /* Check Photon Count */
                if(extent_photons[t].length() < reader->parms->minimum_photon_count)
                {
                    extent_valid[t] = false;
                }

                /* Check Along Track Spread */
                if(extent_photons[t].length() > 1)
                {
                    int32_t last = extent_photons[t].length() - 1;
                    double along_track_spread = extent_photons[t][last].distance - extent_photons[t][0].distance;
                    if(along_track_spread < reader->parms->along_track_spread)
                    {
                        extent_valid[t] = false;
                    }
                }
            }

            /* Create Extent Record */
            if(extent_valid[PRT_LEFT] || extent_valid[PRT_RIGHT])
            {
                /* Calculate Extent Record Size */
                int num_photons = extent_photons[PRT_LEFT].length() + extent_photons[PRT_RIGHT].length();
                int extent_bytes = sizeof(extent_t) + (sizeof(photon_t) * num_photons);

                /* Allocate and Initialize Extent Record */
                RecordObject* record = new RecordObject(exRecType, extent_bytes);
                extent_t* extent = (extent_t*)record->getRecordData();
                extent->reference_pair_track = track;
                extent->spacecraft_orientation = (*reader->sc_orient)[0];
                extent->reference_ground_track_start = (*reader->start_rgt)[0];
                extent->cycle_start = (*reader->start_cycle)[0];

                /* Populate Extent */
                uint32_t ph_out = 0;
                for(int t = 0; t < PAIR_TRACKS_PER_GROUND_TRACK; t++)
                {
                    /* Find Background */
                    double background_rate = bckgrd_rate.gt[t][bckgrd_rate.gt[t].size - 1];
                    while(bckgrd_in[t] < bckgrd_rate.gt[t].size)
                    {
                        double curr_bckgrd_time = bckgrd_delta_time.gt[t][bckgrd_in[t]];
                        double segment_time = segment_delta_time.gt[t][extent_segment[t]];
                        if(curr_bckgrd_time >= segment_time)
                        {
                            /* Interpolate Background Rate */
                            if(bckgrd_in[t] > 0)
                            {
                                double prev_bckgrd_time = bckgrd_delta_time.gt[t][bckgrd_in[t] - 1];
                                double prev_bckgrd_rate = bckgrd_rate.gt[t][bckgrd_in[t] - 1];
                                double curr_bckgrd_rate = bckgrd_rate.gt[t][bckgrd_in[t]];

                                double bckgrd_run = curr_bckgrd_time - prev_bckgrd_time;
                                double bckgrd_rise = curr_bckgrd_rate - prev_bckgrd_rate;
                                double segment_to_bckgrd_delta = segment_time - prev_bckgrd_time;

                                background_rate = ((bckgrd_rise / bckgrd_run) * segment_to_bckgrd_delta) + prev_bckgrd_rate;
                            }
                            else
                            {
                                /* Use First Background Rate (no interpolation) */
                                background_rate = bckgrd_rate.gt[t][0];
                            }
                            break;
                        }
                        else
                        {
                            /* Go To Next Background Rate */
                            bckgrd_in[t]++;
                        }
                    }

                    /* Calculate Spacecraft Velocity */
                    int32_t sc_v_offset = extent_segment[t] * 3;
                    double sc_v1 = velocity_sc.gt[t][sc_v_offset + 0];
                    double sc_v2 = velocity_sc.gt[t][sc_v_offset + 1];
                    double sc_v3 = velocity_sc.gt[t][sc_v_offset + 2];
                    double spacecraft_velocity = sqrt((sc_v1*sc_v1) + (sc_v2*sc_v2) + (sc_v3*sc_v3));

                    /* Calculate Segment ID (attempt to arrive at closest ATL06 segment ID represented by extent) */
                    double atl06_segment_id = (double)segment_id.gt[t][extent_segment[t]];              // start with first segment in extent
                    atl06_segment_id += start_seg_portion[t];                                           // add portion of first segment that first photon is included
                    atl06_segment_id += (reader->parms->extent_length / ATL03_SEGMENT_LENGTH) / 2.0;    // add half the left of the extent

                    /* Populate Attributes */
                    extent->valid[t]                = extent_valid[t];
                    extent->segment_id[t]           = (uint32_t)(atl06_segment_id + 0.5);
                    extent->extent_length[t]        = reader->parms->extent_length;
                    extent->spacecraft_velocity[t]  = spacecraft_velocity;
                    extent->background_rate[t]      = background_rate;
                    extent->photon_count[t]         = extent_photons[t].length();

                    /* Populate Photons */
                    if(num_photons > 0)
                    {
                        for(int32_t p = 0; p < extent_photons[t].length(); p++)
                        {
                            extent->photons[ph_out++] = extent_photons[t][p];
                        }
                    }
                }

                /* Set Photon Pointer Fields */
                extent->photon_offset[PRT_LEFT] = sizeof(extent_t); // pointers are set to offset from start of record data
                extent->photon_offset[PRT_RIGHT] = sizeof(extent_t) + (sizeof(photon_t) * extent->photon_count[PRT_LEFT]);

                /* Post Segment Record */
                uint8_t* rec_buf = NULL;
                int rec_bytes = record->serialize(&rec_buf, RecordObject::REFERENCE);
                int post_status = MsgQ::STATE_ERROR;
                while(reader->active && (post_status = reader->outQ->postCopy(rec_buf, rec_bytes, SYS_TIMEOUT)) <= 0)
                {
                    local_stats.extents_retried++;
                    mlog(DEBUG, "Atl03 reader failed to post to stream %s: %d", reader->outQ->getName(), post_status);
                }

                /* Update Statistics */
                if(post_status > 0) local_stats.extents_sent++;
                else                local_stats.extents_dropped++;

                /*
                 * Clean Up Record
                 *  It should be safe to delete here because there should be no
                 *  way an exception is thrown between the code that allocates the record
                 *  and the code below that deletes it.
                 */
                delete record;
            }
            else // neither pair in extent valid
            {
                local_stats.extents_filtered++;
            }

        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Failure during processing of resource %s track %d: %s", resource, track, e.what());
    }

    /* Handle Global Reader Updates */
    reader->threadMut.lock();
    {
        /* Update Statistics */
        reader->stats.segments_read += local_stats.segments_read;
        reader->stats.extents_filtered += local_stats.extents_filtered;
        reader->stats.extents_sent += local_stats.extents_sent;
        reader->stats.extents_dropped += local_stats.extents_dropped;
        reader->stats.extents_retried += local_stats.extents_retried;

        /* Count Completion */
        reader->numComplete++;
        if(reader->numComplete == reader->threadCount)
        {
            mlog(CRITICAL, "Completed processing resource %s", resource);

            /* Indicate End of Data */
            reader->outQ->postCopy("", 0);
            reader->signalComplete();
        }
    }
    reader->threadMut.unlock();

    /* Clean Up Info */
    delete [] info->resource;
    delete info;

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}

/*----------------------------------------------------------------------------
 * luaParms - :parms() --> {<key>=<value>, ...} containing parameters
 *----------------------------------------------------------------------------*/
int Atl03Reader::luaParms (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;
    Atl03Reader* lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = (Atl03Reader*)getLuaSelf(L, 1);
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }

    try
    {
        /* Create Parameter Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, LUA_PARM_SURFACE_TYPE,         lua_obj->parms->surface_type);
        LuaEngine::setAttrInt(L, LUA_PARM_SIGNAL_CONFIDENCE,    lua_obj->parms->signal_confidence);
        LuaEngine::setAttrNum(L, LUA_PARM_ALONG_TRACK_SPREAD,   lua_obj->parms->along_track_spread);
        LuaEngine::setAttrInt(L, LUA_PARM_MIN_PHOTON_COUNT,     lua_obj->parms->minimum_photon_count);
        LuaEngine::setAttrNum(L, LUA_PARM_EXTENT_LENGTH,        lua_obj->parms->extent_length);
        LuaEngine::setAttrNum(L, LUA_PARM_EXTENT_STEP,          lua_obj->parms->extent_step);

        /* Set Success */
        status = true;
        num_obj_to_return = 2;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error returning parameters %s: %s", lua_obj->getName(), e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_obj_to_return);
}

/*----------------------------------------------------------------------------
 * luaStats - :stats(<with_clear>) --> {<key>=<value>, ...} containing statistics
 *----------------------------------------------------------------------------*/
int Atl03Reader::luaStats (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;
    Atl03Reader* lua_obj = NULL;

    try
    {
        /* Get Self */
        lua_obj = (Atl03Reader*)getLuaSelf(L, 1);
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }

    try
    {
        /* Get Clear Parameter */
        bool with_clear = getLuaBoolean(L, 2, true, false);

        /* Create Statistics Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, LUA_STAT_SEGMENTS_READ,        lua_obj->stats.segments_read);
        LuaEngine::setAttrInt(L, LUA_STAT_EXTENTS_FILTERED,     lua_obj->stats.extents_filtered);
        LuaEngine::setAttrInt(L, LUA_STAT_EXTENTS_SENT,         lua_obj->stats.extents_sent);
        LuaEngine::setAttrInt(L, LUA_STAT_EXTENTS_DROPPED,      lua_obj->stats.extents_dropped);
        LuaEngine::setAttrInt(L, LUA_STAT_EXTENTS_RETRIED,      lua_obj->stats.extents_retried);

        /* Clear if Requested */
        if(with_clear) LocalLib::set(&lua_obj->stats, 0, sizeof(lua_obj->stats));

        /* Set Success */
        status = true;
        num_obj_to_return = 2;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error returning stats %s: %s", lua_obj->getName(), e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_obj_to_return);
}
