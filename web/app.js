"use strict";
// ReliefNet page. It keeps no data of its own: every click is sent to the C++ engine, and the full
// state that comes back replaces S and is drawn again. ui holds view settings only (which tab is
// open, which camp is selected, where the user clicked, which form is open), never camps, roads
// or requests.
let S = null;
const ui = { tab: "avl", selected: 0, path: [], draft: null, editing: 0, pane: "list", confirm: null, editReq: null };
let frame = null;                      // how the map turns km into pixels, set each time it is drawn
let W = 800, H = 520;                  // map size in pixels, read from the page each time it is drawn
const $ = id => document.getElementById(id);
const num = v => (v.trim() === "" ? null : Number(v));          // empty or bad input goes to the server as null and is refused there
const sign = n => (n > 0 ? "+" : "") + n;
const urgencyName = ["", "low", "medium", "high", "critical"];
const enterKey = (e, fn) => { if (e.target === e.currentTarget && (e.key === "Enter" || e.key === " ")) { e.preventDefault(); fn(); } };

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
// A button inside a redrawn area. fk is a key so keyboard focus can be put back after the redraw.
const btn = (text, fn, fk, cls) => h("button", Object.assign({ type: "button", onclick: e => { e.stopPropagation(); fn(); } },
  fk ? { "data-fk": fk } : {}, cls ? { class: cls } : {}), text);
const tbl = (head, rows) => h("table", {}, [h("tr", {}, head.map(t => h("th", {}, t)))].concat(rows.map(r => h("tr", {}, r.map(c => h("td", {}, c))))));
const empty = text => h("p", { class: "empty" }, text);
const pos = id => (id === 0 ? S.depot : S.camps.find(c => c.id === id));

// ---- talking to the engine -------------------------------------------------------------------
function say(text, bad) { $("msg").textContent = text; $("msg").className = bad ? "bad" : ""; }

// Calls run one after another, so two quick clicks can never overlap and a late reply can never
// draw over a newer one.
let line = Promise.resolve();
function api(method, path, body) {
  const run = async () => {
    let r, j;
    try {
      r = await fetch(path, { method, headers: { "Content-Type": "application/json" }, body: body === undefined ? undefined : JSON.stringify(body) });
    } catch (e) {
      say("Cannot reach the server. Is relief_server still running?", true);
      return null;
    }
    try { j = await r.json(); } catch (e) { say("The server sent a reply the page cannot read (HTTP " + r.status + ").", true); return null; }
    if (j.state) { S = j.state; render(); }
    say(j.message, !j.ok);
    return j;
  };
  line = line.then(run);
  return line;
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
function render() {
  if (!S) return;
  const a = document.activeElement, fk = a && a.dataset ? a.dataset.fk : null;   // redrawing replaces buttons, so remember which had focus
  drawHeader(); drawMap(); drawPane(); drawCamps(); drawRequests(); drawTabs();
  if (fk) { const el = document.querySelector('[data-fk="' + fk + '"]'); if (el) el.focus({ preventScroll: true }); }
}

function options(sel, items) {         // refills a dropdown and keeps the current choice
  const keep = sel.value;
  sel.replaceChildren(...items.map(([v, t]) => h("option", { value: v }, t)));
  if (items.some(i => String(i[0]) === keep)) sel.value = keep;
}

function drawHeader() {
  options($("scenario"), S.scenarios.map(n => [n, n]));
  if (document.activeElement !== $("speed")) $("speed").value = S.speedKmh;
  $("stock").replaceChildren(h("span", { class: "lbl" }, "Depot stock"), ...["food", "water", "medicine"].map(k =>
    h("span", { class: "pill" + (S.stock[k] === 0 ? " zero" : "") }, [h("b", {}, S.stock[k]), " " + k])));
}

function routeTarget() {               // the camp whose route is highlighted
  const id = ui.selected || (S.active.length ? S.active[0].campId : 0);
  return S.camps.some(c => c.id === id) ? id : 0;
}

function drawMap() {
  const wrap = $("mapwrap");
  W = Math.max(320, wrap.clientWidth); H = Math.max(200, wrap.clientHeight);
  $("map").setAttribute("viewBox", "0 0 " + W + " " + H);
  const pts = [S.depot].concat(S.camps), xs = pts.map(p => p.x), ys = pts.map(p => p.y);
  const x0 = Math.min(...xs), x1 = Math.max(...xs), y0 = Math.min(...ys), y1 = Math.max(...ys);
  const k = Math.min((W - 150) / Math.max(x1 - x0, 1), (H - 150) / Math.max(y1 - y0, 1), 60);   // px per km, capped so a one-camp map is not blown up
  frame = { mx: (x0 + x1) / 2, my: (y0 + y1) / 2, k };
  const px = x => W / 2 + (x - frame.mx) * k, py = y => H / 2 - (y - frame.my) * k;   // y points up on the map
  const g = [sv("rect", { id: "bg", x: 0, y: 0, width: W, height: H, onclick: placeCamp })];

  const next = { open: "slow", slow: "blocked", blocked: "open" };
  const rf = $("rfrom").value, rt2 = $("rto").value;
  // Places that a road label should not cover: every node, every camp name, and the labels already placed.
  const boxes = [{ x: px(S.depot.x) - 14, y: py(S.depot.y) - 14, w: 28, h: 46 }];
  S.camps.forEach(c => boxes.push({ x: px(c.x) - Math.max(14, c.name.length * 3.4 + 2), y: py(c.y) - 24, w: Math.max(28, c.name.length * 6.8 + 4), h: 56 }));
  const hits = (b, o) => b.x < o.x + o.w && o.x < b.x + b.w && b.y < o.y + o.h && o.y < b.y + b.h;
  const labels = [];
  S.roads.forEach(r => {
    const a = pos(r.a), b = pos(r.b);
    const c = { x1: px(a.x), y1: py(a.y), x2: px(b.x), y2: py(b.y) };
    const cycle = () => { $("rfrom").value = r.a; $("rto").value = r.b; act("PUT", "/api/roads", { a: r.a, b: r.b, status: next[r.status] }); };
    const label = "Road " + r.a + " to " + r.b + ", " + r.distanceKm.toFixed(1) + " km, " + r.status + ". Click to make it " + next[r.status] + ".";
    if (String(r.a) === rf && String(r.b) === rt2 || String(r.b) === rf && String(r.a) === rt2) g.push(sv("line", Object.assign({ class: "rsel" }, c)));
    g.push(sv("line", Object.assign({ class: "road " + r.status }, c)));
    g.push(sv("line", Object.assign({ class: "hit", tabindex: "0", role: "button", "aria-label": label, "data-fk": "road-" + r.a + "-" + r.b, onclick: cycle, onkeydown: e => enterKey(e, cycle) }, c),
      [sv("title", {}, label)]));
    const text = r.status === "blocked" ? "blocked" : r.minutes + " min", w = text.length * 6 + 8;
    let at = { x: (c.x1 + c.x2) / 2, y: (c.y1 + c.y2) / 2 - 5 };
    for (const t of [.5, .42, .58, .34, .66, .26, .74]) {            // the middle of the road if free, else the nearest free spot
      const cand = { x: c.x1 + (c.x2 - c.x1) * t, y: c.y1 + (c.y2 - c.y1) * t - 5 };
      if (!boxes.some(o => hits({ x: cand.x - w / 2, y: cand.y - 12, w, h: 14 }, o))) { at = cand; break; }
    }
    boxes.push({ x: at.x - w / 2, y: at.y - 12, w, h: 14 });
    labels.push(sv("text", { class: "rl", x: at.x, y: at.y }, text));
  });
  g.push(...labels);

  const target = routeTarget(), rt = S.routes.find(r => r.campId === target);
  if (rt && rt.reachable) g.push(sv("polyline", { class: "route", points: rt.path.map(id => px(pos(id).x) + "," + py(pos(id).y)).join(" ") }));
  $("routeNote").textContent = !rt ? "No route selected." : rt.reachable
    ? "Route to camp " + rt.campId + ": " + rt.path.join(" → ") + ", " + rt.minutes + " min" : "Camp " + rt.campId + ": " + rt.note;
  $("routeNote").className = rt && !rt.reachable ? "bad" : "";

  if (ui.draft) g.push(sv("circle", { class: "draft", cx: px(ui.draft.x), cy: py(ui.draft.y), r: 14 }));
  S.camps.forEach(c => {
    const open = () => pick(c.id), cx = px(c.x), cy = py(c.y);
    g.push(sv("g", { class: "camp" + (c.id === target ? " sel" : ""), tabindex: "0", role: "button", "data-fk": "camp-" + c.id,
      "aria-label": "Camp " + c.id + " " + c.name + ", " + c.pending + " pending requests. Select to show its route.", onclick: open, onkeydown: e => enterKey(e, open) }, [
      sv("title", {}, c.name + ": " + c.families + " families, " + c.pending + " pending request(s)"),
      sv("circle", { cx, cy, r: 12 }),
      sv("text", { class: "cid", x: cx, y: cy + 4 }, c.id),
      sv("text", { class: "cname", x: cx, y: cy + 27 }, c.name)].concat(c.pending ? [
      sv("circle", { class: "pendDot", cx: cx + 12, cy: cy - 12, r: 8 }), sv("text", { class: "pendNum", x: cx + 12, y: cy - 9 }, c.pending)] : [])));
  });
  g.push(sv("rect", { class: "depot", x: px(S.depot.x) - 10, y: py(S.depot.y) - 10, width: 20, height: 20 }, [sv("title", {}, "Depot (node 0)")]));
  g.push(sv("text", { class: "cname", x: px(S.depot.x), y: py(S.depot.y) + 27 }, "Depot"));
  $("map").replaceChildren(...g);
  options($("rfrom"), [[0, "0 Depot"]].concat(S.camps.map(c => [c.id, c.id + " " + c.name])));
  options($("rto"), [[0, "0 Depot"]].concat(S.camps.map(c => [c.id, c.id + " " + c.name])));
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
  ui.editing = 0; ui.draft = { x, y }; ui.pane = "form"; ui.confirm = null; formMode(); drawPane();
  $("cid").value = id; $("cname").value = "Camp" + id; $("cx").value = x; $("cy").value = y; $("clinks").value = nearestNode(x, y);
  $("cname").select(); drawMap(); drawCamps();
}

// The left panel shows either the camp list or the camp form.
function drawPane() {
  const form = ui.pane === "form";
  $("paneList").hidden = form; $("paneForm").hidden = !form;
  $("segList").classList.toggle("on", !form); $("segForm").classList.toggle("on", form);
  $("segList").setAttribute("aria-selected", String(!form)); $("segForm").setAttribute("aria-selected", String(form));
  $("segForm").textContent = ui.editing ? "Edit camp " + ui.editing : "Add camp";
}

function drawCamps() {
  $("campCount").textContent = S.camps.length;
  if (ui.confirm && !S.camps.some(c => c.id === ui.confirm.id)) ui.confirm = null;
  const cb = $("confirmBar"), cf = ui.confirm;
  cb.hidden = !cf;
  cb.replaceChildren(...(cf ? [h("span", {}, "Camp " + cf.id + " has " + cf.pending + " pending request(s). Delete it and cancel them?"),
    h("div", { class: "acts" }, [btn("Delete and cancel requests", () => confirmDelete(cf.id), "cdel-yes", "danger"), btn("Keep camp", () => { ui.confirm = null; drawCamps(); }, "cdel-no")])] : []));
  $("campList").replaceChildren(...(S.camps.length ? S.camps.map(c => {
    const open = () => pick(c.id);
    return h("li", { class: "card" + (c.id === routeTarget() ? " sel" : ""), tabindex: "0", "data-fk": "card-" + c.id, onclick: open, onkeydown: e => enterKey(e, open) }, [
      h("div", { class: "top" }, [h("b", {}, c.id + " " + c.name), c.pending ? h("span", { class: "badge pend", title: "Requests waiting in the queue or the active list" }, c.pending + " pending") : ""]),
      h("div", { class: "meta", title: "Families in the camp. Received: F = food, W = water, M = medicine" },
        c.families + " families · received F " + c.received.food + " · W " + c.received.water + " · M " + c.received.medicine),
      h("div", { class: "acts" }, [btn("Edit", () => editCamp(c), "edit-" + c.id), btn("Delete", () => deleteCamp(c.id), "del-" + c.id)])]);
  }) : [h("li", { class: "empty" }, "No camps. Load a scenario or click the map.")]));
}

const reqLine = r => h("div", { class: "meta" }, [r.resource + " " + r.quantity + ". ", h("span", { class: "badge u" + r.urgency }, "urgency " + r.urgency + " " + urgencyName[r.urgency]), " " + r.families + " families"]);

function reqEditor(r) {                // the inline form that replaces a row's Edit button
  const e = ui.editReq;
  const field = (label, key) => h("label", {}, [label, h("input", { type: "number", step: "1", min: "1", value: e[key], "data-fk": "re-" + key, oninput: ev => { e[key] = ev.target.value; } })]);
  return h("form", { class: "inl", onsubmit: ev => { ev.preventDefault(); saveRequest(); } }, [
    field("Urgency (1-4)", "urgency"), field("Families", "families"), field("Quantity", "quantity"),
    h("div", { class: "acts" }, [h("button", { class: "primary", "data-fk": "re-save" }, "Save"), btn("Cancel", () => { ui.editReq = null; drawRequests(); }, "re-cancel")])]);
}

function drawRequests() {
  options($("rcamp"), S.camps.map(c => [c.id, c.id + " " + c.name]));
  if (ui.editReq && !S.active.some(r => r.id === ui.editReq.id)) ui.editReq = null;
  $("queue").replaceChildren(...(S.queue.length ? S.queue.map(r => h("li", { class: "card" }, [
    h("div", { class: "top" }, [h("b", {}, "#" + r.id + " camp " + r.campId)]), reqLine(r)])) : [h("li", { class: "empty" }, "Empty. Submit a request.")]));
  $("active").replaceChildren(...(S.active.length ? S.active.map((r, i) => {
    const editing = ui.editReq && ui.editReq.id === r.id;
    return h("li", { class: "card" }, [
      h("div", { class: "top" }, [h("b", {}, r.rank + ". #" + r.id + " camp " + r.campId), editing ? "" : btn("Edit", () => editRequest(r), "ereq-" + r.id)]),
      reqLine(r),
      h("div", { class: "why" }, r.reason ? "Ranks above #" + S.active[i + 1].id + " because " + r.reasonText : "Last in the ranking."),
      editing ? reqEditor(r) : ""]);
  }) : [h("li", { class: "empty" }, "Empty. Click Process incoming.")]));
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
  (function walk(id, d) { if (!id) return; walk(by[id].leftId, d + 1); at[id] = { x: 30 + cnt++ * 60, y: 24 + d * 62 }; walk(by[id].rightId, d + 1); })(A.rootId, 0);
  const rotated = A.rotations.map(r => r.nodeId), g = [];
  A.nodes.forEach(n => [n.leftId, n.rightId].forEach(c => { if (c) g.push(sv("line", { class: "edge", x1: at[n.id].x, y1: at[n.id].y, x2: at[c].x, y2: at[c].y })); }));
  A.nodes.forEach(n => g.push(sv("g", { class: "avln" + (ui.path.includes(n.id) ? " path" : "") + (rotated.includes(n.id) ? " rot" : "") }, [
    sv("circle", { cx: at[n.id].x, cy: at[n.id].y, r: 18 }), sv("text", { class: "cid", x: at[n.id].x, y: at[n.id].y + 5 }, n.id),
    sv("text", { class: "hb", x: at[n.id].x, y: at[n.id].y + 31 }, "h" + n.height + " bf" + sign(n.balance))])));
  const w = cnt * 60, ht = A.height * 62 + 8;
  return h("div", {}, [h("p", { class: "note" }, "Tree height " + A.height + ". h is a node's height, bf is its balance factor (left height minus right height). " +
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
  $("tabs").replaceChildren(...TABS.map(([k, t]) => h("button", { type: "button", role: "tab", "aria-selected": String(k === ui.tab), class: k === ui.tab ? "on" : "",
    "data-fk": "tab-" + k, onclick: () => { ui.tab = k; drawTabs(); const b = document.querySelector('[data-fk="tab-' + k + '"]'); if (b) b.focus(); } }, t)));
  $("tab").replaceChildren(VIEWS[ui.tab]());
}

// ---- forms and buttons -----------------------------------------------------------------------
function formMode() {                  // the camp form either adds a camp or edits the one in ui.editing
  const e = ui.editing;
  $("cid").disabled = !!e; $("clinks").disabled = !!e;
  $("cadd").textContent = e ? "Save changes" : "Add camp"; $("ccancel").hidden = !e;
  $("ctitle").textContent = e ? "Edit camp " + e : "Add camp";
}
function leaveForm() { ui.editing = 0; ui.draft = null; ui.pane = "list"; $("campForm").reset(); formMode(); if (S) render(); }
function editCamp(c) {
  ui.editing = c.id; ui.draft = null; ui.pane = "form"; ui.confirm = null; formMode(); drawPane();
  $("cid").value = c.id; $("cname").value = c.name; $("cx").value = c.x; $("cy").value = c.y; $("cfam").value = c.families; $("clinks").value = "";
  $("cname").focus(); drawMap(); drawCamps();
}
async function deleteCamp(id) {
  const j = await act("DELETE", "/api/camps/" + id);
  if (j && j.error === "ConfirmNeeded") { ui.confirm = { id, pending: j.pending }; say("Camp " + id + " has pending requests. Confirm in the left panel to delete it."); drawCamps(); const b = document.querySelector('[data-fk="cdel-no"]'); if (b) b.focus(); }
  else if (j && j.ok && ui.selected === id) { ui.selected = 0; render(); }
}
async function confirmDelete(id) {
  ui.confirm = null;
  const j = await act("DELETE", "/api/camps/" + id + "?confirm=true");
  if (j && j.ok && ui.selected === id) { ui.selected = 0; render(); }
}
function editRequest(r) {
  ui.editReq = { id: r.id, urgency: r.urgency, families: r.families, quantity: r.quantity };
  drawRequests();
  const f = document.querySelector('[data-fk="re-urgency"]'); if (f) f.focus();
}
async function saveRequest() {
  const e = ui.editReq;
  const j = await act("PUT", "/api/requests/" + e.id, { urgency: num(String(e.urgency)), families: num(String(e.families)), quantity: num(String(e.quantity)) });
  if (j && j.ok) { ui.editReq = null; render(); }
}

$("campForm").onsubmit = async e => {
  e.preventDefault();
  const body = { name: $("cname").value, x: num($("cx").value), y: num($("cy").value), families: num($("cfam").value) };
  const j = ui.editing ? await act("PUT", "/api/camps/" + ui.editing, body)
    : await act("POST", "/api/camps", Object.assign({ id: num($("cid").value), links: $("clinks").value.split(/[\s,]+/).filter(Boolean).map(Number) }, body));
  if (j && j.ok) leaveForm();
};
$("ccancel").onclick = leaveForm;
$("segList").onclick = () => { ui.pane = "list"; drawPane(); };
$("segForm").onclick = () => { ui.pane = "form"; drawPane(); };
$("findForm").onsubmit = e => { e.preventDefault(); ui.pane = "list"; drawPane(); pick(num($("find").value), true); };
$("reqForm").onsubmit = e => {
  e.preventDefault();
  act("POST", "/api/requests", { campId: num($("rcamp").value), resource: $("rres").value, quantity: num($("rqty").value), urgency: num($("rurg").value), families: num($("rfam").value) });
};
$("process").onclick = () => act("POST", "/api/process");
$("handle").onclick = async () => {
  const j = await act("POST", "/api/handle-next");
  if (j && j.ok && S.log.length) { ui.selected = S.log[S.log.length - 1].campId; render(); }   // show the route just used
};
$("load").onclick = () => { ui.selected = 0; ui.draft = null; ui.confirm = null; ui.editReq = null; act("POST", "/api/scenario/" + $("scenario").value); };
$("reset").onclick = () => { ui.selected = 0; ui.draft = null; ui.confirm = null; ui.editReq = null; act("POST", "/api/reset"); };
$("setSpeed").onclick = () => act("POST", "/api/settings", { speedKmh: num($("speed").value) });
$("addRoad").onclick = () => act("POST", "/api/roads", { a: num($("rfrom").value), b: num($("rto").value) });
$("delRoad").onclick = () => act("DELETE", "/api/roads", { a: num($("rfrom").value), b: num($("rto").value) });
// Changing the road dropdowns only moves the amber highlight on the map; nothing is sent until a button is pressed.
$("rfrom").onchange = $("rto").onchange = () => { if (S) drawMap(); };
$("dsToggle").onclick = () => {
  const hide = !document.body.classList.contains("dscol");
  document.body.classList.toggle("dscol", hide);
  $("dsToggle").textContent = hide ? "Show panel" : "Hide panel";
  $("dsToggle").setAttribute("aria-expanded", String(!hide));
};
// The map is drawn to fit its box, so draw it again whenever the box changes size.
if (typeof ResizeObserver === "function") new ResizeObserver(() => { if (S) drawMap(); }).observe($("mapwrap"));
else window.addEventListener("resize", () => { if (S) drawMap(); });

formMode(); drawPane();
$("dsToggle").setAttribute("aria-expanded", "true");
api("GET", "/api/state");
