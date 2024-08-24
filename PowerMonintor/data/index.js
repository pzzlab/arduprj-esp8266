const NPWR = 2, NMIN = 15, SAMPD = 1440/NMIN, SAMPM = 24*31, SAMPY = 366, SHOUR = 60 / NMIN; 
var Tmr, Errors = 0, XhrReq = 0, Cyc = 10, Nxtcyc = 10, EveryN = 0, Auth = 0, CmdArg = "";
var Chart, Wh = [0], LogTyp = 0, X = 0, Y = 0, Lmin = 0,ClkMin = -1;



// Entry on page, start timer
function EntryPage()	
{
	Chart = document.getElementById('chart').getContext('2d'); 
	document.getElementById('hlp').checked = false;
	document.getElementById('dbg').checked = false;
	Tmr = setTimeout(Cyclic, 100);
}



function Cyclic()
{
	// Popup if too many errors
	if (Errors >= 10)	{alert('Too many consecitive errors (10)\nReload Page'); Errors = 0;return;}
	// delay during boot is lower
	if (Cyc >= 10) dly = 500; else dly = 250;
	// update log @ every minute
//	if (!LogTyp && (ClkMin != Lmin))	NxtCyc = 20;
//	 else															NxtCyc = 10;	
	Lmin = ClkMin;
	// communication hander: write(read) the goto an 
	switch (Cyc)
	{
		case 0:		// Idle state, do nothing
						break;
		case 1:		// Get Years Log Names
						NxtCyc = 10;	
						Xhr(200,'Read?LstY',false,WriteDone); 
						Cyc = 0; 
						break;

		case 10:	// Read the  Data block 
						NxtCyc = 10;	
						Xhr(200,'Read?Data',true,DecodeData,false);
						document.getElementById('msg').innerHTML = '';
						Cyc = 0; 
						break;
		case 10:	// Read the Tab block 
						NxtCyc = 10;	
						Xhr(200,'Read?Data',true,DecodeData,true);
						document.getElementById('msg').innerHTML = '';
						Cyc = 0; 
						break;

		case 20:	// Write Command #1 to device (parse MCP msg)
						NxtCyc = 10;
						SendCmd(1);
				    Cyc = 0; 
						break;
		case 21:	// Write Command #2 to device (parsed on CmdCallBack)
						NxtCyc = 10;
						SendCmd(2);
						Cyc = 0; 
						break;

		case 30:	// Start reading log (type and index are parm. 2 & 3 )
						EveryN = 0; NxtCyc = 10;
						if (LogTyp == 0) Xhr(200,'Read?Log&Ggroup&Index',true,ViewChart);
						Cyc = 0; 
						break;

						
		case 40:// writing Manual Data, wait until done (nxt cyc is the previous)
						Xhr200,('Write?Man',false,WriteDone,TMan); dly = 500;
						document.getElementById('msg').innerHTML = 'Write Manual Temperature';
						Cyc	 = 0;
						break;
	}
	// Reload cycic timer
	if (Tmr) clearTimeout(Tmr);		Tmr = setTimeout(Cyclic, dly);
}

// Show/Hide help window
function Help(wich)
{
	switch (wich)
			{
				case 'h':	// help commands
									txt = document.getElementById('hlptxt'); chk = document.getElementById('hlp');
									if (chk.checked) txt.style.display = 'block';
									 else						 txt.style.display = 'none'; 
									break;
				case 'r':	// help regs
									txt = document.getElementById('regstxt'); chk = document.getElementById('regs');
									if (chk.checked) 
										{
 										 if (device == '65') RdTxtFile('MCP39F511A.def',txt);
											else
											 if (device == '78') RdTxtFile('MCP39F511N.def',txt);
												else								txt.innerText = "Unknown Device";
											txt.style.display = 'block';
										}
									 else		txt.style.display = 'none'; 
									break;

	
			}
			
}

// callback of "Read?Data"
function DecodeData(http,complete)
{
	pf=[0,0], am=[0,0], w=[0,0], wr=[0,0], whi=[0,0], who=[0,0], tm=[0,0], wmi=[0,0], wmo=[0,0], valid=false;
	// TClock decoding
	a = new Uint8Array(http.response);ofs = 0;
	year		= a.slice(ofs, ofs + 1);	ofs ++;
	mon			= a.slice(ofs, ofs + 1);	ofs ++;
	day			= a.slice(ofs, ofs + 1);	ofs ++;
	hour		= a.slice(ofs, ofs + 1);	ofs ++;
	min 		= a.slice(ofs, ofs + 1);	ofs ++;
	sec			= a.slice(ofs, ofs + 1);	ofs ++;
	a	= new Uint16Array(http.response);ofs /=2;	
	yday		= a.slice(ofs, ofs + 1);	ofs ++;
	// Temp,Hum,Flags,_
	temp		= a.slice(ofs, ofs + 1);	ofs ++; 	
	hum			= a.slice(ofs, ofs + 1);	ofs ++;
  flags		= a.slice(ofs, ofs + 1);	ofs +=2; 	
	// Mcp.Data
	stat		= a.slice(ofs, ofs + 1);	ofs +=2;
	v				= a.slice(ofs, ofs + 1);	ofs +=3;
	hz  		= a.slice(ofs, ofs + 1);	ofs ++;
	a = new Int16Array(http.response);
	pf[0]		= a.slice(ofs, ofs + 1);	ofs ++;
	pf[1]		= a.slice(ofs, ofs + 1);	ofs ++;
	a = new Uint16Array(http.response);
	am[0]		= a.slice(ofs, ofs + 1);	ofs ++;
	am[1]		= a.slice(ofs, ofs + 1);	ofs ++;
	w[0]		= a.slice(ofs, ofs + 1);	ofs ++; 
	w[1]		= a.slice(ofs, ofs + 1);	ofs +=3;
	wr[0]		= a.slice(ofs, ofs + 1);	ofs ++; 
	wr[1]		= a.slice(ofs, ofs + 1);	ofs ++;
	a = new Uint32Array(http.response);ofs /= 2;	// Wh
	whi[0]	= a.slice(ofs, ofs + 1);	ofs ++; 
	whi[1]	= a.slice(ofs, ofs + 1);	ofs ++;
	who[0]	= a.slice(ofs, ofs + 1);	ofs ++; 
	who[1]	= a.slice(ofs, ofs + 1);	ofs ++; 
	a = new Uint16Array(http.response);ofs *= 2;	// Misc Data
	comms		= a.slice(ofs, ofs + 1);	ofs ++; 
	lost	  = a.slice(ofs, ofs + 1);	ofs ++; 
	chk   	= a.slice(ofs, ofs + 1);	ofs ++; 
	nack  	= a.slice(ofs, ofs + 1);	ofs ++; 
	a = new Uint8Array(http.response);ofs *= 2;
	device	= a.slice(ofs, ofs + 1);	ofs ++; 
	valid		= a.slice(ofs, ofs + 1);	ofs ++;
	deb 		= a.slice(ofs, ofs + 34);	ofs +=34;			// debug buffer
	// Time[2]
	a = new Uint16Array(http.response);ofs /=2;
	tm[0]		= a.slice(ofs, ofs + 1);	ofs ++; 
	tm[1]		= a.slice(ofs, ofs + 1);	ofs ++;
	__			= a.slice(ofs, ofs + 8);	ofs +=8;
	if (complete)
		Wh	  = a.slice(ofs, ofs + 192);					// log records [96[2]]
	
	Auth  	= flags & 0x3;						// copy Ws::Auth(copied into Data) to JS var
	ClkMin  = min;

 // show/hide debug area and diplay values
	d = document.getElementById('dbgtxt');
	if (document.getElementById('dbg').checked)
		{
		  d.style.display = 'block';
			for (s = '' , i = 0; i < deb.length; i++)	
					{s += Number(deb[i]).toString(16).padStart(2,'0') + ' ';}
			document.getElementById('dbgtxt').value = s;
		}
	else d.style.display = 'none';

  // update the datetime,temp and hum.
	document.getElementById('now').innerHTML = day + "-" + mon + "-" + (Number(year) + Number(1900)) + "   " + hour + ":" + min.toString(10).padStart(2,'0') + "."
																						 + sec + '    T= ' + (temp/10).toFixed(1) + '\xb0C  H=' + hum + '% ';

	// Show the runtime data if recognized
	if (valid && ((device == '65') || (device == '78')))	
		{
     document.getElementById('dvhz').innerHTML = String.fromCharCode(device) + '&nbsp&nbsp&nbsp&nbsp' + (v/10).toFixed(1) + 'V / ' + (hz/1000).toFixed(2) + 'Hz';
 	
	// Show the runtime data
	for (i = 0; i < NPWR; i++)
		{
		 document.getElementById('pf'  + Number(i)).innerHTML = (pf[i]  / 127).toFixed(2);
		 document.getElementById('a'   + Number(i)).innerHTML = (am[i]  / 1000).toFixed(3);
		 document.getElementById('w'   + Number(i)).innerHTML = (w[i]   / 5).toFixed(1);
		 document.getElementById('wmi' + Number(i)).innerHTML = (wmi[i] / 5).toFixed(1);
		 document.getElementById('wmo' + Number(i)).innerHTML	= (wmo[i] / 5).toFixed(1);
		 document.getElementById('t'   + Number(i)).innerHTML = (tm[i]  / 60).toFixed(1);
		 document.getElementById('p'	 + Number(i)).innerHTML = (whi[i] / 100).toFixed(0);
		 document.getElementById('q'	 + Number(i)).innerHTML = (who[i] / 100).toFixed(0);
		}

		} 
  else 
		{document.getElementById('dvhz').innerHTML = '** Unknown **';}
	// now
	
	// show/hide user/maint.mode 		
	if (Auth == 1) 
		{
		 document.getElementById('srvc').style.display = "block";
		 document.getElementById('usr').style.display = "none";
		 document.getElementById('srvcbtn').style.display = "none";
		 document.getElementById('usrbtn').style.display = "inline";

		} 
  else			
 		{
		 document.getElementById('srvc').style.display = "none"; 
		 document.getElementById('usr').style.display = "block";
		 document.getElementById('usrbtn').style.display = "none";
		 document.getElementById('srvcbtn').style.display = "inline";
		}

		// Counters
	document.getElementById('dbgcomm').value = 'Comm(Err) ' + XhrReq  + '(' + Errors + '); Req(Lost,Chk,Nack) ' + comms + '(' + lost + ',' + chk + ',' + nack +')';
  Cyc	= NxtCyc;
} 

// hlog.OnClick copy content to Cmd
function HClick(idx)
{
 h =	document.getElementById("hlog");
 txt = h.options[h.selectedIndex].text;
 c = document.getElementById('cmd');
 c.value = txt; c.focus();
}
// Hlog.OnChange force same resp line
function HChg(idx)
{document.getElementById("hresp").selectedIndex = idx;}

// Cmd.OnKeyDown
function CmdKdn(event)	{if (event.key == 'Enter') Cyc = 20;}

// Selectors callback
function Kdn(event)
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

// callback of canvas "onclick"
function ChartClick()
{
 rect = Chart.canvas.getBoundingClientRect();
 X = window.event.clientX - rect.left; Y = window.event.clientY - rect.top;
 PlotChart(0);  
}

function PlotChart(xd)
{
	if (Wh == undefined) return;
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
			 awh[0] = Math.abs(Wh[i][0]);	if (max[0] < awh[0]) max[0] =	awh[0]; wht[0] += Wh[0][i];
			 awh[1] = Math.abs(Wh[i][1]); if (max[1] < awh[1]) max[1] = awh[1]; wht[1] += Wh[1][i];
			}
	// calc.y scaling then plot bars
	scy[0] = (h-2) / max[0]; scy[1] = (h-2) / max[1];
	xlim -= dx; X += xd * dx; if (X < 0) X = 0; if (X > xlim) X = xlim;
	idx = Math.floor(X / dx); 
	cx = undefined,v;
	for (x = i = 0; i < samp; i++, x += dx)
			{
			 awh[0] = -Math.abs(Wh[i][0]) * scy[0]; awh[1] = -Math.abs(Wh[i][1]) * scy[1];
			 if ((Y < h) && (i == idx))		{Chart.fillStyle = 'gray'; cx = x + 1; v = Wh[0][i];}
				else				 
				 if (Wh[i][0] > 0)	Chart.fillStyle = 'red';
					else							Chart.fillStyle = 'green'; 
			 Chart.fillRect(x, h,   dx, awh[0]);
			 if ((Y > h) && (i == idx))		{Chart.fillStyle = 'gray'; cx = x + 1; v = Wh[1][i];}
				else
				 if (Wh[0][1] > 0)	Chart.fillStyle = 'red';		
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


// callback of any write data , return back to ciclic call state
function WriteDone(http)  {Cyc = NxtCyc;}
// callback of SendCmd 
function CmdResp(http)  
	{
	 resp = document.getElementById("hresp");
	 opt = document.createElement("option"); 			opt.text = http.responseText; resp.add(opt,0);	
	 document.getElementById('cmd').value = "";   Cyc = NxtCyc;
	}
//														---- COMMON FUNCTIONS ----
function SendCmd(cmd)
{
 switch (cmd)
	{
		case   1:		txt = document.getElementById('cmd').value.toLowerCase();
								hist = document.getElementById("hlog");
								opt = document.createElement("option"); opt.text = txt; hist.add(opt,0);
								Xhr(2000,'Cmd?1',false,CmdResp,false,txt);
								break;
		case   2:		a = 0x0; Xhr(200,'Cmd?2',false,WriteDone,false,a); EveryN = 100; break;
	}
}
// ---- XMLHttpReq generic function ----
function Xhr(tmt,cmd,binary,callback,param,value)
{
	if ((http = new XMLHttpRequest()) != null)
	{
		XhrReq++; if (value != undefined) cmd += '=' + value;
		http.timeout = tmt; http.responseType = '';
		http.open("GET", cmd, true); if (binary) http.responseType = 'arraybuffer';
		http.onloadend = function ()	
			{if (!http.status) return; Cyc = NxtCyc; Errors = 0; if (callback)	callback(http,param);}
		http.ontimeout = function ()  
			{Cyc = NxtCyc; Errors++; console.log('Timeout');}
		if (value != undefined)		http.send(value); 
			else										http.send(); 
	}
	return(true);
}
// Get a text FILE and display on OUT (textarea)
function RdTxtFile(file, out)
{
    var rawFile = new XMLHttpRequest();
    rawFile.open("GET", file, false);
    rawFile.onreadystatechange = function ()
    {
        if(rawFile.readyState === 4)
        {
            if(rawFile.status === 200 || rawFile.status == 0)
            	{out.innerText = rawFile.responseText;}
        }
    }
    rawFile.send(null);
}