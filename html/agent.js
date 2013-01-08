var agent = {
	stats:new Array(),
	params:null,
	out:"Initializing frontend",
	setstate:false,
	vcl:"",
	vclId:"",
	vclList:null,
	activeVcl:""
};

/*
 * Old legacy variables. Remove "after a while".
 */
var stats = null;
var paramlist = null;
var out = null;
var setstate = null;

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
		agent.out = client.responseText;
	} catch (err) {
		agent.out = "Com. errors with agent: \n" + err;
		return false;
	}
	if (client.status == 201) {
		return true;
	} else {
		return false;
	}
}

function out_clear() {
	document.getElementById("out").innerHTML = "";
	agent.out = "";
}

function out_up() {
	document.getElementById("out").innerHTML = agent.out;
}

function vcl_show(id) {
	var client = new XMLHttpRequest();
	assertText(id);
	try {
		client.open("GET", "/vcl/" + id, false);
		client.send();
	} catch (err) {
		agent.out = "Com. errors with agent: \n" + err.message;
		return false;
	}
	if (client.status == 200) {
		agent.vcl = client.responseText;
		agent.vclId = id;
		return true;
	} else {
		agent.out = client.responseText;
		return false;
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
	var but = document.getElementById("status-btn");
	assert(but != null);
	var stat;
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
	agent.setstate = false;
}	

function show_status(state,message)
{
	assertText(state);
	assertText(message);
	agent.setstate = true;
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
	var ret;
	out_clear();
	var id = document.getElementById("loader").value;
	assertText(id);
	ret = vcl_show(id);
	if (!ret)
		show_status("warn","VCL load failed");
	else
		show_status("ok", "VCL loaded");
	if (ret) {
		document.getElementById("vcl-text").value = agent.vcl;
		document.getElementById("vclID").value = agent.vclId;
	}
	out_up();
}


function listVCL() {
	var client = new XMLHttpRequest();
	client.open("GET", "/vcljson/", false);
	try {
		client.send();
		var vclList = JSON.parse(client.responseText);
		/*
		 * XXX: If json parsing fails, this leaves agent.vcllist
		 * intact.
		 */
		agent.vclList = vclList;
		var txt = "";
		for (x in agent.vclList["vcls"]) {
			v = agent.vclList["vcls"][x];
			if (v.status == "discarded") {
				continue;
			}
			if (v.status == "active") {
				agent.activeVcl = v.name;
				document.getElementById("vcl-btn").innerHTML = "Active VCL: " + v.name;
			}
			txt = txt + "\n" + "<option>" + v.name + "</option>";
		}
		var d = document.getElementById("loader");
		d.innerHTML = txt;
		if (document.getElementById("vcl-text").value == "") {
			d.value = agent.activeVcl;
			document.getElementById("vclID").value = agent.activeVcl;
			loadVCL();
		}
	} catch (err) {
		agent.out = "Listing VCL failed.";
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
		var paramlist = JSON.parse(client.responseText);
		agent.params = paramlist;
		var list = document.getElementById("param-sel");
		var v = list.value;
		var arry = new Array();
		for (x in agent.params) {
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
		agent.out = "Error listing parameters.\n";
		agent.out += "Communication error with agent: \n";
		agent.out += err;
		out_up();
	}
	return true;
}

function paramChange()
{
	var pname = document.getElementById("param-sel").value;
	assertText(pname);
	document.getElementById("param-val").value = agent.params[pname].value;
	out_clear();
	agent.out =  "Default: " + agent.params[pname].default + "\n";
	agent.out += "Value:   " + agent.params[pname].value + "\n";
	agent.out += "Unit:    " + agent.params[pname].unit + "\n";
	agent.out += agent.params[pname].description;
	out_up();
}

function paramListDiff()
{
	out_clear();
	agent.out = "Non-default parameters:\n\n";
	assert(agent.params != null);
	for (x in agent.params) {
		if (agent.params[x].unit == "seconds" || agent.params[x].unit == "s" || agent.params[x].unit == "bitmap") {
			var one = Number(agent.params[x].value);
			var two = Number(agent.params[x].default);
			if (one != two) {
				agent.out +=  x + "(";
				agent.out += one + " vs ";
				agent.out += two + ")\n";
			}
		} else if (agent.params[x].unit == "bytes") {
			var one = agent.params[x].value == "-1" ? "unlimited" : agent.params[x].value;
			var two = agent.params[x].default == "-1" ? "unlimited" : agent.params[x].default;
			if (one != two) {
				agent.out += x + "(";
				agent.out +=  one + " vs ";
				agent.out +=  two + ")\n";
			}

		} else if (agent.params[x].value != agent.params[x].default) {
			agent.out += x + "(";
			agent.out += agent.params[x].value + " vs ";
			agent.out += agent.params[x].default + ")\n";
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
	agent.out = client.responseText;
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
	assertText(agent.params[pname].default);
	pval.value = agent.params[pname].default;
}

function vcl_use(id)
{
	var client = new XMLHttpRequest();
	assertText(id);
	try {
		client.open("PUT", "/vcldeploy/" + id, false);
		client.send();
		agent.out = client.responseText;
	} catch (err) {
		agent.out = "Com. errors with agent: \n" + err;
		return false;
	}
	if (client.status = "200") {
		agent.activeVcl = id;
		return true;
	} else {
		return false;
	}
}

function deployVCL()
{
	out_clear();
	var id = document.getElementById("loader").value;
	assertText(id);
	var doc = vcl_use(id);
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
	agent.vclId = "";
}

function vcl_discard(id)
{
	assertText(id);
	try {
		var client = new XMLHttpRequest();
		client.open("DELETE", "/vcl/" + id, false);
		client.send();
		agent.out = client.responseText;
	} catch (err) {
		agent.out = "Com. errors with agent: \n" + err;
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
	var doc = vcl_discard(document.getElementById("loader").value);
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
	var doc = client.responseText;
	document.getElementById("out").innerHTML = doc;
	status();
}

function start()
{
	var client = new XMLHttpRequest();
	client.open("PUT", "/start", false);
	client.send();
	var doc = client.responseText;
	document.getElementById("out").innerHTML = doc;
	status();
}

function status()
{
	var client = new XMLHttpRequest();
	listVCL();
	if (!agent.setstate)
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
			agent.stats[i] = agent.stats[i+1];
		}
		agent.stats[3] = JSON.parse(client.responseText);
		var n_req = 0;
		var n_n_req = 0;
		for (i = 3; agent.stats[i-1] != null; i--) {
			if (agent.stats[i] == null) 
				break;
			if (agent.stats[i].client_req == null)
				break;
			if (agent.stats[i-1] != null && agent.stats[i-1].client_req != null)
				n_req += agent.stats[i].client_req.value - agent.stats[i-1].client_req.value;
			else
				n_req += agent.stats[i].client_req.value;
			n_n_req++;
		}
		if (n_n_req>0)
			d.innerHTML = Number(n_req/n_n_req).toFixed(0) + "req/s\n";
	} catch (err) {
		d.innerHTML = "Couldn't get stats: " + err;
	}

}

function sanity() {
	assert(stats == null);
	assert(paramlist == null);
	assert(out == null);
	assert(setstate == null);
}

$('.btn').button();
setInterval(function(){status()},10000);
setInterval(function(){update_stats()},1000);
setInterval(function(){sanity()},1000);
listVCL();
