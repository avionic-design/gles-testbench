#!/bin/sh

SYSFS_CPU=/sys/devices/system/cpu

usage()
{
	echo "Usage: $1 [options] test-case"
	echo "Options:"
	echo "  --disable-vsync  Disable synchronization to VBLANK."
	echo "  --performance    Run CPUs at maximum frequency."
	echo "  --regenerate     Regenerate test pattern for every frame."
	echo "  -h, --help       Display help screen and exit."
}

xserver_args=
disable_vsync=no
performance=no
regenerate=no
depth=24

while test $# -gt 0; do
	if test -n "$prev"; then
		eval $prev=$1
		prev=; shift
		continue
	fi

	case $1 in
		--depth)
			prev=depth
			shift
			;;

		--disable-vsync)
			disable_vsync=yes
			shift
			;;

		-h | --help)
			usage $(basename $0)
			exit 0
			;;

		--performance)
			performance=yes
			shift
			;;

		--regenerate)
			regenerate=yes
			shift
			;;

		*)
			usage $(basename $0)
			exit 0
			;;
	esac
done

export LD_LIBRARY_PATH=/usr/lib

if test -z "$DISPLAY"; then
	export DISPLAY=:0
fi

if test -f /tmp/.X0-lock; then
	echo "An X server is already running on display :0, aborting..."
	exit 1
fi

echo "=============================================="
echo " Avionic Design GL Pipeline Benchmark"
echo "=============================================="

if test "$disable_vsync" = "yes"; then
	if test -f /sys/module/window/parameters/no_vsync; then
		echo -n " Disabling synchronization to VBLANK..."
		echo 1 | sudo tee /sys/module/window/parameters/no_vsync > /dev/null
		echo "done"
	fi
fi

if test "$performance" = "yes"; then
	echo -n " Running CPU at maximum frequency..."
	echo performance | sudo tee $SYSFS_CPU/cpu0/cpufreq/scaling_governor > /dev/null
	echo "done"
fi

echo " Using depth: $depth"
xserver_args="-depth $depth"
test_args="--depth $depth"

if test "$regenerate" = "yes"; then
	test_args="$test_args --regenerate"
fi

echo -n " Starting X server..."

/usr/bin/X $xserver_args > /dev/null 2>&1 &
X_PID=$!

while ! test -f /tmp/.X0-lock; do
	sleep 1
done

echo "done (PID:$X_PID)"

echo "=============================================="
echo " Test 1: 1 to 1 Texture copy"
./src/gles-standalone $test_args copy

echo "=============================================="
echo " Test 2: 1 to All Texture copy"
./src/gles-standalone $test_args one_source

echo "=============================================="
echo " Test 3: 3-line Linear blend"
./src/gles-standalone $test_args deinterlace

echo "=============================================="
echo " Test 4: GL Blanking (no shaders)"
./src/gles-standalone $test_args blank

echo "=============================================="

echo -n " Stopping X server..."
kill $X_PID
echo "done"

if test "$performance" = "yes"; then
	echo -n " Restoring CPU frequency scaling..."
	echo ondemand | sudo tee $SYSFS_CPU/cpu0/cpufreq/scaling_governor > /dev/null
	echo "done"
fi

if test "$disable_vsync" = "yes"; then
	if test -f /sys/module/window/parameters/no_vsync; then
		echo -n " Enabling synchronization to VBLANK..."
		echo 0 | sudo tee /sys/module/window/parameters/no_vsync > /dev/null
		echo "done"
	fi
fi

echo "=============================================="
