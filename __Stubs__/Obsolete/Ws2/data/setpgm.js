const NZONE = 6, NPRG = 8, NPWR = 6;
var		Tmr, Errors = 0, XhrReq = 0, PgmSel, Cyc = 1, NxtCyc = 0, Sel = 0, T1, T2, DaySel, ZoneSel, ToSave = 0;
var		Pgms = {}, ZNum = {}, ZNames = {}, PNames = {}, TAdj = {}, Wmax = {}, Kw = [0x12,0x00];
var		Pgms = {}, ZNum = {}, ZNames = {}, PNames = {}, TAdj = {}, Wmax = {}, Kw = [0x12,0x00];

// Entry on page, start timer
function EntryPage()	{Tmr = setTimeout(Cyclic, 100);}

// exit, if changed any, save zone names

// cyclic section
function Cyclic()
{
	dly = 250;
	// communication hander: write(read) the goto an 
	switch (Cyc)
	{
		case 0:	// Idle state, do nothing
						break;
		case 1:	// Read the entire Set Structure and decode (if ok, callback cyc++)
						NxtCyc = 2; Xhr('Read?Set',true,DecodeSet);
						document.getElementById('Bootmsg').innerHTML = 'Read Set Data';
						Cyc	 = 0;
						break;
		case 2:	// stay here and do nothing (void state)
						document.getElementById('Bootmsg').innerHTML = '';
						break;
		case 10:// Save ZNum
						if (!ToSave) {Cyc = 15; break;}
						if (ToSave & 0x1)
						{
							Cyc = NxtCyc = 11;
							document.getElementById('Bootmsg').innerHTML = 'Saving Zone-Pgm[Day]';
							for(s = '', z = 0; z < NZONE; z++)
								for(d = 0; d < 7; d++) 
									{ZNum[z * 7 + d] = v = document.getElementById('zp' + Number(z) + Number(d)).value; s += v + ',';}
							Xhr('Write?ZNum',false,WriteDone,s);
						break;
						}
		case 11:// Save ZNames
						if (ToSave & 0x2)
						{
							Cyc = NxtCyc = 12;						
							document.getElementById('Bootmsg').innerHTML = 'Saving Zone Names';
							for(s = '', i = 0; i < NZONE; i++) {ZNames[i] = document.getElementById('zname'+ Number(i)).value; s += ZNames[i] + ',';}
							Xhr('Write?ZNames',false,WriteDone,s);
							break;
						}
		case 12:// Save TAdj
						if (ToSave & 0x4)
						{
							Cyc = NxtCyc = 13;						
							document.getElementById('Bootmsg').innerHTML = 'Saving Offsets';
							for(s = '', i = 0; i < NZONE; i++) {TAdj[i] = document.getElementById('ofs'+ Number(i)).value * 10; s += TAdj[i] + ',';}
							Xhr('Write?TAdj',false,WriteDone,s);
							break;
						}
		case 13:// Save PNames
						if (ToSave & 0x8)
						{
							Cyc = NxtCyc = 14;						
							document.getElementById('Bootmsg').innerHTML = 'Saving Power Names';
							for(s = '', i = 0; i < NZONE; i++) {PNames[i] = document.getElementById('pname'+ Number(i)).value; s += PNames[i] + ',';}
							Xhr('Write?PNames',false,WriteDone,s);
							break;
						}
		case 14:// Save PMax[]
						if (ToSave & 0x10)
						{
							Cyc = NxtCyc = 15;						
							document.getElementById('Bootmsg').innerHTML = 'Saving Offsets';
							for(s = '', i = 0; i < NZONE; i++) {Wmax[i] = document.getElementById('pwr'+ Number(i)).value * 1000; s += Wmax[i] + ',';}
							Xhr('Write?PMax',false,WriteDone,s);
							break;
						}

		case 15:// Close this page (then open main page (index.html)
						location.href = "index.html";
						break;
	}

	// Reload cycic timer
	if (Tmr) clearTimeout(Tmr);		Tmr = setTimeout(Cyclic, dly);
}

// Decode the Set data then show (only heat)
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
	// Kw[]
  Kw = a.slice(ofs,ofs+2); ofs += 2;
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
	
	// and show all (graphigs and zone sets)
	ShowPgms();
	// build heating table with actual data
	for (i = 0; i < NZONE; i++)	
		{
		 document.getElementById('zname' + Number(i)).value = ZNames[i];
		 document.getElementById('ofs'   + Number(i)).value = TAdj[i] / 10;
		 document.getElementById('ofsv'  + Number(i)).value = TAdj[i];
		 for (j = 0; j < 7; j++)	document.getElementById('zp'   + Number(i) + Number(j)).value = ZNum[i *7 + j];
		}
	// build power table with actual data
	for (i = 0; i < NPWR; i++)	
		{
		 document.getElementById('pname' + Number(i)).value = PNames[i];
		 document.getElementById('pwr'   + Number(i)).value = Wmax[i] / 1000;
		 document.getElementById('pwrv'  + Number(i)).value = Wmax[i] / 1000;
		}
	// show/hide request password
	if (Kw[1]) 
		{
		 document.getElementById('access').style.display = 'none'; 
		 document.getElementById('selec').style.display = 'block';
		 document.getElementById('testo').innerHTML = 'Cosa vuoi fare?';
	  }
	// Data received,next step
  Cyc = NxtCyc;
}

// show All the programs 
function ShowPgms() {for (i = 0; i < NPRG; i++)	 ShowPgm(i);}

// show the heating program
function ShowPgm(num)
{
	let pgm = Pgms[num];
	if (num != PgmSel) col = 'green'; else col = 'red';									// selected: red, else green
	c = document.getElementById('Bar' + Number(num)).getContext('2d');
	h = c.canvas.height; w = c.canvas.width; c.clearRect(0,0,w,h);	
	c.fillStyle = col; s = w / 48;
	for (n=0; n < pgm.length; n++)	{c.fillRect(n * s, h, s, -pgm[n] * h / 256);}
	// show a vertical blue every 4 hours if the selected
	if (num == PgmSel)
	{
		c.fillStyle = c.strokeStyle = 'blue';	c.font = '10px Arial';
		for (n = 8, p = s*n ; n < 48; n +=8,p = s * n) 
			{
				c.beginPath(); c.moveTo(p, h - 10 ); 
				if (n == 24) c.lineTo(p, h -10 -h / 4); else c.lineTo(p, h -10 -h / 8); c.stroke();
				c.fillText(n / 2, p-4, h-1);		
			}
	}
	c.font = '20px Arial'; c.fillStyle = 'orange'; c.fillText(num, 5, 20);
}

// onclick on radio button (also onclick on)
function SelFun(sel)
{
	Sel = sel; PgmSel = ZoneSel = DaySel = undefined;
	document.getElementById('tmbtn').value = 'Dalle';
	switch(sel)
		{
			case  0:// Hide all and invalidate last selections
							document.getElementById('testo').innerHTML = 'Cosa vuoi fare?';
							document.getElementById('pgms').style.display = 'none';
							document.getElementById('intv').style.display = 'none';
							document.getElementById('zones').style.display = 'none';
							document.getElementById('pwrs').style.display = 'none';
							for (i = 0; i < NZONE; i++) document.getElementById('zname'+ Number(i)).readOnly = true;
							Cyc = 1; 
							break;
			case  1:// unlock program
							document.getElementById('testo').innerHTML = 'Seleziona il programma da modificare';
							document.getElementById('pgms').style.display = 'block';
							document.getElementById('zones').style.display = 'none';
							document.getElementById('pwrs').style.display = 'none';
							break;
			case  2:// unlock program->zone
							document.getElementById('testo').innerHTML = 'Selezionare il giorno nella casella';
							document.getElementById('pgms').style.display = 'none';
							document.getElementById('intv').style.display = 'none';
							document.getElementById('zones').style.display = 'block';
							document.getElementById('pwrs').style.display = 'none';
							for (i = 0; i < NZONE; i++) document.getElementById('zname'+ Number(i)).readOnly = true;
							DaySel = undefined;
							break;
			case  3:// unlock zone-description
							for (i = 0; i < NZONE; i++) document.getElementById('zname'+ Number(i)).readOnly = false;
							document.getElementById('testo').innerHTML = 'Selezionare la descrizione della zona da modificare';
							document.getElementById('pgms').style.display = 'none';
							document.getElementById('intv').style.display = 'none';
							document.getElementById('zones').style.display = 'block';
							document.getElementById('pwrs').style.display = 'none';
							break;
			case  4:// unlock Powers-description
							for (i = 0; i < NZONE; i++) document.getElementById('pname'+ Number(i)).readOnly = false;
							document.getElementById('testo').innerHTML = 'Selezionare la descrizione della zona da modificare';
							document.getElementById('pgms').style.display = 'none';
							document.getElementById('intv').style.display = 'none';
							document.getElementById('zones').style.display = 'none';
							document.getElementById('pwrs').style.display = 'block';
							break;
		}
	// Call writeback sequence 
}



function SelPgmZn(zone,day)	
{
	if (Sel != 2) return; 
	document.getElementById('pgms').style.display = 'block';	
	document.getElementById('testo').innerHTML = 'Selezionare il programma da attivare';
	DaySel = day; ZoneSel = zone;
}

// callback delected program (change color and move focus)
function SelPgm(num) 
{
	PgmSel = num;	
	if (Sel == 1)
	{
		ShowPgms();
		document.getElementById('intv').style.display = 'block';
		document.getElementById('testo').innerHTML = 'Inserire Ora Inizio/fine [30 Min] e Temperatura [0.1\xb0c]';
		document.getElementById('intv').style.display = 'block';
		window.location.href='#intv';
	}
	if ((Sel == 2)	&& (ZoneSel != undefined) && (DaySel != undefined))
	{
		document.getElementById('pgms').style.display = 'none';
		document.getElementById('zp' + Number(ZoneSel) + Number(DaySel)).value = num;
		ZNum[ZoneSel * 7 + DaySel] = num;
		ToSave |= 0x1;
	}
}


// callback of program modified , update the record and show newly on chart
function ModPgm()
{
	tset = document.getElementById('temp');
	time = document.getElementById('tm');
	btn  = document.getElementById('tmbtn');
	if ( btn.value == 'Dalle')	{T1 = time.value / 30; btn.value = 'Alle'; return;}
	T2 = time.value / 30;
	if (T1 > T2) {alert('Ora di fine deve essere > di ora inizio'); return;}
	for (let i = T1; i < T2; i++) Pgms[PgmSel][i] = tset.value * 10;
	Xhr('Write?Pgm' + Number(PgmSel),false,PgmWritten,Pgms[PgmSel]);
	btn.value = 'Dalle';
}

function SendCmd(cmd)
{
 switch (cmd)
	{
		case   1:		a = 0x7f; Xhr('Cmd?1',false,WriteDone,a); break;
								break;
		case	16:	  en = new TextEncoder(); a = new Uint8Array(15); 
								ap = en.encode(document.getElementById('pwd').value);
								for (n = 0; n < ap.length; n++) {a[n] = (ap[n] + n) ^ Kw[0];}
								Xhr('Cmd?113',true,Reload,a); break;
	}
}


function Reload(http)	{location.reload();}

// callback of program modified,update the chart thenreturn back to ciclic call state
function PgmWritten(http)  {ShowPgm(PgmSel); Cyc = NxtCyc;}

// callback of any generic write data , return back to NxtCyc
function WriteDone(http)  {Cyc = NxtCyc;}

// ---- XMLHttpReq generic function ----
function Xhr(cmd,binary,callback,value)
{
	if ((http = new XMLHttpRequest()) != null)
	{
		XhrReq++; if (value != undefined) cmd += '=' + value;
		http.timeout = 450; http.responseType = '';
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

