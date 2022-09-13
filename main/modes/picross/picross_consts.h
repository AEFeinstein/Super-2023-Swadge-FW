//I had a cyclic dependency when this was in picross_menu (the root picross file), so i moved it to its own file.
//I plan to add more for display settings.
#define PICROSS_LEVEL_COUNT 19
#define PICROSS_MAX_LEVELSIZE 15
#define PICROSS_MAX_HINTCOUNT 8
#define PICROSS_EXTRA_PADDING 10 // actually double the padding we want around.
#define PICROSS_UIFONT_WIDTH 8 // 6px w + 2 px kern

//If you add files, but haven't changed level select or this file, it might not recompile.
//go to terminal and run: make -f emu.mk clean all
//which will force everything to refresh