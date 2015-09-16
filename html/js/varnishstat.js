/*
 * Copyright (C) 2015  Norwegian Broadcasting Corporation (NRK)
 *
 * Author: Kristian Lyngstøl <kristian@bohemians.org>
 *
 * FIXME: The various averages don't necessarily check the time but assumes
 * that the timer is precise. This can lead to incorrect values if the
 * network/response is unreliable.
 *
 * FIXME: If a counter is removed or added, things might not work. Workaround: reload.
 *
 * FIXME: The name space here is rather random, and entirely global. Should be
 * fixed.
 *
 */

/*
 * vdata is the actual stats data, up to 1000 seconds.
 */
var vdata = new Array();

/*
 * This is/was a normalized-variant of vdata. It's only used by the infoboxes
 * on top and is largely unnecessary. Should be phased out.
 */
var vdatap = new Array();

/*
 * Setting: Do we ignore counters with 0 value or not.
 */
var ignore_null = true;

/*
 * We want to make sure that we show counters that _had_ a value at some point,
 * so we can't just check vdata[0]. This is a simple hash of all counters with
 * true/false on whether they've been spotted with a value or not.
 */
var ignoreNullTable = {};

/*
 * Set to true if we need to re-create the table. Useful so updateTable() can
 * spot if an element is missing or not. Without it we get a problem as
 * constructTable() calls updateTable() as the last thing it does.
 */
var reconstructTable = true;

/*
 * Setting: Do we draw sparklines or not.
 */
var varnishstatSparkLines = false;

/*
 * Performancetweak: History for sparklines to avoid redrawing identical lines.
 */
var sparklineDataHistory = {};

/*
 * Performancetweak: Used in updateTable(). Set to true if we've detected that
 * a particular counter is below the viewport. Used to avoid calling
 * getBoundingRect().
 */
var wasBelow = false;

/*
 * Used for drawing hitrates, bandwidth and storage usage. Note that the other
 * values are set by the implementation.
 */
var hitrate = { 
	10: { v: 0, d: 0 },
	100: { v: 0, d: 0 },
	1000: { v: 0, d: 0 }
}

/*
 * interval handler to start/stop varnishstat.
 */
var varnishstathandler = false;

/*
 * Store the last 1000 seconds of varnishstat data, but also the very first.
 *
 * XXX: Might want to remove 'description' from old fields if we care about
 * memory.
 */
function compressData()
{
	if (vdata.length > 1000) {
		vdata.splice(1000,vdata.length-1001);
	}
	if ((vdata.length-1) < 1) 
		return;

	if (vdata[0]['MAIN.uptime'] == undefined  
		|| vdata[vdata.length-1]['MAIN.uptime'] == undefined 
		|| vdata[vdata.length-1]['MAIN.uptime']['value'] > vdata[0]['MAIN.uptime']['value']) {
		/*
		 * XXX: Varnish restarted. Discard old stats to avoid weird averages.
		 */
		vdata.splice(1,vdata.length-1);
		vdatap.splice(1,vdatap.length-1);
	}
}

/*
 * Updates stats.
 *
 * Called by interval.
 */
function updateArray()
{
	$.ajax({
		type: "GET",
		url: "/stats",
		dataType: "text",
		success: function (data, textStatus, jqXHR) {
			var tmp = JSON.parse(data);
			var tmpp = {};
			if (vdatap.length != 0) {
				if (vdatap[0]['timestamp'] == tmp['timestamp']) {
					return;
				}
			}
			for (v in tmp) {
				if (tmp[v]['value'] != undefined)  {
					tmp[v]['value'] = parseInt(tmp[v]['value']);
					tmpp[v.replace("MAIN.","")] = parseInt(tmp[v]['value']);
				}
			}
			vdata.unshift(tmp);
			vdatap.unshift(tmpp);
			compressData();
			updateHitrate();
			if (reconstructTable) {
				sparklineDataHistory = {};
				constructTable();
				reconstructTable = false;
				sparklineDataHistory = {};
			} else {
				updateTable();
			}
		}
	})	
}

/*
 * Construct the varnishstat table if needed.
 *
 * Also empties it first.
 *
 * Used both on init, if variables get added (upgraded agent/varnish?), or if
 * the toggle-button for null-values is pressed.
 */
function constructTable()
{
	var el = document.getElementById("varnishstat");
	var table = document.getElementById("varnishstat-inner");
	el.removeChild(table);
	table = document.createElement("table");
	table.className = "table table-condensed";
	table.id = "varnishstat-inner";
	header = table.createTHead();
	tr = header.insertRow(0);
	td = tr.insertCell(0);
	td.textContent = "Name";
	td.className = "varnishstat-Name";
	td = tr.insertCell(1);
	td.textContent = "Current";
	td.className = "varnishstat-Current";
	td = tr.insertCell(2);
	td.textContent = "Change";
	td.className = "varnishstat-Change";
	td = tr.insertCell(3);
	td.textContent = "Average";
	td.className = "varnishstat-Average";
	td = tr.insertCell(4);
	td.textContent = "Average 10";
	td.className = "varnishstat-Average10";
	td = tr.insertCell(5);
	td.textContent = "Average 100";
	td.className = "varnishstat-Average100";
	td = tr.insertCell(6);
	td.textContent = "Average 1000";
	td.className = "varnishstat-Average1000";
	td = tr.insertCell(7);
	td.textContent = "Description";
	td.className = "varnishstat-Description";
	
	for (v in vdata[0]) {
		if (v == 'timestamp')
			continue;
		ignoreNullTable[v] = ignoreNullTable[v] || vdata[0][v]['value'] > 0;
		if (!ignoreNullTable[v] && ignore_null)
			continue;
		origv = v;
		v = n(v);
		tr = table.insertRow(-1);
		td = tr.insertCell(0);
		td.id = v + "-name";
		td.textContent = origv;
		td = tr.insertCell(1);
		x = document.createElement("div");
		x.id = v + "-cur";
		y = document.createElement("SPAN");
		y.id = v + "-spark";
		y.style.float = "right";
		y.style.width = "50%";
		td.appendChild(y);
		td.appendChild(x);
		
		td = tr.insertCell(2);
		x = document.createElement("div");
		x.id = v + "-diff";
		y = document.createElement("SPAN");
		y.id = v + "-spark2";
		y.style.float = "right";
		y.style.width = "50%";
		td.appendChild(y);
		td.appendChild(x);
		td = tr.insertCell(3);
		td.id = v + "-avg";
		td = tr.insertCell(4);
		td.id = v + "-avg10";
		td = tr.insertCell(5);
		td.id = v + "-avg100";
		td = tr.insertCell(6);
		td.id = v + "-avg1000";
		td = tr.insertCell(7);
		td.id = v + "-sdec";
		td.textContent = vdata[0][origv]['description'];
	}
	el.appendChild(table);
	updateTable();
}

/*
 * Convert seconds to a duration. (d+HH:MM:SS)
 */
function secToDiff(x)
{
	s = x % 60;
	x -= s;
	m = x % 3600;
	x -= m;
	h = x % (3600*24);
	x -= h;
	d = x / (3600*24);

	h = h/3600;
	m = m/60;
	if (s<10) 
		s = "0" + s;
	if (m < 10)
		m = "0" + m;
	if (h < 10)
		h = "0" + h;

	return  parseInt(d) + "+" +  h + ":" + m + ":" + s ;
}

function toggleIgnoreNull()
{
	ignore_null = !ignore_null;
	constructTable();
}

function toggleSparkLines()
{
	varnishstatSparkLines = !varnishstatSparkLines;
	constructTable();
}

/*
 * Normalize x for id/tag names.
 */
function n(x)
{
	return x.replace(/[^a-z0-9]/ig,'-');
}

/*
 * Returns true if the data in sparkData is different from sparkDataHistory[v]
 */
function sparkUpdated(v, sparkData)
{
	if (sparklineDataHistory[v] == undefined)
		return true;
	if (sparklineDataHistory[v].length != sparkData.length)
		return true;
	for (var i = 0; i < sparkData.length;i++) {
		if (sparkData[i] != sparklineDataHistory[v][i])
			return true;
	}
	return false;
}

/*
 * Performance tweak/hack:
 * We know we iterate over a list and draw from top to bottom, so if one items
 * is below the screen, it's safe to assume the next ones are too.
 *
 * This is done because getBoundingClientRect() can be surprisingly slow. Some
 * tests showed that it was almost cheaper to render all the sparklines than to
 * try to figure out if they were visible...
 */
function isScrolledIntoView(elem)
{
	if (wasBelow)
		return false;
	var rect = document.getElementById(elem).getBoundingClientRect();
	if (below(rect)) {
		wasBelow = true;
	}
	return (!below(rect) && (rect.top > 0));
}

function below(rect)
{
	return ((window.innerHeight - rect.bottom) < 0);
}

/*
 * Primary update-function, after the data has changed.
 *
 * Could/should probably be a bit prettier.
 */
function updateTable()
{
	tmpvdata = vdata[0];
	wasBelow = false;
	for (v in tmpvdata) {
		if (v == 'timestamp')
			continue;
		ignoreNullTable[v] = ignoreNullTable[v] || vdata[0][v]['value'] > 0;
		if (!ignoreNullTable[v] && ignore_null)
			continue;
		el = document.getElementById(n(v) + "-cur");
		if (el == undefined) {
			reconstructTable = true;
			continue;
		}
		el.textContent = vdata[0][v]['value'];
		if (vdata[1] == undefined || vdata[1][v] == undefined)
			continue;
		if (vdata.length > 1) {
			if (varnishstatSparkLines) {
				sparkData = [];
				sparkData2 = [];
				for (i = 0; i < vdata.length && i < 30; i ++) {
					sparkData.unshift(vdata[i][v]['value']);
					if (i>0)
						sparkData2.unshift(vdata[i-1][v]['value'] - vdata[i][v]['value']);
				}
				if (sparkUpdated(v,sparkData)) {
					if (isScrolledIntoView(n(v) + '-spark')) {
						sparklineDataHistory[v] = sparkData;
						sparkOptions = {
							disableHiddenCheck: true,
							disableHighlight: true
						};
						$('#' + n(v) + '-spark').sparkline(sparkData,sparkOptions);
						$('#' + n(v) + '-spark2').sparkline(sparkData2,sparkOptions);
					}
				}
			} else {
				sparklineDataHistory[v] = undefined;
			}
			el = document.getElementById(n(v) + "-diff");
			el.textContent = vdata[0][v]['value'] - vdata[1][v]['value'];
			if (vdata[0][v]['flag'] == 'a') {
				el = document.getElementById(n(v) + "-avg");
				el.textContent = (vdata[0][v]['value'] / vdata[0]['MAIN.uptime']['value']).toFixed(2);
				scope = [10, 100, 1000];
				for (a in scope) {
					a = scope[a];
					el = document.getElementById(n(v) + "-avg" + a);
					now =  vdata[0][v]['value'];
					nthen = vdata.length > a ? a : vdata.length - 1;
					if (vdata[nthen][v] == undefined)
						continue;
					then = vdata[nthen][v]['value'];
					timed = vdata[0]['MAIN.uptime']['value'] - vdata[nthen]['MAIN.uptime']['value'];
					el.textContent = ((now - then) / timed).toFixed(2);
				}
			}
		}
		
	}
}

/*
 * Updates the hitrate and uptime-boxes
 * FIXME: Should be cleaned up a bit, mostly for style/readability and naming.
 * FIXME: Remove vdatap.
 */
function updateHitrate()
{
	var s1 = collect(makeProperJson(vdata[0]),['VBE'],undefined);
	var tmp = [10, 100, 1000];	
	for (x in tmp ) {
		a = tmp[x];
		var origa = a;
		if (vdatap.length-1 < a) {
			a = vdatap.length-1;
		}
		proper = makeProperJson(vdata[a]);
		s2 = collect(proper,['VBE'],undefined);
		storage = collect(proper,['SMA','SMF'],'Transient');
		hitrate[origa].d = a;
		hitrate[origa].h = ((vdatap[0]['cache_hit'] - vdatap[a]['cache_hit']) / (vdatap[0]['client_req'] - vdatap[a]['client_req'])).toFixed(5);
		hitrate[origa].b = (s1['beresp_hdrbytes'] + s1['beresp_bodybytes'] - 
				   s2['beresp_hdrbytes'] - s2['beresp_bodybytes']) / a;
		hitrate[origa].f = (vdatap[0]['s_resp_hdrbytes'] + vdatap[0]['s_resp_bodybytes']
				- (vdatap[a]['s_resp_hdrbytes'] + vdatap[a]['s_resp_bodybytes']))/a;
		document.getElementById("hitrate" + origa + "d").textContent = hitrate[origa].d;
		document.getElementById("hitrate" + origa + "v").textContent = (hitrate[origa].h*100).toFixed(3) + "%";
		document.getElementById("bw" + origa + "v").textContent = toHuman(hitrate[origa].b) + "B/s";
		document.getElementById("fbw" + origa + "v").textContent = toHuman(hitrate[origa].f) + "B/s";
		
		document.getElementById("storage" + origa + "v").textContent = toHuman(storage['g_bytes']);

	}
	document.getElementById("MAIN.uptime-1").textContent = secToDiff(vdatap[0]['uptime']);
	document.getElementById("MGT.uptime-1").textContent = secToDiff(vdatap[0]['MGT.uptime']);
}

/*
 * Varnish has a hierarchy of dynamic counters, but the JSON doesn't clearly
 * reflect this in a way we can use directly.
 *
 * This function parses the vdata in d and returns a proper tree structure.
 *
 * FIXME: Should probably be cached instead of recreating it on every update.
 */
function makeProperJson(d) {
	var out = {};
	for (item in d) {
		if (item == "timestamp")
			continue;
		ident = "";
		if (d[item]['ident'])
			ident = d[item]['ident'];
		if (ident != "")
			name = item.replace(d[item]['type'] + "." +  ident + "." ,"");
		else
			name = item.replace(d[item]['type'] + "." ,"");
		if (out[d[item]['type']] == undefined)
			out[d[item]['type']] = {};
		if (ident != "") {
			if (out[d[item]['type']][ident] == undefined)
				out[d[item]['type']][ident] = {};
			out[d[item]['type']][ident][name] = d[item]['value'];
		} else {
			out[d[item]['type']][name] = d[item]['value'];
		}
	}
	return out;
}

/*
 * Add up all same-named counters under the family 'types' (array), except
 * 'skip'.
 *
 * Or: Give us stats for all combined backends.
 * Or: Give us stats for all combined memory allocators, except Transient.
 */
function collect(d,types,skip) {
	var out = {};
	for (t in types) {
		if (d[types[t]] == undefined)
			continue;

		for (s in d[types[t]]) {
			if (s == skip)
				continue;
			for (v in d[types[t]][s]) {
				if (out[v] == undefined)
					out[v] = 0;
				out[v] += d[types[t]][s][v];
			}
			
		}
	}
	return out;
}

/*
 * Convert i to a human-readable number (add T/G/M/k postfixes).
 *
 * 1024-based.
 */
function toHuman(i) {
	var units = ['T','G','M','k'];
	var unit = '';
	if (i == undefined)
		return "";
	while(i > 1024 && units.length > 0) {
		unit = units.pop();
		i = i/1024;
	}
	return i.toFixed(1) + unit;
}

function startVarnishstat()
{
	stopVarnishstat();
	varnishstathandler = setInterval(updateArray,1000);
}

function stopVarnishstat()
{
	if (varnishstathandler)
		clearInterval(varnishstathandler);
}
