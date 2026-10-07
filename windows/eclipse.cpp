// Eclipse — Aether'in web tarayıcısı, Windows sürümü
// Aether 1.0 "Nebula" · Yapımcı: Hot Zot
// Arayüz: Win32 · Sayfa motoru: Microsoft Edge WebView2 (Windows 10/11'de yerleşik)
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#ifndef DECLSPEC_XFGVIRT
#define DECLSPEC_XFGVIRT(base, func)
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <objbase.h>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <ctime>
#include "WebView2.h"
#include "iids.h"
#include "kaynak.h"

using std::wstring;

// ---------------------------------------------------------------- COM geri çağırma yardımcısı
template <class I, const IID *Kimlik, class... A>
struct Cb : I {
    LONG ref = 1;
    std::function<HRESULT(A...)> f;
    explicit Cb(std::function<HRESULT(A...)> fn) : f(std::move(fn)) {}
    virtual ~Cb() {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID r, void **o) override {
        if (IsEqualIID(r, IID_IUnknown) || IsEqualIID(r, *Kimlik)) { *o = static_cast<I *>(this); AddRef(); return S_OK; }
        *o = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&ref); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r = InterlockedDecrement(&ref); if (!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE Invoke(A... a) override { return f ? f(a...) : S_OK; }
};
#define CB(Arayuz, ...) Cb<Arayuz, &AIID_##Arayuz, __VA_ARGS__>

typedef CB(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler, HRESULT, ICoreWebView2Environment *) OrtamHazir;
typedef CB(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler, HRESULT, ICoreWebView2Controller *) DenetleyiciHazir;
typedef CB(ICoreWebView2DocumentTitleChangedEventHandler, ICoreWebView2 *, IUnknown *) BaslikDegisti;
typedef CB(ICoreWebView2SourceChangedEventHandler, ICoreWebView2 *, ICoreWebView2SourceChangedEventArgs *) AdresDegisti;
typedef CB(ICoreWebView2HistoryChangedEventHandler, ICoreWebView2 *, IUnknown *) GecmisDegisti;
typedef CB(ICoreWebView2NavigationStartingEventHandler, ICoreWebView2 *, ICoreWebView2NavigationStartingEventArgs *) GezintiBasladi;
typedef CB(ICoreWebView2NavigationCompletedEventHandler, ICoreWebView2 *, ICoreWebView2NavigationCompletedEventArgs *) GezintiBitti;
typedef CB(ICoreWebView2NewWindowRequestedEventHandler, ICoreWebView2 *, ICoreWebView2NewWindowRequestedEventArgs *) YeniPencere;
typedef CB(ICoreWebView2WebResourceRequestedEventHandler, ICoreWebView2 *, ICoreWebView2WebResourceRequestedEventArgs *) KaynakIstendi;
typedef CB(ICoreWebView2ContainsFullScreenElementChangedEventHandler, ICoreWebView2 *, IUnknown *) TamEkran;
typedef CB(ICoreWebView2WindowCloseRequestedEventHandler, ICoreWebView2 *, IUnknown *) KapatIstendi;
typedef CB(ICoreWebView2AcceleratorKeyPressedEventHandler, ICoreWebView2Controller *, ICoreWebView2AcceleratorKeyPressedEventArgs *) TusBasildi;

// ---------------------------------------------------------------- dil
static bool EN = false;
#define T(tr, en) (EN ? L##en : L##tr)

// ---------------------------------------------------------------- metin yardımcıları
static std::string utf8(const wstring &w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0); WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr); return s;
}
static wstring genis(const std::string &s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    wstring w(n, 0); MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n); return w;
}
static wstring al(LPWSTR p) { wstring s = p ? p : L""; if (p) CoTaskMemFree(p); return s; }
static wstring kacis(const wstring &s) {
    wstring r; for (wchar_t c : s) { switch (c) { case L'<': r += L"&lt;"; break; case L'>': r += L"&gt;"; break; case L'&': r += L"&amp;"; break; case L'"': r += L"&quot;"; break; default: r += c; } }
    return r;
}
static bool basla(const wstring &s, const wchar_t *o) { return s.compare(0, wcslen(o), o) == 0; }

// ---------------------------------------------------------------- dosyalar
static wstring veri_dizini, yerimi_yolu, gecmis_yolu;
static std::string dosya_oku(const wstring &yol) {
    HANDLE h = CreateFileW(yol.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return {};
    DWORD n = GetFileSize(h, nullptr), o = 0; std::string s(n, 0);
    if (n) ReadFile(h, &s[0], n, &o, nullptr); CloseHandle(h); s.resize(o); return s;
}
static void dosya_yaz(const wstring &yol, const std::string &s, bool ekle) {
    HANDLE h = CreateFileW(yol.c_str(), ekle ? FILE_APPEND_DATA : GENERIC_WRITE, FILE_SHARE_READ, nullptr, ekle ? OPEN_ALWAYS : CREATE_ALWAYS, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD o; WriteFile(h, s.data(), (DWORD)s.size(), &o, nullptr); CloseHandle(h);
}
static std::vector<std::pair<wstring, wstring>> yerimleri() {        // (başlık, adres)
    std::vector<std::pair<wstring, wstring>> v; wstring c = genis(dosya_oku(yerimi_yolu)); size_t b = 0;
    while (b < c.size()) {
        size_t e = c.find(L'\n', b); if (e == wstring::npos) e = c.size();
        wstring s = c.substr(b, e - b); size_t t = s.find(L'\t');
        if (t != wstring::npos) v.push_back({s.substr(0, t), s.substr(t + 1)});
        b = e + 1;
    }
    return v;
}
static bool yerimi_var(const wstring &u) { for (auto &y : yerimleri()) if (y.second == u) return true; return false; }
static void yerimi_degistir(wstring baslik, const wstring &u) {
    auto v = yerimleri(); bool sil = false; std::string s;
    for (auto &y : v) { if (y.second == u) { sil = true; continue; } s += utf8(y.first + L"\t" + y.second + L"\n"); }
    if (!sil) { std::replace(baslik.begin(), baslik.end(), L'\t', L' '); if (baslik.empty()) baslik = u; s += utf8(baslik + L"\t" + u + L"\n"); }
    dosya_yaz(yerimi_yolu, s, false);
}
static void gecmise_ekle(wstring baslik, const wstring &u) {
    if (u.empty() || basla(u, L"https://aether.local") || basla(u, L"about:")) return;
    std::replace(baslik.begin(), baslik.end(), L'\t', L' ');
    dosya_yaz(gecmis_yolu, std::to_string((long long)time(nullptr)) + "\t" + utf8(u) + "\t" + utf8(baslik.empty() ? u : baslik) + "\n", true);
}

// ---------------------------------------------------------------- Aether sayfaları
static const wchar_t *EV = L"https://aether.local/ev";
static const wchar_t *CSS =
    L"<style>:root{color-scheme:dark}"
    L"body{margin:0;font-family:'Cascadia Mono',Consolas,monospace;background:#070a1f url(/arkaplan.png) center/cover fixed;color:#e8e6f5;min-height:100vh}"
    L".kap{max-width:880px;margin:0 auto;padding:6vh 24px 40px}"
    L"h1{font-weight:300;letter-spacing:.6em;text-align:center;font-size:44px;margin:0 0 6px;text-shadow:0 0 24px #a08cff88}"
    L".alt{text-align:center;color:#b6a8ff;letter-spacing:.3em;font-size:13px;margin-bottom:36px}"
    L"form{display:flex;border:1px solid #8f7cf0;background:#ffffffee}"
    L"input{flex:1;border:0;padding:14px 16px;font:inherit;font-size:16px;background:transparent;color:#1e1a3a;outline:none}"
    L"button{border:0;background:#5b4bd6;color:#fff;font:inherit;padding:0 22px;cursor:pointer}button:hover{background:#6d5ce8}"
    L".kutular{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:12px;margin-top:34px}"
    L".kutu{display:block;padding:16px 14px;background:#ffffff14;border:1px solid #ffffff2a;color:#fff;text-decoration:none;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;backdrop-filter:blur(4px)}"
    L".kutu:hover{background:#5b4bd6aa;border-color:#b6a8ff}.kutu small{display:block;color:#b8b2d8;font-size:11px;margin-top:6px;overflow:hidden;text-overflow:ellipsis}"
    L"h2{font-weight:400;letter-spacing:.2em;font-size:15px;color:#c9bcff;margin:34px 0 10px}"
    L".liste a{color:#fff}.liste div{padding:6px 0;border-bottom:1px solid #ffffff18;font-size:13px}.liste span{color:#a9a3c9;margin-right:12px}"
    L".alt-not{margin-top:40px;text-align:center;font-size:11px;color:#8f88b8}a{color:#b6a8ff}</style>";

static wstring sayfa_basi(const wstring &baslik) {
    return L"<!doctype html><html><head><meta charset=utf-8><title>" + baslik + L"</title>" + CSS + L"</head><body><div class=kap>";
}
static wstring kutu(const wstring &ad, const wstring &u) {
    return L"<a class=kutu href=\"" + kacis(u) + L"\">" + kacis(ad) + L"<small>" + kacis(u) + L"</small></a>";
}
static wstring ev_sayfasi(bool gizli) {
    wstring h = sayfa_basi(T("Yeni sekme", "New tab"));
    h += L"<div id=saat style=\"text-align:center;font-size:56px;font-weight:300;letter-spacing:.08em;margin-bottom:4px\"></div>"
         L"<div id=tarih style=\"text-align:center;color:#b6a8ff;font-size:13px;letter-spacing:.2em;margin-bottom:30px\"></div>";
    h += EN ? L"<script>var AY=['JANUARY','FEBRUARY','MARCH','APRIL','MAY','JUNE','JULY','AUGUST','SEPTEMBER','OCTOBER','NOVEMBER','DECEMBER'],GUN=['SUNDAY','MONDAY','TUESDAY','WEDNESDAY','THURSDAY','FRIDAY','SATURDAY'];</script>"
            : L"<script>var AY=['OCAK','ŞUBAT','MART','NİSAN','MAYIS','HAZİRAN','TEMMUZ','AĞUSTOS','EYLÜL','EKİM','KASIM','ARALIK'],GUN=['PAZAR','PAZARTESİ','SALI','ÇARŞAMBA','PERŞEMBE','CUMA','CUMARTESİ'];</script>";
    h += L"<script>(function(){function iki(n){return (n<10?'0':'')+n}function t(){var d=new Date();"
         L"document.getElementById('saat').textContent=iki(d.getHours())+':'+iki(d.getMinutes());"
         L"document.getElementById('tarih').textContent=d.getDate()+' '+AY[d.getMonth()]+' '+d.getFullYear()+' · '+GUN[d.getDay()];}"
         L"t();setInterval(t,1000);})();</script>";
    h += L"<h1>E C L I P S E</h1><div class=alt>";
    h += gizli ? T("GİZLİ PENCERE · GEÇMİŞ KAYDEDİLMEZ", "PRIVATE WINDOW · NO HISTORY") : L"AETHER 1.0 · NEBULA · WINDOWS";
    h += L"</div><form onsubmit=\"var q=document.getElementById('q').value.trim();if(q)location.href='https://duckduckgo.com/?q='+encodeURIComponent(q);return false\">"
         L"<input id=q autofocus placeholder=\"";
    h += T("Web'de ara veya adres yaz…", "Search the web or type an address…");
    h += L"\"><button>"; h += T("Ara", "Search"); h += L"</button></form><div class=kutular>";
    h += kutu(L"Google", L"https://www.google.com") + kutu(L"YouTube", L"https://www.youtube.com")
       + kutu(T("Vikipedi", "Wikipedia"), EN ? L"https://en.wikipedia.org" : L"https://tr.wikipedia.org")
       + kutu(L"GitHub", L"https://github.com") + kutu(L"DuckDuckGo", L"https://duckduckgo.com") + kutu(L"Alpine Linux", L"https://alpinelinux.org");
    h += L"</div>";
    auto y = yerimleri();
    if (!y.empty()) { h += L"<h2>"; h += T("YER İMLERİ", "BOOKMARKS"); h += L"</h2><div class=kutular>"; for (auto &b : y) h += kutu(b.first, b.second); h += L"</div>"; }
    h += L"<div class=alt-not>";
    h += T("Ctrl+T yeni sekme · Ctrl+D yer imi · Ctrl+Shift+N gizli pencere", "Ctrl+T new tab · Ctrl+D bookmark · Ctrl+Shift+N private window");
    h += L" · <a href=/gecmis>"; h += T("Geçmiş", "History"); h += L"</a> · Hot Zot</div></div></body></html>";
    return h;
}
static wstring gecmis_sayfasi() {
    wstring h = sayfa_basi(T("Geçmiş", "History"));
    h += L"<h1 style=font-size:30px>"; h += T("G E Ç M İ Ş", "H I S T O R Y");
    h += L"</h1><div class=alt><a href=/gecmis/temizle>"; h += T("Geçmişi temizle", "Clear history"); h += L"</a></div><div class=liste>";
    wstring c = genis(dosya_oku(gecmis_yolu)); std::vector<wstring> satirlar; size_t b = 0;
    while (b < c.size()) { size_t e = c.find(L'\n', b); if (e == wstring::npos) e = c.size(); satirlar.push_back(c.substr(b, e - b)); b = e + 1; }
    int n = 0;
    for (auto it = satirlar.rbegin(); it != satirlar.rend() && n < 300; ++it) {
        size_t t1 = it->find(L'\t'), t2 = t1 == wstring::npos ? t1 : it->find(L'\t', t1 + 1);
        if (t2 == wstring::npos) continue;
        time_t z = (time_t)_wtoi64(it->substr(0, t1).c_str()); wchar_t tarih[32]; tm *lt = localtime(&z);
        if (lt) wcsftime(tarih, 32, L"%d.%m.%Y %H:%M", lt); else tarih[0] = 0;
        h += L"<div><span>" + wstring(tarih) + L"</span><a href=\"" + kacis(it->substr(t1 + 1, t2 - t1 - 1)) + L"\">" + kacis(it->substr(t2 + 1)) + L"</a></div>"; n++;
    }
    if (!n) h += T("<div>Geçmiş boş.</div>", "<div>History is empty.</div>");
    return h + L"</div></div></body></html>";
}

// ---------------------------------------------------------------- pencere ve sekmeler
enum { ID_GERI = 101, ID_ILERI, ID_YENILE, ID_EV, ID_ADRES, ID_YILDIZ, ID_INDIR, ID_YENI, ID_MENU, ID_KAPAT, ID_SEKMELER,
       M_YENI = 300, M_GIZLI, M_GECMIS, M_INDIRMELER, M_HAKKINDA, M_YERIMI = 400 };
static const wchar_t *G_GERI = L"", *G_ILERI = L"", *G_YENILE = L"", *G_DUR = L"", *G_EV = L"",
                     *G_YILDIZ = L"", *G_YILDIZ_DOLU = L"", *G_INDIR = L"", *G_YENI = L"", *G_MENU = L"", *G_KAPAT = L"";

struct Pencere;
struct Sekme {
    Pencere *p = nullptr;
    ICoreWebView2Controller *den = nullptr;
    ICoreWebView2 *wv = nullptr;
    wstring baslik, adres, bekleyen;
    bool yukleniyor = false;
};
struct Pencere {
    HWND hwnd = nullptr, geri, ileri, yenile, ev, adres, yildiz, indir, yeni, menu, kapat, sekmeler;
    std::vector<Sekme *> liste;
    int etkin = -1;
    bool gizli = false, tam = false;
    RECT eski{}; LONG eski_stil = 0;
};
static ICoreWebView2Environment *ortam;
static HFONT yazi_simge, yazi_metin;
static HINSTANCE uyg;
static int pencere_sayisi = 0;
static HBRUSH firca_acik, firca_gizli;
static UINT dpi = 96;
static int S(int x) { return MulDiv(x, dpi, 96); }

static Pencere *pencere_ac(bool gizli, const wstring &url);
static void sekme_ac(Pencere *p, const wstring &url);
static void yerlestir(Pencere *p);

static Sekme *etkin(Pencere *p) { return p->etkin >= 0 && p->etkin < (int)p->liste.size() ? p->liste[p->etkin] : nullptr; }
static wstring adres_coz(wstring g) {
    while (!g.empty() && g[0] == L' ') g.erase(0, 1);
    while (!g.empty() && g.back() == L' ') g.pop_back();
    if (g.empty()) return EV;
    if (g.find(L"://") != wstring::npos || basla(g, L"about:") || basla(g, L"edge:")) return g;
    if (g.size() > 2 && g[1] == L':' && (g[2] == L'\\' || g[2] == L'/')) return L"file:///" + g;
    if (g.find(L' ') == wstring::npos && (g.find(L'.') != wstring::npos || basla(g, L"localhost"))) return L"https://" + g;
    wchar_t kod[2048]; DWORD n = 2048;
    if (UrlEscapeW(g.c_str(), kod, &n, URL_ESCAPE_SEGMENT_ONLY | URL_ESCAPE_PERCENT) != S_OK) wcscpy(kod, g.c_str());
    wstring q = kod; size_t i; while ((i = q.find(L' ')) != wstring::npos) q.replace(i, 1, L"%20");
    for (i = 0; (i = q.find(L'&', i)) != wstring::npos; i += 3) q.replace(i, 1, L"%26");
    return L"https://duckduckgo.com/?q=" + q;
}
static void arayuz(Pencere *p) {
    Sekme *s = etkin(p); if (!s) return;
    if (GetFocus() != p->adres) SetWindowTextW(p->adres, basla(s->adres, L"https://aether.local/ev") ? L"" : s->adres.c_str());
    BOOL g = FALSE, i = FALSE;
    if (s->wv) { s->wv->get_CanGoBack(&g); s->wv->get_CanGoForward(&i); }
    EnableWindow(p->geri, g); EnableWindow(p->ileri, i);
    SetWindowTextW(p->yenile, s->yukleniyor ? G_DUR : G_YENILE);
    SetWindowTextW(p->yildiz, yerimi_var(s->adres) ? G_YILDIZ_DOLU : G_YILDIZ);
    wstring t = (p->gizli ? wstring(T("[Gizli] ", "[Private] ")) : wstring()) + (s->baslik.empty() ? L"Eclipse" : s->baslik) + L" — Eclipse";
    SetWindowTextW(p->hwnd, t.c_str());
    for (size_t k = 0; k < p->liste.size(); k++) {
        wstring b = p->liste[k]->baslik.empty() ? wstring(T("Yeni sekme", "New tab")) : p->liste[k]->baslik;
        if (b.size() > 24) b = b.substr(0, 23) + L"…";
        TCITEMW it{}; it.mask = TCIF_TEXT; it.pszText = (LPWSTR)b.c_str();
        TabCtrl_SetItem(p->sekmeler, (int)k, &it);
    }
}
static void sekme_sec(Pencere *p, int i) {
    if (i < 0 || i >= (int)p->liste.size()) return;
    for (size_t k = 0; k < p->liste.size(); k++) if (p->liste[k]->den) p->liste[k]->den->put_IsVisible((int)k == i);
    p->etkin = i; TabCtrl_SetCurSel(p->sekmeler, i);
    yerlestir(p); arayuz(p);
    Sekme *s = etkin(p);
    if (s && s->den && !basla(s->adres, L"https://aether.local/ev")) s->den->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
    else { SetFocus(p->adres); }
}
static void sekme_kapat(Pencere *p, int i) {
    if (i < 0 || i >= (int)p->liste.size()) return;
    Sekme *s = p->liste[i];
    if (s->den) { s->den->Close(); s->den->Release(); }
    if (s->wv) s->wv->Release();
    delete s; p->liste.erase(p->liste.begin() + i); TabCtrl_DeleteItem(p->sekmeler, i);
    if (p->liste.empty()) { DestroyWindow(p->hwnd); return; }
    sekme_sec(p, std::min(i, (int)p->liste.size() - 1));
}
static void git(Sekme *s, const wstring &u) { if (s->wv) s->wv->Navigate(u.c_str()); else s->bekleyen = u; }

static void tam_ekran(Pencere *p, bool ac) {
    if (ac == p->tam) return;
    p->tam = ac;
    if (ac) {
        p->eski_stil = GetWindowLongW(p->hwnd, GWL_STYLE); GetWindowRect(p->hwnd, &p->eski);
        MONITORINFO mi{sizeof(mi)}; GetMonitorInfoW(MonitorFromWindow(p->hwnd, MONITOR_DEFAULTTONEAREST), &mi);
        SetWindowLongW(p->hwnd, GWL_STYLE, p->eski_stil & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(p->hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED);
    } else {
        SetWindowLongW(p->hwnd, GWL_STYLE, p->eski_stil);
        SetWindowPos(p->hwnd, nullptr, p->eski.left, p->eski.top, p->eski.right - p->eski.left, p->eski.bottom - p->eski.top, SWP_FRAMECHANGED | SWP_NOZORDER);
    }
    yerlestir(p);
}
static void yakinlastir(Pencere *p, double k) {
    Sekme *s = etkin(p); if (!s || !s->den) return;
    double z = 1; s->den->get_ZoomFactor(&z); z = k == 0 ? 1.0 : z * k;
    z = std::max(0.3, std::min(4.0, z)); s->den->put_ZoomFactor(z);
}
static void yildiz(Pencere *p) {
    Sekme *s = etkin(p); if (!s || s->adres.empty() || basla(s->adres, L"https://aether.local")) return;
    yerimi_degistir(s->baslik, s->adres); arayuz(p);
}
static void hakkinda(Pencere *p) {
    wstring surum = L"?"; LPWSTR v = nullptr;
    if (SUCCEEDED(GetAvailableCoreWebView2BrowserVersionString(nullptr, &v))) surum = al(v);
    wstring m = wstring(T("Aether'in web tarayıcısı — Windows sürümü\nYapımcı: Hot Zot\n\nSayfa motoru: Microsoft Edge WebView2 ", "Aether's web browser — Windows edition\nMade by Hot Zot\n\nPage engine: Microsoft Edge WebView2 ")) + surum;
    MessageBoxW(p->hwnd, m.c_str(), L"Eclipse", MB_OK | MB_ICONINFORMATION);
}
static void indirmeler(Pencere *p) {
    Sekme *s = etkin(p); if (!s || !s->wv) return;
    ICoreWebView2_9 *w9 = nullptr;
    if (SUCCEEDED(s->wv->QueryInterface(AIID_ICoreWebView2_9, (void **)&w9)) && w9) { w9->OpenDefaultDownloadDialog(); w9->Release(); }
}
static void menu_goster(Pencere *p) {
    HMENU m = CreatePopupMenu(), y = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, M_YENI, T("Yeni sekme\tCtrl+T", "New tab\tCtrl+T"));
    AppendMenuW(m, MF_STRING, M_GIZLI, T("Yeni gizli pencere\tCtrl+Shift+N", "New private window\tCtrl+Shift+N"));
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    auto v = yerimleri();
    for (size_t i = 0; i < v.size() && i < 90; i++) AppendMenuW(y, MF_STRING, M_YERIMI + i, v[i].first.c_str());
    if (v.empty()) AppendMenuW(y, MF_STRING | MF_GRAYED, 0, T("(Ctrl+D ile ekle)", "(add with Ctrl+D)"));
    AppendMenuW(m, MF_POPUP, (UINT_PTR)y, T("Yer imleri", "Bookmarks"));
    AppendMenuW(m, MF_STRING, M_GECMIS, T("Geçmiş\tCtrl+H", "History\tCtrl+H"));
    AppendMenuW(m, MF_STRING, M_INDIRMELER, T("İndirmeler\tCtrl+J", "Downloads\tCtrl+J"));
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, M_HAKKINDA, T("Eclipse hakkında", "About Eclipse"));
    RECT r; GetWindowRect(p->menu, &r);
    int c = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTALIGN | TPM_TOPALIGN, r.right, r.bottom, 0, p->hwnd, nullptr);
    DestroyMenu(m);
    if (c == M_YENI) sekme_ac(p, EV);
    else if (c == M_GIZLI) pencere_ac(true, EV);
    else if (c == M_GECMIS) sekme_ac(p, L"https://aether.local/gecmis");
    else if (c == M_INDIRMELER) indirmeler(p);
    else if (c == M_HAKKINDA) hakkinda(p);
    else if (c >= M_YERIMI && c < M_YERIMI + (int)v.size()) sekme_ac(p, v[c - M_YERIMI].second);
}

// kısayollar: true = işlendi
static bool kisayol(Pencere *p, UINT tus) {
    bool ctrl = GetKeyState(VK_CONTROL) & 0x8000, shift = GetKeyState(VK_SHIFT) & 0x8000, alt = GetKeyState(VK_MENU) & 0x8000;
    Sekme *s = etkin(p);
    if (ctrl && shift && tus == 'N') { pencere_ac(true, EV); return true; }
    if (ctrl && shift && tus == VK_TAB) { int n = (int)p->liste.size(); sekme_sec(p, (p->etkin + n - 1) % n); return true; }
    if (ctrl && !shift && !alt) switch (tus) {
        case 'T': sekme_ac(p, EV); return true;
        case 'N': pencere_ac(false, EV); return true;
        case 'W': sekme_kapat(p, p->etkin); return true;
        case 'L': SetFocus(p->adres); SendMessageW(p->adres, EM_SETSEL, 0, -1); return true;
        case 'R': if (s && s->wv) s->wv->Reload(); return true;
        case 'D': yildiz(p); return true;
        case 'H': sekme_ac(p, L"https://aether.local/gecmis"); return true;
        case 'J': indirmeler(p); return true;
        case VK_OEM_PLUS: case VK_ADD: yakinlastir(p, 1.1); return true;
        case VK_OEM_MINUS: case VK_SUBTRACT: yakinlastir(p, 1 / 1.1); return true;
        case '0': case VK_NUMPAD0: yakinlastir(p, 0); return true;
        case VK_TAB: sekme_sec(p, (p->etkin + 1) % (int)p->liste.size()); return true;
    }
    if (alt && tus == VK_LEFT && s && s->wv) { s->wv->GoBack(); return true; }
    if (alt && tus == VK_RIGHT && s && s->wv) { s->wv->GoForward(); return true; }
    if (alt && tus == VK_HOME && s) { git(s, EV); return true; }
    if (tus == VK_F5 && s && s->wv) { s->wv->Reload(); return true; }
    if (tus == VK_F11) { tam_ekran(p, !p->tam); return true; }
    if (tus == VK_F12 && s && s->wv) { s->wv->OpenDevToolsWindow(); return true; }
    if (tus == VK_ESCAPE && p->tam) { tam_ekran(p, false); return true; }
    return false;
}

static void yanit_ver(ICoreWebView2WebResourceRequestedEventArgs *a, const std::string &veri, const wchar_t *tur) {
    IStream *st = SHCreateMemStream((const BYTE *)veri.data(), (UINT)veri.size());
    ICoreWebView2WebResourceResponse *y = nullptr;
    wstring baslik = wstring(L"Content-Type: ") + tur + L"\r\nCache-Control: no-store";
    if (st && SUCCEEDED(ortam->CreateWebResourceResponse(st, 200, L"OK", baslik.c_str(), &y)) && y) { a->put_Response(y); y->Release(); }
    if (st) st->Release();
}

static void sekme_kur(Sekme *s, ICoreWebView2Controller *den) {
    Pencere *p = s->p;
    s->den = den; den->AddRef();
    den->get_CoreWebView2(&s->wv);
    ICoreWebView2Settings *ay = nullptr;
    if (SUCCEEDED(s->wv->get_Settings(&ay)) && ay) { ay->put_IsScriptEnabled(TRUE); ay->put_AreDevToolsEnabled(TRUE); ay->put_IsStatusBarEnabled(TRUE); ay->Release(); }
    EventRegistrationToken tk;
    s->wv->AddWebResourceRequestedFilter(L"https://aether.local/*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    s->wv->add_WebResourceRequested(new KaynakIstendi([p](ICoreWebView2 *, ICoreWebView2WebResourceRequestedEventArgs *a) -> HRESULT {
        ICoreWebView2WebResourceRequest *r = nullptr; a->get_Request(&r); if (!r) return S_OK;
        LPWSTR u = nullptr; r->get_Uri(&u); wstring uri = al(u); r->Release();
        wstring yol = uri.substr(wcslen(L"https://aether.local"));
        size_t q = yol.find_first_of(L"?#"); if (q != wstring::npos) yol = yol.substr(0, q);
        if (yol == L"/arkaplan.png") {
            HRSRC k = FindResourceW(uyg, MAKEINTRESOURCEW(KAYNAK_ARKAPLAN), RT_RCDATA);
            if (k) { HGLOBAL g = LoadResource(uyg, k); yanit_ver(a, std::string((const char *)LockResource(g), SizeofResource(uyg, k)), L"image/png"); }
            return S_OK;
        }
        wstring h;
        if (yol == L"/gecmis/temizle") { dosya_yaz(gecmis_yolu, "", false); h = gecmis_sayfasi(); }
        else if (yol == L"/gecmis") h = gecmis_sayfasi();
        else h = ev_sayfasi(p->gizli);
        yanit_ver(a, utf8(h), L"text/html; charset=utf-8");
        return S_OK;
    }), &tk);
    s->wv->add_DocumentTitleChanged(new BaslikDegisti([s](ICoreWebView2 *w, IUnknown *) -> HRESULT {
        LPWSTR t = nullptr; w->get_DocumentTitle(&t); s->baslik = al(t); arayuz(s->p); return S_OK; }), &tk);
    s->wv->add_SourceChanged(new AdresDegisti([s](ICoreWebView2 *w, ICoreWebView2SourceChangedEventArgs *) -> HRESULT {
        LPWSTR u = nullptr; w->get_Source(&u); s->adres = al(u); if (etkin(s->p) == s) arayuz(s->p); return S_OK; }), &tk);
    s->wv->add_HistoryChanged(new GecmisDegisti([s](ICoreWebView2 *, IUnknown *) -> HRESULT { if (etkin(s->p) == s) arayuz(s->p); return S_OK; }), &tk);
    s->wv->add_NavigationStarting(new GezintiBasladi([s](ICoreWebView2 *, ICoreWebView2NavigationStartingEventArgs *) -> HRESULT {
        s->yukleniyor = true; if (etkin(s->p) == s) arayuz(s->p); return S_OK; }), &tk);
    s->wv->add_NavigationCompleted(new GezintiBitti([s](ICoreWebView2 *w, ICoreWebView2NavigationCompletedEventArgs *a) -> HRESULT {
        s->yukleniyor = false; BOOL ok = FALSE; a->get_IsSuccess(&ok);
        if (ok && !s->p->gizli) gecmise_ekle(s->baslik, s->adres);
        if (etkin(s->p) == s) arayuz(s->p); return S_OK; }), &tk);
    s->wv->add_NewWindowRequested(new YeniPencere([s](ICoreWebView2 *, ICoreWebView2NewWindowRequestedEventArgs *a) -> HRESULT {
        LPWSTR u = nullptr; a->get_Uri(&u); wstring uri = al(u); a->put_Handled(TRUE); sekme_ac(s->p, uri); return S_OK; }), &tk);
    s->wv->add_ContainsFullScreenElementChanged(new TamEkran([s](ICoreWebView2 *w, IUnknown *) -> HRESULT {
        BOOL t = FALSE; w->get_ContainsFullScreenElement(&t); tam_ekran(s->p, t); return S_OK; }), &tk);
    s->wv->add_WindowCloseRequested(new KapatIstendi([s](ICoreWebView2 *, IUnknown *) -> HRESULT {
        auto &l = s->p->liste; auto it = std::find(l.begin(), l.end(), s); if (it != l.end()) sekme_kapat(s->p, (int)(it - l.begin())); return S_OK; }), &tk);
    den->add_AcceleratorKeyPressed(new TusBasildi([p](ICoreWebView2Controller *, ICoreWebView2AcceleratorKeyPressedEventArgs *a) -> HRESULT {
        COREWEBVIEW2_KEY_EVENT_KIND k; a->get_KeyEventKind(&k);
        if (k == COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN || k == COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN) {
            UINT v = 0; a->get_VirtualKey(&v); if (kisayol(p, v)) a->put_Handled(TRUE);
        }
        return S_OK; }), &tk);
    yerlestir(p);
    auto &l = p->liste; int i = (int)(std::find(l.begin(), l.end(), s) - l.begin());
    den->put_IsVisible(i == p->etkin);
    git(s, s->bekleyen.empty() ? wstring(EV) : s->bekleyen); s->bekleyen.clear();
    if (i == p->etkin) sekme_sec(p, i);
}

static void sekme_ac(Pencere *p, const wstring &url) {
    Sekme *s = new Sekme; s->p = p; s->bekleyen = url;
    p->liste.push_back(s);
    TCITEMW it{}; it.mask = TCIF_TEXT; it.pszText = (LPWSTR)T("Yeni sekme", "New tab");
    int i = (int)p->liste.size() - 1; TabCtrl_InsertItem(p->sekmeler, i, &it);
    p->etkin = i; TabCtrl_SetCurSel(p->sekmeler, i);
    for (size_t k = 0; k + 1 < p->liste.size(); k++) if (p->liste[k]->den) p->liste[k]->den->put_IsVisible(FALSE);
    auto hazir = new DenetleyiciHazir([s](HRESULT hr, ICoreWebView2Controller *d) -> HRESULT {
        if (FAILED(hr) || !d) { MessageBoxW(s->p->hwnd, T("Sayfa görünümü oluşturulamadı.", "Could not create the web view."), L"Eclipse", MB_ICONERROR); return S_OK; }
        sekme_kur(s, d); return S_OK; });
    ICoreWebView2Environment10 *o10 = nullptr;
    if (p->gizli && SUCCEEDED(ortam->QueryInterface(AIID_ICoreWebView2Environment10, (void **)&o10)) && o10) {
        ICoreWebView2ControllerOptions *se = nullptr; o10->CreateCoreWebView2ControllerOptions(&se);
        if (se) { se->put_IsInPrivateModeEnabled(TRUE); o10->CreateCoreWebView2ControllerWithOptions(p->hwnd, se, hazir); se->Release(); }
        o10->Release();
    } else ortam->CreateCoreWebView2Controller(p->hwnd, hazir);
    hazir->Release();
    arayuz(p);
}

static void yerlestir(Pencere *p) {
    RECT r; GetClientRect(p->hwnd, &r);
    int W = r.right, H = r.bottom, b = S(34), pad = S(6), y = pad;
    int arac = p->tam ? 0 : b + pad * 2, sek = p->tam ? 0 : S(28);
    int sw = p->tam ? SW_HIDE : SW_SHOW;
    HWND hepsi[] = {p->geri, p->ileri, p->yenile, p->ev, p->adres, p->yildiz, p->indir, p->yeni, p->menu, p->kapat, p->sekmeler};
    for (HWND h : hepsi) ShowWindow(h, sw);
    if (!p->tam) {
        int x = pad;
        HWND sol[] = {p->geri, p->ileri, p->yenile, p->ev};
        for (HWND h : sol) { MoveWindow(h, x, y, b, b, TRUE); x += b + S(2); }
        int sag = W - pad;
        HWND sg[] = {p->menu, p->yeni, p->indir, p->yildiz};
        for (HWND h : sg) { sag -= b; MoveWindow(h, sag, y, b, b, TRUE); sag -= S(2); }
        MoveWindow(p->adres, x + S(4), y + S(3), sag - x - S(10), b - S(6), TRUE);
        MoveWindow(p->sekmeler, 0, arac, W - S(30), sek, TRUE);
        MoveWindow(p->kapat, W - S(28), arac + S(2), S(26), sek - S(4), TRUE);
    }
    RECT g{0, arac + sek, W, H};
    for (auto *s : p->liste) if (s->den) s->den->put_Bounds(g);
}

static LRESULT CALLBACK adres_alt(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR veri) {
    Pencere *p = (Pencere *)veri;
    if (m == WM_KEYDOWN && w == VK_RETURN) {
        int n = GetWindowTextLengthW(h); wstring t(n, 0); GetWindowTextW(h, &t[0], n + 1);
        Sekme *s = etkin(p); if (s) { git(s, adres_coz(t)); if (s->den) s->den->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC); }
        return 0;
    }
    if (m == WM_CHAR && (w == VK_RETURN || w == 1 /* Ctrl+A */)) { if (w == 1) SendMessageW(h, EM_SETSEL, 0, -1); return 0; }
    if (m == WM_KEYDOWN && w == VK_ESCAPE) { arayuz(p); return 0; }
    return DefSubclassProc(h, m, w, l);
}

static LRESULT CALLBACK pencere_yordami(HWND h, UINT m, WPARAM w, LPARAM l) {
    Pencere *p = (Pencere *)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (m) {
    case WM_SIZE: if (p) yerlestir(p); return 0;
    case WM_GETMINMAXINFO: ((MINMAXINFO *)l)->ptMinTrackSize = {S(480), S(320)}; return 0;
    case WM_CTLCOLORSTATIC: case WM_CTLCOLORBTN:
        if (p && p->gizli) { SetBkColor((HDC)w, RGB(42, 35, 80)); SetTextColor((HDC)w, RGB(255, 255, 255)); return (LRESULT)firca_gizli; }
        SetBkColor((HDC)w, RGB(244, 244, 248)); return (LRESULT)firca_acik;
    case WM_ERASEBKGND: { RECT r; GetClientRect(h, &r); FillRect((HDC)w, &r, p && p->gizli ? firca_gizli : firca_acik); return 1; }
    case WM_COMMAND:
        if (!p) break;
        switch (LOWORD(w)) {
            case ID_GERI: if (etkin(p) && etkin(p)->wv) etkin(p)->wv->GoBack(); return 0;
            case ID_ILERI: if (etkin(p) && etkin(p)->wv) etkin(p)->wv->GoForward(); return 0;
            case ID_YENILE: if (Sekme *s = etkin(p)) { if (!s->wv) return 0; if (s->yukleniyor) s->wv->Stop(); else s->wv->Reload(); } return 0;
            case ID_EV: if (etkin(p)) git(etkin(p), EV); return 0;
            case ID_YILDIZ: yildiz(p); return 0;
            case ID_INDIR: indirmeler(p); return 0;
            case ID_YENI: sekme_ac(p, EV); return 0;
            case ID_MENU: menu_goster(p); return 0;
            case ID_KAPAT: sekme_kapat(p, p->etkin); return 0;
        }
        break;
    case WM_NOTIFY:
        if (p && ((NMHDR *)l)->hwndFrom == p->sekmeler && ((NMHDR *)l)->code == TCN_SELCHANGE) { sekme_sec(p, TabCtrl_GetCurSel(p->sekmeler)); return 0; }
        break;
    case WM_DPICHANGED: {
        dpi = HIWORD(w); RECT *r = (RECT *)l;
        SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0; }
    case WM_DESTROY:
        if (p) {
            for (auto *s : p->liste) { if (s->den) { s->den->Close(); s->den->Release(); } if (s->wv) s->wv->Release(); delete s; }
            delete p; SetWindowLongPtrW(h, GWLP_USERDATA, 0);
        }
        if (--pencere_sayisi == 0) PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

// sekme şeridinde orta tıkla kapat
static LRESULT CALLBACK sekme_alt(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR veri) {
    if (m == WM_MBUTTONUP) {
        TCHITTESTINFO ht{}; ht.pt = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        int i = TabCtrl_HitTest(h, &ht); if (i >= 0) sekme_kapat((Pencere *)veri, i); return 0;
    }
    return DefSubclassProc(h, m, w, l);
}

static HWND dugme(HWND ust, int id, const wchar_t *glif, const wchar_t *ipucu, HWND ipucu_pen) {
    HWND b = CreateWindowExW(0, L"BUTTON", glif, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_FLAT, 0, 0, 10, 10, ust, (HMENU)(INT_PTR)id, uyg, nullptr);
    SendMessageW(b, WM_SETFONT, (WPARAM)yazi_simge, TRUE);
    TOOLINFOW ti{}; ti.cbSize = sizeof(ti); ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS; ti.hwnd = ust; ti.uId = (UINT_PTR)b; ti.lpszText = (LPWSTR)ipucu;
    SendMessageW(ipucu_pen, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    return b;
}

static Pencere *pencere_ac(bool gizli, const wstring &url) {
    Pencere *p = new Pencere; p->gizli = gizli;
    p->hwnd = CreateWindowExW(0, L"AetherEclipse", L"Eclipse", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, S(1180), S(780), nullptr, nullptr, uyg, nullptr);
    pencere_sayisi++;
    SetWindowLongPtrW(p->hwnd, GWLP_USERDATA, (LONG_PTR)p);
    HWND tt = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP, 0, 0, 0, 0, p->hwnd, nullptr, uyg, nullptr);
    p->geri = dugme(p->hwnd, ID_GERI, G_GERI, T("Geri (Alt+Sol)", "Back (Alt+Left)"), tt);
    p->ileri = dugme(p->hwnd, ID_ILERI, G_ILERI, T("İleri (Alt+Sağ)", "Forward (Alt+Right)"), tt);
    p->yenile = dugme(p->hwnd, ID_YENILE, G_YENILE, T("Yenile (F5)", "Reload (F5)"), tt);
    p->ev = dugme(p->hwnd, ID_EV, G_EV, T("Ana sayfa (Alt+Home)", "Home (Alt+Home)"), tt);
    p->yildiz = dugme(p->hwnd, ID_YILDIZ, G_YILDIZ, T("Yer imi ekle/kaldır (Ctrl+D)", "Bookmark (Ctrl+D)"), tt);
    p->indir = dugme(p->hwnd, ID_INDIR, G_INDIR, T("İndirmeler (Ctrl+J)", "Downloads (Ctrl+J)"), tt);
    p->yeni = dugme(p->hwnd, ID_YENI, G_YENI, T("Yeni sekme (Ctrl+T)", "New tab (Ctrl+T)"), tt);
    p->menu = dugme(p->hwnd, ID_MENU, G_MENU, T("Menü", "Menu"), tt);
    p->kapat = dugme(p->hwnd, ID_KAPAT, G_KAPAT, T("Sekmeyi kapat (Ctrl+W)", "Close tab (Ctrl+W)"), tt);
    p->adres = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 10, 10, p->hwnd, (HMENU)ID_ADRES, uyg, nullptr);
    SendMessageW(p->adres, WM_SETFONT, (WPARAM)yazi_metin, TRUE);
    SendMessageW(p->adres, EM_SETCUEBANNER, TRUE, (LPARAM)T("Ara veya adres yaz", "Search or enter address"));
    SetWindowSubclass(p->adres, adres_alt, 1, (DWORD_PTR)p);
    p->sekmeler = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | TCS_FOCUSNEVER | TCS_FIXEDWIDTH, 0, 0, 10, 10, p->hwnd, (HMENU)ID_SEKMELER, uyg, nullptr);
    SendMessageW(p->sekmeler, WM_SETFONT, (WPARAM)yazi_metin, TRUE);
    TabCtrl_SetItemSize(p->sekmeler, S(190), S(24));
    SetWindowSubclass(p->sekmeler, sekme_alt, 2, (DWORD_PTR)p);
    ShowWindow(p->hwnd, SW_SHOW);
    yerlestir(p);
    sekme_ac(p, url.empty() ? wstring(EV) : url);
    return p;
}

int WINAPI wWinMain(HINSTANCE h, HINSTANCE, LPWSTR, int) {
    uyg = h;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    EN = PRIMARYLANGID(GetUserDefaultUILanguage()) != LANG_TURKISH;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX ic{sizeof(ic), ICC_TAB_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES}; InitCommonControlsEx(&ic);
    HDC dc = GetDC(nullptr); dpi = GetDeviceCaps(dc, LOGPIXELSY); ReleaseDC(nullptr, dc);

    wchar_t yol[MAX_PATH];
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, yol);
    veri_dizini = wstring(yol) + L"\\Aether\\Eclipse";
    SHCreateDirectoryExW(nullptr, veri_dizini.c_str(), nullptr);
    yerimi_yolu = veri_dizini + L"\\yerimleri.txt"; gecmis_yolu = veri_dizini + L"\\gecmis.txt";
    SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, yol);
    wstring motor_verisi = wstring(yol) + L"\\Aether\\Eclipse\\Motor";

    yazi_simge = CreateFontW(-S(15), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe MDL2 Assets");
    yazi_metin = CreateFontW(-S(14), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    firca_acik = CreateSolidBrush(RGB(244, 244, 248)); firca_gizli = CreateSolidBrush(RGB(42, 35, 80));

    WNDCLASSEXW wc{sizeof(wc)}; wc.lpfnWndProc = pencere_yordami; wc.hInstance = h; wc.lpszClassName = L"AetherEclipse";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hIcon = LoadIconW(h, MAKEINTRESOURCEW(KAYNAK_SIMGE)); wc.hIconSm = wc.hIcon;
    RegisterClassExW(&wc);

    // komut satırı: [--gizli] [adres]
    int argc = 0; LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool gizli = false; wstring url;
    for (int i = 1; i < argc; i++) { wstring a = argv[i]; if (a == L"--gizli" || a == L"--private") gizli = true; else url = adres_coz(a); }
    LocalFree(argv);

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, motor_verisi.c_str(), nullptr,
        new OrtamHazir([gizli, url](HRESULT hr, ICoreWebView2Environment *o) -> HRESULT {
            if (FAILED(hr) || !o) {
                if (MessageBoxW(nullptr, T("Eclipse'in sayfa motoru (Microsoft Edge WebView2 Runtime) bu bilgisayarda bulunamadı.\n\nİndirme sayfası açılsın mı?",
                                           "Eclipse's page engine (Microsoft Edge WebView2 Runtime) was not found on this computer.\n\nOpen the download page?"),
                                L"Eclipse", MB_YESNO | MB_ICONWARNING) == IDYES)
                    ShellExecuteW(nullptr, L"open", L"https://developer.microsoft.com/microsoft-edge/webview2/", nullptr, nullptr, SW_SHOWNORMAL);
                PostQuitMessage(1); return S_OK;
            }
            ortam = o; ortam->AddRef();
            pencere_ac(gizli, url);
            return S_OK;
        }));
    if (FAILED(hr)) {
        MessageBoxW(nullptr, T("Microsoft Edge WebView2 Runtime bulunamadı. Lütfen kurun:\nhttps://developer.microsoft.com/microsoft-edge/webview2/",
                               "Microsoft Edge WebView2 Runtime not found. Please install it:\nhttps://developer.microsoft.com/microsoft-edge/webview2/"), L"Eclipse", MB_ICONERROR);
        return 1;
    }
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        // adres çubuğundayken de kısayollar çalışsın
        if ((msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN)) {
            HWND kok = GetAncestor(msg.hwnd, GA_ROOT);
            Pencere *p = kok ? (Pencere *)GetWindowLongPtrW(kok, GWLP_USERDATA) : nullptr;
            if (p && msg.wParam != VK_RETURN && kisayol(p, (UINT)msg.wParam)) continue;
        }
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    CoUninitialize();
    return 0;
}
