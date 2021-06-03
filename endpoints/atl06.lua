--
-- ENDPOINT:    /source/atl06
--
-- PURPOSE:     generate customized atl06 data product
--
-- INPUT:       rqst
--              {
--                  "atl03-asset":  "<name of asset to use, defaults to atlas-local>"
--                  "resource":     "<url of hdf5 file or object>"
--                  "track":        <track number: 1, 2, 3>
--                  "parms":        {<table of parameters>}
--                  "timeout":      <milliseconds to wait for first response>
--              }
--
--              rspq - output queue to stream results
--
-- OUTPUT:      atl06rec (ATL06 algorithm results)
--
-- NOTES:       1. The rqst is provided by arg[1] which is a json object provided by caller
--              2. The rspq is the system provided output queue name string
--              3. The output is a raw binary blob containing serialized 'atl06rec' and 'atl06rec.elevation' RecordObjects
--

local json = require("json")

-- Create User Status --
local userlog = msg.publish(rspq)

-- Request Parameters --
local rqst = json.decode(arg[1])
local atl03_asset = rqst["atl03-asset"] or "atlas-s3"
local resource = rqst["resource"]
local track = rqst["track"] or icesat2.ALL_TRACKS
local parms = rqst["parms"]
local timeout = rqst["timeout"] or core.PEND

-- Get Asset --
asset = core.getbyname(atl03_asset)
if not asset then
    userlog:sendlog(core.INFO, string.format("invalid asset specified: %s", atl03_asset))
    return
end

-- Check Stages --
local recq = rspq .. "-atl03"

-- Post Initial Status Progress --
userlog:sendlog(core.INFO, string.format("atl06 processing initiated on %s ...", resource))

-- ATL06 Dispatch Algorithm --
atl06_algo = icesat2.atl06(rspq, parms)
atl06_algo:name("atl06_algo")

-- ATL06 Dispatcher --
atl06_disp = core.dispatcher(recq)
atl06_disp:name("atl06_disp")
atl06_disp:attach(atl06_algo, "atl03rec")
atl06_disp:run()

-- ATL03 Reader --
atl03_reader = icesat2.atl03(asset, resource, recq, parms, track)
atl03_reader:name("atl03_reader")

-- Wait Until Completion --
local duration = 0
local interval = 10000 -- 10 seconds
while not atl06_disp:waiton(interval) do
    duration = duration + interval
    -- Check for Timeout --
    if timeout > 0 and duration == timeout then
        userlog:sendlog(core.INFO, string.format("request for %s timed-out after %d seconds", resource, duration / 1000))
        return
    end
    -- Get Stats --
    local atl03_stats = atl03_reader:stats(false)
    local atl06_stats = atl06_algo:stats(false)
    -- Dispay Progress --
    if atl06_stats.h5atl03 == 0 then
        userlog:sendlog(core.INFO, string.format("... continuing to read %s (after %d seconds)", resource, duration / 1000))
    else
        userlog:sendlog(core.INFO, string.format("processed %d out of %d segments in %s (after %d seconds)", atl06_stats.h5atl03, atl03_stats.read, resource, duration / 1000))
    end
end

-- Processing Complete
userlog:sendlog(core.INFO, string.format("processing of %s complete", resource))
return
