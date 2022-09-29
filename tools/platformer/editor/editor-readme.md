editor-readme

# How to make levels for Platformer (tentatively titled Super Swadge Land):

## Introduction
Super Swadge Land is a conventional 2D platformer action game. The base game mechanics are inspired by a certain funny mustachioed plumber wahoo game. However, while overcoming obstacle course-like levels is the primary goal, high score chasing is a highly encouraged (but not required) side goal. The combo multiplier advances with most actions: defeating enemies, collecting coins, beaking brick blocks, collecting powerups, etc; thus adding hidden depth to each level!

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
2. If you don't see the Layers pane, add a new layer and then delete it to make it appear.
3. Select the Tilemap layer (may be named "Tilemap 1")
4. On the top left of the window, underneath the lock icon, make sure the brick icon button is pressed. (You should see the tileset instead of the color palette)
5. Click on a tile to select it, then draw in the canvas as you would in any pixel art editor!
6. You can place objects just like any other tile. See the tiles with ID of 128 and greater.
7. When you're done, use File -> Scripts -> export-tilemap-binary to export your level. Using the dialog box, select the filename/location to which it will be saved. Filenames must be limited to 12 characters including .bin extension!!
8. Place the exported file in assets/platformer/levels.
9. If you're overwriting an existing level file, delete the old version from the spiffs_image directory before building!
10. Update the leveldef_t struct in mode_platformer.c to add your level to the game!

# General rules for level creation:
- Levels must be between 19x14 and 255x255 tiles in size. (Use Sprite -> Canvas Size to select a size)
- Every level must have exactly one START tile. This determines where the player starts.
- Every level must have at least one goal tile (dark green blocks with white lines and a letters from A to D, including a star).
- Tiles that look like boxes or circles with hex numbers are unimplemented. Don't use them.
- The player spawn tile (ID=128) actually will spawn another player object, and weird stuff will happen. Don't use it! If you are trying to set a start location for the player, use the START tile (ID=1).

# Level design hints:
- This game implements conventional platformer mechanics: Holding the B button to run, momentum, jump heights that depend on the player's horizontal velocity at the time of jump and can be cancelled by releasnig the A button, time limits, etc. If you design your levels similar to a certain funny mustachioed plumber wahoo game, you will be on the right track.
- Try to include opportunities for the player to maintain a combo multiplier from the beginning of the level all the way to the end. Coins, enemies, brick blocks, and powerups will all count towards keeping combos alive.
- Use invisible blocks to prevent the player from leaving the left or right side of a level.
- Make the end of the level fun. Challenge the player to jump as far as possible across a field of goal tiles of increasing point value. Or mix things up by putting the star-rank goal tile in the middle! Or put enemies in the goal zone to make things really interesting! The possibilities are endless...

# Tileset Reference
## Control tiles
### Empty
All empty spaces are represented with tile 0.

| Tile Id | Tile Name | Appearance in editor |
| --- | --- | --- |
| 0 | TILE_EMPTY | (no image) |

### Warp tiles
These tiles represent specific locations within the level to which the player can be teleported, usually by touching a Warp object.

In the editor they can either represent a warp entrance or warp destination, depending on what kind of tile it is placed above:
- When placed directly above a Container Block or Brick Block:
    - The Container Block or Brick Block will release a Warp Vortex that will
    send the player to the warp destination when touched.
- Otherwise:
    - Defines the corresponding warp destination. All level files will include 16 warp locations. If a warp destination is not defined, it will be set to 0,0.

Warp 0, a.k.a. the "START" tile, represents the starting location for the player in the level.

| Tile Id | Tile Name | Appearance in editor |
| --- | --- | --- |
| 1 | TILE_WARP_0 | ![TILE_WARP_0](./editor-readme-assets/tile001.png) |
| 2 | TILE_WARP_1 | ![TILE_WARP_1](./editor-readme-assets/tile002.png) |
| 3 | TILE_WARP_2 | ![TILE_WARP_2](./editor-readme-assets/tile003.png) |
| 4 | TILE_WARP_3 | ![TILE_WARP_3](./editor-readme-assets/tile004.png) |
| 5 | TILE_WARP_4 | ![TILE_WARP_4](./editor-readme-assets/tile005.png) |
| 6 | TILE_WARP_5 | ![TILE_WARP_5](./editor-readme-assets/tile006.png) |
| 7 | TILE_WARP_6 | ![TILE_WARP_6](./editor-readme-assets/tile007.png) |
| 8 | TILE_WARP_7 | ![TILE_WARP_7](./editor-readme-assets/tile008.png) |
| 9 | TILE_WARP_8 | ![TILE_WARP_8](./editor-readme-assets/tile009.png) |
| 10 | TILE_WARP_9 | ![TILE_WARP_9](./editor-readme-assets/tile010.png) |
| 11 | TILE_WARP_A | ![TILE_WARP_A](./editor-readme-assets/tile011.png) |
| 12 | TILE_WARP_B | ![TILE_WARP_B](./editor-readme-assets/tile012.png) |
| 13 | TILE_WARP_C | ![TILE_WARP_C](./editor-readme-assets/tile013.png) |
| 14 | TILE_WARP_D | ![TILE_WARP_D](./editor-readme-assets/tile014.png) |
| 15 | TILE_WARP_E | ![TILE_WARP_E](./editor-readme-assets/tile015.png) |
| 16 | TILE_WARP_F | ![TILE_WARP_F](./editor-readme-assets/tile016.png) |

### Container Block Content Tiles
When placed directly above a Container Block or Brick Block, that block will release the item denoted by the tile.

| Tile Id | Tile Name | Appearance in editor | Notes |
| --- | --- | --- | --- |
| 17 | TILE_CTNR_COIN | ![TILE_CTNR_COIN](./editor-readme-assets/tile017.png) | The coin is added directly to the current total, rather than emerging from the block. |
| 18 | TILE_CTNR_10COIN | ![TILE_CTNR_10COIN](./editor-readme-assets/tile018.png) | (Unimplemented) |
| 19 | TILE_CTNR_POW1 | ![TILE_CTNR_POW1](./editor-readme-assets/tile019.png) | The main powerup, "Gaming" or "Music" (depending on the player's current powerup state) |
| 20 | TILE_CTNR_POW2 | ![TILE_CTNR_POW2](./editor-readme-assets/tile020.png) | (Unimplemented) |
| 21 | TILE_CTNR_POW3 | ![TILE_CTNR_POW3](./editor-readme-assets/tile021.png) | (Unimplemented) |
| 22 | TILE_CTNR_1UP | ![TILE_CTNR_1UP](./editor-readme-assets/tile022.png) | The 1UP heart. |

## Invisible Blocks

| Tile Id | Tile Name | Appearance in editor | Notes |
| --- | --- | --- | --- |
| 30 | TILE_INVISIBLE_BLOCK | ![TILE_INVISIBLE_BLOCK](./editor-readme-assets/tile030.png) | A solid invisible block. Use these to prevent the player from leaving the left or right side of the level. |
| 31 | TILE_INVISIBLE_CONTAINER_BLOCK | ![TILE_INVISIBLE_CONTAINER_BLOCK](./editor-readme-assets/tile031.png) | An invisible version of Container Block. Can be activated from any direction. |

## Solid Tiles
The basic building blocks of any level. Objects cannot pass or fall through them.

| Tile Id | Tile Name | Appearance |
| --- | --- | --- |
| 32 | TILE_GRASS | ![TILE_GRASS](../../../assets/platformer/tiles/tile032.png) |
| 33 | TILE_GROUND | ![TILE_GROUND](../../../assets/platformer/tiles/tile033.png) |
| 35 | TILE_BLOCK | ![TILE_BLOCK](../../../assets/platformer/tiles/tile035.png) |
| 36 | TILE_METAL_BLOCK | ![TILE_METAL_BLOCK](../../../assets/platformer/tiles/tile036.png) |
| 37 | TILE_METAL_PIPE_H | ![TILE_METAL_PIPE_H](../../../assets/platformer/tiles/tile037.png) |
| 38 | TILE_METAL_PIPE_V | ![TILE_METAL_PIPE_V](../../../assets/platformer/tiles/tile038.png) |
| 41 | TILE_GIRDER | ![TILE_GIRDER](../../../assets/platformer/tiles/tile041.png) |

## Bounce Block
Unimplemented. 

| Tile Id | Tile Name | Appearance | Notes |
| --- | --- | --- | --- |
| 39 | TILE_METAL_PIPE_HEND | ![TILE_METAL_PIPE_HEND](../../../assets/platformer/tiles/tile039.png) | The name is wrong.|

## Goal Tiles
When the player lands on these the level is complete and the appropriate bonus is given.

| Tile Id | Tile Name | Appearance |
| --- | --- | --- |
| 59 | TILE_GOAL_100PTS | ![TILE_GRASS](../../../assets/platformer/tiles/tile059.png) |
| 60 | TILE_GOAL_500PTS | ![TILE_GROUND](../../../assets/platformer/tiles/tile060.png) |
| 61 | TILE_GOAL_1000PTS | ![TILE_BRICK_BLOCK](../../../assets/platformer/tiles/tile061.png) |
| 62 | TILE_GOAL_2000PTS | ![TILE_BLOCK](../../../assets/platformer/tiles/tile062.png) |
| 63 | TILE_GOAL_5000PTS | ![TILE_METAL_BLOCK](../../../assets/platformer/tiles/tile063.png) |

## Container Block
The standard Container Block. Can be activated from any direction. If a Container Block Content Tile is placed above, the block will yield that item when hit.

When a player hits the Container Block, it will bounce in the direction it was hit. Any enemies touching the bouncing block will be defeated!

| Tile Id | Tile Name | Appearance |
| --- | --- | --- |
| 64 | TILE_CONTAINER_1 | ![TILE_CONTAINER_1](../../../assets/platformer/tiles/tile064.png) |
### Container Block Animation Tiles
Animation frames for the Container Block. Don't use these.

| Tile Id | Tile Name | Appearance |
| --- | --- | --- |
| 65 | TILE_CONTAINER_2 | ![TILE_CONTAINER_2](../../../assets/platformer/tiles/tile065.png) |
| 66 | TILE_CONTAINER_3 | ![TILE_CONTAINER_2](../../../assets/platformer/tiles/tile066.png) |

## Brick Block
Has all the same functionality as the Container Block, but can break depending on how the player collides with it:
- Jumping into the Brick Block from below
- Running into the Brick Block from the side at full speed

However, if the player lands on top of the Brick Block, it will not break and thus is still safe to use as a platform.

| Tile Id | Tile Name | Appearance |
| --- | --- | --- |
| 34 | TILE_BRICK_BLOCK | ![TILE_BRICK_BLOCK](../../../assets/platformer/tiles/tile034.png) |

## Coin
A coin. If the player touches it, it will be collected.

| Tile Id | Tile Name | Appearance |
| --- | --- | --- |
| 67 | TILE_COIN_1 | ![TILE_COIN_1](../../../assets/platformer/tiles/tile067.png) |
### Coin Animation Tiles
Animation frames for the Coin. Don't use these.

| Tile Id | Tile Name | Appearance |
| --- | --- | --- |
| 68 | TILE_COIN_2 | ![TILE_COIN_2](../../../assets/platformer/tiles/tile068.png) |
| 69 | TILE_COIN_3 | ![TILE_COIN_3](../../../assets/platformer/tiles/tile069.png) |

## Background tiles
Objects will pass through these tiles and not interact with them.

### Goal Zone Marker
Precede your Goal Block areas with these, so that the player knows to prerpare a jump to land on the highest value block possible.

| Tile Id | Tile Name | Appearance |
| --- | --- | --- |
| 80 | TILE_BG_GOAL_ZONE | ![TILE_BG_GOAL_ZONE](../../../assets/platformer/tiles/tile080.png) |

### Arrow Signs
Use these to direct the player when a level layout is potentially confusing.

| Tile Id | Tile Name | Appearance |
| --- | --- | --- |
| 81 | TILE_BG_ARROW_L | ![TILE_BG_ARROW_L](../../../assets/platformer/tiles/tile081.png) | 
| 82 | TILE_BG_ARROW_R | ![TILE_BG_ARROW_R](../../../assets/platformer/tiles/tile082.png) | 
| 83 | TILE_BG_ARROW_U | ![TILE_BG_ARROW_U](../../../assets/platformer/tiles/tile083.png) | 
| 84 | TILE_BG_ARROW_D | ![TILE_BG_ARROW_D](../../../assets/platformer/tiles/tile084.png) | 
| 85 | TILE_BG_ARROW_LU | ![TILE_BG_ARROW_LU](../../../assets/platformer/tiles/tile085.png) |
| 86 | TILE_BG_ARROW_RU | ![TILE_BG_ARROW_RU](../../../assets/platformer/tiles/tile086.png) |
| 87 | TILE_BG_ARROW_LD | ![TILE_BG_ARROW_LD](../../../assets/platformer/tiles/tile087.png) | 
| 88 | TILE_BG_ARROW_RD | ![TILE_BG_ARROW_RD](../../../assets/platformer/tiles/tile088.png) |

## Ladder
Unimplemented.

| Tile Id | Tile Name | Appearance |
| --- | --- | --- |
| 70 | TILE_LADDER | ![TILE_LADDER](../../../assets/platformer/tiles/tile070.png) |

## Entity Spawn Location Tiles
When these tiles scroll into view, the corresponding entity will spawn at that location.
NOTE: if 16 entities are active, including the player, further entities will not be spawned until at least 1 becomes inactive.

### Scroll Lock Entities
Use these entities to prevent the screen from scrolling past a certain point in the level. This is useful to keep bonus areas hidden until the player warps into it.

The spawn tile must be scrolled onto the screen for it to take effect. It will immediately despawn in 1 frame after it changes tilemap scrolling parameters.

In the editor, the arrow will point to the line past which the screen will not be allowed to scroll.

| Tile Id | Entity Name | Appearance in editor |
| --- | ---| --- |
| 130 | ENTITY_SCROLL_LOCK_LEFT | ![ENTITY_SCROLL_LOCK_LEFT](./editor-readme-assets/tile130.png) | 
| 131 | ENTITY_SCROLL_LOCK_RIGHT | ![ENTITY_SCROLL_LOCK_RIGHT](./editor-readme-assets/tile131.png) | 
| 132 | ENTITY_SCROLL_LOCK_UP | ![ENTITY_SCROLL_LOCK_UP](./editor-readme-assets/tile132.png) |
| 133 | ENTITY_SCROLL_LOCK_DOWN | ![ENTITY_SCROLL_LOCK_DOWN](./editor-readme-assets/tile133.png) |

### Scroll Unlock Entity
This entity unlocks scrolling in all directions after at least 1 scroll lock entity has set scrolling limits.

| Tile Id | Entity Name | Appearance in editor |
| --- | ---| --- |
| 134 | ENTITY_SCROLL_UNLOCK | ![ENTITY_SCROLL_UNLOCK](./editor-readme-assets/tile134.png) | 

### Enemies
The player can defeat any enemy by jumping on top of it ("stomping").
#### Bush-like enemy (no name yet)
A basic enemy that walks forward, falls off ledges, and turns around at walls.

| Tile Id | Entity Name | Appearance | Notes |
| --- | ---| --- | --- |
| 129 | ENTITY_TEST | ![ENTITY_TEST](../../../assets/platformer/sprites/sprite009.png) | |
| 141 | ENTITY_BUSH_2 | ![ENTITY_BUSH_2](../../../assets/platformer/sprites/sprite033.png) | Moves a little faster and turns around at platform edges. When it changes direction, it does a small hop. |
| 142 | ENTITY_BUSH_3 | ![ENTITY_BUSH_3](../../../assets/platformer/sprites/sprite034.png) | Moves fast and randomly changes direction to avoid being stomped. When it changes direction, it does a small hop. |

#### Dust Bunny
A rabbit enemy that is a bit more difficult to stomp as it constantly jumps.

| Tile Id | Entity Name | Appearance | Notes |
| --- | ---| --- | --- |
| 139 | ENTITY_DUST_BUNNY | ![ENTITY_DUST_BUNNY](./editor-readme-assets/tile139.png) | Jumps in place at random heights at random intervals. |
| 143 | ENTITY_DUST_BUNNY_2 | ![ENTITY_DUST_BUNNY_2](./editor-readme-assets/tile143.png) | Jumps forward at random heights at more frequent intervals, turning around at walls. |
| 144 | ENTITY_DUST_BUNNY_3 | ![ENTITY_DUST_BUNNY_3](./editor-readme-assets/tile144.png) | Jumps at random heights at random intervals, always moving towards the player. |

#### Wasp
An aggressive enemy that flies through the air, dropping rapidly to the ground in attempt to attack an approaching player below.
Note: If the player is above the wasp, it will not attack.

| Tile Id | Entity Name | Appearance | Notes |
| --- | ---| --- | --- |
| 140 | ENTITY_WASP | ![ENTITY_WASP](./editor-readme-assets/tile140.png) | Flies slowly and attacks when the player is within a few blocks horizontally from itself. Always returns to its original flying height before resuming horizontal flight. |
| 145 | ENTITY_WASP_2 | ![ENTITY_WASP_2](./editor-readme-assets/tile145.png) | Flies a little bit more quickly and ttacks when the player is within a random number of blocks horizontally from itself. Always returns to its original flying height before resuming horizontal flight. |
| 146 | ENTITY_WASP_3 | ![ENTITY_WASP_3](./editor-readme-assets/tile146.png) | Flies a quickly towards the player and attacks when the player is within a close range of blocks horizontally from itself. Rises to a random flying height before resuming horizontal flight. |

### Background Color Control Entities
Changes the background color to its specified color when its spawn tile is scrolled onto the screen.
Use it to apply a sky color background and to change it to a color that gives the impression of being underground or indoors.

Note: the color shown in the editor does not match the exact color shown in-game. See the hex values below.

| Tile Id | Entity Name | Appearance in editor | RGB hex color | Notes |
| --- | ---| --- | --- | --- |
| 147 | ENTITY_BGCOL_BLUE | ![ENTITY_BGCOL_BLUE](./editor-readme-assets/tile147.png) | 0x99999ff | |
| 148 | ENTITY_BGCOL_YELLOW | ![ENTITY_BGCOL_YELLOW](./editor-readme-assets/tile148.png) | 0xffcc66 | |
| 149 | ENTITY_BGCOL_ORANGE | ![ENTITY_BGCOL_ORANGE](./editor-readme-assets/tile149.png) | 0xff9966 | |
| 150 | ENTITY_BGCOL_PURPLE | ![ENTITY_BGCOL_PURPLE](./editor-readme-assets/tile150.png) | 0x6633cc | |
| 151 | ENTITY_BGCOL_DARK_PURPLE | ![ENTITY_BGCOL_DARK_PURPLE](./editor-readme-assets/tile151.png) | 0x330099 | |
| 152 | ENTITY_BGCOL_BLACK | ![ENTITY_BGCOL_BLACK](./editor-readme-assets/tile152.png) | 0x000000 | |
| 153 | ENTITY_BGCOL_NEUTRAL_GREEN | ![ENTITY_BGCOL_NEUTRAL_GREEN](./editor-readme-assets/tile153.png) | 0x336666 | Color is likely to be changed to a completely different color. |
| 154 | ENTITY_BGCOL_DARK_RED | ![ENTITY_BGCOL_DARK_RED](./editor-readme-assets/tile154.png) | 0x660000 | |
| 155 | ENTITY_BGCOL_DARK_GREEN | ![ENTITY_BGCOL_DARK_GREEN](./editor-readme-assets/tile155.png) | 0x003300 | Color is likely to be changed to a completely different color. |

### Checkpoint Entity
Places a checkpoint flag into the level. Must be placed directly underneath TILE_WARPx tile to function! The player will respawn at that warp if the flag is activated.

| Tile Id | Entity Name | Appearance in editor |
| --- | ---| --- |
| 158 | ENTITY_CHECKPOINT | ![ENTITY_CHECKPOINT](../../../assets/platformer/sprites/sprite048.png) |

## Don't Place the Following Entities using the Editor.

| Tile Id | Entity Name | Appearance in editor | Notes |
| --- | ---| --- | --- | 
| 128 | ENTITY_PLAYER | ![ENTITY_PLAYER](../../../assets/platformer/sprites/sprite000.png) | Use TILE_WARP_0 (The START tile) instead. Otherwise weird stuff will happen. |
| 135 | ENTITY_HIT_BLOCK | ![ENTITY_HIT_BLOCK](./editor-readme-assets/tile135.png) | The object that makes a Container Block or Brick Block look like it's bouncing. Useless if placed from the editor. |
| 136 | ENTITY_DEAD | ![ENTITY_DEAD](./editor-readme-assets/tile136.png) | Represents a defeated player or enemy. Useless if placed from the editor. |
| 137 | ENTITY_POWERUP | ![ENTITY_POWERUP](../../../assets/platformer/sprites/sprite016.png) ![ENTITY_POWERUP](../../../assets/platformer/sprites/sprite019.png)  | The "Gaming" or "Music" powerup dropped by TILE_CTNR_POW1. |
| 138 | ENTITY_WARP | ![ENTITY_WARP](../../../assets/platformer/sprites/sprite021.png) | Warp vortex dropped using the TILE_WARPx control tiles. Won't work properly if placed directly using the editor. |
| 156 | ENTITY_1UP | ![ENTITY_1UP](../../../assets/platformer/sprites/sprite027.png) | The 1UP Heart dropped by TILE_CTNR_1UP. Just use that instead, please. |
| 157 | ENTITY_WAVE_BALL | ![ENTITY_WAVE_BALL](../../../assets/platformer/sprites/sprite030.png) | The Square-wave Ball thrown by the player by pressing the B button at 3 HP. Why would you place this directly into a level? |
