const NPWR = 2;
var Tmr, Errors = 0, XhrReq = 0, Cyc = 10, NxtCyc = 0, EveryN = 0;


// Entry on page, start timer
function EntryPage()	
{
 document.getElementById('hlp').checked = false; document.getElementById('dbg').checked = false;
 Tmr = setTimeout(Cyclic, 100);
}

function Cyclic()
{
	// Popup if too many errors
	if (Errors >= 10)	{alert('Too many consecutive errors (10)\nReload Page'); Errors = 0;return;}
	dly = 250;
	switch (Cyc)
	{
		case 0:		// Idle state, do nothing
						break;
		case 10:	// Read the Data block 
						NxtCyc = 10;
						Xhr(200,'Read?Data',true,DecodeData);
						Cyc = 0; 
						break;
		case 20:	// Write Command to device
						NxtCyc = 10;
						SendCmd(1);
						Cyc = 0; 
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
function DecodeData(http)
{
	pf = [0,0], am = [0,0], w = [0,0], wr = [0,0], whi = [0,0], who = [0,0], valid = false;
	// 
	a	= new Uint16Array(http.response);	ofs = 0;
	stat		= a.slice(ofs, ofs + 1);		ofs ++; ofs++;
	v				= a.slice(ofs, ofs + 1);		ofs ++;
	hz  		= a.slice(ofs, ofs + 1);		ofs ++;
	a = new Int16Array(http.response);
	pf[0]		= a.slice(ofs, ofs + 1);		ofs ++;
	pf[1]		= a.slice(ofs, ofs + 1);		ofs ++;
	a = new Uint16Array(http.response);
	am[0]		= a.slice(ofs, ofs + 1);		ofs ++;
	am[1]		= a.slice(ofs, ofs + 1);		ofs ++;
	w[0]		= a.slice(ofs, ofs + 1);		ofs ++; 
	w[1]		= a.slice(ofs, ofs + 1);		ofs ++;
	wr[0]		= a.slice(ofs, ofs + 1);		ofs ++; 
	wr[1]		= a.slice(ofs, ofs + 1);		ofs ++;
	a = new Uint32Array(http.response);	ofs /= 2;
	whi[0]	= a.slice(ofs, ofs + 1);		ofs ++; 
	whi[1]	= a.slice(ofs, ofs + 1);		ofs ++;
	who[0]	= a.slice(ofs, ofs + 1);		ofs ++; 
	who[1]	= a.slice(ofs, ofs + 1);		ofs ++; 
	a = new Uint16Array(http.response);	ofs *= 2;
	comms		= a.slice(ofs, ofs + 1);		ofs ++; 
	lost	  = a.slice(ofs, ofs + 1);		ofs ++; 
	chk   	= a.slice(ofs, ofs + 1);		ofs ++; 
	nack  	= a.slice(ofs, ofs + 1);		ofs ++; 
	a = new Uint8Array(http.response);	ofs *= 2;
	device	= a.slice(ofs, ofs + 1);		ofs ++; 
	valid		= a.slice(ofs, ofs + 1);		ofs ++;
	a = new Int16Array(http.response);	ofs /=2;
	temp	= a.slice(ofs, ofs + 1);			ofs ++; 
	hum		= a.slice(ofs, ofs + 1);			ofs ++;

	// Show Debug Buffer if checked
	d = document.getElementById('dbgtxt');
	if (document.getElementById('dbg').checked)
		{
		  d.style.display = 'block';
			a = new Uint8Array(http.response); l = a.length - 12; // last 12 bytes are counters
			for (s = '' , i = 0; i < l; i++)	
					{s += Number(a[i]).toString(16).padStart(2,'0') + ' ';}
			document.getElementById('dbgtxt').value = s;
		}
	else d.style.display = 'none';
	// Show the runtime data if recognized
	if (valid && ((device == '65') || (device == '78')))	
		{
 		  devtxt = String.fromCharCode(device);
			document.getElementById('volt'		).innerHTML = (v/10).toFixed(1) + ' V  ';
			document.getElementById('hz'			).innerHTML	= (hz/1000).toFixed(2) + ' Hz';
			document.getElementById('stat'		).innerHTML = 'Stat:' + Number(stat).toString(2);
			for (c = 0; c < NPWR; c++)
				{
				 document.getElementById('pf' + c).innerHTML	= (pf[c]/32767).toFixed(2);
				 document.getElementById('a'  + c).innerHTML	= (am[c]/1000).toFixed(2);
				 document.getElementById('w'  + c).innerHTML	= (w[c]/100).toFixed(2);
				 document.getElementById('whi'+ c).innerHTML	= (whi[c]/1000).toFixed(1);
				 document.getElementById('who'+ c).innerHTML	= (who[c]/1000).toFixed(1);
				}
		}
  else 
		{devtxt ='** Unknown **';}
  // Counters
	document.getElementById('dbgcomm').value = 'Comm(Err) ' + XhrReq  + '(' + Errors + '); Req(Lost,Chk,Nack) ' + 
				comms + '(' + lost + ',' + chk + ',' + nack +')';
	document.getElementById('device'  ).value = devtxt + '  T=' + (temp/10).toFixed(1) + '\xb0C  H=' + hum + '% ';

  Cyc	= NxtCyc;
} 
// Callback key down (at enter send cmd)
function Kdn(event)
{
 if (event.key == 'Enter') Cyc = 20;
}
// Callback click (copy selected to cmd line)
function LogClick(idx)
{
 x = document.getElementById("log");
 y = x.options[x.selectedIndex].text;
 c = document.getElementById('cmd');
 c.value = y; c.focus();
}
// Callback change position (sync response text)
function LogChg(idx)
{
 x = document.getElementById("resp");
 x.selectedIndex = idx;
}

//														---- COMMON FUNCTIONS ----
function SendCmd(cmd)
{
 Cyc = 0;
 switch (cmd)
	{
		case   1:		txt = document.getElementById('cmd').value.toLowerCase();
								log = document.getElementById("log");
								opt = document.createElement("option"); opt.text = txt; log.add(opt,0);
	;							Xhr(2000,'Cmd?1',false,CmdResp,txt);
								break;
		case   2:		a = 0x0; Xhr('Cmd?2',false,WriteDone,a);
								break;
		case	16:	  en = new TextEncoder(); a = new Uint8Array(15); 
								ap = en.encode(document.getElementById('pwd').value);
								for (n = 0; n < ap.length; n++) {a[n] = (ap[n] + n) ^ Kw[0];}
								Xhr(200,'Cmd?113',true,Reload,a); 
								break;
	}
}

// callback of any write data
function WriteDone(http)  {Cyc = NxtCyc;}

// callback of SendCmd 
function CmdResp(http)  
	{
	 resp = document.getElementById("resp");
	 opt = document.createElement("option"); opt.text = http.responseText; resp.add(opt,0);	
	 document.getElementById('cmd').value = "";   Cyc = NxtCyc;
	}

// ---- XMLHttpReq generic function ----
function Xhr(tmt,cmd,binary,callback,value)
{
	if ((http = new XMLHttpRequest()) != null)
	{
		XhrReq++; if (value != undefined) cmd += '=' + value;
		http.timeout = tmt; http.responseType = '';
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

function RdTxtFile(file, out)
{
    var rawFile = new XMLHttpRequest();
    rawFile.open("GET", file, false);
    rawFile.onreadystatechange = function ()
    {
        if(rawFile.readyState === 4)
        {
            if(rawFile.status === 200 || rawFile.status == 0)
            {
                out.innerText = rawFile.responseText;
            }
        }
    }
    rawFile.send(null);
}