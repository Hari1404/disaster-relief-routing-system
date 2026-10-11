"use strict";
// ReliefNet page. It keeps no data of its own: every click is sent to the C++ engine, and the full
// state that comes back replaces S and is drawn again. ui holds view settings only (which tab is
// open, which camp is selected, where the user clicked), never camps, roads or requests.
let S = null;
const ui = { tab: "avl", selected: 0, path: [], draft: null, editing: 0 };
let frame = null;                      // how the map turns km into pixels, set each time it is drawn
const W = 800, H = 520;
const $ = id => document.getElementById(id);
const num = v => (v.trim() === "" ? null : Number(v));          // empty or bad input goes to the server as null and is refused there
const sign = n => (n > 0 ? "+" : "") + n;
const urgencyName = ["", "low", "medium", "high", "critical"];

// Builds a DOM element. Strings become text nodes, so a camp name can never inject HTML.
function mk(svg, tag, attrs, kids) {
  const e = svg ? document.createElementNS("http://www.w3.org/2000/svg", tag) : document.createElement(tag);
  for (const k in attrs || {}) {
    if (k.startsWith("on")) e.addEventListener(k.slice(2), attrs[k]); else e.setAttribute(k, attrs[k]);
  }
  for (const c of [].concat(kids === undefined ? [] : kids)) e.append(c == null ? "" : c);
  return e;
}
const h = (tag, attrs, kids) => mk(false, tag, attrs, kids);
const sv = (tag, attrs, kids) => mk(true, tag, attrs, kids);
const btn = (text, fn) => h("button", { type: "button", onclick: e => { e.stopPropagation(); fn(); } }, text);
const tbl = (head, rows) => h("table", {}, [h("tr", {}, head.map(t => h("th", {}, t)))].concat(rows.map(r => h("tr", {}, r.map(c => h("td", {}, c))))));
const empty = text => h("p", { class: "empty" }, text);
const reqText = r => "#" + r.id + ": camp " + r.campId + ", " + r.resource + " " + r.quantity + ", urgency " + r.urgency + " (" + urgencyName[r.urgency] + "), " + r.families + " families";
const pos = id => (id === 0 ? S.depot : S.camps.find(c => c.id === id));

// ---- talking to the engine -------------------------------------------------------------------
function say(text, bad) { $("msg").textContent = text; $("msg").className = bad ? "bad" : ""; }

async function api(method, path, body) {
  let j;
  try {
    const r = await fetch(path, { method, headers: { "Content-Type": "application/json" }, body: body === undefined ? undefined : JSON.stringify(body) });
    j = await r.json();
  } catch (e) {
    say("Cannot reach the server. Is relief_server still running?", true);
    return null;
  }
  if (j.state) { S = j.state; render(); }
  say(j.message, !j.ok);
  return j;
}
function act(method, path, body) { ui.path = []; return api(method, path, body); }   // an action that may change the engine

// Searches the AVL tree for a camp, selects it and remembers the path the search took.
async function pick(id, showTree) {
  const j = await api("GET", "/api/camps/" + id);
  if (j && j.ok) {
    ui.selected = id; ui.path = j.searchPath; $("rcamp").value = id;
    if (showTree) ui.tab = "avl";
    say("Camp " + id + " found. The AVL search visited " + j.searchPath.join(" → ") + ".");
  }
  render();
}

// ---- drawing ---------------------------------------------------------------------------------
function render() { if (!S) return; drawHeader(); drawMap(); drawCamps(); drawRequests(); drawTabs(); }

function options(sel, items) {         // refills a dropdown and keeps the current choice
  const keep = sel.value;
  sel.replaceChildren(...items.map(([v, t]) => h("option", { value: v }, t)));
  if (items.some(i => String(i[0]) === keep)) sel.value = keep;
}

function drawHeader() {
  options($("scenario"), S.scenarios.map(n => [n, n]));
  if (document.activeElement !== $("speed")) $("speed").value = S.speedKmh;
  $("stock").textContent = "Depot stock: food " + S.stock.food + ", water " + S.stock.water + ", medicine " + S.stock.medicine;
}

function routeTarget() {               // the camp whose route is highlighted
  const id = ui.selected || (S.active.length ? S.active[0].campId : 0);
  return S.camps.some(c => c.id === id) ? id : 0;
}

function drawMap() {
  const pts = [S.depot].concat(S.camps), xs = pts.map(p => p.x), ys = pts.map(p => p.y);
  const x0 = Math.min(...xs), x1 = Math.max(...xs), y0 = Math.min(...ys), y1 = Math.max(...ys);
  const k = Math.min((W - 120) / Math.max(x1 - x0, 20), (H - 120) / Math.max(y1 - y0, 20));
  frame = { mx: (x0 + x1) / 2, my: (y0 + y1) / 2, k };
  const px = x => W / 2 + (x - frame.mx) * k, py = y => H / 2 - (y - frame.my) * k;   // y points up on the map
  const g = [sv("rect", { id: "bg", x: 0, y: 0, width: W, height: H, onclick: placeCamp })];

  const next = { open: "slow", slow: "blocked", blocked: "open" };
  S.roads.forEach(r => {
    const a = pos(r.a), b = pos(r.b);
    const c = { x1: px(a.x), y1: py(a.y), x2: px(b.x), y2: py(b.y) };
    g.push(sv("line", Object.assign({ class: "road " + r.status }, c)));
    g.push(sv("line", Object.assign({ class: "hit", onclick: () => act("PUT", "/api/roads", { a: r.a, b: r.b, status: next[r.status] }) }, c),
      [sv("title", {}, "Road " + r.a + "-" + r.b + ", " + r.distanceKm.toFixed(1) + " km, " + r.status + ". Click to make it " + next[r.status] + ".")]));
    g.push(sv("text", { class: "rl", x: (c.x1 + c.x2) / 2, y: (c.y1 + c.y2) / 2 - 5 }, r.status === "blocked" ? "blocked" : r.minutes + " min"));
  });

  const target = routeTarget(), rt = S.routes.find(r => r.campId === target);
  if (rt && rt.reachable) g.push(sv("polyline", { class: "route", points: rt.path.map(id => px(pos(id).x) + "," + py(pos(id).y)).join(" ") }));
  $("routeNote").textContent = !rt ? "No route selected." : rt.reachable
    ? "Route to camp " + rt.campId + ": " + rt.path.join(" → ") + ", " + rt.minutes + " min" : "Camp " + rt.campId + ": " + rt.note;

  if (ui.draft) g.push(sv("circle", { class: "draft", cx: px(ui.draft.x), cy: py(ui.draft.y), r: 13 }));
  S.camps.forEach(c => g.push(sv("g", { class: "camp" + (c.id === target ? " sel" : ""), onclick: () => pick(c.id) }, [
    sv("title", {}, c.name + ": " + c.families + " families, " + c.pending + " pending request(s)"),
    sv("circle", { cx: px(c.x), cy: py(c.y), r: 12 }),
    sv("text", { class: "cid", x: px(c.x), y: py(c.y) + 4 }, c.id),
    sv("text", { class: "cname", x: px(c.x), y: py(c.y) + 26 }, c.name)])));
  g.push(sv("rect", { class: "depot", x: px(S.depot.x) - 10, y: py(S.depot.y) - 10, width: 20, height: 20 }, [sv("title", {}, "Depot (node 0)")]));
  g.push(sv("text", { class: "cname", x: px(S.depot.x), y: py(S.depot.y) + 26 }, "Depot"));
  $("map").replaceChildren(...g);
}

function nearestNode(x, y) {           // only used to suggest a road for a camp the user is about to add
  let best = 0, bd = Infinity;
  [{ id: 0, x: S.depot.x, y: S.depot.y }].concat(S.camps).forEach(p => { const d = (p.x - x) ** 2 + (p.y - y) ** 2; if (d < bd) { bd = d; best = p.id; } });
  return best;
}

function placeCamp(e) {                // a click on empty map: fill the add-camp form with that position
  const svg = $("map"), p = svg.createSVGPoint();
  p.x = e.clientX; p.y = e.clientY;
  const q = p.matrixTransform(svg.getScreenCTM().inverse());
  const x = Math.round((frame.mx + (q.x - W / 2) / frame.k) * 10) / 10, y = Math.round((frame.my - (q.y - H / 2) / frame.k) * 10) / 10;
  const id = 1 + Math.max(0, ...S.camps.map(c => c.id));
  ui.editing = 0; ui.draft = { x, y }; formMode();
  $("cid").value = id; $("cname").value = "Camp" + id; $("cx").value = x; $("cy").value = y; $("clinks").value = nearestNode(x, y);
  $("cname").select(); drawMap();
}

function drawCamps() {
  $("campTable").replaceChildren(...[h("tr", {}, ["ID", "Name", "Fam.", "Received F/W/M", "Pend.", ""].map(t => h("th", {}, t)))].concat(
    S.camps.map(c => h("tr", { class: c.id === routeTarget() ? "sel" : "", onclick: () => pick(c.id) }, [
      h("td", {}, c.id), h("td", {}, c.name), h("td", {}, c.families),
      h("td", {}, c.received.food + "/" + c.received.water + "/" + c.received.medicine), h("td", {}, c.pending),
      h("td", {}, [btn("Edit", () => editCamp(c)), btn("Delete", () => deleteCamp(c.id))])]))));
  if (!S.camps.length) $("campTable").append(h("caption", { class: "empty" }, "No camps. Load a scenario or click the map."));
}

function drawRequests() {
  options($("rcamp"), S.camps.map(c => [c.id, c.id + " " + c.name]));
  $("queue").replaceChildren(...(S.queue.length ? S.queue.map(r => h("li", {}, reqText(r))) : [h("li", { class: "empty" }, "Empty. Submit a request.")]));
  $("active").replaceChildren(...(S.active.length ? S.active.map((r, i) => h("li", {}, [
    h("div", {}, reqText(r)),
    h("div", { class: "why" }, r.reason ? "ranks above #" + S.active[i + 1].id + " because " + r.reasonText : "last in the ranking"),
    btn("Edit", () => editRequest(r))])) : [h("li", { class: "empty" }, "Empty. Click Process incoming.")]));
}

// ---- the bottom panel ------------------------------------------------------------------------
const TABS = [["queue", "Queue"], ["active", "Active list"], ["log", "Delivery log"], ["avl", "AVL tree"], ["dijkstra", "Dijkstra steps"]];

function chain(rows, first, last, none) {   // a linked list drawn as boxes and arrows
  if (!rows.length) return empty(none);
  const kids = [];
  rows.forEach((r, i) => {
    const tag = i === 0 && i === rows.length - 1 ? first + " and " + last : i === 0 ? first : i === rows.length - 1 ? last : "";
    kids.push(h("div", { class: "node" }, [h("b", {}, "#" + r.id), h("span", {}, "camp " + r.campId + ", " + r.resource + " " + r.quantity),
      h("small", {}, "urgency " + r.urgency + ", families " + r.families + ", arrival " + r.arrivalSeq), tag ? h("em", {}, tag) : ""]));
    kids.push(h("span", { class: "arrow" }, i === rows.length - 1 ? "→ null" : "→"));
  });
  return h("div", { class: "chain" }, kids);
}

function avlView() {
  const A = S.avl;
  if (!A.rootId) return empty("The tree is empty. Add a camp or load a scenario.");
  const by = {}, at = {};
  A.nodes.forEach(n => { by[n.id] = n; });
  let cnt = 0;                         // in-order position gives each node its column; depth gives its row
  (function walk(id, d) { if (!id) return; walk(by[id].leftId, d + 1); at[id] = { x: 30 + cnt++ * 60, y: 28 + d * 74 }; walk(by[id].rightId, d + 1); })(A.rootId, 0);
  const rotated = A.rotations.map(r => r.nodeId), g = [];
  A.nodes.forEach(n => [n.leftId, n.rightId].forEach(c => { if (c) g.push(sv("line", { class: "edge", x1: at[n.id].x, y1: at[n.id].y, x2: at[c].x, y2: at[c].y })); }));
  A.nodes.forEach(n => g.push(sv("g", { class: "avln" + (ui.path.includes(n.id) ? " path" : "") + (rotated.includes(n.id) ? " rot" : "") }, [
    sv("circle", { cx: at[n.id].x, cy: at[n.id].y, r: 18 }), sv("text", { class: "cid", x: at[n.id].x, y: at[n.id].y + 5 }, n.id),
    sv("text", { class: "hb", x: at[n.id].x, y: at[n.id].y + 33 }, "h" + n.height + " bf" + sign(n.balance))])));
  const w = cnt * 60, ht = A.height * 74 + 10;
  return h("div", {}, [h("p", {}, "Tree height " + A.height + ". h is a node's height, bf is its balance factor (left height minus right height). " +
    (A.rotations.length ? "Rotations in the last insert or delete: " + A.rotations.map(r => r.type + " at node " + r.nodeId).join(", ") + " (ringed in amber)." : "The last insert or delete needed no rotation.") +
    (ui.path.length ? " Search path: " + ui.path.join(" → ") + " (blue)." : "")),
    sv("svg", { width: w, height: ht, viewBox: "0 0 " + w + " " + ht, role: "img", "aria-label": "AVL tree of camp ids" }, g)]);
}

function dijkstraView() {
  const steps = S.dijkstra.steps.map((s, i) => [i + 1, s.node === 0 ? "0 (depot)" : s.node, s.minutes + " min", s.improved.length ? s.improved.join(", ") : "none"]);
  const routes = S.routes.map(r => ["camp " + r.campId, r.reachable ? r.minutes + " min" : r.note, r.reachable ? r.path.join(" → ") : ""]);
  return h("div", { class: "two" }, [
    h("div", {}, [h("h3", {}, "Nodes in the order Dijkstra settles them"), tbl(["Step", "Node", "Time", "Neighbours improved"], steps)]),
    h("div", {}, [h("h3", {}, "Routes from the depot"), routes.length ? tbl(["Camp", "Time", "Path"], routes) : empty("No camps yet.")])]);
}

const VIEWS = {
  queue: () => chain(S.queue, "front: dequeue here", "back: enqueue here", "The queue is empty."),
  active: () => chain(S.active, "head: top priority", "tail", "The active list is empty."),
  log: () => S.log.length ? tbl(["Request", "Camp", "Resource", "Delivered", "Minutes", "Route"], S.log.map(d =>
    ["#" + d.requestId, d.campId, d.resource, d.quantityDelivered + " of " + d.quantityRequested + (d.quantityDelivered < d.quantityRequested ? " (partial)" : ""), d.totalMinutes, d.path.join(" → ")])) : empty("No deliveries yet."),
  avl: avlView,
  dijkstra: dijkstraView
};

function drawTabs() {
  $("trace").textContent = "Operation trace: " + S.trace;
  $("tabs").replaceChildren(...TABS.map(([k, t]) => h("button", { type: "button", class: k === ui.tab ? "on" : "", onclick: () => { ui.tab = k; drawTabs(); } }, t)));
  $("tab").replaceChildren(VIEWS[ui.tab]());
}

// ---- forms and buttons -----------------------------------------------------------------------
function formMode() {                  // the camp form either adds a camp or edits the one in ui.editing
  const e = ui.editing;
  $("cid").disabled = !!e; $("clinks").disabled = !!e;
  $("cadd").textContent = e ? "Save changes" : "Add camp"; $("ccancel").hidden = !e;
  $("ctitle").textContent = e ? "Edit camp " + e : "Add camp";
}
function editCamp(c) {
  ui.editing = c.id; ui.draft = null; formMode();
  $("cid").value = c.id; $("cname").value = c.name; $("cx").value = c.x; $("cy").value = c.y; $("cfam").value = c.families; $("clinks").value = "";
  $("cname").focus(); drawMap();
}
async function deleteCamp(id) {
  let j = await act("DELETE", "/api/camps/" + id);
  if (j && j.error === "ConfirmNeeded" && confirm("Camp " + id + " has " + j.pending + " pending request(s). Delete the camp and cancel them?")) {
    j = await act("DELETE", "/api/camps/" + id + "?confirm=true");
  }
  if (j && j.ok && ui.selected === id) { ui.selected = 0; render(); }
}
async function editRequest(r) {
  const v = prompt("Urgency (1-4), families, quantity for request #" + r.id, [r.urgency, r.families, r.quantity].join(", "));
  if (v === null) return;
  const [urgency, families, quantity] = v.split(/[\s,]+/).filter(Boolean).map(Number);
  await act("PUT", "/api/requests/" + r.id, { urgency, families, quantity });
}

$("campForm").onsubmit = async e => {
  e.preventDefault();
  const body = { name: $("cname").value, x: num($("cx").value), y: num($("cy").value), families: num($("cfam").value) };
  const j = ui.editing ? await act("PUT", "/api/camps/" + ui.editing, body)
    : await act("POST", "/api/camps", Object.assign({ id: num($("cid").value), links: $("clinks").value.split(/[\s,]+/).filter(Boolean).map(Number) }, body));
  if (j && j.ok) { ui.editing = 0; ui.draft = null; e.target.reset(); formMode(); render(); }
};
$("ccancel").onclick = () => { ui.editing = 0; ui.draft = null; $("campForm").reset(); formMode(); drawMap(); };
$("findForm").onsubmit = e => { e.preventDefault(); pick(num($("find").value), true); };
$("reqForm").onsubmit = e => {
  e.preventDefault();
  act("POST", "/api/requests", { campId: num($("rcamp").value), resource: $("rres").value, quantity: num($("rqty").value), urgency: num($("rurg").value), families: num($("rfam").value) });
};
$("process").onclick = () => act("POST", "/api/process");
$("handle").onclick = async () => {
  const j = await act("POST", "/api/handle-next");
  if (j && j.ok && S.log.length) { ui.selected = S.log[S.log.length - 1].campId; render(); }   // show the route just used
};
$("load").onclick = () => { ui.selected = 0; ui.draft = null; act("POST", "/api/scenario/" + $("scenario").value); };
$("reset").onclick = () => { ui.selected = 0; ui.draft = null; act("POST", "/api/reset"); };
$("setSpeed").onclick = () => act("POST", "/api/settings", { speedKmh: num($("speed").value) });

formMode();
api("GET", "/api/state");
