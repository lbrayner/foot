#!/bin/sh

set -eu

usage_and_die() {
    echo "Usage: ${0} none|partial|full-current-session|full-headless-sway|full-headless-cage|[auto] <source-dir> <build-dir> [meson options]"
    exit 1
}

[ ${#} -ge 3 ] || usage_and_die

mode=${1}
srcdir=$(realpath "${2}")
blddir=$(realpath "${3}")
shift 3

# if [ -e "${blddir}" ]; then
#     echo "error: ${blddir}: build directory already exists"
#     exit 1
# fi

if [ ! -f "${srcdir}"/generate-version.sh ]; then
   echo "error: ${srcdir}: does not appear to be a foot source directory"
   exit 1
fi

compiler=other
do_pgo=no

CFLAGS="${CFLAGS-} -O3"

case $(${CC-cc} --version) in
    *Free\ Software\ Foundation*)
        compiler=gcc
        do_pgo=yes
        ;;

    *clang*)
        compiler=clang

        if command -v llvm-profdata > /dev/null; then
            do_pgo=yes
            CFLAGS="${CFLAGS} -Wno-ignored-optimization-argument"
        fi
        ;;
esac

case ${mode} in
    partial|full-current-session|full-headless-sway|full-headless-cage)
    ;;

    none)
        do_pgo=no
        ;;

    auto)
        if [ -n "${WAYLAND_DISPLAY+x}" ]; then
            mode=full-current-session
        elif command -v sway > /dev/null; then
            mode=full-headless-sway
        elif command -v cage > /dev/null; then
            mode=full-headless-cage
        else
            mode=partial
        fi
        ;;

    *)
        usage_and_die
        ;;
esac

set -x

# echo "source: ${srcdir}"
# echo "build: ${blddir}"
# echo "compiler: ${compiler}"
# echo "mode: ${mode}"
# echo "CFLAGS: ${CFLAGS}"

export CFLAGS
export CCACHE_DISABLE=1
meson setup --buildtype=release -Db_lto=true "${@}" "${blddir}" "${srcdir}"

if [ ${do_pgo} = yes ]; then
    find "${blddir}" \
         '(' \
           -name "*.gcda" -o \
           -name "*.profraw" -o \
           -name default.profdata \
         ')' \
         -delete

    meson configure "${blddir}" -Db_pgo=generate
    ninja -C "${blddir}"

    # If fcft/tllist are subprojects, we need to ensure their tests
    # have been executed, or we'll get "profile count data file not
    # found" errors.
    ninja -C "${blddir}" test

    # Run mode-dependent script to generate profiling data
    export LLVM_PROFILE_FILE="${blddir}/default_%m.profraw"
    "${srcdir}"/pgo/${mode}.sh "${srcdir}" "${blddir}"

    if [ ${compiler} = clang ]; then
        llvm-profdata \
            merge \
            "${blddir}"/default_*.profraw \
            --output="${blddir}"/default.profdata
    fi

    meson configure "${blddir}" -Db_pgo=use
fi

ninja -C "${blddir}"
