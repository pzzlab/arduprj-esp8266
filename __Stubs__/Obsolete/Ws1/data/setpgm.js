const NZONE = 6, NPRG = 8;
var Tmr, Errors = 0, XhrReq = 0, PgmSel, Cyc = 0, NxtCyc = 0;
var Vars = {}, Pgms = {}, ZNum = {}, Names = {};


function EntryPage()	{Xhr('ReadBlk?0',true,LoadSet);}

function LoadSet(http)
{
	let a = new Uint8Array(http.response); 
	for (ofs = i = 0; i < NPRG; i++, ofs+=48)	 {Pgms[i] = a.slice(i * 48, i * 48 + 48);}
	ZNum = a.slice(ofs,ofs + NZONE * 7); ofs += NZONE * 7;
	for (i = 0; i < NPRG; i++)	 ShowPgm(i,'green'); 
}

function ShowPgm(num,col)
{
	let pgm = Pgms[num];
	c = document.getElementById('Bar' + Number(num)).getContext('2d');
	h = c.canvas.height; w = c.canvas.width;	c.clearRect(0,0,w,h);	
	c.fillStyle = col; w = w / 48;
	for (n=0; n < pgm.length; n++)	{c.fillRect(n * w, h, w, -pgm[n] * h / 256);}
	
}

function SelPgm(num) 
{
	if (PgmSel != undefined) return; 
	PgmSel = num; ShowPgm(num,'red'); document.getElementById('intv').style.display = 'block';
	document.getElementById('testo').innerHTML = 'Inserire Tempi (30 Min) e Gradi (0.1)'
	window.location.href='#intv';
}
 
function ModifyPgm(ta,tb,tset)
{
	if (ta == '') {window.alert('Orario Inizio Non Valido'); return;}
	if (tb == '') {window.alert('Orario Fine Non Valido'); return;}
	t1 = ta.split(':'); t2 = tb.split(':'); 
	t1 = Math.round(t1[0] * 2 + t1[1] / 30)
	t2 = Math.round(t2[0] * 2 + t2[1] / 30);
	if (t1 >= t2) {window.alert('Ora di fine DEVE essere dopo inizio'); return;}
	document.getElementById('t1').value = tb;
	for (let i = t1; i < t2; i++) Pgms[PgmSel][i] = tset;
	Xhr('WriteArray?Pgm'+ PgmSel, false, '', Pgms[PgmSel]);
	ShowPgm(PgmSel,'red');
}
// setpgm::
function Savecursup()
{
  if (!window.confirm('Sicuro di salvare i nuovi valori?\nUna volta salvati saranno resi definitivi')) return;
  for (let i = 0; i < elem.length;i++)
    WriteData(elem[i].id,elem[i].value);
}

// ---- XMLHttpReq generic function ----
function Xhr(cmd,binary,callback,value)
{
	if ((http = new XMLHttpRequest()) != null)
	{
		XhrReq++; if (value != undefined) cmd += '=' + value;
		http.timeout = 400; http.responseType = '';
		http.open("GET", cmd, true); if (binary) http.responseType = 'arraybuffer';
		http.onloadend = function ()	
			{if (!http.status) return; Cyc = NxtCyc; Errors = 0; if (callback && (http.status == 200))	callback(http);}
		http.ontimeout = function ()  
			{Cyc = NxtCyc; Errors++; console.log('Timeout');}
		if (value != undefined)		http.send(value); 
			else										http.send(); 
	}
	return(true);
}

