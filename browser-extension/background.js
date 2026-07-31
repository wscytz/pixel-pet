// service worker:popup / 快捷键(toggle-capture)→ 先停旧 stream → 拿 streamId → 转 offscreen 消费
// (MV3 service worker 无 DOM,不能 getUserMedia,必须 offscreen document)
// 抓的是「标签页音频输出层」,不挑站:任意网页播放站点都能抓。

let capturing = false;   // SW 存活期内的捕获状态(快捷键 toggle 用;SW 重启会丢,默认按"开始")
let manualStop = false;  // 用户手动 stop 后尊重,切 tab 不自动重抓(直到再 start)

async function ensureOffscreen() {
  const contexts = await chrome.runtime.getContexts({});
  const exists = contexts.find(c => c.contextType === 'OFFSCREEN_DOCUMENT');
  if (!exists) {
    await chrome.offscreen.createDocument({
      url: 'offscreen.html',
      reasons: ['USER_MEDIA'],
      justification: 'Capture tab audio via chrome.tabCapture for PixelPet',
    });
  }
}

async function startCapture(targetTabId) {
  const tab = targetTabId ? await chrome.tabs.get(targetTabId)
                          : (await chrome.tabs.query({ active: true, currentWindow: true }))[0];
  if (!tab) throw new Error('no active tab');
  await ensureOffscreen();
  // 先停旧 stream,等 track 释放,避免 "Cannot capture a tab with an active stream"
  await chrome.runtime.sendMessage({ type: 'stop-stream' }).catch(() => {});
  await new Promise(r => setTimeout(r, 150));
  const streamId = await chrome.tabCapture.getMediaStreamId({ targetTabId: tab.id });
  await chrome.runtime.sendMessage({ type: 'start-stream', streamId, tabTitle: tab.title });
  return tab.title;
}

async function stopCapture() {
  await chrome.runtime.sendMessage({ type: 'stop-stream' }).catch(() => {});
}

// 快捷键 toggle(免每次点 popup)
chrome.commands.onCommand.addListener(async (cmd) => {
  if (cmd !== 'toggle-capture') return;
  capturing = !capturing;
  try { capturing ? await startCapture() : await stopCapture(); }
  catch (e) { capturing = false; }
});

chrome.runtime.onMessage.addListener((msg, sender, sendResponse) => {
  if (msg.type === 'start-capture') {
    manualStop = false;
    startCapture().then(title => { capturing = true; sendResponse({ ok: true, title }); })
                  .catch(e => sendResponse({ ok: false, err: String(e) }));
    return true;  // async sendResponse
  }
  if (msg.type === 'stop-capture') {
    manualStop = true;
    stopCapture().then(() => { capturing = false; sendResponse({ ok: true }); });
    return true;
  }
});

// 自动跟随:切到别的 tab(如新歌 tab)自动重新抓,免每次手动点扩展。
// 手动 stop 后尊重(不自动抢),直到再次手动 start。
let pendingTab = null;
chrome.tabs.onActivated.addListener((info) => {
  if (manualStop) return;
  pendingTab = info.tabId;
  // 防抖 500ms:快速切 tab 只抓最后一个;且只抓正在发声(audible)的 tab,切到没声音的不重抓(避免卡)
  setTimeout(() => {
    if (pendingTab !== info.tabId) return;   // 期间又切了 → 取消
    chrome.tabs.get(info.tabId)
      .then(t => { if (t && t.audible) startCapture(info.tabId).catch(() => {}); })
      .catch(() => {});
  }, 500);
});
