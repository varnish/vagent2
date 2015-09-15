/*
 * Copyright (C) 2015  Norwegian Broadcasting Corporation (NRK)
 *
 * Author: Kristian Lyngstøl <kristian@bohemians.org>
 *
 * FIXME: This thing is entirely separate from the rest of agent.js, for better
 * or worse. This is arguably a feature, as it's quite reasonable to want to
 * use this stand-alone, which, in fact, is possible with varnishstat.html.
 *
 * FIXME: The name space here is rather random, and entirely global. Should be
 * fixed.
 */


var vdata = new Array();
var vdatap = new Array();
var ignore_null = true;
var reconstructTable = true;
var hitrate = { 
	10: { v: 0, d: 0 },
	100: { v: 0, d: 0 },
	1000: { v: 0, d: 0 }
}
/*
 * Store the last 1000 seconds of varnishstat data, but also the very first.
 */
function compressData()
{
	if (vdata.length > 1000) {
		console.log("compressing");
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
 * Updates stats
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
				constructTable();
				reconstructTable = false;
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
	td.innerHTML = "Name";
	td.className = "varnishstat-Name";
	td = tr.insertCell(1);
	td.innerHTML = "Current";
	td.className = "varnishstat-Current";
	td = tr.insertCell(2);
	td.innerHTML = "Change";
	td.className = "varnishstat-Change";
	td = tr.insertCell(3);
	td.innerHTML = "Average";
	td.className = "varnishstat-Average";
	td = tr.insertCell(4);
	td.innerHTML = "Average 10";
	td.className = "varnishstat-Average10";
	td = tr.insertCell(5);
	td.innerHTML = "Average 100";
	td.className = "varnishstat-Average100";
	td = tr.insertCell(6);
	td.innerHTML = "Average 1000";
	td.className = "varnishstat-Average1000";
	td = tr.insertCell(7);
	td.innerHTML = "Description";
	td.className = "varnishstat-Description";
	
	for (v in vdata[0]) {
		if (v == 'timestamp')
			continue;
		if (vdata[0][v]['value'] == 0 && ignore_null)
			continue;
		tr = table.insertRow(-1);
		td = tr.insertCell(0);
		td.id = v + "-name";
		td.innerHTML = v;
		td = tr.insertCell(1);
		td.id = v + "-cur";
		td.innerHTML = vdata[0][v]['value'];
		td = tr.insertCell(2);
		td.id = v + "-diff";
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
		td.innerHTML = vdata[0][v]['description'];
	}
	el.appendChild(table);
	updateTable();
}

/*
 * You don't want to read this. Pretend it's not here.
 * (converts 124124123 to "day+hour:minute:second")
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

	d = d/(3600*24);
	h = h/3600;
	m = m/60;
	if (s<10) 
		s = "0" + s;
	if (m < 10)
		m = "0" + m;
	if (h < 10)
		h = "0" + h;

	return  d + "+" +  h + ":" + m + ":" + s ;
}

function toggleIgnoreNull()
{
	ignore_null = !ignore_null;
	constructTable();
}

/*
 * Primary update-function, after the data has changed.
 *
 * Could/should probably be a bit prettier.
 */
function updateTable()
{
	for (v in vdata[0]) {
		if (v == 'timestamp')
			continue;
		if (vdata[0][v]['value'] == 0 && ignore_null)
			continue;
		el = document.getElementById(v + "-cur");
		if (el == undefined) {
			reconstructTable = true;
			continue;
		}
		el.innerHTML = vdata[0][v]['value'];
		if (vdata[1] == undefined || vdata[1][v] == undefined)
			continue;
		if (vdata.length > 1) {
			el = document.getElementById(v + "-diff");
			el.innerHTML = vdata[0][v]['value'] - vdata[1][v]['value'];
			if (vdata[0][v]['flag'] == 'a') {
				el = document.getElementById(v + "-avg");
				el.innerHTML = (vdata[0][v]['value'] / vdata[0]['MAIN.uptime']['value']).toFixed(2);
				scope = [10, 100, 1000];
				for (a in scope) {
					a = scope[a];
					el = document.getElementById(v + "-avg" + a);
					now =  vdata[0][v]['value'];
					nthen = vdata.length > a ? a : vdata.length - 1;
					if (vdata[nthen][v] == undefined)
						continue;
					then = vdata[nthen][v]['value'];
					timed = vdata[0]['MAIN.uptime']['value'] - vdata[nthen]['MAIN.uptime']['value'];
					el.innerHTML = ((now - then) / timed).toFixed(2);
				}
			}
		}
		
	}
}

/*
 * Updates the hitrate and uptime-boxes
 * FIXME: Should be cleaned up a bit, mostly for style/readability and naming.
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
		document.getElementById("hitrate" + origa + "d").innerHTML = hitrate[origa].d;
		document.getElementById("hitrate" + origa + "v").innerHTML = (hitrate[origa].h*100).toFixed(3) + "%";
		document.getElementById("bw" + origa + "v").innerHTML = toHuman(hitrate[origa].b) + "B/s";
		document.getElementById("fbw" + origa + "v").innerHTML = toHuman(hitrate[origa].f) + "B/s";
		
		document.getElementById("storage" + origa + "v").innerHTML = toHuman(storage['g_bytes']);

	}
	document.getElementById("MAIN.uptime-1").innerHTML = secToDiff(vdatap[0]['uptime']);
	document.getElementById("MGT.uptime-1").innerHTML = secToDiff(vdatap[0]['MGT.uptime']);
}

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

/*
 * FIXME: Should not really be here. Hidden away from the rest of the agent.js code...
 */
setInterval(updateArray,1000);

