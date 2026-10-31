// Fragment of the HUD IIFE (not a module). The join order is JS_FRAGMENTS in web/hud_build.py.
// ---------------- Start ----------------
sizeOverlay();
renderSpeed();
renderAll();
renderLog();
connectStream();
App.connect();
refreshWifi();
setInterval(refreshWifi, 10000);
requestAnimationFrame(loop);
