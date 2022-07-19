-- Script to export tilemap data as a binary file.
-- Original script by Zeltrix (https://pastebin.com/mQGiKAgR)
-- Export to binary by JVeg199X
-- Note: This script only works with tilemaps of 255x255 tiles or less

if TilesetMode == nil then return app.alert "Use Aseprite 1.3" end
local spr = app.activeSprite

if not spr then return end

local d = Dialog("Export Tilemap as .bin File")
d:label{id="lab1",label="",text="Export Tilemap as .bin File for your own GameEngine"}
 :file{id = "path", label="Export Path", filename="",open=false,filetypes={"bin"}, save=true, focus=true}
 :label{id="lab3", label="",text="Max supported tilemap size: 255x255"}
 :separator{}
 :label{id="lab2", label="",text="In the last row of the tilemap-layer there has to be at least one Tile \"colored\" to fully export the whole Tilemap"}
 :button{id="ok",text="&OK",focus=true}
 :button{text="&Cancel" }
 :show()

local data = d.data
if not data.ok then return end
    local lay = app.activeLayer
    if(#data.path<=0)then app.alert("No path selected") end
    if not lay.isTilemap then return app.alert("Layer is not tilemap") end
    pc = app.pixelColor
    mapFile = io.open(data.path,"w")

  for _,c in ipairs(lay.cels) do
    local img = c.image

    --The first two bytes contain the width and height of the tilemap in tiles
    mapFile:write(string.char(img.width))
    mapFile:write(string.char(img.height))

    for p in img:pixels() do
      if(p ~= nil) then
        mapFile:write(string.char(p()))
      end
    end
  end

    mapFile:close()
     