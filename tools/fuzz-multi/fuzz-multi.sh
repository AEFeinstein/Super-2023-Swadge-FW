#!/bin/bash

# Should be a semicolon-separated value file with 1 header line
# First column is mode name
# Second column is keys to fuzz
# Third column is any extra args to pass to the emu
SETTINGS_FILE=fuzz.csv

GDB=0
ARRANGE=0
VALGRIND=0

CLEAN_NVS=0
CLEAN_LOGS=0

# Emu window size
WIN_W=320
# Emu window width
WIN_H=290

# Offset of each emu window from the previous
TILE_X=320
TILE_Y=275

# Constant offset for window X and Y
OFF_X=0
OFF_Y=30

WIN_COLS=5

OUT_DIR="fuzz"

EMU_ARGS=""

# Title of the emulator window for arranging
EMU_NAME="SQUAREWAVEBIRD Simulator"

function die()
{
    echo "${1}" >2
    exit 1
}

while [ "$#" -gt "0" ]; do
    case "${1}" in
	--gdb|-gdb)
	    GDB=1
	    ;;

	--arrange|-arrange)
	    ARRANGE=1
	    ;;

	--valgrind|-valgrind)
	    VALGRIND=1
	    ;;

	--clean-nvs|-clean-nvs)
	    CLEAN_NVS=1
	    ;;

	--clean-logs|-clean-logs)
	    CLEAN_LOGS=1
	    ;;

	--cols|-cols)
	    shift || die "Missing argument to --cols"
	    WIN_COLS="${1}"
	    ;;

	--w|-w)
	    shift || die "Missing argument to --w"
	    WIN_W="${1}"
	    ;;

	--h|-h)
	    shift || die "Missing argument to --h"
	    WIN_H="${1}"
	    ;;

	--tx|-tx)
	    shift || die "Missing argument to --tx"
	    TILE_X="${1}"
	    ;;

	--ty|-ty)
	    shift || die "Missing argument to --ty"
	    TILE_Y="${1}"
	    ;;

	--ox|-ox)
	    shift || die "Missing argument to --ox"
	    OFF_X="${1}"
	    ;;

	--oy|-oy)
	    shift || die "Missing argument to --oy"
	    OFF_Y="${1}"
	    ;;

	--out|-out)
	    shift || die "Missing argument to --out"
	    OUT_DIR="${1}"
	    ;;

	--)
	    HAS_EMU_ARGS=1
	    ;;

	--help|-help)
	    echo "Usage: ${0} [--gdb] [--valgrind] [--clean-nvs] [--clean-logs] [--arrange [-w WIDTH] [-h HEIGHT] [--tx TILE_X] [--ty TILE_Y] [--ox OFFSET_X] [--oy OFFSET_Y] [--cols COLUMNS]] [--out OUT_DIR] [MODE_CSV] [ -- EMU_ARG ... ]"
	    exit 0
	    ;;

	*)
	    if [ "${HAS_EMU_ARGS}" -eq "1" ]; then
		EMU_ARGS="${EMU_ARGS} ${1}"
	    else
		SETTINGS_FILE="${1}"
	    fi
	    ;;
    esac
    
    shift
done

test -e "${SETTINGS_FILE}" || die "Cannot read file ${SETTINGS_FILE}"

readarray -t FUZZ_MODES < <( cut -d ';' -f 1 < "${SETTINGS_FILE}" | tail -n +2 )
readarray -t FUZZ_BTNS < <( cut -d ';' -f 2 < "${SETTINGS_FILE}" | tail -n +2 )
readarray -t FUZZ_ARGS < <( cut -d ';' -f 3 < "${SETTINGS_FILE}" | tail -n +2 )

mkdir -p "${OUT_DIR}/"{logs,nvs}

if [ "${CLEAN_LOGS}" -eq "1" ]; then
    find "${OUT_DIR/logs}" -iname "*.log" exec rm {} \;
fi

EMPTY="-empty"
if [ "${CLEAN_NVS}" -eq "1" ]; then
    EMPTY=""
fi

find "${OUT_DIR}/nvs" -iname '*.json' ${EMPTY} -exec rm {} \;

if [ "${ARRANGE}" -eq "1" ]; then
    OLD_WINDOWS=( $(xdotool search --name "${EMU_NAME}") )
fi

GDB_CMD="gdb -batch -ex 'run' -ex 'bt' --args"
VALGRIND_CMD="valgrind --leak-check=full --show-leak-kinds=all --log-file=\"${OUT_DIr}/logs/${MODE_CLEAN}_vg.log\""

set -x
for (( i = 0 ; i < ${#FUZZ_MODES[@]} ; i++ )); do
    MODE="${FUZZ_MODES[$i]}"
    MODE_CLEAN="$(echo "${MODE}" | tr -d '\n' | tr -c 'a-zA-Z0-9_' '_')"
    BUTTONS="$(echo ${FUZZ_BTNS[$i]} | tr -d '[:space:]')"
    EXTRA_ARGS="${EMU_ARGS} ${FUZZ_ARGS[$i]}"

    FUZZ_CMD="./swadge_emulator --fuzz --fuzz-buttons \"${BUTTONS}\" --lock --start-mode \"${MODE}\" --nvs-file \"${OUT_DIR}/nvs/${MODE_CLEAN}_${i}.json\" ${EXTRA_ARGS} > \"${OUT_DIR}/logs/${MODE_CLEAN}_${i}_stdout.log\" 2>\"${OUT_DIR}/logs/${MODE_CLEAN}_${i}_stderr.log\""

    if [ "${GDB}" -eq "1" ]; then
	FUZZ_CMD="${GDB_CMD} ${FUZZ_CMD}"
    fi

    if [ "${VALGRIND}" -eq "1" ]; then
	FUZZ_CMD="${VALGRIND_CMD} ${FUZZ_CMD}"
    fi

    eval ${FUZZ_CMD} &
done

if [ "${ARRANGE}" -eq "1" ]; then
    while true; do
	ALL_WINDOWS=( $(xdotool search --name "${EMU_NAME}") )
	# Set NEW_WINDOWS to all windows in ALL_WINDOWS but not OLD_WINDOWS
	NEW_WINDOWS=( $(comm -13 <(printf "%s\n" ${OLD_WINDOWS[@]} | sort) <(printf "%s\n" ${ALL_WINDOWS[@]} | sort)) )

	[[ "${#NEW_WINDOWS[@]}" -eq "${#FUZZ_MODES[@]}" ]] && break;
	sleep 1
    done

    I=$((${#NEW_WINDOWS[@]} - 1))
    for WINDOW_ID in "${NEW_WINDOWS[@]}"; do
	ROW=$(($I / $WIN_COLS))
	COL=$(($I % $WIN_COLS))
	X=$(($OFF_X + $TILE_X * $COL))
	Y=$(($OFF_Y + $TILE_Y * $ROW))

	xdotool windowactivate ${WINDOW_ID}
	xdotool windowsize ${WINDOW_ID} ${WIN_W} ${WIN_H}
	xdotool windowmove ${WINDOW_ID} ${X} ${Y}

	I=$(($I-1))
    done
fi

wait $(jobs -p)
