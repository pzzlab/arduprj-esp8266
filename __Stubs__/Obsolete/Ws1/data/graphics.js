// JavaScript source code to handle the graphics
Arr = {};
var Ok = 0, Errors = 0, Pending1 = false;
// setpgm::body.onload
function GetPageNum(id)	{var s = document.location.href.split("?"); PgmNum = s[1]; document.getElementById(id).value = Number(PgmNum) + 1;}
// setpgm::body.onload
function ShowPgm()		{LoadArray('pgmx', PgmNum, 'red', 'green', 'blue',undefined);}
// pgms::onload
function ShowAllPgm()	{for (let n = 0; n < 8; n++)	LoadArray('Pgm',n,'red','green','blue',n);}
// internal
function LoadArray(id,num,color1,color2,color3,label)
{
  var xmlHttp  = null; 
  xmlHttp = new XMLHttpRequest();
  if (xmlHttp)
  {
    xmlHttp.timeout = 700;
    xmlHttp.open("GET", 'ReadArray?Arr[' + num + ']=', true);
    xmlHttp.onloadend = function ()			// response handler
    {
      var a = xmlHttp.responseText.split(';'); Arr = a;
			if (id == 'Pgm')	id += num;
 			c = document.getElementById(id).getContext('2d');
      if (c.canvas.width > (window.innerWidth -20)) c.canvas.width = window.innerWidth -20;
      var n, h = c.canvas.height - 1, w = c.canvas.width - 1;
      var s = w / a.length,x = 0;
      for (n=0; n < a.length; n++) if (a[n] > x) x = a[n]; if (x < 8) x = 8; x = h / x;
			c.clearRect(0,0,w,h);  c.globalAlpha = 1;
      var gr = c.createLinearGradient(0,0,0,h); 
			gr.addColorStop(0,color1); gr.addColorStop(0.4,color2); gr.addColorStop(1.0,color3); c.fillStyle = gr;
      for (n=0; n < a.length; n++) c.fillRect(n * s, h, s, -a[n] * x );
			if (label != undefined) 
			{c.fillStyle = 'black'; c.font = "24px Arial";c.fillText(Number(num) + 1, 4, 28);}
      Pending1 = false; Errors = 0; Ok++; return;
		}
    xmlHttp.ontimeout = function ()  {Errors++; Pending1 = false; console.log('Timeout'); return;}	// timeout
    xmlHttp.send(); Pending1 = true;	// send msg
  }
}
// setpgm::
function ModifyPgm(id,ta,tb,tset)
{
  var t1 = document.getElementById(ta).value, t2 = document.getElementById(tb).value, temp = document.getElementById(tset).value;
	if (t1 == "") {window.alert('Orario Inizio Non Valido'); return;}
	if (t2 == "") {window.alert('Orario Fine Non Valido'); return;}
	t1 = t1.split(':'); t2 = t2.split(':'); 
	t1 = t1[0] * 60 + Number(t1[1]);
	t2 = t2[0] * 60 + Number(t2[1]);
  t1 /= 15; t2 /= 15;
	if (t1 > t2) {window.alert('Ora di inizio > Ora di fine'); return;}
	var val = "";
	for (let i = t1; i < t2; i++) Arr[i] = temp;
	for (let i = 0; i < Arr.length; i++) val += Arr[i] + ' ';
	if (t1 < t2) WriteData(id,val);
}
// setpgm::
function Savecursup()
{
  if (!window.confirm('Sicuro di salvare i nuovi valori?\nUna volta salvati saranno resi definitivi')) return;
  for (let i = 0; i < elem.length;i++)
    WriteData(elem[i].id,elem[i].value);
}
// adjust the temperature refererence
function AdjTemp(num)
{
	var curs = document.getElementById('tset'+num), val = document.getElementById('tval'+num), n = num;
	val.value = curs.value;

	if (num < 8)
		for (let i = num + 1; i <= 8; i++)
		{
			var curs1 =document.getElementById('tset'+i);
			var val = document.getElementById('tval'+i);
			if (Number(curs.value) > Number(curs1.value)) {curs1.value = curs.value; val.value = curs.value;}
			
		}
	if (num > 1)
		for (let i = num - 1; i >=1; i--)
		{
			var curs1 =document.getElementById('tset'+i)
			var val = document.getElementById('tval'+i);
			if (Number(curs.value) < Number(curs1.value)) {curs1.value = curs.value; val.value = curs.value;}
		}
}

function ShowTemps()
{
	for (let i = 1; i <= 8; i++)
	{
		var curs = document.getElementById('tset'+i), val = document.getElementById('tval'+i);
		val.value = curs.value;
	}
}

