const status = document.getElementById('status');
function set(s) { status.textContent = s; }

document.getElementById('start').onclick = async () => {
  set('请求权限…');
  const res = await chrome.runtime.sendMessage({ type: 'start-capture' });
  if (res && res.ok) set('抓取中: ' + (res.title || '(无标题)'));
  else set('失败: ' + (res && res.err ? res.err : '未知'));
};

document.getElementById('stop').onclick = async () => {
  await chrome.runtime.sendMessage({ type: 'stop-capture' });
  set('已停止');
};
