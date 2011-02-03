#! /bin/sh

python=''

[ $# -lt 2 ] && {
    echo "$0 VERSION SCRIPT [ARGUMENTS]"
    exit 1
}

required="$1"
shift

for name in python python2 python2.6 python2.7 python2.8 python2.9; do
    which $name > /dev/null && {
        $name -c "
import platform
major = int(platform.python_version_tuple()[0])
minor = int(platform.python_version_tuple()[1])
try:
    required = [int(s) for s in '$required'.split('.')]
except ValueError:
    import sys
    sys.stderr.write('$required is not a valid version number\n')
    required = (0, 0)
if major != required[0] or minor < required[1]: raise SystemExit(1)" && {
            python=$name
            break
        }
    }
done

[ -z "$python" ] && {
    echo "Cannot find a Python interpreter to run $1" >&2
    exit 1
}

$python "$@"
