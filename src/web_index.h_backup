#pragma once
#include <Arduino.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart Reflow Plate</title>
<style>
:root{color-scheme:dark}
body{font-family:system-ui,-apple-system,sans-serif;background:#121212;color:#fff;text-align:center;margin:0;padding:10px}
.card{background:#1e1e1e;border-radius:12px;padding:14px;margin:10px auto;max-width:520px;box-shadow:0 4px 12px rgba(0,0,0,.5);text-align:left}
h1{color:#ff5722;font-size:20px;margin:0 0 6px;text-align:center}
h3{margin:0 0 10px;font-size:16px;color:#bbb}
.temp{font-size:54px;font-weight:700;color:#00e676;margin:5px 0;text-align:center}
button{background:#ff5722;border:0;color:#fff;padding:10px 20px;font-size:15px;border-radius:6px;cursor:pointer;margin:4px;transition:0.2s}
button:hover{filter:brightness(1.1)}
button.stop{background:#d32f2f}button.save{background:#0288d1}button.sm{padding:7px 12px;font-size:13px}
button.preset{background:#333;border:1px solid #555;padding:10px 14px;text-align:left;width:100%;margin:5px 0}
button.preset:hover{background:#444;border-color:#ff5722}
input,select{padding:9px;border-radius:6px;border:1px solid #444;background:#292929;color:#fff;margin:4px 0;width:100%;box-sizing:border-box}
label{font-size:13px;color:#aaa;display:block;margin-top:6px}
.badge{display:inline-block;padding:3px 9px;border-radius:12px;font-size:12px;background:#333;margin:2px;color:#aaa}
#fault{display:none;background:#b71c1c;border-radius:8px;padding:10px;margin:8px 0;font-weight:700;text-align:center}
canvas{width:100%;height:180px;display:block;background:#181818;border-radius:6px}
.status-box{background:#181818;padding:10px;border-radius:8px;margin:8px 0;font-size:13px;border-left:4px solid #00e676}
details{background:#252525;padding:10px;border-radius:8px;margin:8px 0;cursor:pointer}
summary{font-weight:bold;color:#ff5722}
table{width:100%;border-collapse:collapse;margin-top:8px;font-size:13px}
th,td{border:1px solid #444;padding:6px;text-align:left}
th{background:#333;color:#ff5722}

.tabs{display:flex;justify-content:center;gap:4px;max-width:520px;margin:0 auto 10px;overflow-x:auto}
.tab-btn{background:#252525;border:1px solid #333;color:#888;padding:8px 12px;border-radius:8px;cursor:pointer;flex:1;font-size:13px;font-weight:600;white-space:nowrap}
.tab-btn.active{background:#ff5722;color:#fff;border-color:#ff5722}
.tab-content{display:none}
.tab-content.active{display:block}
</style></head><body>

<div class="tabs">
  <button class="tab-btn active" onclick="tab(0)">🔥 Control</button>
  <button class="tab-btn" onclick="tab(1)">📋 Profiles</button>
  <button class="tab-btn" onclick="tab(2)">⚙ Network</button>
  <button class="tab-btn" onclick="tab(3)">📖 Info & OTA</button>
</div>

<!-- ВКЛАДКА 1 -->
<div id="t0" class="tab-content active">
  <div class="card" style="text-align:center">
    <h1>Reflow Plate</h1>
    <div class="temp"><span id="t">--</span> &deg;C</div>
    <p>Power <b id="p">0</b>% &nbsp;&middot;&nbsp; SSR <b id="s">off</b></p>
    <div>
      <button class="sm" onclick="dt(-5)">&minus;5</button>
      <input id="g" type="number" style="width:90px;text-align:center;display:inline-block" onchange="send({a:'target',v:+this.value,name:'Manual'})">
      <button class="sm" onclick="dt(5)">+5</button> &deg;C
    </div>
    <div id="fault"><span id="ft"></span><br><button class="sm" onclick="send({a:'ack'})">Reset fault</button></div>
    <div style="display:flex;justify-content:center;gap:8px;margin-top:10px">
      <button id="hb" onclick="send({a:'heat',on:!heat})" style="flex:2;font-weight:bold;margin:0">Start</button>
      <button id="fb" onclick="send({a:'fan',on:!fan_state})" class="preset sm" style="flex:1;text-align:center;margin:0">🌀 Fan: OFF</button>
    </div>
    <div style="margin-top:8px">
      <span class="badge" id="b_prof" style="color:#00e676;font-weight:bold">Profile: Manual</span>
      <span class="badge" id="b_phase" style="display:none;background:#ff5722;color:#fff;font-weight:bold">PHASE</span>
      <span class="badge" id="b_stby" style="display:none;background:#fbc02d;color:#000;font-weight:bold">STANDBY 100&deg;C</span>
      <span class="badge" id="b_sn"></span>
    </div>
    <div style="margin-top:12px;display:flex;justify-content:center;align-items:center;gap:15px">
      <label style="margin:0;cursor:pointer"><input type="checkbox" id="mute_btn" onchange="send({a:'mute',v:this.checked})" style="width:auto"> 🔇 Mute Buzzer</label>
      <input id="pin" type="password" placeholder="PIN" style="width:70px;margin:0;display:inline-block" onchange="setPin(this.value)">
    </div>
  </div>
  <div class="card"><canvas id="c"></canvas></div>
</div>

<!-- ВКЛАДКА 2 -->
<div id="t1" class="tab-content">
  <div class="card">
    <h3>🚀 Auto Reflow Curves (JEDEC)</h3>
    <button class="preset" onclick="startProf('Sn63', 150, 165, 60, 215, 25)">
      <b>🥫 Leaded Sn63Pb37 Auto Cycle</b><br>
      <small style="color:#aaa">Preheat 150&deg;C &rarr; Soak 165&deg;C (60s) &rarr; Peak 215&deg;C (25s) &rarr; Cool</small>
    </button>
    <button class="preset" onclick="startProf('LeadFree', 160, 180, 75, 235, 30)">
      <b>✨ Lead-Free SAC305 Auto Cycle</b><br>
      <small style="color:#aaa">Preheat 160&deg;C &rarr; Soak 180&deg;C (75s) &rarr; Peak 235&deg;C (30s) &rarr; Cool</small>
    </button>
    <h3 style="margin-top:15px">⚡ Manual Presets</h3>
    <div style="display:flex;gap:6px">
      <button class="sm preset" style="text-align:center" onclick="setPreset(80, 'Phone')">📱 Phone (80&deg;)</button>
      <button class="sm preset" style="text-align:center" onclick="setPreset(150, 'LED')">💡 LED (150&deg;)</button>
    </div>
  </div>

  <div class="card">
    <h3>Sensor & PID Tuning</h3>
    <label>Sensor Hardware</label>
    <select id="sn">
      <option value="auto">Auto (MAX6675, else simulation)</option>
      <option value="max">MAX6675 thermocouple</option>
      <option value="ntc">NTC 100k on A0</option>
      <option value="sim">Simulation (Virtual plate)</option>
    </select>
    <button class="save sm" onclick="send({a:'sensor',v:$('sn').value})">Apply sensor</button>
    <label>Kp (%/&deg;C)</label><input id="kp" type="number" step="any">
    <label>Ki (%/&deg;C&middot;s)</label><input id="ki" type="number" step="any">
    <label>Kd (%&middot;s/&deg;C)</label><input id="kd" type="number" step="any">
    <div style="display:flex;gap:6px;margin-top:6px">
      <button class="save sm" style="flex:1" onclick="send({a:'pid',kp:+$('kp').value,ki:+$('ki').value,kd:+$('kd').value})">Save PID</button>
      <button class="preset sm" style="flex:1;background:#6a1b9a;border-color:#8e24aa" onclick="startTune()">🎯 Auto-Tune (150&deg;)</button>
    </div>
    <div id="tune_box" style="display:none;background:#2a1b3d;border:1px solid #8e24aa;padding:8px;border-radius:6px;margin-top:8px;font-size:13px">
      <span id="tune_txt">Auto-Tune running: Cycle 1/3...</span>
    </div>
    
    <h3 style="margin-top:15px">💤 Auto-Sleep / Standby Timer</h3>
    <label>Inactivity Timeout</label>
    <select id="stby_time" onchange="send({a:'set_stby', m:+this.value})">
      <option value="0">Disabled (Always On)</option>
      <option value="5">5 Minutes</option>
      <option value="10">10 Minutes (Default)</option>
      <option value="15">15 Minutes</option>
      <option value="30">30 Minutes</option>
    </select>
    </div>
</div>

<!-- ВКЛАДКА 3 -->
<div id="t2" class="tab-content">
  <div class="card">
    <h3>Wi-Fi Setup</h3>
    <div class="status-box" id="net_stat">Status: Checking...</div>
    <div id="wf_fields">
      <label>Wi-Fi Network Name (SSID)</label>
      <div style="display:flex;gap:5px">
        <input id="ssid" type="text" placeholder="SSID" style="margin:0;flex:1">
        <button class="save sm" style="margin:0" onclick="scanWifi()">🔍 Scan</button>
      </div>
      <div id="scan_res" style="display:none;margin-top:5px">
        <select id="scan_sel" onchange="$('ssid').value=this.value"></select>
      </div>
      <label>Wi-Fi Password</label><input id="wpw" type="password" placeholder="Empty = keep current">
      <label>Security PIN (optional)</label><input id="npin" type="password" placeholder="Empty = keep, '-' = disable">
      <button class="save" onclick="saveNet()" style="margin-top:10px">Save Wi-Fi & Reboot</button>
    </div>
  </div>

  <div class="card">
    <h3>MQTT Broker</h3>
    <label><input type="checkbox" id="en" style="width:auto"> Enable MQTT</label>
    <label>Server IP / Host</label><input id="srv" type="text">
    <label>Port</label><input id="prt" type="number">
    <label>Topic</label><input id="top" type="text">
    <label>User (optional)</label><input id="usr" type="text">
    <label>Password (optional)</label><input id="mpw" type="password" placeholder="Empty = keep current">
    <button class="save" onclick="saveMqtt()" style="margin-top:10px">Save MQTT</button>
    <p style="margin-top:8px">Status: <span id="mq" style="font-weight:bold">...</span></p>
  </div>
</div>

<!-- ВКЛАДКА 4 -->
<div id="t3" class="tab-content">
  <div class="card">
    <h3>📊 Usage & Energy Statistics</h3>
    <div style="display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:6px;text-align:center">
      <div style="background:#252525;padding:8px;border-radius:6px">
        <small style="color:#aaa">Energy (Session)</small><br>
        <b style="font-size:18px;color:#00e676" id="st_wh">0.0 Wh</b>
      </div>
      <div style="background:#252525;padding:8px;border-radius:6px">
        <small style="color:#aaa">Session Time</small><br>
        <b style="font-size:18px;color:#fff" id="st_time">0m 0s</b>
      </div>
      <div style="background:#252525;padding:8px;border-radius:6px">
        <small style="color:#aaa">Reflow Cycles</small><br>
        <b style="font-size:18px;color:#ff5722" id="st_cyc">0</b>
      </div>
      <div style="background:#252525;padding:8px;border-radius:6px">
        <small style="color:#aaa">Total Run Time</small><br>
        <b style="font-size:18px;color:#aaa" id="st_total">0.0 hrs</b>
      </div>
    </div>
    <div style="display:flex;gap:6px;margin-top:8px">
      <button class="preset sm" style="flex:1;text-align:center" onclick="resetStats(false)">🔄 Reset Session</button>
      <button class="preset sm" style="flex:1;text-align:center;color:#ff5722;border-color:#552222" onclick="resetStats(true)">⚠️ Reset All</button>
    </div>
  </div>

  <div class="card">
    <h3>Hardware Pinout & Help</h3>
    <details open>
      <summary>1. Основные подключения NodeMCU (ESP8266)</summary>
      <table>
        <tr><th>Периферия</th><th>Пин NodeMCU</th><th>GPIO</th><th>Примечание</th></tr>
        <tr><td>SSR Реле (220V)</td><td><b>D8</b></td><td>GPIO15</td><td>Медленный ШИМ (окно 1 сек)</td></tr>
        <tr><td>Зуммер (Buzzer)</td><td><b>RX</b></td><td>GPIO3</td><td>ШИМ генератор tone()</td></tr>
        <tr><td>MAX6675 SO</td><td><b>D3</b></td><td>GPIO0</td><td>SPI Data (MISO)</td></tr>
        <tr><td>MAX6675 CS</td><td><b>D4</b></td><td>GPIO2</td><td>SPI Chip Select</td></tr>
        <tr><td>MAX6675 SCK</td><td><b>D0</b></td><td>GPIO16</td><td>SPI Clock</td></tr>
        <tr><td>Энкодер CLK</td><td><b>D5</b></td><td>GPIO14</td><td>Прерывание вращения</td></tr>
        <tr><td>Энкодер DT</td><td><b>D6</b></td><td>GPIO12</td><td>Направление вращения</td></tr>
        <tr><td>Энкодер Кнопка</td><td><b>D7</b></td><td>GPIO13</td><td>Старт / Стоп нагрева</td></tr>
        <tr><td>I2C Шина SDA</td><td><b>D2</b></td><td>GPIO4</td><td>Общая шина: OLED + PCF8574</td></tr>
        <tr><td>I2C Шина SCL</td><td><b>D1</b></td><td>GPIO5</td><td>Общая шина: OLED + PCF8574</td></tr>
        <tr><td>NTC Термистор</td><td><b>A0</b></td><td>ADC0</td><td>Аналоговый делитель 100k на GND</td></tr>
      </table>
    </details>

    <details open>
      <summary>2. Расширитель портов PCF8574 (I2C: 0x20 / 0x27 / 0x38)</summary>
      <table>
        <tr><th>Пин PCF8574</th><th>Назначение</th><th>Функция при нажатии</th><th>Схема подключения</th></tr>
        <tr><td><b>P0</b></td><td>Кулер охлаждения (Fan)</td><td>Выход: Обдув стола</td><td>N-Ch MOSFET к GND кулера</td></tr>
        <tr><td><b>P1</b></td><td>Вытяжка дыма / Свет</td><td>Выход: Резерв</td><td>Реле или N-Ch MOSFET</td></tr>
        <tr><td><b>P2</b></td><td>Кнопка <b>BACK</b></td><td>Вход: Стоп ТЭНа / Обдув / Сброс Fault</td><td>Кнопка замыкает <b>P2 на GND</b> (Pull-up внутри)</td></tr>
        <tr><td><b>P3</b></td><td>Кнопка <b>CONFIRM</b></td><td>Вход: Быстрая смена профилей по кругу</td><td>Кнопка замыкает <b>P3 на GND</b> (Pull-up внутри)</td></tr>
        <tr><td><b>P4 - P7</b></td><td>Свободный резерв</td><td>Входы / Выходы</td><td>Доступны на гребенке платы</td></tr>
      </table>
      <div style="font-size:12px;color:#aaa;margin-top:6px;background:#181818;padding:6px;border-radius:4px">
        💡 <i>Подключение кнопок платки:</i> Один контакт кнопки "Back" &rarr; на <b>P2</b>. Один контакт кнопки "Confirm" &rarr; на <b>P3</b>. Общие вторые контакты обеих кнопок &rarr; на <b>GND</b>. Внешние резисторы не нужны (в PCF8574 встроена подтяжка).
      </div>
    </details>

    <details>
      <summary>Шпаргалка: Как настраивать ПИД (PID Tuning)</summary>
      <div style="font-size:13px;line-height:1.5;color:#ccc;margin-top:8px">
        <p><b>1. Kp (Реакция / Газ):</b> Основная мощность. Чем толще алюминий, тем выше Kp.<br>
        • <i>Слишком мало:</i> Нагрев идет слишком медленно.<br>
        • <i>Слишком много:</i> Будет постоянный перелет и колебания температуры.</p>

        <p><b>2. Kd (Тормоз / Прогноз):</b> Гасит мощность заранее при подлете к цели.<br>
        • <i>Слишком мало:</i> Температура по инерции проскакивает уставку на 10-20 &deg;C.<br>
        • <i>Слишком много:</i> Мощность дергается, столик не может доехать до уставки.</p>

        <p><b>3. Ki (Точность):</b> Дожимает последние 1-3 градуса ровно до цели.<br>
        • Начинайте с малых значений (0.01 - 0.05). Большие значения раскачивают систему волнами.</p>

        <p style="background:#2a2a2a;padding:6px;border-left:3px solid #ff5722;color:#fff">
        <b>Базовый рецепт для столика 400-500W:</b><br>
        Поставьте <code>Ki = 0</code>, <code>Kd = 15</code>. Подбирайте <code>Kp</code> (обычно 3 - 6), пока столик не станет быстро греться. Затем увеличивайте <code>Kd</code> (до 20 - 35), чтобы убрать перелет. В конце добавьте <code>Ki = 0.05</code> для идеальной точки.
        </p>
      </div>
    </details>
  </div>

  <div class="card">
    <h3>OTA Firmware Update (По воздуху)</h3>
    <p style="font-size:13px;color:#aaa">Выберите файл <b>firmware.bin</b> из папки <code>.pio/build/nodemcuv2/</code>:</p>
    <input type='file' id='ota_file' style="background:#222;border:1px dashed #555;padding:10px;border-radius:6px">
    <button type='button' class='save' onclick='uploadOTA()' style="width:100%;margin-top:8px">Flash Firmware (.bin)</button>
    <div id='ota_progress_box' style="display:none;margin-top:12px">
      <div style="background:#333;border-radius:6px;overflow:hidden;height:20px;position:relative">
        <div id='ota_bar' style="background:#00e676;width:0%;height:100%;transition:0.1s"></div>
        <span id='ota_percent' style="position:absolute;left:50%;top:50%;transform:translate(-50%,-50%);font-size:11px;font-weight:bold;color:#000">0%</span>
      </div>
      <p id='ota_status' style="font-size:12px;color:#aaa;margin-top:5px;text-align:center">Uploading...</p>
    </div>
  </div>
</div>

<script>
const $=id=>document.getElementById(id);
let ws,heat=false,tgt=150,pin='',fan_state=false;
window.wf_user_editing = false;

function toggleWifiBox(){
  window.wf_user_editing = !window.wf_user_editing;
  $('wf_fields').style.display = window.wf_user_editing ? 'block' : 'none';
}

try{pin=localStorage.getItem('pin')||'';$('pin').value=pin}catch(e){}
function setPin(v){pin=v;try{localStorage.setItem('pin',v)}catch(e){}}
function send(o){o.pin=pin;if(ws&&ws.readyState===1)ws.send(JSON.stringify(o))}
function dt(d){send({a:'target',v:tgt+d,name:'Manual'})}
function setPreset(t, name){ send({a:'target', v:t, name:name}); tab(0); }
function startTune(){
  if(confirm("Start automatic PID tuning around 150°C?\nThis takes ~3-5 minutes.")){
    send({a:'tune_start'});
    tab(0);
  }
}
function resetStats(all){
  if(all){
    if(confirm("DANGER: Reset ALL lifetime stats, total hours and reflow counter?")){
      send({a:'reset_stats', all:true});
    }
  } else {
    send({a:'reset_stats', all:false});
  }
}
function startProf(name, pre, soak, soak_t, ref, ref_t){
  send({a:'start_prof', name:name, pre:pre, soak:soak, soak_t:soak_t*1000, ref:ref, ref_t:ref_t*1000});
  tab(0);
}

function tab(idx){
  for(let i=0;i<4;i++){
    $('t'+i).className='tab-content'+(i===idx?' active':'');
    document.querySelectorAll('.tab-btn')[i].className='tab-btn'+(i===idx?' active':'');
  }
}

function connect(){
  ws=new WebSocket('ws://'+location.host+'/ws');
  ws.onmessage=e=>{const d=JSON.parse(e.data);
    if(d.type==='t')tele(d);
    else if(d.type==='stat')renderStat(d);
    else if(d.type==='config')cfg(d);
    else if(d.type==='scan')renderScan(d.list);
    else if(d.type==='denied'){$('pin').style.borderColor='#d32f2f';alert('Wrong PIN')}};
  ws.onclose=()=>setTimeout(connect,2000);
}

function renderStat(d){
  if(d.wh !== undefined) $('st_wh').textContent = d.wh < 1000 ? (d.wh.toFixed(1) + ' Wh') : ((d.wh/1000).toFixed(2) + ' kWh');
  if(d.s_sec !== undefined) $('st_time').textContent = Math.floor(d.s_sec/60) + 'm ' + (d.s_sec%60) + 's';
  if(d.r_cyc !== undefined) $('st_cyc').textContent = d.r_cyc;
  if(d.tot_m !== undefined) $('st_total').textContent = (d.tot_m / 60.0).toFixed(1) + ' hrs';
}

const hist={t:[],g:[]},N=300;
function push(a,v){a.push(v);if(a.length>N)a.shift()}
function draw(){
  const c=$('c'),x=c.getContext('2d'),W=c.width=c.clientWidth,H=c.height=180;
  const all=hist.t.concat(hist.g);if(all.length<2)return;
  const lo=Math.min(20,...all)-5,hi=Math.max(60,...all)+5;
  const line=(a,col,dash)=>{x.beginPath();x.strokeStyle=col;x.setLineDash(dash);x.lineWidth=2;
    a.forEach((v,i)=>{const px=(i+N-a.length)/(N-1)*W,py=H-(v-lo)/(hi-lo)*H;i?x.lineTo(px,py):x.moveTo(px,py)});x.stroke()};
  line(hist.g,'#ff5722',[5,5]);line(hist.t,'#00e676',[]);
  x.setLineDash([]);x.fillStyle='#888';x.font='11px sans-serif';
  x.fillText(hi.toFixed(0),2,11);x.fillText(lo.toFixed(0),2,H-3);
}

function tele(d){
  heat=!!d.h;tgt=d.g;
  $('t').textContent=d.ok?d.t.toFixed(1):'--';
  $('p').textContent=d.p;$('s').textContent=d.s?'ON':'off';
  if(document.activeElement!==$('g'))$('g').value=d.g;
  $('hb').textContent=heat?'Stop':'Start';$('hb').className=heat?'stop':'';
  $('fault').style.display=d.f?'block':'none';$('ft').textContent=d.ft;
  $('b_prof').textContent='Profile: '+(d.prof||'Manual');
  
    fan_state = !!d.fan;
  $('fb').textContent = fan_state ? '🌀 Fan: ON' : '🌀 Fan: OFF';
  $('fb').style.borderColor = fan_state ? '#00e676' : '#555';

  if(d.phase && d.phase !== "IDLE"){
    $('b_phase').style.display='inline-block';
    $('b_phase').textContent = d.phase;
  } else {
    $('b_phase').style.display='none';
  }

  // Индикатор Standby
  if (d.stby === 1) {
    $('b_stby').style.display = 'inline-block';
  } else {
    $('b_stby').style.display = 'none';
  }

  // Индикатор Auto-Tune
  if (d.tune && d.tune > 0) {
    $('tune_box').style.display = 'block';
    $('tune_txt').textContent = `🎯 Auto-Tune in progress: Cycle ${d.tune_c}/3 (Oscillating...)`;
  } else {
    $('tune_box').style.display = 'none';
  }

  $('b_sn').textContent='Sensor: '+d.sn+(d.w?' ('+d.w+')':'');
  $('mq').textContent=d.mq;$('mq').style.color=d.mq==='CONNECTED'?'#00e676':'#bbb';
  if(d.ok){push(hist.t,d.t);push(hist.g,d.g);draw()}

  // Обновление статистики
  if(d.wh !== undefined) $('st_wh').textContent = d.wh < 1000 ? (d.wh.toFixed(1) + ' Wh') : ((d.wh/1000).toFixed(2) + ' kWh');
  if(d.s_sec !== undefined) $('st_time').textContent = Math.floor(d.s_sec/60) + 'm ' + (d.s_sec%60) + 's';
  if(d.r_cyc !== undefined) $('st_cyc').textContent = d.r_cyc;
  if(d.tot_m !== undefined) $('st_total').textContent = (d.tot_m / 60.0).toFixed(1) + ' hrs';
  
  if(d.wifi_ok){
    $('net_stat').innerHTML = `<span style="color:#00e676;font-weight:bold">● Connected to "${d.ssid}"</span><br>IP: <b>${d.ip}</b><br><button class="sm" onclick="toggleWifiBox()" style="margin-top:6px">⚙ Change Wi-Fi</button>`;
    if(!window.wf_user_editing) $('wf_fields').style.display = 'none';
  } else {
    $('net_stat').innerHTML = `<span style="color:#ff9800;font-weight:bold">● Access Point Active</span><br>Connect to local Wi-Fi below:`;
    $('wf_fields').style.display = 'block';
  }
}

function cfg(d){
  $('ssid').value=d.ssid;$('en').checked=d.mq_en;$('srv').value=d.mq_srv;$('prt').value=d.mq_prt;
  $('top').value=d.mq_top;$('usr').value=d.mq_usr;$('sn').value=d.sensor;
  $('kp').value=d.kp.toFixed(2);$('ki').value=d.ki.toFixed(4);$('kd').value=d.kd.toFixed(1);
  $('mute_btn').checked = d.mute;
  if(d.stby_m !== undefined) $('stby_time').value = d.stby_m;
  if(d.r_cyc !== undefined) $('st_cyc').textContent = d.r_cyc;
  if(d.tot_m !== undefined) $('st_total').textContent = (d.tot_m / 60.0).toFixed(1) + ' hrs';
}

function scanWifi(){ $('scan_res').style.display='none'; send({a:'scan'}); }
function renderScan(list){
  const s=$('scan_sel'); s.innerHTML='<option value="">-- Click to choose Wi-Fi --</option>';
  list.forEach(i=>{
    const opt=document.createElement('option');
    opt.value=i.s; opt.textContent=`${i.s} (${i.r}%)`;
    s.appendChild(opt);
  });
  $('scan_res').style.display='block';
}
function saveNet(){
  const np=$('npin').value;
  send({a:'net',ssid:$('ssid').value,pass:$('wpw').value,newpin:np});
  if(np)setPin(np==='-'?'':np);
  alert('Rebooting...');
}
function saveMqtt(){
  send({a:'mqtt',en:$('en').checked,srv:$('srv').value,port:+$('prt').value,
        top:$('top').value,usr:$('usr').value,pass:$('mpw').value});
}

function uploadOTA() {
  const fileInput = $('ota_file');
  if (!fileInput.files.length) { alert('Please select firmware.bin first!'); return; }
  const file = fileInput.files[0];
  const formData = new FormData();
  formData.append('update', file);

  $('ota_progress_box').style.display = 'block';
  $('ota_bar').style.width = '0%';
  $('ota_percent').textContent = '0%';
  $('ota_status').textContent = 'Uploading firmware...';

  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/update', true);
  xhr.upload.onprogress = function(e) {
    if (e.lengthComputable) {
      const p = Math.round((e.loaded / e.total) * 100);
      $('ota_bar').style.width = p + '%';
      $('ota_percent').textContent = p + '%';
    }
  };
  xhr.onload = function() {
    if (xhr.status === 200) {
      $('ota_status').innerHTML = '<b style="color:#00e676">Success! Rebooting ESP8266...</b>';
      let sec = 5;
      setInterval(() => {
        $('ota_status').textContent = `Reconnecting in ${sec} sec...`;
        if (--sec < 0) location.reload();
      }, 1000);
    } else {
      $('ota_status').innerHTML = '<b style="color:#d32f2f">Upload Failed!</b>';
    }
  };
  xhr.onerror = function() { $('ota_status').innerHTML = '<b style="color:#d32f2f">Network Error during flash!</b>'; };
  xhr.send(formData);
}

connect();
</script>
</body></html>
)rawliteral";