;(function () { 'use strict';
var localCookies = function(){
function get(nm){
	var name = nm + "=";
	var ca = document.cookie.split(';');
	for(var i=0; i < ca.length; i++){
		var c = ca[i];
		while (c.charAt(0)==' ') c = c.substring(1);
		if (c.indexOf(name) != -1)
			return c.substring(name.length,c.length);
	}
	return "";
}
function set(nm, val){
	var date = new Date();
	date.setFullYear(date.getFullYear() + 1);
	document.cookie = nm + "=" + val + ";" + "expires=" + date.toUTCString() + ";";
}
return{
	getItem : get,
	setItem : set
	};
}();

var stor;

if(!window.localStorage){
	console.log("No localStorage found, use cookies");
	stor = localCookies;
}else
	stor = window.localStorage;

/*
 * Load object nm from local storage
 * if it's absent set it to defval or return null if devfal undefined
 */
function LoadObject(nm, defval){
	var val = null;
	try{
		var X = stor.getItem(nm);
		console.log("get "+nm+", got: "+X);
		val = JSON.parse(X);
	}catch(e){
		console.log(e);
	}
	if(val == null && typeof(defval) != "undefined"){
		val = defval;
	}
	console.log("load: " + nm +" ("+val+")");
	return val;
}
/*
 * Save object obj in local storage as nm
 */
function SaveObject(nm, obj){
	stor.setItem(nm, JSON.stringify(obj));
	console.log("save: " + nm +" ("+JSON.stringify(obj)+")");
}
	window.Storage = {};
	window.Storage.load = LoadObject;
	window.Storage.save = SaveObject;
}());
