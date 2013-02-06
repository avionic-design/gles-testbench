#!/bin/sh

dir="$(dirname $0)"

for output in hdmi lvds; do
	output_args="--$output --subdivisions 5 --disable-vsync"

	for regenerate in "" regenerate; do
		if test -n "$regenerate"; then
			regen_args="--$regenerate"
		else
			regen_args=""
		fi

		for governor in "" performance; do
			if test -n "$governor"; then
				gov_args="--$governor"
			else
				gov_args=""
			fi

			for transform in "" transform; do
				if test -n "$transform"; then
					transform_args="--$transform"
				else
					transform_args=""
				fi

				for depth in 16 24; do
					depth_args="--depth $depth"

					args="$output_args $regen_args"
					args="$args $gov_args $transform_args"
					args="$args $depth_args"

					echo "running" run-tests.sh $args
					$SHELL "$dir/run-tests.sh" $args
				done
			done
		done
	done
done
