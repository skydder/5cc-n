#!/bin/bash
assert() {
  expected="$1"
  input="$2"

  ./5cc "$input" > tmp.s
  cc -o tmp tmp.s
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

assert 0 0
assert 42 42
assert 16 '5 * 4 / 4 * 3 + 1'
assert 21 '(4 + 3) * 3'

echo OK