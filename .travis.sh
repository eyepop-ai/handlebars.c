#!/usr/bin/env bash

# this might break stuff in travis. most of the work is run in subshells so
# they should be safe
#set -e -o pipefail

export CC="$MYCC"
export PREFIX="$HOME/build"
export PATH="$PREFIX/bin:$PATH"
export CFLAGS="-I$PREFIX/include -I$PREFIX/include/json-c"
export LDFLAGS="-L$PREFIX/lib $LDFLAGS"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:/usr/lib/$ARCH-linux-gnu/pkgconfig"
export LD_LIBRARY_PATH="$PREFIX/lib:$LD_LIBRARY_PATH"
export SUDO="sudo"

# these mess up our configuration for LTO, make sure they are unset
unset RANLIB
unset AR
unset NM
unset LD

mkdir -p "${PREFIX}/include" "${PREFIX}/include/json-c" "${PREFIX}/lib/pkgconfig"

if [ -z "$ARCH" ]; then
	export ARCH=${TRAVIS_CPU_ARCH}
fi

if [ "$ARCH" = "i386" ]; then
	export BUILD_ARCH=$ARCH
	export CFLAGS="$CFLAGS -m32"
	#export LDFLAGS="$LDFLAGS -m32"
fi

function install_apt_packages() (
	set -e -o pipefail

	# we only need ubuntu-toolchain-r/test for gcc-10 now
	if [ "$MYCC" = "gcc-10" ]; then
		$SUDO apt-add-repository -y ppa:ubuntu-toolchain-r/test
	fi

	$SUDO apt-get update -y

	# we commit the generated files for these now, purge them
	$SUDO apt-get purge -y bison flex gperf re2c valgrind

	local apt_packages_to_install="${MYCC} autotools-dev autoconf automake libtool m4 make bats pkg-config:${ARCH} check:${ARCH} libpcre3-dev:${ARCH} libtalloc-dev:${ARCH} libsubunit-dev:${ARCH}"
	if [ "$ARCH" = "i386" ]; then
		apt_packages_to_install="${apt_packages_to_install} gcc-multilib"
	fi
	if [ ! -z "$GCOV" ]; then
		apt_packages_to_install="${apt_packages_to_install} lcov"
	fi
	if [ "$MINIMAL" != "true" ]; then
		# json-c might be having issues: https://bugs.launchpad.net/ubuntu/+source/json-c/+bug/1878738
		apt_packages_to_install="${apt_packages_to_install} libjson-c-dev:${ARCH} liblmdb-dev:${ARCH} libyaml-dev:${ARCH}"
	fi
	if [ "$VALGRIND" == "true" ]; then
		apt_packages_to_install="${apt_packages_to_install} valgrind"
	fi

	$SUDO apt-get install -y ${apt_packages_to_install}
)

function install_coveralls_lcov() (
	set -e -o pipefail

	if [ ! -z "$GCOV" ]; then
		gem install coveralls-lcov
	fi
)

function configure_handlebars() {
	set -e -o pipefail

	# cflags
	export CFLAGS="$CFLAGS -g -O2"

	# json-c undeprecated json_object_object_get, but the version in xenial
	# is too old, so let's silence deprecated warnings. le sigh.
	export CFLAGS="$CFLAGS -Wno-deprecated-declarations -Wno-error=deprecated-declarations"

	# configure flags
	local extra_configure_flags="--prefix=${PREFIX} --enable-benchmark"

	if [ -n "$BUILD_ARCH" ]; then
		extra_configure_flags="${extra_configure_flags} --build=${BUILD_ARCH}"
	fi

	if [ ! -z "$GCOV" ]; then
		extra_configure_flags="$extra_configure_flags --enable-code-coverage --with-gcov=$GCOV"
	fi

	if [ "$DEBUG" == "true" ]; then
		extra_configure_flags="$extra_configure_flags --enable-debug"
	else
		extra_configure_flags="$extra_configure_flags --disable-debug"
	fi

	if [ "$HARDENING" != "false" ]; then
		extra_configure_flags="$extra_configure_flags --enable-hardening"
	else
		extra_configure_flags="$extra_configure_flags --disable-hardening"
	fi

	if [ "$LTO" = "true" ]; then
		extra_configure_flags="${extra_configure_flags} --enable-lto --disable-shared"
		# seems to break on Travis with: Error: no such instruction: `endbr64'
		export CFLAGS="$CFLAGS -fcf-protection=none"
	else
		extra_configure_flags="${extra_configure_flags} --disable-lto"
	fi

	if [ "$MINIMAL" = "true" ]; then
		extra_configure_flags="${extra_configure_flags} --disable-testing-exports --disable-handlebars-memory --enable-check --disable-json --disable-lmdb --enable-pcre --disable-pthread --enable-subunit --disable-yaml"
	else
		extra_configure_flags="${extra_configure_flags} --enable-testing-exports --enable-handlebars-memory --enable-check --enable-json --enable-lmdb --enable-pcre  --enable-pthread --enable-subunit --enable-yaml"
	fi

	if [ "$VALGRIND" = "true" ]; then
		extra_configure_flags="${extra_configure_flags} --enable-valgrind"
	else
		extra_configure_flags="${extra_configure_flags} --disable-valgrind"
	fi

	autoreconf -v

	echo "Configuring with flags: ${extra_configure_flags}"
	trap "cat config.log" ERR
	./configure ${extra_configure_flags}
	trap - ERR
}

function make_handlebars() (
	set -e -o pipefail

	make clean all
)

function install_handlebars() (
	set -e -o pipefail

	make install
)

function test_handlebars() (
	set -e -o pipefail

	trap "dump_logs" ERR

	if [ "$VALGRIND" == "true" ]; then
		make check-valgrind
	elif [ ! -z "$GCOV" ]; then
		make check-code-coverage
	else
		make check
	fi

	echo "Printing benchmark results"
	cat ./bench/run.sh.log
)

function upload_coverage() (
	set -e -o pipefail

	if [ ! -z "$GCOV" ]; then
		coveralls-lcov handlebars-coverage.info
	fi
)

function dump_logs() (
	set -e -o pipefail

	for i in `find bench tests -name "*.log" 2>/dev/null`; do
		echo "-- START ${i}";
		cat "${i}";
		echo "-- END";
	done
)
