// JavaScript source code for runrimwe update of data

var Tmr = 0, Elem = undefined, i = 0, Pending = false, Actpage = "";
var Ok = 0, Errors = 0, Oneshot = false;

function EntryPage(page,runonce) 
{
  var i, id = "";
  Actpage = "";
  Pending = false;
  Alem = document.getElementsByClassName('var'); 
  for (i = 0; i < elem.length; i++)    {id += Elem[i].id; id +=' '}
  if (Tmr) clearTimeout(Tmr);
  WriteData(page,id);
  if (Elem.length > 0)    Tmr = setTimeout(RefreshPage, 200);
  if (runonce) Oneshot = true;
  
}

function ExitPage()  {Elem = 0; Actpage = null;}

function RefreshPage()
{
 if (Actpage == "") return;
 if (Ok < 5)  document.getElementById('comms').value += '.'; 
 else        {document.getElementById('comms').value  = ' '; Ok = 0;}
if (Errors > 4) {alert('Too many errors\nReaload the page'); return;}
 ReadVars();
  if (Tmr) clearTimeout(Tmr);
  if ((Elem.length > 0) &&! Oneshot)  Tmr = setTimeout(Refresh, 1000);
}

function ReadData(id)
{
 var xmlHttp  = null; 
 xmlHttp = new XMLHttpRequest();
 if (xmlHttp)
 {
   xmlHttp.timeout = 500;
   xmlHttp.open("GET", "ReadVar?" + id +"=", true);
   xmlHttp.onloadend = function ()  {document.getElementById(id).innerHTML = xmlHttp.responseText; Pending = false; return;}
   xmlHttp.ontimeout = function ()  {pending = false; console.log('Timeout'); return;}
   xmlHttp.send(); Pending = true;
 }
};

function WriteData(id, value)
{
  var xmlHttp = null;
  var cmd = "WriteVar?";  
  if (id.startsWith('@')) cmd = "SetPage?"
   else
    if (id.startsWith('Arr[')) cmd = "WriteArray?";
  xmlHttp = new XMLHttpRequest();
  if (xmlHttp) 
  {
    xmlHttp.timeout = 500;
    xmlHttp.open("GET", cmd + id + "=" + value, true);
    xmlHttp.onloadend = function () {Pending = false; value = xmlHttp.responseText; if (id.startsWith('@')) actpage = id; return;}
    xmlHttp.onTimeout = function () {pending = false; value = 'Timeout'; alert('Write Failed,Retry'); return;}
    xmlHttp.send(); pending = true;
  }
};
 
function ReadVars()
{
  if(Actpage == null) return;
  var xmlHttp  = null; 
  xmlHttp = new XMLHttpRequest();
  if (xmlHttp)
  {
    xmlHttp.timeout = 500;
    xmlHttp.open("GET", "ReadPage?" + actpage +"=", true);
    xmlHttp.onloadend = function ()  
    {
      var a = xmlHttp.responseText.split(';'); 
      if (a.length != elem.length) {errors++; console.log('data size incoherent' + a.length +'/' + Elem.length); Pending = false; return;}
      for (let i = 0; i < elem.length; i++)   document.getElementById(Elem[i].id).value = a[i];
      Pending = false; Errors = 0; Ok++; return;
    }
    xmlHttp.ontimeout = function ()  {Eerrors++; Pending = false; console.log('Timeout'); return;}
    xmlHttp.send(); Pending = true;
  }
}
// 
function ReadArray(id,num)
{
  var i, xmlHttp  = null; 
  xmlHttp = new XMLHttpRequest();
  if (xmlHttp)
  {
    xmlHttp.timeout = 700;
    xmlHttp.open("GET", 'ReadArray?Arr[' + num + ']=', true);
    xmlHttp.onloadend = function ()			// response handler
    {
      Arr = xmlHttp.responseText.split(';');
      Pending1 = false; Errors = 0; Ok++; return;
		}
    xmlHttp.ontimeout = function ()  {Errors++; Pending1 = false; console.log('Timeout'); return;}	// timeout
    xmlHttp.send(); Pending1 = true;	// send msg
  }
}

