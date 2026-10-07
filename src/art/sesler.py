# Aether ses efektleri (kısa, yumuşak sentez sesler)
import wave, struct, math, os
D = "/work/overlay/usr/share/aether/sesler"; os.makedirs(D, exist_ok=True)
SR = 22050
def ton(notalar, dosya, sure_carp=1.0):
    ornek = []
    for f, bas, sure, ses in notalar:
        pass
    toplam = max(b + s for _, b, s, _ in notalar) * sure_carp
    n = int(SR * (toplam + 0.3)); buf = [0.0] * n
    for f, b, s, v in notalar:
        i0 = int(SR * b); m = int(SR * (s + 0.25))
        for i in range(m):
            t = i / SR
            env = min(1, t / 0.01) * math.exp(-t * 3.2 / s)
            x = math.sin(2 * math.pi * f * t) + 0.25 * math.sin(4 * math.pi * f * t) + 0.08 * math.sin(6 * math.pi * f * t)
            if i0 + i < n: buf[i0 + i] += x * env * v
    pk = max(abs(x) for x in buf) or 1
    with wave.open(os.path.join(D, dosya), "w") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(b"".join(struct.pack("<h", int(x / pk * 0.6 * 32767)) for x in buf))
# Do-Mi-Sol-Do yükselen "Nebula" akoru
ton([(523.25, 0, .5, 1), (659.25, .12, .5, .9), (783.99, .24, .6, .8), (1046.5, .38, .9, .7)], "giris.wav")
ton([(880, 0, .18, 1), (1318.5, .09, .25, .7)], "bildirim.wav")
ton([(311.1, 0, .2, 1), (233.1, .14, .35, 1)], "hata.wav")
ton([(1046.5, 0, .4, .7), (783.99, .13, .4, .8), (659.25, .26, .45, .9), (523.25, .4, .8, 1)], "kapanis.wav")
print("sesler tamam")
