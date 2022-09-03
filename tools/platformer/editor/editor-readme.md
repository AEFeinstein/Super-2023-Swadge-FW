editor-readme

# How to make levels for Platformer (tentatively titled Super Swadge Land):

## Prerequisites
- Aseprite v1.3 beta https://www.aseprite.org
    - Pixel art editor, paid software ($20)
    - Purchase will give you access to stable and beta versions
    - Choose the beta version, as that is currently the only one that supports tilemaps
- Level template file
    - https://github.com/AEFeinstein/Super-2023-Swadge-FW/blob/platformer/tools/platformer/editor/blank-template.aseprite
    - or make a copy of Level 1-1:
        - https://github.com/AEFeinstein/Super-2023-Swadge-FW/blob/platformer/tools/platformer/editor/level1-1.aseprite
- Aseprite script for exporting levels
    - https://github.com/AEFeinstein/Super-2023-Swadge-FW/blob/platformer/tools/platformer/editor/export-tilemap-binary.lua
    - Place in Aseprite Scripts folder
        - Use "File -> Scripts -> Open Scripts Folder" to access the folder.

## Usage:
1. Open the template file in Aesprite
2. On the top left of the window, underneath the lock icon, make sure the brick icon button is pressed. (You should see the tileset instead of the color palette)
3. Click on a tile to select it, then draw in the canvas as you would in any pixel art editor!
4. You can place objects just like any other tile. See the tiles with ID of 128 and greater.
5. When you're done, use File -> Scripts -> export-tilemap-binary to export your level. Using the dialog box, select the filename/location to which it will be saved. Filenames must be limited to 12 characters including .bin extension!!
6. Place the exported file in assets/platformer/levels.
7. If you're overwriting an existing level file, delete the old version from the spiffs_image directory before building!

# General rules for level creation:
- Levels must be between 19x14 and 255x255 tiles in size. (Use Sprite -> Canvas Size to select a size)
- Every level must have exactly one START tile. This determines where the player starts.
- Every level must have at least one goal tile (dark green blocks with white lines and a letters from A to D, including a star).
- Tiles that look like boxes or circles with hex numbers are unimplemented. Don't use them.
- The player spawn tile (ID=128) actually will spawn another player object, and weird stuff will happen. Don't use it! If you are trying to set a start location for the player, use the START tile (ID=1).

# Tileset Reference

## High level overview:

Tiles are arranged within the tileset according to their attributes.
See the chart below.

ATTRIBUTES                           |  RANGE       |  TOTAL
------------------------------------------------------------------------
nonsolid static background invisible | 0-29         | 30
solid static background invisible    | 30           | 1
solid static interactive invisible   | 31           | 1
solid static interactive visible     | 32-63        | 32
solid animated interactive visible   | 64-66        | 3
nonsolid animated interactive visible| 67-69        | 3
nonsolid static interactive visible  | 70-79        | 10
nonsolid static background visible   | 80-127       | 48
object spawn locations               | 128-255      | 127

## Detailed overview:

### nonsolid static background invisible

TILE_EMPTY (Id=0)
    - An empty space of course!
TILE_WARP_0 (Id=1),
TILE_WARP_1 (Id=2),
TILE_WARP_2 (Id=3),
TILE_WARP_3 (Id=4),
TILE_WARP_4 (Id=5),
TILE_WARP_5 (Id=6),
TILE_WARP_6 (Id=7),
TILE_WARP_7 (Id=8),
TILE_WARP_8 (Id=9),
TILE_WARP_9 (Id=10),
TILE_WARP_A (Id=11),
TILE_WARP_B (Id=12),
TILE_WARP_C (Id=13),
TILE_WARP_D (Id=14),
TILE_WARP_E (Id=15),
TILE_WARP_F (Id=16)
    - When placed directly above a Container Block or Brick Block:
        - The Container Block or Brick Block will release a Warp Vortex that will
        send the player to the warp destination when touched.
    - Otherwise:
        - Defines the corresponding warp destination. If a warp destination is not defined, it will be set to 0,0.
TILE_CTNR_COIN (Id=17)
    - When placed directly above a Container Block or Brick Block, the block will give the player a coin when hit.
TILE_CTNR_10COIN (Id=18)
    - (Unimplemented)
TILE_CTNR_POW1 (Id=19)
    - When placed directly above a Container Block or Brick Block, the block will release the a powerup when hit. If the player has not collected a powerup yet, it will release the Joystick powerup, otherwise it will release the Speaker powerup.
TILE_CTNR_POW2 (Id=20),
TILE_CTNR_POW3 (Id=21),
    - (Unimplemented)
TILE_CTNR_1UP (Id=22)
    - (Unimplemented)
      - When placed directly above a Container Block or Brick Block, the block will release a 1UP icon when hit.
TILE_CTRL_LEFT (Id=23),
TILE_CTRL_RIGHT (Id=24),
TILE_CTRL_UP (Id=25),
TILE_CTRL_DOWN (Id=26)
    - (Unimplemented)
    - When a moving platform touches this location it will move in the direction the arrow points.
TILE_UNUSED_27 (Id=27),
TILE_UNUSED_28 (Id=28),
TILE_UNUSED_29 (Id=29)
    - (Unimplemented)


### solid static background invisible 

TILE_INVISIBLE_BLOCK (Id=30),
    - A solid invisible block. Use these to prevent objects from leaving the intended area.

### solid static interactive invisible

TILE_INVISIBLE_CONTAINER (Id=31),
    - (Unimplemented)
    - An invisible version of Container Block. Can be activated from below OR above.


### solid static interactive visible  

TILE_GRASS (Id=32),
TILE_GROUND (Id=33),
TILE_BRICK_BLOCK (Id=34),
TILE_BLOCK (Id=35),
TILE_METAL_BLOCK (Id=36),
TILE_METAL_PIPE_H (Id=37),
TILE_METAL_PIPE_V (Id=38),
TILE_METAL_PIPE_HEND (Id=39),
TILE_METAL_PIPE_VEND (Id=40),
TILE_GIRDER (Id=41),
TILE_SOLID_UNUSED_42 (Id=42),
TILE_SOLID_UNUSED_43 (Id=43),
TILE_SOLID_UNUSED_44 (Id=44),
TILE_SOLID_UNUSED_45 (Id=45),
TILE_SOLID_UNUSED_46 (Id=46),
TILE_SOLID_UNUSED_47 (Id=47),
TILE_SOLID_UNUSED_48 (Id=48),
TILE_SOLID_UNUSED_49 (Id=49),
TILE_SOLID_UNUSED_50 (Id=50),
TILE_SOLID_UNUSED_51 (Id=51),
TILE_SOLID_UNUSED_52 (Id=52),
TILE_SOLID_UNUSED_53 (Id=53),
TILE_SOLID_UNUSED_54 (Id=54),
TILE_SOLID_UNUSED_55 (Id=55),
TILE_SOLID_UNUSED_56 (Id=56),
TILE_SOLID_UNUSED_57 (Id=57),
TILE_SOLID_UNUSED_58 (Id=58),
    - Solid tiles.
TILE_GOAL_100PTS (Id=59),
TILE_GOAL_500PTS (Id=60),
TILE_GOAL_1000PTS (Id=61),
TILE_GOAL_2000PTS (Id=62),
TILE_GOAL_5000PTS (Id=63)
    - When the player lands on these the level is complete and the appropriate bonus is given.


### solid animated interactive visible

TILE_CONTAINER_1 (Id=64),
    - The standard Container Block. Can be activated from any direction. If a CTNR control tile is placed above, the block will yield that item when hit.
TILE_CONTAINER_2 (Id=65),
TILE_CONTAINER_3 (Id=66)
    - Animation frames for the Container Block. Don't use these.


### nonsolid animated interactive visible

TILE_COIN_1 (Id=67)
    - A coin. If the player touches it, it will be collected.
TILE_COIN_2 (Id=68)
TILE_COIN_3 (Id=69)
    - Animation frames for the Coin. Don't use these.
TILE_LADDER (Id=70),
    - (Unimplemented, may not be included in this version of the game)
TILE_NONSOLID_INTERACTIVE_VISIBLE_71 (Id=71),
TILE_NONSOLID_INTERACTIVE_VISIBLE_72 (Id=72),
TILE_NONSOLID_INTERACTIVE_VISIBLE_73 (Id=73),
TILE_NONSOLID_INTERACTIVE_VISIBLE_74 (Id=74),
TILE_NONSOLID_INTERACTIVE_VISIBLE_75 (Id=75),
TILE_NONSOLID_INTERACTIVE_VISIBLE_76 (Id=76),
TILE_NONSOLID_INTERACTIVE_VISIBLE_77 (Id=77),
TILE_NONSOLID_INTERACTIVE_VISIBLE_78 (Id=78),
TILE_NONSOLID_INTERACTIVE_VISIBLE_79 (Id=79),
    - (Unimplemented)

### nonsolid static background visible

TILE_BG_GOAL_ZONE (Id=80),
TILE_BG_ARROW_L (Id=81),
TILE_BG_ARROW_R (Id=82),
TILE_BG_ARROW_U (Id=83),
TILE_BG_ARROW_D (Id=84),
TILE_BG_ARROW_LU (Id=85),
TILE_BG_ARROW_RU (Id=86),
TILE_BG_ARROW_LD (Id=87),
TILE_BG_ARROW_RD (Id=88)
    - Background tiles.


### object spawn locations

ENTITY_PLAYER (Id=128)
    - Spawns a player object. Weird stuff will happen. Don't use it! If you are trying to set a start location for the player, use the START tile (ID=1).
 ENTITY_TEST (Id=129)
    - Simple stompable enemy. Walks slowly along platforms. Will turn around when it reaches a wall.
ENTITY_SCROLL_LOCK_LEFT (Id=130)
ENTITY_SCROLL_LOCK_RIGHT (Id=131),
ENTITY_SCROLL_LOCK_UP (Id=132),
ENTITY_SCROLL_LOCK_DOWN (Id=133)
    - When spawned, prevents the screen from scrolling past the specified area. The arrow points to the horizontal or vertical line where the screen will be stopped.
ENTITY_SCROLL_UNLOCK (Id=134)
    - When spawned, unlocks screen so it can scroll anywhere.
ENTITY_HIT_BLOCK (Id=135)
    - The object that makes a Container Block or Brick Block look like it's bounciung. Don't place it with the editor. 
ENTITY_DEAD (Id=136)
    - Represents a defeated player or enemy. Don't place it with the editor.
ENTITY_POWERUP (Id=137)
    - Powerup dropped by TILE_CTNR_POW1.
ENTITY_WARP (Id=138)
    - Warp vortex dropped using the TILE_WARPx control tiles.
ENTITY_DUST_BUNNY (Id=139)
    - (Unimplemented)
    - A stompable bunny enemy that moves only by jumping at random intervals and random angles. Turns around at walls.
ENTITY_WASP (Id=140)
    - (Unimplemented)
    - A stompable wasp enemy that flies through the air, unaffected by gravity until the player passes underneath it, at which point it will drop straight down. After dropping down, it will stop for a short time when it meets a block, then fly back up. Turns around at walls.