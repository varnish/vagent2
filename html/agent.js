var stats = new Array();
var paramlist;
var out;
var setstate = false;

function assert(val)
{
	if (!(val)) {
		throw "Assertion failure";
	}
}

function assertText(val)
{
	assert(val != null);
	assert(val.length > 0);
}

function vcl_load(vcl, id) {
	var client = new XMLHttpRequest();
	assertText(vcl);
	assertText(id);
	try {
		client.open("PUT", "/vcl/" + id, false);
		client.send(vcl);
		out = client.responseText;
	} catch (err) {
		out = "Com. errors with agent: \n" + err;
	}
	if (client.status == 201) {
		return true;
	} else {
		return false;
	}
}

function out_clear() {
	document.getElementById("out").innerHTML = "";
	out = "";
}

function out_up() {
	document.getElementById("out").innerHTML = out;
}

function vcl_show(id) {
	var client = new XMLHttpRequest();
	assertText(id);
	try {
		client.open("GET", "/vcl/" + id, false);
		client.send();
	} catch (err) {
		out = "Com. errors with agent: \n" + err.message;
		return null;
	}
	if (client.status == 200) {
		return client.responseText;
	} else {
		out = client.responseText;
		return null;
	}
}

function topActive(head) {
	var navs = new Array('nav-home', 'nav-vcl', 'nav-param');
	assertText(head);

	for (x in navs) {
		document.getElementById(navs[x]).className = "";
	}
	document.getElementById("nav-" + head).className = "active";
}

function showVCL() {
	document.getElementById("params").style.display = "NONE";
	document.getElementById("home").style.display = "NONE";
	document.getElementById("vcl").style.display = "block";
	topActive("vcl");
}
function showHome() {
	document.getElementById("params").style.display = "NONE";
	document.getElementById("home").style.display = "block";
	document.getElementById("vcl").style.display = "NONE";
	topActive("home");
}
function showParam() {
	document.getElementById("vcl").style.display = "NONE";
	document.getElementById("home").style.display = "NONE";
	document.getElementById("params").style.display = "block";
	list_params();
	topActive("param");
}


function reset_status()
{
	but = document.getElementById("status-btn");
	assert(but != null);
	try {
		var client = new XMLHttpRequest();
		client.open("GET", "/status", false);
		client.send();
		stat = client.responseText;
		assertText(stat);
	} catch (err) {
		stat = "Error communicating with agent: " + err;
	}
	but.textContent = stat;
	if (stat == "Child in state running") {
		but.className = "btn btn-primary btn-block disabled";
	} else {
		but.className = "btn btn-danger btn-block disabled";
	}
	setstate = false;
}	

function show_status(state,message)
{
	assertText(state);
	assertText(message);
	setstate = true;
	but = document.getElementById("status-btn");
	if (state == "ok")
		but.className = "btn btn-success btn-block disabled";
	if (state == "warn")
		but.className = "btn btn-warning btn-block disabled";
	if (state == "error")
		but.className = "btn btn-danger btn-block disabled";
	but.textContent = message;
	setTimeout(function() { reset_status(); }, 3000);
}

function uploadVCL()
{
	out_clear();
	ret = vcl_load(document.getElementById("vcl-text").value, document.getElementById("vclID").value);
	out_up();
	if (ret) {
		show_status("ok","VCL stored");
	} else {
		show_status("warn", "VCL save failed");
	}
}

function loadVCL()
{
	out_clear();
	ret = vcl_show(document.getElementById("loader").value);
	if (ret == null)
		show_status("warn","VCL load failed");
	else
		show_status("ok", "VCL loaded");
	document.getElementById("vcl-text").value = ret;
	document.getElementById("vclID").value = document.getElementById("loader").value;
	out_up();
}

function listVCL() {
	var client = new XMLHttpRequest();
	client.open("GET", "/vcljson/", false);
	try {
		client.send();
		var vcllist = JSON.parse(client.responseText);
		if (client.responseText == vcl)
			return;
		vcl = client.responseText;
		var txt = "";
		var active = "";
		for (x in vcllist["vcls"]) {
			v = vcllist["vcls"][x];
			if (v.status == "discarded") {
				continue;
			}
			if (v.status == "active") {
				active = v.name;
				document.getElementById("vcl-btn").innerHTML = "Active VCL: " + v.name;
			}
			txt = txt + "\n" + "<option>" + v.name + "</option>";
		}
		var d = document.getElementById("loader");
		d.innerHTML = txt;
		if (document.getElementById("vcl-text").value == "") {
			d.value = active;
			document.getElementById("vclID").value = active;
			loadVCL();
		}
	} catch (err) {
		out = "Listing VCL failed.";
	}
}

function list_params()
{
	try {
		var client = new XMLHttpRequest();
		client.open("GET", "/paramjson/", false);
		client.send();
		if (client.status != 200)
			throw new Error("Varnish-Agent returned " +
					client.status + ":" + client.responseText);
		paramlist = JSON.parse(client.responseText);
		var list = document.getElementById("param-sel");
		var v = list.value;
		var arry = new Array();
		for (x in paramlist) {
			arry.push(x);
		}
		arry.sort();
		list.options.length = 0;
		for (x in arry) {
			list.add(new Option(arry[x]));
		}
		if (v)
			list.value = v;
		paramChange();
	} catch (err) {
		out_clear();
		out = "Error listing parameters.\n"
			out = out + "Communication error with agent: \n"
			out = out + err;
		out_up();
	}
	return paramlist;
}

function paramChange()
{
	var pname = document.getElementById("param-sel").value;
	assertText(pname);
	document.getElementById("param-val").value = paramlist[pname].value;
	out_clear();
	out = "Default: " + paramlist[pname].default + "\n";
	out = out + "Value:   " + paramlist[pname].value + "\n";
	out = out + "Unit:    " + paramlist[pname].unit + "\n";
	out = out + paramlist[pname].description;
	out_up();
}

function paramListDiff()
{
	out_clear();
	out = "Non-default parameters:\n\n";
	assert(paramlist != null);
	for (x in paramlist) {
		if (paramlist[x].unit == "seconds" || paramlist[x].unit == "s" || paramlist[x].unit == "bitmap") {
			one = Number(paramlist[x].value);
			two = Number(paramlist[x].default);
			if (one != two) {
				out = out + x + "(";
				out = out + one + " vs ";
				out = out + two + ")\n";
			}
		} else if (paramlist[x].unit == "bytes") {
			one = paramlist[x].value == "-1" ? "unlimited" : paramlist[x].value;
			two = paramlist[x].default == "-1" ? "unlimited" : paramlist[x].default;
			if (one != two) {
				out = out + x + "(";
				out = out + one + " vs ";
				out = out + two + ")\n";
			}

		} else if (paramlist[x].value != paramlist[x].default) {
			out = out + x + "(";
			out = out + paramlist[x].value + " vs ";
			out = out + paramlist[x].default + ")\n";
		}
	}
	out_up();
}

function saveParam()
{
	var pname = document.getElementById("param-sel").value;
	var pval = document.getElementById("param-val").value;
	var client = new XMLHttpRequest();
	assertText(pname);
	assertText(pval);
	out_clear();
	client.open("PUT", "/param/" + pname, false);
	client.send(pval);
	out = client.responseText;
	out_up();
	if (client.status == 200) {
		show_status("ok","Parameter saved");
		list_params();
	} else {
		show_status("warn","Couldn't save parameter");
	}
}

function setParamDef()
{
	var pname = document.getElementById("param-sel").value;
	var pval = document.getElementById("param-val");
	assertText(pname);
	pval.value = paramlist[pname].default;
}

function vcl_use(id)
{
	var client = new XMLHttpRequest();
	assertText(id);
	try {
		client.open("PUT", "/vcldeploy/" + id, false);
		client.send();
		out = client.responseText;
	} catch (err) {
		out = "Com. errors with agent: \n" + err;
		return false;
	}
	if (client.status = "200") {
		return true;
	} else {
		return false;
	}
}

function deployVCL()
{
	out_clear();
	id = document.getElementById("loader").value;
	assertText(id);
	doc = vcl_use(id);
	if (doc) {
		show_status("ok","VCL deployed");
	} else {
		show_status("warn","vcl deploy failed");
	}
	out_up();	
}

function clearID()
{
	document.getElementById("vclID").value = "";
}

function vcl_discard(id)
{
	assertText(id);
	try {
		var client = new XMLHttpRequest();
		client.open("DELETE", "/vcl/" + id, false);
		client.send();
		out = client.responseText;
	} catch (err) {
		out = "Com. errors with agent: \n" + err;
		return false;
	}
	if (client.status = "200" && client.responseText == "") {
		return true;
	} else {
		return false;
	}
}

function discardVCL()
{
	out_clear();
	doc = vcl_discard(document.getElementById("loader").value);
	if (doc) {
		show_status("ok","VCL discarded");
	} else {
		show_status("warn","VCL discard failed");
	}
	out_up();	
}

function stop()
{
	var client = new XMLHttpRequest();
	client.open("PUT", "/stop", false);
	client.send();
	doc = client.responseText;
	document.getElementById("out").innerHTML = doc;
	status();
}

function start()
{
	var client = new XMLHttpRequest();
	client.open("PUT", "/start", false);
	client.send();
	doc = client.responseText;
	document.getElementById("out").innerHTML = doc;
	status();
}

function status()
{
	var client = new XMLHttpRequest();
	listVCL();
	if (!setstate)
		reset_status();
}

function update_stats()
{
	var client = new XMLHttpRequest();
	var d = document.getElementById("stats-btn");
	assert(d != null);
	try {
		client.open("GET","/stats", false);
		client.send();
		for (i = 0; i < 3; i++) {
			stats[i] = stats[i+1];
		}
		stats[3] = JSON.parse(client.responseText);
		var n_req = 0;
		var n_n_req = 0;
		for (i = 3; stats[i-1] != null; i--) {
			if (stats[i] == null) 
				break;
			if (stats[i].client_req == null)
				break;
			if (stats[i-1] != null && stats[i-1].client_req != null)
				n_req += stats[i].client_req.value - stats[i-1].client_req.value;
			else
				n_req += stats[i].client_req.value;
			n_n_req++;
		}
		if (n_n_req>0)
			d.innerHTML = Number(n_req/n_n_req).toFixed(0) + "req/s\n";
	} catch (err) {
		d.innerHTML = "Couldn't get stats: " + err;
	}

}
$('.btn').button();
setInterval(function(){status()},10000);
setInterval(function(){update_stats()},1000);
listVCL();
