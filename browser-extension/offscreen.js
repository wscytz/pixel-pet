// offscreen document:消费 tabCapture 流 → 取时域 PCM 样本 → WebSocket 推给 PixelPet
// 推 PCM(不是 AnalyserNode 频谱):C++ 自己 FFT,与本地文件同路径,chroma/调式才准
// (getByteFrequencyData 是 dB 压缩 + smoothing + Blackman 窗,chroma 算不准;PCM 绕过这些)
// 关键:capture 后原 tab 不再自己出声,必须 connect(destination) 回放,用户才听得见

const WS_URL = 'ws://127.0.0.1:17632';
const FFT = 2048;                 // 时域 PCM 窗(对齐 C++ kN;4096 改变真实 chroma,回退)
const SEND_HZ = 30;
const SEND_INTERVAL = 1000 / SEND_HZ;

let audioCtx = null, analyser = null, source = null, stream = null;
let pcmBuf = null;
let ws = null, loopTimer = null;
let lastTitle = '';
let lastSend = 0;

function connectWS() {
  ws = new WebSocket(WS_URL);
  ws.onopen = () => console.log('[PixelPet] ws connected');
  ws.onclose = () => setTimeout(connectWS, 2000);
  ws.onerror = () => {};
}

function stopStream() {
  if (loopTimer) { clearInterval(loopTimer); loopTimer = null; }
  try { if (source) source.disconnect(); } catch (e) {}
  try { if (stream) stream.getTracks().forEach(t => t.stop()); } catch (e) {}  // 释放 tab capture
  try { if (audioCtx) audioCtx.close(); } catch (e) {}
  audioCtx = analyser = source = stream = null;
}

async function startStream(streamId, title) {
  lastTitle = title || '';
  stopStream();  // 先清旧的,避免 "Cannot capture a tab with an active stream"

  stream = await navigator.mediaDevices.getUserMedia({
    audio: { mandatory: { chromeMediaSource: 'tab', chromeMediaSourceId: streamId } },
    video: false,
  });
  audioCtx = new AudioContext();
  if (audioCtx.state === 'suspended') await audioCtx.resume();   // autoplay 解锁
  source = audioCtx.createMediaStreamSource(stream);
  analyser = audioCtx.createAnalyser();
  analyser.fftSize = FFT;
  source.connect(analyser);
  source.connect(audioCtx.destination);   // ★ 回放!否则标签页静音
  pcmBuf = new Float32Array(analyser.fftSize);
  if (!ws || ws.readyState !== WebSocket.OPEN) connectWS();
  loopTimer = setInterval(loop, SEND_INTERVAL);
  console.log('[PixelPet] capturing pcm, fftSize=' + analyser.fftSize + ' sr=' + audioCtx.sampleRate);
}

function loop() {
  if (!analyser) return;
  analyser.getFloatTimeDomainData(pcmBuf);   // 时域 PCM(-1..1),C++ 自己 FFT
  const now = performance.now();
  if (ws && ws.readyState === WebSocket.OPEN && now - lastSend >= SEND_INTERVAL) {
    lastSend = now;
    ws.send(JSON.stringify({ pcm: Array.from(pcmBuf), sr: audioCtx.sampleRate, title: lastTitle }));
  }
}

chrome.runtime.onMessage.addListener((msg) => {
  if (msg.type === 'start-stream') startStream(msg.streamId, msg.tabTitle);
  else if (msg.type === 'stop-stream') stopStream();
});

connectWS();
