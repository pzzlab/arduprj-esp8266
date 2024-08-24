const NPWR = 2, NMIN = 15, SAMPD = 1440/NMIN, SAMPM = 24*31, SAMPY = 366, SHOUR = 60 / NMIN; 
var Tmr, Errors = 0, XhrReq = 0, Cyc = 1, Nxtcyc = 0, EveryN = 0, Clk ={};
var Chart, Wh = [0,0], LogTyp = 0, X = 0, Y = 0, Lmin = 0;


// Entry on page, start timer
function EntryPage()	
{
	Chart = document.getElementById('chart').getContext('2d'); 
	Tmr = setTimeout(Cyclic, 100);
}



function Cyclic()
{
	// Communications counters
	document.getElementById('comms').value = Errors + '/' + XhrReq;
	// Popup if too many errors
	if (Errors >= 10)	{alert('Too many consecitive errors (5)\nReload Page'); Errors = 0;return;}
	// delay during boot is lower
	if (Cyc >= 10) dly = 500; else dly = 250;
	// update log @ every minute
	if (!LogTyp && (Clk[4] != Lmin))	NxtCyc = 20;
	 else															NxtCyc = 10;	
	Lmin = Clk[4];
	// communication hander: write(read) the goto an 
	switch (Cyc)
	{
		case 0:		// Idle state, do nothing
						break;
		case 1:		// Get Years Log Names
						NxtCyc = 10;	
						Xhr('Read?LstY',false,LstY); 
						document.getElementById('msg').innerHTML = 'Getting Years List';
						Cyc = 0; 
						break;

		case 10:	// Read the  Data block 
						Xhr('Read?Data',true,DecodeData);
//						document.getElementById('msg').innerHTML = '';
						Cyc = 0; 
						break;

		case 20:	// Read the Log (LogTyp define which)
						EveryN = 0; NxtCyc = 10;
						txt = document.getElementById('msg').innerHTML;
						if (LogTyp == 0) Xhr('Read?Log',true,ViewChart);
						 else
							if (LogTyp == 1)	{n = document.getElementById("months").value; Xhr('FRead?/log/m' + n,true,ViewChart);}
							 else							
							  if (LogTyp == 2){n = document.getElementById("years").value;	Xhr('FRead?/log/y' + n,true,ViewChart);}
						Cyc = 0; 
						break;
	
		case 30:// writing Manual Data, wait until done (nxt cyc is the previous)
						Xhr('Write?Man',false,WriteDone,TMan); dly = 500;
						document.getElementById('msg').innerHTML = 'Write Manual Temperature';
						Cyc	 = 0;
						break;
	}
	// Reload cycic timer
	if (Tmr) clearTimeout(Tmr);		Tmr = setTimeout(Cyclic, dly);
}


// callback of "Read?Data"
function DecodeData(http)
{
	v  = 0,			hz	= 0,			pf	= [0,0],	am	= [0,0],	w		= [0,0];
	tm = [0,0], wmi = [0,0],	wmo = [0,0],	p = [0,0],	  q = [0,0];

	// Clock 
	a = new Uint8Array(http.response);	ofs  = 0;
	Clk			= a.slice(ofs, ofs + 8);		ofs += 8;
	// Vaw 
	a	= new Uint16Array(http.response);	ofs /= 2;
	v				= a.slice(ofs, ofs + 1);		ofs ++;
	hz  		= a.slice(ofs, ofs + 1);		ofs ++;
	a = new Int16Array(http.response);
	pf[0]		= a.slice(ofs, ofs + 1);		ofs ++;
	pf[1]		= a.slice(ofs, ofs + 1);		ofs ++;
	am[0]		= a.slice(ofs, ofs + 1);		ofs ++;
	am[1]		= a.slice(ofs, ofs + 1);		ofs ++;
	w[0]		= a.slice(ofs, ofs + 1);		ofs ++; 
	w[1]		= a.slice(ofs, ofs + 1);		ofs ++;
	wmi[0]	= a.slice(ofs, ofs + 1);		ofs ++;
	wmi[1]	= a.slice(ofs, ofs + 1);		ofs ++;
	wmo[0]	= a.slice(ofs, ofs + 1);		ofs ++;
	wmo[1]	= a.slice(ofs, ofs + 1);		ofs ++;
	// Wh
	a = new Uint16Array(http.response);	
	tm[0]		= a.slice(ofs, ofs + 1);		ofs ++;
	tm[1]		= a.slice(ofs, ofs + 1);		ofs ++; 
	a = new Int32Array(http.response);	ofs /= 2;
	p[0]	= a.slice(ofs, ofs + 1);			ofs ++;
	p[1]	= a.slice(ofs, ofs + 1);			ofs ++;
	q[0]	= a.slice(ofs, ofs + 1);			ofs ++;
	q[1]	= a.slice(ofs, ofs + 1);			ofs ++;

	// Show date (Year,Mon,Day,Hour,Min,Sec)
	d = Clk[2] + '/' + Clk[1] + '/' + (Clk[0] + 1900) + ' ' + Clk[3] + ':' + String(Clk[4]).padStart(2,'0') + '.' + String(Clk[5]).padStart(2,'0')
	document.getElementById('now').innerHTML  = d;
	document.getElementById('vhz').innerHTML = ' ' + (v/10).toFixed(1) + 'V / ' + (hz/100).toFixed(2) + 'Hz';
	
	// Show the runtime data
	for (i = 0; i < NPWR; i++)
			{
			 document.getElementById('pf'  + Number(i)).innerHTML = (pf[i]  / 127).toFixed(2);
			 document.getElementById('a'   + Number(i)).innerHTML = (am[i]  / 1000).toFixed(3);
			 document.getElementById('w'   + Number(i)).innerHTML = (w[i]   / 5).toFixed(1);
			 document.getElementById('wmi' + Number(i)).innerHTML = (wmi[i] / 5).toFixed(1);
			 document.getElementById('wmo' + Number(i)).innerHTML	= (wmo[i] / 5).toFixed(1);
			 document.getElementById('t'   + Number(i)).innerHTML = (tm[i]  / 60).toFixed(1);
			 document.getElementById('p'	 + Number(i)).innerHTML = (p[i] / 100).toFixed(0);
			 document.getElementById('q'	 + Number(i)).innerHTML = (q[i] / 100).toFixed(0);
			}
  Cyc	= NxtCyc;
} 

// callback of "Read?Log(M,Y)"
function ViewChart(http)
{

	if (http.status == 404)	{document.getElementById('msg').innerHTML = http.responseURL + ': >> File Not Found'; return;}
	if (http.status != 200) return;
	Y			= [0,0,0,0,0,0];
	if (LogTyp == 0)				{samp = SAMPD;}
	 else	if (LogTyp == 1)	{samp = SAMPM;}
		else if (LogTyp == 2)	{samp = SAMPY;}
	a = new Int32Array(http.response);		ofs = 0;
	Wh[0]		= a.slice(ofs, ofs + samp);		ofs += samp;
	Wh[1]		= a.slice(ofs, ofs + samp);		ofs += samp;
	a = new Uint16Array(http.response);		ofs *=2;
	date		= a.slice(ofs, ofs + 1);			ofs ++;
	PlotChart(0);
	
}

function PlotChart(xd)
{
 msg = document.getElementById('msg');
	if (LogTyp == 0)					{k = 0; samp = SAMPD; grd = 24; txt = 'Giornaliero (15Min) ';}
	 else	if (LogTyp == 1)		{k = 1; samp = SAMPM; grd = 31; txt = 'Mensile (1ora) ';}
		else if (LogTyp == 2)		{k = 1; samp = SAMPY; grd = 12; txt = 'Annuale (1giorno) ';}
	awh = [0,0], max = [1,1],scy = [0,0];
 	h = Chart.canvas.clientHeight / 2;  w = Chart.canvas.clientWidth; 
	dx = Math.floor(w / samp);	xlim = samp * dx; dg = Math.floor(xlim / grd); 
	Chart.canvas.fillStyle = 'black';		Chart.clearRect(0,0,Chart.canvas.clientWidth, h * 2);
	wht = [0,0];
	// scan to calc max values
	for (i = 0; i < samp; i++) 
			{
			 awh[0] = Math.abs(Wh[0][i]);	if (max[0] < awh[0]) max[0] =	awh[0]; wht[0] += Wh[0][i];
			 awh[1] = Math.abs(Wh[1][i]); if (max[1] < awh[1]) max[1] = awh[1]; wht[1] += Wh[1][i];
			}
	// calc.y scaling then plot bars
	scy[0] = (h-2) / max[0]; scy[1] = (h-2) / max[1];
	xlim -= dx; X += xd * dx; if (X < 0) X = 0; if (X > xlim) X = xlim;
	idx = Math.floor(X / dx); 
	cx = undefined,v;
	for (x = i = 0; i < samp; i++, x += dx)
			{
			 awh[0] = -Math.abs(Wh[0][i]) * scy[0]; awh[1] = -Math.abs(Wh[1][i]) * scy[1];
			 if ((Y < h) && (i == idx))		{Chart.fillStyle = 'gray'; cx = x + 1; v = Wh[0][i];}
				else				 
				 if (Wh[0][i] > 0)	Chart.fillStyle = 'red';
					else							Chart.fillStyle = 'green'; 
			 Chart.fillRect(x, h,   dx, awh[0]);
			 if ((Y > h) && (i == idx))		{Chart.fillStyle = 'gray'; cx = x + 1; v = Wh[1][i];}
				else
				 if (Wh[1][i] > 0)	Chart.fillStyle = 'red';		
					else							Chart.fillStyle = 'green'; 
			 Chart.fillRect(x, h*2,   dx, awh[1]);
			}
	// horizontal references
	Chart.font = '10px arial'; Chart.textAlign = 'center'; Chart.fillStyle = 'yellow';
	// Info chart
	msg.innerHTML = txt + ' ---- Totale zona A: ' + (wht[0] / 1000).toFixed(2) + ' KWh  ----   Totale zona B:  ' + (wht[1] / 1000).toFixed(2) + 'KWh';

	for (g = dg, i = 1; i < grd; i++, g += dg)	Chart.fillText(i+k,g,h + 3);	
	// show values onclick(now else text may be overwritten)
		if (cx)
		{
			Chart.fillStyle = 'white';	Chart.textAlign = 'left';
			if (LogTyp == 0)	{m = (idx +1 ) * NMIN; hr = Math.floor(m / 60); m = Math.floor(m % 60); t = hr + '.' + String(m).padStart(2,'0');}
			 else 
				if (LogTyp == 1)	{t = 'G: ' + Math.floor(idx / dg + 1) + ' H ' + Math.floor(idx % dg + 1);}
				 else
				  if (LogTyp == 2)	{t = 'G: ' + (idx + 1); }
			if (Math.abs(v) >= 1000) v = (v/1000).toFixed(2) + ' Kwh'; else v = v + ' Wh';
			if (cx > w - 70) Chart.textAlign = 'right';
			Chart.fillText(t + ' = ' + v, cx, Y);
		}
}

// callback of canvas "onclick"
function ChartClick()
{
 rect = Chart.canvas.getBoundingClientRect();
 X = window.event.clientX - rect.left; Y = window.event.clientY - rect.top;
 PlotChart(0);  
}

// callback of "Read?LstD"
function LstD(http)  
{
	sel = document.getElementById("days");
	a = http.responseText.split(";");
	for (i = 0; i < a.length; i++)	
		{opt = document.createElement("option"); opt.text = a[i]; sel.add(opt);}
	Cyc = NxtCyc;
}

// callback of "Read?LstY"
function LstY(http)  
{
	sel = document.getElementById("years");
	a = http.responseText.split(";");
	for (i = a.length - 1; i >=0 ; i--)	
			{opt = document.createElement("option"); opt.text = a[i]; if (opt.text != "") sel.add(opt);}
//	sel.selectedIndex = sel.length - 1;
	Cyc = NxtCyc;
}

//														---- COMMON FUNCTIONS ----
function SendCmd(cmd)
{
 switch (cmd)
	{
		case   1:		a = 0x0; Xhr('Cmd?1',false,WriteDone,a); EveryN = 100; break;
		case   2:		a = 0x0; Xhr('Cmd?2',false,WriteDone,a); EveryN = 100; break;
		case	16:	  en = new TextEncoder(); a = new Uint8Array(15); 
								ap = en.encode(document.getElementById('pwd').value);
								for (n = 0; n < ap.length; n++) {a[n] = (ap[n] + n) ^ Kw[0];}
								Xhr('Cmd?113',true,Reload,a); break;
	}
}

// callback of any write data , return back to ciclic call state
function WriteDone(http)  {Cyc = NxtCyc;}

// ---- XMLHttpReq generic function ----
function Xhr(cmd,binary,callback,value)
{
	if ((http = new XMLHttpRequest()) != null)
	{
		XhrReq++; if (value != undefined) cmd += '=' + value;
		http.timeout = 200; http.responseType = '';
		http.open("GET", cmd, true); if (binary) http.responseType = 'arraybuffer';
		http.onloadend = function ()	
			{if (!http.status) return; Cyc = NxtCyc; Errors = 0; if (callback)	callback(http);}
		http.ontimeout = function ()  
			{Cyc = NxtCyc; Errors++; console.log('Timeout');}
		if (value != undefined)		http.send(value); 
			else										http.send(); 
	}
	return(true);
}
