Usage: 

From repository root, build the emulator and run `./tools/fuzz-multi/fuzz-multi.sh --arrange tools/fuzz-multi/fuzz.csv`

This will load `fuzz.csv`, and open one emulator for each line. The first column is the mode name, the second column is the buttons to fuzz with (e.g. `up,down,left,right,a,b,select,start,x,y,1,2,3,4,5`), and the third column is any extra arguments to pass to the emulator for that mode (e.g. `--fuzz-button-delay 50` for buttons twice as fast as the default).

Logs will be placed by default in `fuzz/logs` and NVS will be in `fuzz/nvs`. Change the dir with `--out DIRNAME`, and use `--clean-nvs` and `--clean-logs` to delete them before running.

To pass extra arguments to every emulator instance, pass `--` followed by any number of emulator arguments.

There's also arrangements for window size, tiling, and number of columns to arrange into: `-w/-h, --tx/--ty, --cols`
