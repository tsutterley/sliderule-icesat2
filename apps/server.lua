local global = require("global")
local asset = require("asset")
local json = require("json")

-- Function to return all available system scripts
function available_scripts()
    local pdir = io.popen('ls "' .. __confdir .. '"')
    local scripts = {}
    local i = 0
    for filename in pdir:lines() do
        i = i + 1
        scripts[i] = filename
    end
    pdir:close()
    return scripts
end

-- Process Arguments: JSON Configuration File
local cfgtbl = {}
local json_input = arg[1]
if json_input and string.match(json_input, ".json") then
    sys.log(core.CRITICAL, string.format('Reading json file: %s', json_input))
    local f = io.open(json_input, "r")
    local content = f:read("*all")
    f:close()
    cfgtbl = json.decode(content)
end

-- Pull Out Parameters --
local event_format = global.eval(cfgtbl["event_format"]) or core.FMT_TEXT
local event_level = global.eval(cfgtbl["event_level"]) or core.INFO
local port = cfgtbl["server_port"] or 9081
local asset_directory = cfgtbl["asset_directory"] or nil

-- Configure Monitoring --
sys.setlvl(core.LOG | core.TRACE | core.METRIC, event_level) -- set level globally
local monitor = core.monitor(core.LOG, core.INFO, event_format) -- monitor only logs
monitor:name("EventMonitor")
monitor:tail(1024)
local dispatcher = core.dispatcher(core.EVENTQ)
dispatcher:name("EventDispatcher")
dispatcher:attach(monitor, "eventrec")
dispatcher:run()

-- Configure Assets --
assets = asset.loaddir(asset_directory, true)

-- Configure Endpoint --
endpoint = core.endpoint()
endpoint:name("LuaEndpoint")
endpoint:metric() -- catchall
for _,script in ipairs(available_scripts()) do
    local s = script:find(".lua")
    if s then
        local metric_name = script:sub(0,s-1)
        endpoint:metric(metric_name) 
    end
end

-- Run Earth Data Authentication Script --
auth_script = core.script("earth_data_auth", "")
auth_script:name("auth_script")

-- Run Server --
server = core.httpd(port)
server:name("HttpServer")
server:attach(endpoint, "/source")
