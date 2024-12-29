const NPWR = 2, NMIN = 15,NHOURW = 2, NHOURM = 8, NDAYY = 4; 
Months = [31,28,31,30,31,30,31,31,30,31,30,31];
var Tmr, Errors = 0, XhrReq = 0, Cyc = 10, Nxtcyc = 10, EveryN = 0, Auth = 0;
var Chart, Wh = [0], VMin = 0, VMax= 0, WMax = [0,0], LogTyp = 0, X = 0, Y = 0, Lmin = 0, LogIdx = 0, ClkMin = -1;
 
function EntryPage()								// 	---- AT OPEN PAGE ----
{
	Chart = document.getElementById('chart').getContext('2d'); 
	document.getElementById('hlp').checked = false;
	document.getElementById('dbg').checked = false;
	Tmr = setTimeout(Cyclic, 100);
}
function Cyclic()										//	---- CYCLIC LOOP ----
{
	// Popup if too many errors
	if (Errors >= 10)	{alert('Too many consecitive errors (10)\nReload Page'); Errors = 0;return;}
	// delay during boot is lower
	if (Cyc >= 10) dly = 500; else dly = 250;

	// update log @ every minute
	if ((Cyc == 10) && (ClkMin != Lmin)) {Lmin = ClkMin; Cyc = 20;}
	
	// communication hander: write(read) the goto an 
	switch (Cyc)
	{
		case 0:		// Idle state, do nothing
						break;

		case 10:	// Read the  Data block 
						NxtCyc = 10;	
						Xhr(250,'Read?Data',true,DecodeData,false);
						Cyc = 0; 
						break;

		case 20:	// Read the  Data block then UpdChart
						NxtCyc = 10;	
						Xhr(250,'Read?Data',true,DecodeData,true);
						Cyc = 0; 
						break;

		case 30:	// Write Command #2 (LogType + LogIdx) codified as word
						NxtCyc = 20;
						Xhr(250,'Write?TableIdx',false,WriteDone,'',(LogTyp << 8) + LogIdx); 
						Cyc = 0; 
						break;

		case 90:	// Write Command #90 (service) to device (parse MCP msg)
						NxtCyc = 10;
						SendCmd(90);
				    Cyc = 0; 
						break;

						
		case 40:// writing Manual Data, wait until done (nxt cyc is the previous)
						Xhr(250,'Write?Man',false,WriteDone,'',TMan); dly = 500;
						Cyc	 = 0;
						break;
	}
	// Reload cycic timer
	if (Tmr) clearTimeout(Tmr);		Tmr = setTimeout(Cyclic, dly);
}

function DecodeData(http,showgraph)	// ---- DECODE AND DISPLAY DATA BLOCK ----
{
	pf=[0,0], am=[0,0], w=[0,0], wr=[0,0], whi=[0,0], who=[0,0], tm=[0,0], wmi=[0,0], wmo=[0,0], valid=false;
	// TClock decoding
	a = new Uint8Array(http.response);ofs = 0;
	year		= a.slice(ofs, ofs + 1);	ofs ++; Year = year;
	mon			= a.slice(ofs, ofs + 1);	ofs ++;
	day			= a.slice(ofs, ofs + 1);	ofs ++;
	hour		= a.slice(ofs, ofs + 1);	ofs ++;
	min 		= a.slice(ofs, ofs + 1);	ofs ++;
	sec			= a.slice(ofs, ofs + 1);	ofs ++; ClkMin = sec / 2;
	a	= new Uint16Array(http.response);ofs /=2;	
	yday		= a.slice(ofs, ofs + 1);	ofs ++;
	// Temp,Hum,Flags,_
	temp		= a.slice(ofs, ofs + 1);	ofs ++; 	
	hum			= a.slice(ofs, ofs + 1);	ofs ++;
  flags		= a.slice(ofs, ofs + 1);	ofs +=2; 	
	// Mcp.Data
	stat		= a.slice(ofs, ofs + 1);	ofs +=2;
	v				= a.slice(ofs, ofs + 1);	ofs ++;
	if (showgraph) {VMin = a.slice(ofs, ofs + 1);	ofs ++;	VMax = a.slice(ofs, ofs + 1);	ofs ++;} else ofs += 2;
	hz  		= a.slice(ofs, ofs + 1);	ofs ++;
	a = new Int16Array(http.response);
	pf[0]		= a.slice(ofs, ofs + 1);	ofs ++;
	pf[1]		= a.slice(ofs, ofs + 1);	ofs ++;
	a = new Uint16Array(http.response);
	am[0]		= a.slice(ofs, ofs + 1);	ofs ++;
	am[1]		= a.slice(ofs, ofs + 1);	ofs ++;
	w[0]		= a.slice(ofs, ofs + 1);	ofs ++; 
	w[1]		= a.slice(ofs, ofs + 1);	ofs ++;
	if (showgraph) {WMax[0]	= a.slice(ofs, ofs + 1); ofs ++; WMax[1]	= a.slice(ofs, ofs + 1);	ofs ++;} else ofs += 2;
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
	a = new Int16Array(http.response); ofs /= 2;
	deb 		= a.slice(ofs, ofs + 17);	ofs +=17;		// debug buffer
//	a = new Uint16Array(http.response);ofs /=2;		// Time[2]
	tm[0]		= a.slice(ofs, ofs + 1);	ofs ++; 
	tm[1]		= a.slice(ofs, ofs + 1);	ofs ++;
	rssi 		= a.slice(ofs, ofs + 1);	ofs ++;
	__			= a.slice(ofs, ofs + 7);	ofs +=7;
	a = new Int16Array(http.response);
	Wh	    = a.slice(ofs, ofs + 192);						// log records [96[2]]
	
	Auth  	= flags & 0x3;	// copy Ws::Auth(copied into Data) to JS var
	ClkMin  = min;

	device ='78';	// force for run without device connected

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
	document.getElementById('now').innerHTML = day + "-" + mon + "-" + (Number(year) + Number(1900)) + "   " + hour + ":" + min.toString().padStart(2,'0') + "."
																						 + sec + '    T= ' + (temp/10).toFixed(1) + '\xb0C  H=' + hum + '% ';

	// Show the runtime data if recognized
	if (valid && ((device == '65') || (device == '78')))	
		{
     document.getElementById('dvhz').innerHTML = String.fromCharCode(device) + '&nbsp&nbsp&nbsp&nbsp' + (v/10).toFixed(1) + 'V / ' + (hz/1000).toFixed(2) + 'Hz';
 	
	// Show the runtime data
	for (i = 0; i < NPWR; i++)
		{
		 document.getElementById('pf'  + Number(i)).innerHTML = (pf[i]  / 32767).toFixed(2);
		 document.getElementById('a'   + Number(i)).innerHTML = (am[i]  / 1000).toFixed(3);
		 document.getElementById('w'   + Number(i)).innerHTML = (w[i]   / 100).toFixed(1);
		 document.getElementById('wmi' + Number(i)).innerHTML = (wmi[i] / 100).toFixed(1);
		 document.getElementById('wmo' + Number(i)).innerHTML	= (wmo[i] / 100).toFixed(1);
		 document.getElementById('t'   + Number(i)).innerHTML = (tm[i]  / 60).toFixed(1);
		 document.getElementById('p'	 + Number(i)).innerHTML = (whi[i] / 1000).toFixed(0);
		 document.getElementById('q'	 + Number(i)).innerHTML = (who[i] / 1000).toFixed(0);
		}

		} 
  else 
		{document.getElementById('dvhz').innerHTML = '** Unknown **';}
	
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

		 
	   document.getElementById('txt').innerHTML = String(deb);


		 if (showgraph)	
				{
					// normalize the values to +/-32767 (-1 will be zero)
				 	for (i = 0; i < 192; i+=2) {if (Wh[i + 0] < 0) Wh[i + 0]++; if (Wh[i + 1] < 0) Wh[i + 1]++;}
 				 	PlotChart();
				}
		}

		// Counters
	document.getElementById('dbgcomm').value = 'Comm(Err) ' + XhrReq  + '(' + Errors + '); Req(Lost,Chk,Nack) ' 
																					+ comms + '(' + lost + ',' + chk + ',' + nack +') RSSI '+ rssi;
  Cyc	= NxtCyc;
} 
// 	---- LOG AND GRAPH ----
function NavigateLog(idx)						// set the type of chart then plot

{
	LogTyp = document.getElementById("logtype").selectedIndex;
	LogIdx = idx; X = undefined;
	Cyc    = 30;	// write wich chart (will read nxt cyc)
}

function ChartClick() 							// callback of canvas "onclick"
{
 rect = Chart.canvas.getBoundingClientRect();
 X = window.event.clientX - rect.left; Y = window.event.clientY - rect.top;
 PlotChart();  
}

function PlotChart()								// Plot the chart
{
	if (Wh == undefined) return;
	if (LogTyp == 0)					{k = 0; grd = 24; maxidx = 192; maxidx2 = maxidx / 2;}
	 else	if (LogTyp == 1)		{k = 1; grd = 7;  maxidx = 168; maxidx2 = maxidx / 2;}
		else if (LogTyp == 2)		{k = 1; grd = 31; maxidx = 186; maxidx2 = maxidx / 2;}
 	 	 else if (LogTyp == 3)	{k = 2; grd = 12; maxidx = 184; maxidx2 = maxidx / 2;}
 
	awh = [0,0], max = [1,1], scy = [0,0];
 	h = Chart.canvas.clientHeight / 2;  w = Chart.canvas.clientWidth; 
	dx = Math.floor(w / maxidx2);	xlim = maxidx2 * dx; 	dg = Math.floor(xlim / grd); 
	Chart.canvas.fillStyle = 'black';		Chart.clearRect(0,0,Chart.canvas.clientWidth, h * 2);
	wht = [0,0];
	// scan to calc max values 
	for (i = 0; i < maxidx; i+=2) 
			{
			 awh[0] = Math.abs(Wh[i + 0]); if (max[0] < awh[0]) max[0] = awh[0]; wht[0] += Wh[i + 0];
			 awh[1] = Math.abs(Wh[i + 1]); if (max[1] < awh[1]) max[1] = awh[1]; wht[1] += Wh[i + 1];
			}
	// calc.y scaling then plot bars
	scy[0] = (h-2) / max[0]; scy[1] = (h-2) / max[1];
	xlim -= dx; if (X < 0) X = 0; if (X > xlim) X = xlim;
	idx = Math.floor(X / dx) * 2; // *2 because samp * 2
	cx = undefined,v;
	// plot bars
	for (x = i = 0; i < maxidx; i+=2, x += dx)
			{
			 awh[0] = -Math.abs(Wh[i + 0]) * scy[0]; awh[1] = -Math.abs(Wh[i + 1]) * scy[1];
			 if ((Y < h) && (i == idx))		{Chart.fillStyle = 'gray'; cx = x + 1; v = Wh[i + 0];}
				else				 
				 if (Wh[i + 0] > 0)	Chart.fillStyle = 'red';
					else							Chart.fillStyle = 'green'; 
			 Chart.fillRect(x, h,   dx, awh[0]);
			 if ((Y > h) && (i == idx))		{Chart.fillStyle = 'gray'; cx = x + 1; v = Wh[i + 1];}
				else
				 if (Wh[i + 1] > 0)	Chart.fillStyle = 'red';		
					else							Chart.fillStyle = 'green'; 
			 Chart.fillRect(x, h*2,   dx, awh[1]);
			}

	Chart.font = '10px arial'; Chart.textAlign = 'center'; Chart.fillStyle = 'yellow';		
	// plot the grid labels	
	if (LogTyp != 3) for (g = dg, i = 1; i < grd; i++, g += dg)	Chart.fillText(i+k,g,h + 3);									// day/week/month
	 else
	  {
			for (g = Math.floor(Months[0] * dx / NDAYY), i = 0; i < 11; i++, g += Math.floor(Months[i] * dx / NDAYY))  // year (comp.bisestile)
			{if ((i==2) && !(Year % 4) == 0) Math.floor(g += dx / NDAYY);	Chart.fillText(i+k,g,h + 3);}
		}	

	// Info chart textarea
	t = ''; idx /= 2;	// natural index value	
	if (cx)
			{	
				if (LogTyp == 0)	
					{n = idx * NMIN; m = Math.floor(n % 60); hr = String(Math.floor(n / 60)); t = 'H' + hr + '.' + String(m).padStart(2,'0') + '->' + hr + '.' +String(m + NMIN -1).padStart(2,'0');}
				else 
				if (LogTyp == 1)	
						{n = idx * NHOURW; hr = Math.floor(n % 24); t = 'H' + hr + '->' + Number(hr + NHOURW - 1) +'.59';}
					else
					if (LogTyp == 2)	
							{n = idx * NHOURM; hr = Math.floor(n % 24); t = 'H' + hr + '->' + Number(hr + NHOURM - 1) +'.59';}
						else
						if (LogTyp == 3)	
								{d = idx * NDAYY;  t = 'Day ' + Number(d + 1) + '..' + Number(d + NDAYY);}

					if (Math.abs(v) >= 1000) v = (v/1000).toFixed(2) + ' Kwh'; else v = v + ' Wh';
					t += ' Energy= ' + v;
			}

	txt = 'Vmin' + VMin.toString().padStart(4,' ') + 'V - Vmax' + VMax.toString().padStart(4,' ') + 'V      - '+ t + '\n'
			+ 'A Tot' + (wht[0] / 1000).toFixed(3).padStart(7,' ') + ' <Max' + (max[0] / 1000).toFixed(3).toString().padStart(5,' ') + '>Kwh - PMax' + (WMax[0] / 1000).toFixed(3).padStart(6,' ') + 'Kw\n'
			+ 'B Tot' + (wht[1] / 1000).toFixed(3).padStart(7,' ') + ' <Max' + (max[1] / 1000).toFixed(3).toString().padStart(5,' ') + '>Kwh - PMax' + (WMax[1] / 1000).toFixed(3).padStart(6,' ') + 'Kw\n'
	document.getElementById('msg').value = txt;
}
// 	---- SERVICE ----
function HClick(idx)								// hlog.OnClick copy content to Cmd
{
 h =	document.getElementById("hlog");
 txt = h.options[h.selectedIndex].text;
 c = document.getElementById('cmd');
 c.value = txt; c.focus();
}

function HChg(idx)			{document.getElementById("hresp").selectedIndex = idx;}	// Hlog.OnChange force same resp line

function CmdKdn(event)	{if (event.key == 'Enter') Cyc = 90;}	// Cmd.OnKeyDown, send the arg. on Cmd

function Help(wich)									// Show/Hide help window
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
//	---- COMMONS ----
function WriteDone(http)  {Cyc = NxtCyc;}	// callback of any write data , return back to ciclic call state
function SendCmd(cmd)								// send a cmd?with number and argument
{
	if (cmd == 90)	// service command
		{		
			txt = document.getElementById('cmd').value.toLowerCase();
			hist = document.getElementById("hlog");
			opt = document.createElement("option"); opt.text = txt; hist.add(opt,0);
			Xhr(2000,'Cmd?90',false,CmdResp,false,txt);
		}
	 else 					// generic command (cmd contain other parametes)
	 		Xhr(250,'Cmd?1', false,WriteDone,false,cmd); 
}
function CmdResp(http)  						// callback of SendCmd
	{
	 resp = document.getElementById("hresp");
	 opt = document.createElement("option"); 			opt.text = http.responseText; resp.add(opt,0);	
	 document.getElementById('cmd').value = "";   Cyc = NxtCyc;
	}
function RdTxtFile(file, out)				// Get a text FILE and display on OUT (textarea)
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

// 	---- XMLHttpReq GENERIC ----
function Xhr(tmt,cmd,binary,callback,param,value)
{
	if ((http = new XMLHttpRequest()) != null)
	{
		XhrReq++; if (value != undefined) cmd += '=' + value;
		http.timeout = tmt * 4; http.responseType = '';
		http.open("GET", cmd, true); if (binary) http.responseType = 'arraybuffer';
		http.onloadend = function ()	
			{if (!http.status) return;  Cyc = NxtCyc; Errors = 0; if (callback && (http.status == 200))	callback(http,param);}
		http.ontimeout = function ()  
			{Cyc = NxtCyc; Errors++; console.log('Timeout');}
		if (value != undefined)		http.send(value); 
			else										http.send(); 
	}
	return(true);
}
