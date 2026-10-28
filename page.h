#pragma once
#include <Arduino.h>

const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>AgriSense</title>
<style>
:root{--g:#2e7d32;--bg:#eef2ee;--mut:#667}
*{box-sizing:border-box}
body{font-family:system-ui,sans-serif;margin:0;background:var(--bg);color:#222}
header{background:var(--g);color:#fff;padding:14px 16px}
header h1{margin:0;font-size:20px}
header small{opacity:.85}
main{max-width:780px;margin:auto;padding:12px}
section{background:#fff;border-radius:12px;padding:14px;margin-bottom:14px;box-shadow:0 1px 3px rgba(0,0,0,.08)}
h2{margin:0 0 10px;font-size:16px;color:var(--g)}
h2 span{background:var(--g);color:#fff;border-radius:50%;display:inline-block;width:22px;height:22px;text-align:center;line-height:22px;font-size:13px;margin-right:6px}
.grid{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}
@media(max-width:520px){.grid{grid-template-columns:repeat(2,1fr)}}
.tile{border-radius:10px;padding:10px;color:#fff}
.tile b{display:block;font-size:22px}
.tile span{font-size:12px;opacity:.9}
.row{margin-top:10px;font-size:14px;display:flex;flex-wrap:wrap;gap:8px;align-items:center}
.badge{display:inline-block;padding:2px 9px;border-radius:10px;font-size:12px;color:#fff}
.on{background:var(--g)} .off{background:#999}
.bar{height:14px;background:#e3e8e3;border-radius:7px;overflow:hidden;margin:6px 0}
.bar div{height:100%;transition:width .4s}
canvas{width:100%;height:270px;background:#fafcfa;border:1px solid #e0e6e0;border-radius:8px;margin-top:10px}
.legend{font-size:12px;display:flex;flex-wrap:wrap;gap:12px;margin-top:6px}
.legend i{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:4px}
button,a.btn{display:inline-block;padding:9px 14px;background:var(--g);color:#fff;border:none;border-radius:8px;font-size:14px;text-decoration:none;cursor:pointer}
button.red{background:#c62828} button.alt{background:#1565c0}
button:disabled{opacity:.6}
input[type=text]{padding:8px;border:1px solid #bbb;border-radius:6px;width:210px;max-width:100%}
small,.mut{color:var(--mut);font-size:12px}
.pill{display:inline-block;padding:3px 12px;border-radius:12px;color:#fff;font-weight:600;font-size:13px}
.s-ok{background:#2e7d32}.s-info{background:#1565c0}.s-soon{background:#ef6c00}.s-urgent{background:#c62828}
.act{border-left:5px solid #ccc;background:#f7f9f7;padding:8px 10px;margin-top:8px;border-radius:0 8px 8px 0}
.act.urgent{border-color:#c62828}.act.soon{border-color:#ef6c00}.act.info{border-color:#1565c0}.act.ok{border-color:#2e7d32}
.act b{display:block;font-size:14px}.act span{font-size:13px;color:#444}
table{width:100%;border-collapse:collapse;font-size:12px;margin-top:10px}
th,td{padding:4px 6px;text-align:right;border-bottom:1px solid #eee}
th:first-child,td:first-child{text-align:left}
th{background:#f1f5f1;position:sticky;top:0}
.tw{max-height:260px;overflow:auto;border:1px solid #e5e9e5;border-radius:8px}
details{margin-top:10px;font-size:13px}
</style>
</head>
<body>
<header><h1>AgriSense</h1><small>Logging runs on the ESP32 itself - closing this page does not stop it.</small></header>
<main>

<section>
  <h2><span>1</span>Live monitor</h2>
  <div class="grid">
    <div class="tile" style="background:#d84315"><b id="vTemp">--</b><span>Temp</span></div>
    <div class="tile" style="background:#1565c0"><b id="vHum">--</b><span>Humidity</span></div>
    <div class="tile" style="background:#2e7d32"><b id="vSoil">--</b><span>Soil moisture</span></div>
    <div class="tile" style="background:#f9a825"><b id="vLight">--</b><span>Light</span></div>
  </div>
  <div class="row" id="acts"></div>
  <div class="row" style="display:block">
    Reservoir (estimated): <b id="vRes">--</b>
    <div class="bar"><div id="resBar" style="width:0"></div></div>
    <button class="alt" onclick="refilled()">Reservoir refilled</button>
  </div>
  <div class="row"><small id="vInfo"></small></div>
  <canvas id="chart"></canvas>
  <div class="legend">
    <span><i style="background:#2e7d32"></i>Soil %</span>
    <span><i style="background:#1565c0"></i>Humidity %</span>
    <span><i style="background:#f9a825"></i>Light %</span>
    <span><i style="background:#d84315"></i>Temp &deg;C (right axis)</span>
    <span><i style="background:rgba(21,101,192,.3)"></i>Pump running</span>
  </div>
</section>

<section>
  <h2><span>2</span>AI advisor</h2>
  <small>Runs <b>advisor.py</b> on a PC / Pi on the same WiFi. Enter that machine's address once (shown when you start the script).</small>
  <div class="row">
    <input type="text" id="advAddr" placeholder="e.g. 192.168.1.50:5000">
    <button id="advBtn" onclick="askAdvisor()">Ask AI advisor</button>
  </div>
  <div id="advOut" style="margin-top:10px"></div>
</section>

<section>
  <h2><span>3</span>Data log</h2>
  <div class="row">
    <a class="btn" href="/download">Download CSV</a>
    <button class="red" onclick="flushLog()">Flush log</button>
  </div>
  <div class="row"><small id="logInfo"></small></div>
  <div class="tw"><table id="tbl"></table></div>
</section>

</main>
<script>
const $=id=>document.getElementById(id);
const esc=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const badge=(n,on)=>'<span class="badge '+(on?'on':'off')+'">'+n+' '+(on?'ON':'OFF')+'</span>';

async function refreshLive(){
  try{
    const d=await (await fetch('/data')).json();
    $('vTemp').innerHTML=d.temp+'&deg;C';
    $('vHum').textContent=d.hum+'%';
    $('vSoil').textContent=d.soil+'%';
    $('vLight').textContent=d.light+'%';
    $('acts').innerHTML=badge('Pump',d.pump)+badge('LED',d.led)+badge('Fan',d.fan);
    const pct=Math.max(0,Math.min(100,d.ml/d.cfg.cap*100));
    $('vRes').textContent='~'+d.ml+' ml of '+d.cfg.cap+' ml ('+Math.round(pct)+'%)';
    $('resBar').style.width=pct+'%';
    $('resBar').style.background=pct<=20?'#c62828':pct<=40?'#ef6c00':'#2e7d32';
    $('vInfo').textContent='WiFi: '+d.ssid+' ('+d.rssi+' dBm) | '+d.ip+' | '+d.time;
    $('logInfo').textContent='Log file: '+(d.logBytes/1024).toFixed(1)+' KB | Storage used: '+(d.fsUsed/1024).toFixed(0)+' / '+(d.fsTotal/1024).toFixed(0)+' KB';
  }catch(e){console.error(e);}
}

function drawChart(rows){
  const c=$('chart'),dpr=window.devicePixelRatio||1,W=c.clientWidth,H=270;
  c.width=W*dpr;c.height=H*dpr;
  const g=c.getContext('2d');g.setTransform(dpr,0,0,dpr,0,0);g.clearRect(0,0,W,H);
  g.font='11px sans-serif';
  if(rows.length<2){g.fillStyle='#888';g.fillText('Waiting for logged readings (1 per minute)...',12,30);return;}
  const L=32,R=34,T=12,B=24,w=W-L-R,h=H-T-B;
  const X=i=>L+i/(rows.length-1)*w, Y=v=>T+h-v/100*h;
  g.fillStyle='rgba(21,101,192,.12)';
  rows.forEach((r,i)=>{if(r.pump){const a=X(Math.max(0,i-.5)),b=X(Math.min(rows.length-1,i+.5));g.fillRect(a,T,b-a,h);}});
  g.strokeStyle='#e2e8e2';g.lineWidth=1;
  for(let k=0;k<=5;k++){
    const y=T+h-k/5*h;
    g.beginPath();g.moveTo(L,y);g.lineTo(L+w,y);g.stroke();
    g.fillStyle='#555';g.textAlign='right';g.fillText(k*20,L-4,y+4);
    g.textAlign='left';g.fillStyle='#d84315';g.fillText(k*10,L+w+4,y+4);
  }
  g.fillStyle='#555';g.textAlign='center';
  [0,Math.floor((rows.length-1)/2),rows.length-1].forEach(i=>g.fillText(rows[i].ts.slice(11,16),Math.min(Math.max(X(i),L+14),L+w-14),H-6));
  const S=[['soil','#2e7d32'],['hum','#1565c0'],['light','#f9a825'],['temp','#d84315']];
  S.forEach(([k,col])=>{
    g.strokeStyle=col;g.lineWidth=2;g.beginPath();
    rows.forEach((r,i)=>{const v=Math.max(0,Math.min(100,k==='temp'?r[k]*2:r[k]));i?g.lineTo(X(i),Y(v)):g.moveTo(X(i),Y(v));});
    g.stroke();
  });
}

function drawTable(rows){
  const last=rows.slice(-15).reverse();
  let h='<tr><th>Time</th><th>Temp</th><th>Hum</th><th>Soil</th><th>Light</th><th>Pump</th><th>LED</th><th>Fan</th><th>Tank ml</th></tr>';
  last.forEach(r=>{h+='<tr><td>'+r.ts.slice(5,16)+'</td><td>'+r.temp+'</td><td>'+r.hum+'</td><td>'+r.soil+'</td><td>'+r.light+'</td><td>'+(r.pump?'ON':'-')+'</td><td>'+(r.led?'ON':'-')+'</td><td>'+(r.fan?'ON':'-')+'</td><td>'+r.ml+'</td></tr>';});
  $('tbl').innerHTML=last.length?h:'<tr><td>No readings logged yet.</td></tr>';
}

async function refreshHistory(){
  try{const rows=await (await fetch('/history')).json();drawChart(rows);drawTable(rows);}catch(e){console.error(e);}
}

async function refilled(){
  if(!confirm('Confirm the reservoir has been refilled to full?'))return;
  await fetch('/refilled',{method:'POST'});refreshLive();
}

async function flushLog(){
  if(!confirm('Delete ALL logged data from the ESP32? Download the CSV first if you need it.'))return;
  await fetch('/flush',{method:'POST'});
  refreshLive();
  alert('Log flushed.');
}

$('advAddr').value=localStorage.getItem('advAddr')||'';
async function askAdvisor(){
  let addr=$('advAddr').value.trim().replace(/^https?:\/\//,'').replace(/\/+$/,'');
  const out=$('advOut'),btn=$('advBtn');
  if(!addr){out.innerHTML='<small>Enter the advisor address first.</small>';return;}
  localStorage.setItem('advAddr',addr);
  btn.disabled=true;btn.textContent='Thinking...';
  const ctl=new AbortController();const to=setTimeout(()=>ctl.abort(),15000);
  try{
    const r=await fetch('http://'+addr+'/advice?esp='+encodeURIComponent(location.host),{signal:ctl.signal,cache:'no-store'});
    const j=await r.json();
    if(j.error){out.innerHTML='<div class="act urgent"><b>Advisor error</b><span>'+esc(j.error)+'</span></div>';return;}
    let h='<span class="pill s-'+esc(j.status)+'">'+esc(j.status.toUpperCase())+'</span> <small>'+esc(j.generated)+'</small>';
    h+='<p style="margin:8px 0;font-size:14px">'+esc(j.summary)+'</p>';
    j.actions.forEach(a=>{h+='<div class="act '+esc(a.priority)+'"><b>'+esc(a.title)+'</b><span>'+esc(a.detail)+'</span></div>';});
    h+='<details><summary>Components the advisor knows about</summary><ul>';
    j.components.forEach(c=>{h+='<li><b>'+esc(c.part)+'</b> - '+esc(c.role)+' <small>['+esc(c.pins)+']</small></li>';});
    h+='</ul></details>';
    out.innerHTML=h;
  }catch(e){
    out.innerHTML='<div class="act urgent"><b>Can\'t reach the advisor at '+esc(addr)+'</b><span>Is advisor.py running on your PC, on the same WiFi? Check the address and any firewall prompt.</span></div>';
  }finally{clearTimeout(to);btn.disabled=false;btn.textContent='Ask AI advisor';}
}

refreshLive();refreshHistory();
setInterval(refreshLive,5000);
setInterval(refreshHistory,30000);
window.addEventListener('resize',refreshHistory);
</script>
</body>
</html>
)rawliteral";
