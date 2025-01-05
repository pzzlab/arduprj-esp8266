const NPWR = 2, NMIN = 15,NHOURW = 2, NHOURM = 8, NDAYY = 4; 
Months = [31,28,31,30,31,30,31,31,30,31,30,31];
var Tmr, Errors = 0, XhrReq = 0, Cyc = 10, Nxtcyc = 10, EveryN = 0, Auth = 0;
var Chart, Wh = [0], VMin = 0, VMax= 0, WMax = [0,0], X = 0, Y = 0, Lmin = 0, LogIdx = 0, ClkMin = -1;
var Http , Day = 0, Month = 0, day = 1, month = 1, GetLog = 0,ReloadLog = 0;
 
// 	---- AT OPEN PAGE ----
function EntryPage()								
{
 Chart = document.getElementById('chart').getContext('2d'); 
 document.getElementById('hlp').checked = false;
 document.getElementById('dbg').checked = false;
 Tmr = setTimeout(Cyclic, 100);				// start cyclic timer
 GetLog = -1;								// Force to read at first cycle
}

//	---- CYCLIC LOOP ----
function Cyclic()										
{
 // Popup if too many errors
 if (Errors >= 15)	{alert('Too many consecutive errors (15)\n(unstable connection?) Try To Reload Page'); Errors = 0;return;}
 // delay during boot is lower
 if (Cyc >= 10) dly = 500; else dly = 250;
 // expiried idle time after selected day/mon, update log
 if (GetLog > 0) 		
  {document.getElementById('msg').value  += '.'; GetLog--; if (GetLog == 1) Cyc = 30;}

 // update log @ every 5 minutes reforcing actual date
 if ((Cyc == 10) && (Lmin != ClkMin) && !(ClkMin % 5))
	{Lmin = ClkMin; NavigateLog(0);}
// {Lmin = ClkMin; LogIdx = Number(Month) * 256 + Number(Day); Cyc = 30;}
	
 // communication hander: write(read) the goto an 
 switch (Cyc)
		{
		 case 0:	// Idle state, do nothing
					break;

		 case 10:	// Read the  Data block 
					NxtCyc = 10;	
					Xhr(250,'Read?Data',true,DecodeData);
					Cyc = 0; 
					if ((GetLog == -1) && (Day != 0)) NavigateLog(0);
					break;

		 case 20:	// Read the Data block and display chart
					NxtCyc = 10;	
					Xhr(250,'Read?Log',true,DecodeLog);
					Cyc = 0; 
					break;

		 case 30:	// Write Command #2 LogIdx codified as byte with month << 8 + day
					NxtCyc = 20;
					SendCmd(10,LogIdx);
					Cyc = 0; 
					break;

		 case 90:	// Write Command #90 (service) to device (parse MCP msg)
					NxtCyc = 10;
					SendCmd(90);
				    Cyc = 0; 
					break;

						
		 case 40:	// writing Manual Data, wait until done (nxt cyc is the previous)
					Xhr(250,'Write?Man',false,WriteDone,'',TMan); dly = 500;
					Cyc	 = 0;
					break;
		}
 // Reload cycic timer
 if (Tmr) clearTimeout(Tmr);	Tmr = setTimeout(Cyclic, dly);
}

function DecodeData(http)	// ---- DECODE AND DISPLAY DATA BLOCK ----
{
 pf=[0,0], am=[0,0], w=[0,0], wr=[0,0], whi=[0,0], who=[0,0], tm=[0,0], wmx=[0,0], el=[0,0], valid=false;
 // TClock decoding
 a = new Uint8Array(http.response);	ofs = 0;
 Year	= a.slice(ofs, ofs + 1);	ofs ++;
 Month	= a.slice(ofs, ofs + 1);	ofs ++;
 Day	= a.slice(ofs, ofs + 1);	ofs ++;
 hour	= a.slice(ofs, ofs + 1);	ofs ++;
 min 	= a.slice(ofs, ofs + 1);	ofs ++;
 sec	= a.slice(ofs, ofs + 1);	ofs ++; 
 a = new Uint16Array(http.response);ofs /=2;	
 yday	= a.slice(ofs, ofs + 1);	ofs ++;
 // Temp,Hum,Flags,_
 temp	= a.slice(ofs, ofs + 1);	ofs ++; 	
 hum	= a.slice(ofs, ofs + 1);	ofs ++;
 flags	= a.slice(ofs, ofs + 1);	ofs +=2; 	
 // Mcp.Data
 stat	= a.slice(ofs, ofs + 1);	ofs +=2;
 v		= a.slice(ofs, ofs + 1);	ofs ++;
 VMin  	= a.slice(ofs, ofs + 1);	ofs ++;	
 VMax 	= a.slice(ofs, ofs + 1);	ofs ++;
 hz  	= a.slice(ofs, ofs + 1);	ofs ++;
 a = new Int16Array(http.response);
 pf[0]	= a.slice(ofs, ofs + 1);	ofs ++;
 pf[1]	= a.slice(ofs, ofs + 1);	ofs ++;
 a = new Uint16Array(http.response);
 am[0]	= a.slice(ofs, ofs + 1);	ofs ++;
 am[1]	= a.slice(ofs, ofs + 1);	ofs ++;
 w[0]	= a.slice(ofs, ofs + 1);	ofs ++; 
 w[1]	= a.slice(ofs, ofs + 1);	ofs ++;
 WMax[0]= a.slice(ofs, ofs + 1); 	ofs ++; 
 WMax[1]= a.slice(ofs, ofs + 1);	ofs ++;
 wr[0]	= a.slice(ofs, ofs + 1);	ofs ++; 
 wr[1]	= a.slice(ofs, ofs + 1);	ofs ++;
 a = new Uint32Array(http.response);ofs /= 2;
 whi[0]	= a.slice(ofs, ofs + 1);	ofs ++; 
 whi[1]	= a.slice(ofs, ofs + 1);	ofs ++;
 who[0]	= a.slice(ofs, ofs + 1);	ofs ++; 
 who[1]	= a.slice(ofs, ofs + 1);	ofs ++; 
 a = new Uint16Array(http.response);ofs *= 2;	
 comms	= a.slice(ofs, ofs + 1);	ofs ++; 
 lost	= a.slice(ofs, ofs + 1);	ofs ++; 
 chk   	= a.slice(ofs, ofs + 1);	ofs ++; 
 nack  	= a.slice(ofs, ofs + 1);	ofs ++; 
 a = new Uint8Array(http.response);	ofs *= 2;
 device	= a.slice(ofs, ofs + 1);	ofs ++; 
 valid	= a.slice(ofs, ofs + 1);	ofs ++;
 a = new Int16Array(http.response); ofs /= 2;
 deb 	= a.slice(ofs, ofs + 17);	ofs +=17;	
 rssi 	= a.slice(ofs, ofs + 1);	ofs ++;		
 // Data
 wmx[0]	= a.slice(ofs, ofs + 1);	ofs ++;
 wmx[1]	= a.slice(ofs, ofs + 1);	ofs ++;
 tm[0]	= a.slice(ofs, ofs + 1);	ofs ++;
 tm[1]	= a.slice(ofs, ofs + 1);	ofs ++;
 el[0]	= a.slice(ofs, ofs + 1);	ofs ++;
 el[1]	= a.slice(ofs, ofs + 1);	ofs ++;
 
 Auth  	= flags & 0x3;	// copy Ws::Auth(copied into Data) to JS var
 ClkMin = Number(min);
 
 // show/hide debug area and diplay values
 d = document.getElementById('dbgtxt');
 if (document.getElementById('dbg').checked)
  {
   d.style.display = 'block';
   for (s = '' , i = 0; i < deb.length; i++)	
	 {s += Number(deb[i]).toString(16).padStart(2,'0') + ' ';}
	  document.getElementById('dbgtxt').value = s;
  }
  else    	d.style.display = 'none';

 // update the datetime,temp and hum.
 document.getElementById('now').innerHTML = Day + "-" + Month + "-" + (Number(Year) + Number(1900)) + " / " + hour + ":" + min.toString().padStart(2,'0') 
 										  + "." + Number(sec).toString().padStart(2,'0');
 document.getElementById('temphum').innerHTML = 'T= ' + (temp/10).toFixed(1) + '\xb0C  H=' + hum + '% ';
 // Show the runtime data if recognized
 if (valid && ((device == '65') || (device == '78')))	
  {
   document.getElementById('dvhz').innerHTML = 'Mcp39F511' + String.fromCharCode(device) + '&nbsp&nbsp&nbsp&nbsp'
   											 + (v/10).toFixed(1) + 'V / ' + (hz/1000).toFixed(2) + 'Hz';
   // Show the runtime data
   for (i = 0; i < NPWR; i++)
     {
	  document.getElementById('pf'	+ Number(i)).innerHTML	= (pf[i]  / 32767).toFixed(2);
	  document.getElementById('a'	+ Number(i)).innerHTML	= (am[i]  / 1000).toFixed(3);
	  document.getElementById('w'	+ Number(i)).innerHTML	= (w[i]   / 100).toFixed(1);
	  document.getElementById('wmx' + Number(i)).innerHTML	= (wmx[i] / 100).toFixed(1);
	  document.getElementById('tm'  + Number(i)).innerHTML	= (tm[i] >> 8) + ":" + (tm[i] & 0x7f) .toString().padStart(2,'0')
	  document.getElementById('t'	+ Number(i)).innerHTML	= (el[i] >> 8) + ":" + (el[i] & 0x7f) .toString().padStart(2,'0')
	  document.getElementById('p'	+ Number(i)).innerHTML	= (whi[i] / 1000).toFixed(0);
	  document.getElementById('q' 	+ Number(i)).innerHTML	= (who[i] / 1000).toFixed(0);
	 }
  } 
  else		{document.getElementById('dvhz').innerHTML = '** Unknown Device **';}
 // show/hide user/maint.mode 		
 if (Auth == 1) 
  {
   document.getElementById('srvc').style.display = "block";
   document.getElementById('usr').style.display = "none";
   document.getElementById('srvcbtn').style.display = "none";
   document.getElementById('usrbtn').style.display = "inline";
   document.getElementById('navbtns').style.display = "none";
  } 
  else			
   {
 	document.getElementById('srvc').style.display = "none"; 
	document.getElementById('usr').style.display = "block";
	document.getElementById('usrbtn').style.display = "none";
	document.getElementById('srvcbtn').style.display = "inline";
	document.getElementById('navbtns').style.display = "inline";
	//	    document.getElementById('txt').innerHTML = String(deb);
   } 
 // Counters
 document.getElementById('dbgcomm').value = 'Comm(Err) ' + XhrReq  + '(' + Errors + '); Req(Lost,Chk,Nack) ' 
										  + comms + '(' + lost + ',' + chk + ',' + nack +') RSSI '+ rssi;
  Cyc = NxtCyc;
} 

function DecodeLog(http)	// ---- DECODE AND DISPLAY Log structure ----
{
 Http = http;		// save for later use
 PlotChart();		
 Cyc	= NxtCyc;	// return to cyclic state
} 

// 	---- LOG AND GRAPH ----
function NavigateLog(idx)						
{
 if (!idx) {day = Number(Day); month = Number(Month);}			// get actual
 day += idx;													// uses local static
 if (day > Months[month-1]) {day = 1; month++;}
  else if (day <= 0) {month--; if (month <= 0) month = 12; day = Months[month-1]; }
 if (month > 12) month = may = 1;
 if (month <= 0) {month = 12; day = 1;}
 LogIdx = month * 256 + day;	
 X = undefined;							
 document.getElementById('nav2').value = String(day + '/' + month);
 document.getElementById('msg').value  = "Wait for selected Data (~ 2.5 secs)";
 GetLog = 5;									// 2.5 secs idle time (@500ms cycle time)
 RealoadLog = 1;								// force to reload log
}

// callback of canvas "onclick"
function ChartClick() 							
{
 rect = Chart.canvas.getBoundingClientRect();
 X = window.event.clientX - rect.left; Y = window.event.clientY - rect.top;
 PlotChart();  
}

// Plot the chart on the canvas
function PlotChart()								
{
 if (Http == undefined) return;
 wmax=[0,0],tm=[0,0,0,0];
 // TClock decoding
 a = new Uint8Array(Http.response); ofs = 0;
 mo		= a.slice(ofs, ofs + 1);	ofs ++;
 dy		= a.slice(ofs, ofs + 1);	ofs ++;
 a = new Uint16Array(Http.response);ofs /=2;	
 vmin  	= a.slice(ofs, ofs + 1);	ofs ++;	
 vmax 	= a.slice(ofs, ofs + 1);	ofs ++;
 wmax[0]= a.slice(ofs, ofs + 1); 	ofs ++; 
 wmax[1]= a.slice(ofs, ofs + 1);	ofs ++;
 tm		= a.slice(ofs, ofs + 4);	ofs +=4;  	// minmax time
 __		= a.slice(ofs, ofs + 55);	ofs +=55; 	// unused filler
 a = new Int16Array(Http.response);	
 Wh	    = a.slice(ofs, ofs + 192);				// log records [96[2]]
 // normalize the values to +/-32767 (-1 will be zero)
 for (i = 0; i < 192; i+=2) {if (Wh[i + 0] < 0) Wh[i + 0]++; if (Wh[i + 1] < 0) Wh[i + 1]++;}

 if (Wh == undefined) return;
 k = 0; grd = 24; maxidx = 192; maxidx2 = maxidx / 2;
 
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
 cx = undefined; v = 0;
 // plot bars
 for (x = i = 0; i < maxidx; i+=2, x += dx)
  {
   awh[0] = -Math.abs(Wh[i + 0]) * scy[0]; awh[1] = -Math.abs(Wh[i + 1]) * scy[1];
   if ((Y < h) && (i == idx))	{Chart.fillStyle = 'gray'; cx = x + 1; v = Wh[i + 0];}
    else				 
	 if (Wh[i + 0] > 0)			 Chart.fillStyle = 'red';
	  else						 Chart.fillStyle = 'green'; 
	   Chart.fillRect(x, h, dx, awh[0]);
   if ((Y > h) && (i == idx))	{Chart.fillStyle = 'gray'; cx = x + 1; v = Wh[i + 1];}
	else
	 if (Wh[i + 1] > 0)			 Chart.fillStyle = 'red';		
	  else						 Chart.fillStyle = 'green'; 
	   Chart.fillRect(x, h * 2, dx, awh[1]);
  }

 Chart.font = '10px arial'; Chart.textAlign = 'center'; Chart.fillStyle = 'yellow';		
 // plot the grid labels	
 for (g = dg, i = 1; i < grd; i++, g += dg)	Chart.fillText(i+k,g,h + 3);									// day/week/month
 t = ''; idx /= 2;	// natural index value	
 if (cx)
  {	
   n  = idx * NMIN; 
   m  = Math.floor(n % 60); 
   hr = Math.floor(n / 60);
   t  = ' -> Energy (' + String(hr) + '.' + String(m).padStart(2,'0');
   if (m == 45) {hr++; m = -15;}
   t += '->' + hr + '.' +String(m + NMIN).padStart(2,'0');
   if (Math.abs(v) >= 1000) v = (v/1000).toFixed(2) + ' Kwh'; else v = v + ' Wh';
   t += ') = ' + v;
  }
 dt  = 'Day: ' + dy + '/' + mo ;
 txt = dt + t +'\n'
			  + 'Vmin' + vmin.toString().padStart(4,' ') 
			  + 'V  (' + (tm[0] >> 8).toString() + '.' + (tm[0] & 0x7f).toString() + ')    '
			  + 'Vmax' + vmax.toString().padStart(4,' ')
			  + 'V  (' + (tm[1] >> 8) + '.' + (tm[1] & 0x7f) + ')\n'
 			  + 'A Tot'+ (wht[0] / 1000).toFixed(3).padStart(7,' ') + ' <Max' + (max[0] / 1000).toFixed(3).toString().padStart(5,' ') 
			  + '>Kwh - PMax ' + Number(wmax[0]).toString().padStart(5,' ') 
			  + 'W  (' + (tm[2] >> 8) + '.' + (tm[2] & 0x7f) + ')\n'
			  + 'B Tot'+ (wht[1] / 1000).toFixed(3).padStart(7,' ') + ' <Max' + (max[1] / 1000).toFixed(3).toString().padStart(5,' ')
			  + '>Kwh - PMax ' + Number(wmax[1]).toString().padStart(5,' ')
			  + 'W  (' + (tm[3] >> 8) + '.' + (tm[3] & 0x7f) + ')';
 document.getElementById('msg').value = txt;
}

// 	---- SERVICE ----
// hlog.OnClick copy content to Cmd
function HClick(idx)								
{
 h 	 =	document.getElementById("hlog");
 txt = h.options[h.selectedIndex].text;
 c	 = document.getElementById('cmd');
 c.value = txt; c.focus();
}

// Hlog.OnChange force same resp line
function HChg(idx)		{document.getElementById("hresp").selectedIndex = idx;}	

// Cmd.OnKeyDown, send the arg. on Cmd
function CmdKdn(event)	{if (event.key == 'Enter') Cyc = 90;}	

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
 					  if (device == '65') 	RdTxtFile('MCP39F511A.def',txt);
					   else
						if (device == '78') RdTxtFile('MCP39F511N.def',txt);
						 else				txt.innerText = "Unknown Device";
					  txt.style.display = 'block';
					 }
					 else					txt.style.display = 'none'; 
					break;
			}
}
//	---- COMMONS ----
// callback of any write data , return back to ciclic call state
function WriteDone(http)  {Cyc = NxtCyc;}	

// send a cmd?with number and argument
function SendCmd(cmd,arg)								
{
 if (cmd == 90)	// service command
   {		
 	txt = document.getElementById('cmd').value.toLowerCase();
	hist = document.getElementById("hlog");
	opt = document.createElement("option"); opt.text = txt; hist.add(opt,0);
	Xhr(2000,'Cmd?90',false,CmdResp,false,txt);
	}
	 else 	    // generic command (Cmd?cmd=arg)
	Xhr(250,'Cmd?'+ cmd, false,WriteDone,false,arg); 
}

// callback of SendCmd
function CmdResp(http)  						
{
 resp = document.getElementById("hresp");
 opt = document.createElement("option"); 			opt.text = http.responseText; resp.add(opt,0);	
 document.getElementById('cmd').value = "";   Cyc = NxtCyc;
}

// Get a text FILE and display on OUT (textarea)
function RdTxtFile(file, out)				
{
 var rawFile = new XMLHttpRequest();
 rawFile.open("GET", file, false);
 rawFile.onreadystatechange = function ()
 {
  if (rawFile.readyState === 4)
     {
      if(rawFile.status === 200 || rawFile.status == 0)	{out.innerText = rawFile.responseText;}
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
	 else						http.send(); 
 }
 return(true);
}
