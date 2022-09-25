#!/bin/bash
#!/bin/bash
tmp=`mktemp -d /tmp/5cc-test-XXXXXX`
trap 'rm -rf $tmp' INT TERM HUP EXIT
echo > $tmp/empty.c

check() {
    if [ $? -eq 0 ]; then
        echo "testing $1 ... passed"
    else
        echo "testing $1 ... failed"
        exit 1
    fi
}

# check

# -o
rm -f $tmp/out
./5cc -o $tmp/out $tmp/empty.c
[ -f $tmp/out ]
check -o

# --help
./5cc --help 2>&1 | grep -q 5cc
check --help

echo OK