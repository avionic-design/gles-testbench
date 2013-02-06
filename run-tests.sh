#!/bin/sh

SYSFS_CPU=/sys/devices/system/cpu

usage()
{
	echo "Usage: $1 [options] test-case"
	echo "Options:"
	echo "  --disable-vsync         Disable synchronization to VBLANK."
	echo "  --hdmi                  Run tests on HDMI output."
	echo "  --lvds                  Run tests on LVDS output."
	echo "  --performance           Run CPUs at maximum frequency."
	echo "  --regenerate            Regenerate test pattern for every frame."
	echo "  -s, --subdivisions NUM  Use NUM subdivisions for geometric adaption."
	echo "  --transform             Simulate geometric adaption."
	echo "  -h, --help              Display help screen and exit."
}

summarize()
{
	while read line; do
		echo "    $line"
	done
}

xserver_args=
disable_vsync=no
performance=no
regenerate=no
subdivs=
transform=no
depth=24
hdmi=no
lvds=no

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

		--hdmi)
			hdmi=yes
			shift
			;;

		-h | --help)
			usage $(basename $0)
			exit 0
			;;

		--lvds)
			lvds=yes
			shift
			;;

		--performance)
			performance=yes
			shift
			;;

		--regenerate)
			regenerate=yes
			shift
			;;

		-s | --subdivisions)
			prev=subdivs
			shift
			;;

		--transform)
			transform=yes
			shift
			;;

		*)
			usage $(basename $0)
			exit 0
			;;
	esac
done

if test "$hdmi" = "yes" -a "$lvds" = "yes"; then
	echo "Conflicting options --hdmi and --lvds"
	exit 1
fi

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

if test "$depth" = "16" -o "$depth" = "24"; then
	xserver_args="-depth $depth"
else
	xserver_args="-depth 24"
fi

test_args="--depth $depth"

if test "$regenerate" = "yes"; then
	test_args="$test_args --regenerate"
fi

if test -n "$subdivs"; then
	test_args="$test_args --subdivisions $subdivs"
fi

if test "$transform" = "yes"; then
	test_args="$test_args --transform"
fi

echo -n " Starting X server..."

/usr/bin/X $xserver_args > /dev/null 2>&1 &
X_PID=$!

while ! test -f /tmp/.X0-lock; do
	sleep 1
done

echo "done (PID:$X_PID)"

if test "$hdmi" = "yes"; then
	echo -n " Enabling HDMI..."
	xrandr --output LVDS-1 --off --output HDMI-1 --auto
	echo "done"
fi

if test "$lvds" = "yes"; then
	echo -n " Enabling LVDS..."
	xrandr --output LVDS-1 --auto --output HDMI-1 --off
	echo "done"
fi

echo "=============================================="
echo " Test 1: 1 to 1 Texture Copy"
./src/gles-standalone $test_args fill copy | summarize

echo "=============================================="
echo " Test 2: 1 to All Texture copy"
./src/gles-standalone $test_args fill copyone | summarize

echo "=============================================="
echo " Test 3: 3-Line Linear Blend"
./src/gles-standalone $test_args fill deinterlace | summarize

echo "=============================================="
echo " Test 4: GL Color Clearing (no shaders)"
./src/gles-standalone $test_args clear | summarize

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
