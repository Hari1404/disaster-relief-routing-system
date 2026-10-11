#!/usr/bin/env bash
# Regression test for BUG-7: how the console reads a line of input.
# Runs the real console binary with scripted input and checks what it prints.
# Needs only bash (on Windows: Git Bash). Run from the repository root:
#
#   make test-console                                   (builds ./relief_console first)
#   bash tests/console/console_input_test.sh ./relief_console.exe    (after build_console.bat)

BIN=${1:-./relief_console}
pass=0
fail=0

# check NAME INPUT MUST_APPEAR [MUST_NOT_APPEAR]   (INPUT is passed to printf, so \n and \r work)
check() {
    local name=$1 input=$2 want=$3 unwanted=$4 out
    out=$(printf "$input" | "$BIN" 2>&1)
    if grep -qF -- "$want" <<<"$out" && { [ -z "$unwanted" ] || ! grep -qF -- "$unwanted" <<<"$out"; }; then
        pass=$((pass + 1)); printf '  ok    %s\n' "$name"
    else
        fail=$((fail + 1)); printf '  FAIL  %s\n        wanted: %s   not wanted: %s\n' "$name" "$want" "$unwanted"
    fi
}

echo "== menu number =="
check "plain 14 still works"             '14\n0\n'        'Trace: Reset.'  'Not a valid number'
check "trailing space: '14 '"            '14 \n0\n'       'Trace: Reset.'  'Not a valid number'
check "leading space: ' 14'"             ' 14\n0\n'       'Trace: Reset.'  'Not a valid number'
check "trailing tab"                     '14\t\n0\n'      'Trace: Reset.'  'Not a valid number'
check "CRLF line endings"                '14\r\n0\r\n'    'Trace: Reset.'  'Not a valid number'
check "CRLF and a trailing space"        '14 \r\n0 \r\n'  'Trace: Reset.'  'Not a valid number'

echo "== numbers inside a menu item =="
check "speed '45 ' is accepted"          '12\n45 \n0\n'   'Speed updated'  'Not a valid number'
check "speed '45' with CRLF"             '12\r\n45\r\n0\r\n' 'Speed updated' 'Not a valid number'
check "decimal speed '37.5 '"            '12\n37.5 \n0\n' 'Speed updated'  'Not a valid number'

echo "== bad numbers are still refused =="
check "'14x' is not a number"            '14x\n0\n'       'Not a valid number'
check "'1 4' is not a number"            '1 4\n0\n'       'Not a valid number'
check "'abc' is not a number"            'abc\n0\n'       'Not a valid number'
check "huge number is refused"           '99999999999\n0\n' 'Not a valid number'
check "speed 'fast' is refused"          '12\nfast\n0\n'  'Not a valid number'

echo "== data-structures sub-menu =="
check "'d ' (trailing space) works"      '13\nd \n\n0\n'  'AVL'           'Invalid option'
check "'d' with CRLF works"              '13\r\nd\r\n\r\n0\r\n' 'AVL'     'Invalid option'
check "whitespace-only line leaves it"   '13\n   \n14\n0\n' 'Trace: Reset.' 'Invalid option'

echo "== input.txt (shipped sample input) =="
if [ -f input.txt ]; then
    out=$("$BIN" < input.txt 2>&1)
    if ! grep -qF 'Invalid option' <<<"$out" && ! grep -qF 'Invalid choice' <<<"$out" && ! grep -qF 'Not a valid number' <<<"$out"; then
        pass=$((pass + 1)); echo "  ok    input.txt runs to the end with no error"
    else
        fail=$((fail + 1)); echo "  FAIL  input.txt produced an error message"
    fi
fi

echo
echo "$pass passed, $fail failed"
[ "$fail" = 0 ]
