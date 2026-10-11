#!/usr/bin/env bash
# Exercises every endpoint of a RUNNING server with curl and checks the status code and the body.
# Needs only bash and curl (on Windows: Git Bash).
#
#   Terminal 1:  ./relief_server 8080          (from the repository root)
#   Terminal 2:  bash tests/server/curl_smoke.sh 8080
#
# It starts with POST /api/reset, so it wipes whatever the server is holding.

PORT=${1:-8080}
BASE="http://127.0.0.1:$PORT"
pass=0
fail=0

# t NAME METHOD PATH BODY EXPECTED_STATUS [TEXT THAT MUST APPEAR IN THE BODY]...
t() {
    local name=$1 method=$2 path=$3 body=$4 want=$5
    shift 5
    local out status ok=1
    if [ -n "$body" ]; then
        out=$(curl -s -m 10 -w $'\n%{http_code}' -X "$method" -H 'Content-Type: application/json' --data "$body" "$BASE$path")
    else
        out=$(curl -s -m 10 -w $'\n%{http_code}' -X "$method" "$BASE$path")
    fi
    status=${out##*$'\n'}
    out=${out%$'\n'*}
    [ "$status" = "$want" ] || ok=0
    local needle
    for needle in "$@"; do
        grep -qF -- "$needle" <<<"$out" || ok=0
    done
    if [ $ok = 1 ]; then
        pass=$((pass + 1))
        printf '  ok    %-52s %s\n' "$name" "$status"
    else
        fail=$((fail + 1))
        printf '  FAIL  %-52s got %s, wanted %s\n        body: %.300s\n' "$name" "$status" "$want" "$out"
    fi
}

echo "== state, reset, scenario 1 =="
t "reset"                      POST   /api/reset                  ""   200 '"ok":true' '"trace":"Reset."'
t "GET state"                  GET    /api/state                  ""   200 '"camps":[]' '"speedKmh":40.0' '"scenarios":["scenario1_normal"'
t "load scenario 1"            POST   /api/scenario/scenario1_normal ""  200 '"requestsQueued":6' '"avl":{' '"rootId":'
t "handle-next before process" POST   /api/handle-next            ""   409 '"error":"Empty"' '"camps":[{'
t "process incoming"           POST   /api/process                ""   200 '"moved":6' '"queue":[]'
t "ranked list has reasons"    GET    /api/state                  ""   200 '"reason":"arrival"' '"reasonText":"same urgency, families 40 > 10"'
t "handle next (request 3)"    POST   /api/handle-next            ""   200 '"delivered":20' '"totalMinutes":10' '"path":[0,1,3]'
t "search camp 6"              GET    /api/camps/6                ""   200 '"searchPath":[' '"name":"Northpoint"'
t "search camp 99"             GET    /api/camps/99               ""   404 '"error":"NotFound"'

echo "== camps =="
t "add camp with links"        POST   /api/camps '{"id":10,"name":"Delta","x":20,"y":0,"families":40,"links":[0,2]}' 200 '"roadsAdded":2'
t "duplicate id rejected"      POST   /api/camps '{"id":10,"name":"Again","x":1,"y":1,"families":1}'              409 '"error":"Duplicate"' '"message":"Camp id already exists"'
t "edit camp"                  PUT    /api/camps/10 '{"name":"Delta2","x":21,"y":0,"families":41}'                 200 '"name":"Delta2"'
t "edit unknown camp"          PUT    /api/camps/99 '{"name":"N","x":1,"y":1,"families":1}'                       404 '"error":"NotFound"'
t "bad type is a 400"          POST   /api/camps '{"id":"11","name":"X","x":1,"y":1,"families":1}'                400 '"error":"BadRequest"'
t "bad JSON is a 400"          POST   /api/camps '{"id":11,'                                                       400 '"error":"BadRequest"'
t "camp id too big"            POST   /api/camps '{"id":99999999999,"name":"X","x":1,"y":1,"families":1}'          400 '"error":"BadRequest"'

echo "== requests =="
t "submit request"             POST   /api/requests '{"campId":10,"resource":"water","quantity":30,"urgency":3,"families":12}' 200 '"requestId":7'
t "unknown camp"               POST   /api/requests '{"campId":55,"resource":"water","quantity":30,"urgency":3,"families":12}' 404 '"error":"NotFound"'
t "urgency out of range"       POST   /api/requests '{"campId":10,"resource":"water","quantity":30,"urgency":9,"families":12}'  400 '"error":"Invalid"'
t "unknown resource"           POST   /api/requests '{"campId":10,"resource":"gold","quantity":30,"urgency":3,"families":12}'   400 '"error":"BadRequest"'
t "process again"              POST   /api/process                ""   200 '"moved":1'
t "update active request"      PUT    /api/requests/7 '{"urgency":4,"families":12,"quantity":30}'                200 '"urgency":4'
t "update unknown request"     PUT    /api/requests/99 '{"urgency":4,"families":12,"quantity":30}'               404 '"error":"NotFound"'

echo "== roads, speed =="
t "block road 3-6"             PUT    /api/roads '{"a":3,"b":6,"status":"blocked"}'   200 '"trace":"Road 3-6 set to blocked | Dijkstra rerun from depot"'
t "bad road status"            PUT    /api/roads '{"a":3,"b":6,"status":"closed"}'    400 '"error":"BadRequest"'
t "duplicate road"             POST   /api/roads '{"a":1,"b":2}'                      409 '"error":"Duplicate"'
t "remove road (DELETE body)"  DELETE /api/roads '{"a":3,"b":6}'                      200 '"message":"Road removed"'
t "remove it again"            DELETE /api/roads '{"a":3,"b":6}'                      404 '"error":"NotFound"'
t "add road back"              POST   /api/roads '{"a":3,"b":6}'                      200 '"message":"Road added"'
t "set speed"                  POST   /api/settings '{"speedKmh":30}'                 200 '"speedKmh":30.0'
t "speed zero refused"         POST   /api/settings '{"speedKmh":0}'                  400 '"error":"Invalid"' '"speedKmh":30.0'

echo "== delete with confirmation =="
t "delete needs confirm"       DELETE /api/camps/10               ""   409 '"error":"ConfirmNeeded"' '"needsConfirm":true' '"pending":1'
t "delete with ?confirm=true"  DELETE '/api/camps/10?confirm=true' "" 200 '"cancelledRequests":1'
t "delete depot refused"       DELETE /api/camps/0                ""   400 '"message":"Cannot delete depot"'
t "delete non-number"          DELETE /api/camps/abc              ""   400 '"error":"BadRequest"'

echo "== scenarios 2 to 4 =="
t "scenario 2"                 POST   /api/scenario/scenario2_blockage ""  200 '"requestsQueued":1'
t "scenario 3"                 POST   /api/scenario/scenario3_priority ""  200 '"requestsQueued":3'
t "scenario 4 shows rotations" POST   /api/scenario/scenario4_avl ""       200 'AVL rotations: RR at node 10, RR at node 30, RL at node 20' '"rootId":30' '"type":"RL"'
t "unknown scenario"           POST   /api/scenario/nope          ""   404 '"error":"NotFound"'
t "scenario path traversal"    POST   '/api/scenario/..%2FREADME' ""   404 '"ok":false'

echo "== routing, methods, transport =="
t "unknown endpoint is JSON"   GET    /api/nothing               ""   404 '"error":"NotFound"'
t "wrong method"               GET    /api/reset                 ""   405 '"error":"MethodNotAllowed"'
t "PATCH not routed"           PATCH  /api/state                 ""   404 '"ok":false'
big=$(head -c 70000 /dev/zero | tr '\0' 'a')
t "body over 64 KB refused"    POST   /api/camps "{\"name\":\"$big\"}"  413 '"ok":false'

# Origin header: a browser sends it on cross-site calls; curl does not, so we add it by hand.
code=$(curl -s -o /dev/null -w '%{http_code}' -X POST -H 'Origin: http://evil.example' "$BASE/api/reset")
if [ "$code" = 403 ]; then pass=$((pass + 1)); printf '  ok    %-52s %s\n' "foreign Origin refused" "$code"
else fail=$((fail + 1)); printf '  FAIL  %-52s got %s, wanted 403\n' "foreign Origin refused" "$code"; fi
code=$(curl -s -o /dev/null -w '%{http_code}' -X POST -H "Origin: http://localhost:$PORT" "$BASE/api/process")
if [ "$code" = 409 ]; then pass=$((pass + 1)); printf '  ok    %-52s %s\n' "same-origin Origin allowed" "$code"
else fail=$((fail + 1)); printf '  FAIL  %-52s got %s, wanted 409\n' "same-origin Origin allowed" "$code"; fi

# ../ must never reach a file outside web/.
out=$(curl -s --path-as-is -m 10 "$BASE/../Makefile")
if grep -q 'CXXFLAGS' <<<"$out"; then fail=$((fail + 1)); echo "  FAIL  path traversal served the Makefile"
else pass=$((pass + 1)); printf '  ok    %-52s\n' "path traversal does not serve ../Makefile"; fi

# 40 requests at once: the lock must keep the Engine consistent. All 40 must be queued, none lost.
curl -s -o /dev/null -X POST "$BASE/api/scenario/scenario4_avl"
seq 1 40 | xargs -P 16 -I{} curl -s -o /dev/null -X POST -H 'Content-Type: application/json' \
    --data '{"campId":10,"resource":"food","quantity":1,"urgency":1,"families":1}' "$BASE/api/requests"
n=$(curl -s "$BASE/api/state" | grep -o '"arrivalSeq"' | wc -l)
if [ "$n" = 40 ]; then pass=$((pass + 1)); printf '  ok    %-52s %s\n' "40 parallel submits all queued" "$n"
else fail=$((fail + 1)); printf '  FAIL  %-52s got %s, wanted 40\n' "40 parallel submits all queued" "$n"; fi
ids=$(curl -s "$BASE/api/state" | grep -o '"arrivalSeq":[0-9]*' | sort -u | wc -l)
if [ "$ids" = 40 ]; then pass=$((pass + 1)); printf '  ok    %-52s %s\n' "arrival numbers are all different" "$ids"
else fail=$((fail + 1)); printf '  FAIL  %-52s got %s distinct, wanted 40\n' "arrival numbers are all different" "$ids"; fi

curl -s -o /dev/null -X POST "$BASE/api/reset"
echo
echo "$pass passed, $fail failed"
[ "$fail" = 0 ]
