local runner = require("test_executive")
local rd = runner.rootdir(arg[0]) -- root directory
local td = rd .. "../tests/" -- test directory
local console = require("console")

-- Initial Configuration --

console.logger:config(core.INFO)

-- Run ICESat2 Unit Tests --

runner.script(td .. "atl06_elements.lua")
runner.script(td .. "atl06_unittest.lua")
runner.script(td .. "atl03_indexer.lua")

-- Report Results --

runner.report()

-- Cleanup and Exit --

sys.quit()
