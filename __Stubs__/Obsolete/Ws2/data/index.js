const NZONE = 6, NPRG = 8, NPWR = 8, NINOUT = 3;
const Days = ['Domenica', 'Lunedi   ','Martedi  ','Mercoledi','Giovedi  ','Venerdi  ','Sabato  '];
var Tmr, Errors = 0, XhrReq = 0, Cyc = 1, Nxtcyc = 0;
var Clk  = new Uint8Array(4);
var Vars = {}, Pgms = {}, ZNum = {}, ZNames = {}, TMan = {}, LtMan = {}, TAct = {}, TAdj = {}, InOut = {}; 
var Mins, ZSel = 0, TClr = 0;
var W = {}, Wpk = {}, Wmax = {}, A = {}, V = {}, Fi = {}, PNames = {}, PFeat = {};
var LDay, PIdx, LPIdx;

// Entry on page, start timer
function EntryPage()	{Tmr = setTimeout(Cyclic, 100);}

function GotoPage(level,page)
{
 if (level == 1) 
 
 window.location.href= page;
}
function Cyclic()
{
	// Communications counters
	document.getElementById('Comms').value = Errors + '/' + XhrReq;
	// Popup if too many errors
	if (Errors >= 5)	{alert('Too many consecitive errors (5)\nReload Page'); Errors = 0;return;}
	let dly = 250;

	// communication hander: write(read) the goto an 
	switch (Cyc)
	{
		case 0:	// Idle state, do nothing
						break;
		case 1:	// Boot, get actual date/time and write on target 
						d = new Date(); Clk[0] = d.getDay(); Clk[1] = d.getHours(); Clk[2] = d.getMinutes(); Clk[3] = d.getSeconds();
						NxtCyc = 2; Xhr('Write?Clock',true,WriteDone,Clk);
						document.getElementById('Bootmsg').innerHTML = 'Sync Clock';
						Cyc	 = 0;
						break;
		case 2:	// Read the entire Set Structure and decode (if ok, callback cyc++)
						NxtCyc = 3; Xhr('Read?Set',true,DecodeSet);
						document.getElementById('Bootmsg').innerHTML = 'Get Set Structure';
						Cyc	 = 0;
						break;
		case 3:	// Read the entire Act Structure and decode (if ok, callback cyc++)
						NxtCyc = 3;	Xhr('Read?Act',true,DecodeAct);
						document.getElementById('Bootmsg').innerHTML = '';
						dly = 950; Cyc	 = 0;
						break;
		case 10:// writing Manual Data, wait until done (nxt cyc is the previous)
						Xhr('Write?Man',false,WriteDone,TMan); dly = 500;
						document.getElementById('Bootmsg').innerHTML = 'Write Manual Temperature';
						Cyc	 = 0;
						break;
	}
	// AutoClear manual zone selection after 30 secs
	if (TClr > 0) {if (TClr == 1)	document.getElementById('tsm').style.display = 'none'; TClr--;}
	// Reload cycic timer
	if (Tmr) clearTimeout(Tmr);		Tmr = setTimeout(Cyclic, dly);
}

function RdArray() {Cyc = 11;}
function WrArray() {Cyc = 12;}
// Decode the Set data then show 
function DecodeSet(http)
{
	// 8 bit data
	let a = new Uint8Array(http.response); 
	// the Heating Programs
	for (ofs = i = 0; i < NPRG; i++, ofs+=48)	 
		{Pgms[i] = a.slice(i * 48, i * 48 + 48);}
	ZNum = a.slice(ofs,ofs + NZONE * 7); ofs += NZONE * 7;
	// the Zone Names
	for (i = 0; i < NZONE; i++, ofs+=16)	
		{
			j = a.slice(ofs,ofs+16);	
			for (ZNames[i] = '', n = 0; (j[n] != 0) && (n < j.length); n++) ZNames[i] += String.fromCharCode(j[n]);
		}
	// the Zone Temp. offset
	TAdj = Int8Array.from(a.slice(ofs,ofs + NZONE)); ofs += NZONE;
	ofs += 4; // do not use the Set.clock but update the offset
	ofs += 2; // do not use the Kw field
	// Power Features (ip and attr)
	PFeat = a.slice(ofs,ofs + NPWR * 2); ofs += NPWR * 2;
	// the Power names
	for (i = 0; i < NPWR; i++, ofs+=12)	
		{
			j = a.slice(ofs,ofs+12);	
			for (PNames[i] = '', n = 0; (j[n] != 0) && (n < j.length); n++) PNames[i] += String.fromCharCode(j[n]);
		}

	// 16 bit data
	a = new Int16Array(http.response); ofs /= 2;
	// the Power max values
	Wmax = a.slice(ofs, ofs + NPWR);	 ofs+= NPWR;
	// show Heating data set
	for (i = 0; i < NZONE; i++)	 ShowPgm(ZNum[Clk[0] * NZONE + i], i); 
	// show Power data set
	for (i = 0; i < NPWR; i++) 	document.getElementById('Pn' +i).innerHTML = PNames[i]; 

	// Data received,next step
  Cyc = NxtCyc;
}


function DecodeAct(http)
{
	// 8 bit data
	a = new Uint8Array(http.response);						
	ofs  = 0;
	Clk  = a.slice(ofs, ofs + 4); ofs += 4;
	TAct = a.slice(ofs,ofs + NZONE + 1); ofs += NZONE + 1;
	TMan = a.slice(ofs,ofs + NZONE * 2); ofs += NZONE * 2;
	InOut= a.slice(ofs,ofs + NINOUT); ofs += NINOUT;
	Mins =  Clk[1] * 60 + Clk[2]; PIdx = Math.floor(Clk[1] * 2 + Clk[2] / 30);
	// update day of week and time
	document.getElementById('Now').value = Days[Clk[0]] + '  ' + Clk[1] + ':' + Clk[2].toString().padStart(2,'0') + '.' + Clk[3];
	document.getElementById('Tex').value = (TAct[NZONE]/10).toFixed(1) + '\xb0c' 
	// show heat data
	for (i = 0; i < NZONE; i++)			ShowHeat(ZNum[Clk[0] * NZONE + i], i);
	Fi = a.slice(ofs,ofs + NPWR); ofs += NPWR;
	// 16 bit data
	a = new Int16Array(http.response); ofs /= 2;	
	W = a.slice		(ofs,ofs + NPWR); ofs += NPWR;
	Wpk = a.slice	(ofs,ofs + NPWR); ofs += NPWR;
	A = a.slice		(ofs,ofs + NPWR); ofs += NPWR;
	V = a.slice		(ofs,ofs + NPWR); ofs += NPWR;
	// refresh PGM graphics when changed time segment
	for (i = 0; i < NZONE; i++)
		{
			if ((PIdx != LPIdx) || (LtMan[i]  && !TMan[i*2]))		ShowPgm(ZNum[Clk[0] * NZONE + i], i); 
			LtMan[i] = TMan[i*2];
		}
	LPIdx = PIdx;
	// show power meters
	if (W[0] != undefined)	for (i = 0; i < NPWR; i++)			ShowPwr(i);
  Cyc = NxtCyc;
} 

// callback when pushed Manual/Auto button 
function ManAutoBtn(zone)
{
	ZSel = zone; 
	if (!TMan[zone*2])	
		{window.location.href='#tsm'; document.getElementById('tsm').style.display = 'inline-block';	TClr = 60;}
		else 							{TMan[zone*2] = 0; Cyc = 10;}
}

// callback of any write data , return back to ciclic call state
function WriteDone(http)  {Cyc = NxtCyc;}

// callback when pushed "confirm" of manual temp. button
function SetTman()
{
	TMan[ZSel*2] = document.getElementById('tman').value; 
	TMan[ZSel*2 +1] = document.getElementById('tset').value * 10;
	document.getElementById('tsm').style.display = 'none';	Cyc = 10;
}

// show the heating program (miniaturized)
function ShowPgm(num,zone)
{
	let pgm = Pgms[num]; if (zone == undefined) zone = num;
	document.getElementById('Name' + Number(zone)).innerHTML = ZNames[zone];
	c = document.getElementById('Bar' + Number(zone)).getContext('2d');
	h = c.canvas.clientHeight; w = c.canvas.clientWidth;	c.clearRect(0,0,w,h);	
	c.fillStyle = 'red'; w = w / 48;
	// build graphic
	for (n=0; n < pgm.length; n++)		c.fillRect(n * w, h, w, -pgm[n] * h / 256);
	// Actual time is a vertical blue line
	c.strokeStyle = 'blue'; c.beginPath(); c.moveTo(PIdx * w, 0); c.lineTo(PIdx * w, h); c.stroke();
}

// show the heating data on table
function ShowHeat(num,zone)
{
	if (zone == undefined) zone = num;
	ta = TAct[zone]; ts = Pgms[num][PIdx] + TAdj[zone]; if (TMan[zone*2]) ts = TMan[zone*2+1]; 
  e = document.getElementById('Temp' + Number(zone));	
	if (ta < ts)	e.style.color ='red'; 
		else				e.style.color ='blue'; 
	e.innerHTML = (ta/10).toFixed(1) + ' / ' + (ts/10).toFixed(1);
	e = document.getElementById('Time' + Number(zone)); 
	if (TMan[zone*2]) 
		{e.innerHTML = TMan[zone*2]; document.getElementById('Btn' + Number(zone)).value = 'Automatico';}
	 else
		{
			document.getElementById('Btn' + Number(zone)).value = 'Manuale';
			for (n = PIdx; n < 48; n++) if (Pgms[num][n] != (ts - TAdj[zone])) break;		// n contain the first changed value
			if (n != 48) e.innerHTML = n * 30 - Mins ; else e.innerHTML = '';
		}
	e = document.getElementById('On'+ zone); 
	if (InOut[0] & (1 << zone))		e.style.visibility = 'visible'; 
		else												e.style.visibility = 'hidden';
}

// show the Power data 
function ShowPwr(num)
{
	if (!PFeat[num * 2] || !PFeat[num * 2 + 1]) return;
	c = document.getElementById('Pwr' + Number(num)).getContext('2d');
	h = c.canvas.clientHeight;	wh = c.canvas.clientWidth - 2; 
	sc = h / Wmax[num];					c.clearRect(0,0,wh,h);	c.font = "bold 16px Arial";
	w = 16;		wa = W[num] / 10; wp = Wpk[num] / 10;			wm = Wmax[num];
	if (wa < (wm * 0.75)) c.fillStyle = 'green'; 
		else								c.fillStyle = 'red'; wh--;
	c.textAlign = 'right';	c.fillRect(0, h, w, -wa * sc);	c.fillText(wa.toFixed(1) + ' W', wh, h - 62); 
	c.fillStyle = 'gray'; c.fillRect(0, h - wa * sc, w , -(wp - wa) * sc);	c.fillText(wp.toFixed(1) + ' W', wh, h - 82);
	c.fillStyle = 'blue'; 
	c.fillText((V[num]/10).toFixed(1)   + ' V', wh, h - 42);
	c.fillText((A[num]/1000).toFixed(2) + ' A', wh, h - 22); 
	c.fillText((Fi[num]/100).toFixed(2) + ' F', wh, h-2);
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


