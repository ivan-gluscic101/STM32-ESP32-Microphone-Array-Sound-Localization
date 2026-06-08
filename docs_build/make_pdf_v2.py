# -*- coding: utf-8 -*-
"""
Generator tehničke dokumentacije (PDF) — DRUGO, PROŠIRENO IZDANJE (v2).

Novosti u odnosu na v1:
  • Cijelo poglavlje "Matematički temelji: Fourier i FFT" — od nule, s ručno
    izračunatim primjerima na običnim uzorcima PRIJE primjene na projekt.
  • Cijelo poglavlje "FreeRTOS od nule" — s malim primjerom (senzor temperature),
    izračunom dostatnosti stoga, odabirom prioriteta i usporedbom queue/semafor/mutex.

Font: Calibri / Consolas (pune hrvatske dijakritike, Unicode).
Paleta: suzdržana (tamno plava za naslove, siva za kod/tablice).
"""

import os
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import cm, mm
from reportlab.lib import colors
from reportlab.lib.enums import TA_JUSTIFY, TA_CENTER, TA_LEFT
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Table, TableStyle,
    PageBreak, ListFlowable, ListItem, KeepTogether, HRFlowable, CondPageBreak
)
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle

# ----------------------------------------------------------------------------
# Fontovi
# ----------------------------------------------------------------------------
FONTS = "C:/Windows/Fonts"
pdfmetrics.registerFont(TTFont("Calibri",    f"{FONTS}/calibri.ttf"))
pdfmetrics.registerFont(TTFont("Calibri-B",  f"{FONTS}/calibrib.ttf"))
pdfmetrics.registerFont(TTFont("Calibri-I",  f"{FONTS}/calibrii.ttf"))
pdfmetrics.registerFont(TTFont("Calibri-BI", f"{FONTS}/calibriz.ttf"))
pdfmetrics.registerFont(TTFont("Consolas",   f"{FONTS}/consola.ttf"))
pdfmetrics.registerFont(TTFont("Consolas-B", f"{FONTS}/consolab.ttf"))
from reportlab.pdfbase.pdfmetrics import registerFontFamily
registerFontFamily("Calibri", normal="Calibri", bold="Calibri-B",
                   italic="Calibri-I", boldItalic="Calibri-BI")

# ----------------------------------------------------------------------------
# Boje
# ----------------------------------------------------------------------------
NAVY   = colors.HexColor("#1F3A5F")
STEEL  = colors.HexColor("#2E5E8C")
INK    = colors.HexColor("#1A1A1A")
GREY   = colors.HexColor("#666666")
LIGHT  = colors.HexColor("#F2F2F2")
LINE   = colors.HexColor("#C9C9C9")
CODEBG = colors.HexColor("#F4F5F7")
HDRBG  = colors.HexColor("#E4EAF1")

# ----------------------------------------------------------------------------
# Stilovi
# ----------------------------------------------------------------------------
styles = getSampleStyleSheet()
def S(name, **kw):
    base = kw.pop("parent", styles["Normal"])
    return ParagraphStyle(name, parent=base, **kw)

st_title   = S("Title2", fontName="Calibri-B", fontSize=26, leading=31, textColor=NAVY, alignment=TA_LEFT, spaceAfter=6)
st_subtitle= S("Sub2", fontName="Calibri", fontSize=13, leading=18, textColor=GREY, spaceAfter=2)
st_h1      = S("H1", fontName="Calibri-B", fontSize=17, leading=21, textColor=NAVY, spaceBefore=16, spaceAfter=7)
st_h2      = S("H2", fontName="Calibri-B", fontSize=13.5, leading=17, textColor=STEEL, spaceBefore=11, spaceAfter=4)
st_h3      = S("H3", fontName="Calibri-BI", fontSize=11.5, leading=15, textColor=INK, spaceBefore=8, spaceAfter=3)
st_body    = S("Body2", fontName="Calibri", fontSize=10.5, leading=15.5, textColor=INK, alignment=TA_JUSTIFY, spaceAfter=6)
st_body_l  = S("BodyL", parent=st_body, alignment=TA_LEFT)
st_note    = S("Note", fontName="Calibri-I", fontSize=9.5, leading=13.5, textColor=GREY, alignment=TA_LEFT, spaceAfter=6)
st_bullet  = S("Bul", fontName="Calibri", fontSize=10.5, leading=15, textColor=INK, alignment=TA_LEFT)
st_code    = S("Code2", fontName="Consolas", fontSize=8.6, leading=11.6, textColor=INK)
st_codecap = S("CodeCap", fontName="Calibri-I", fontSize=9, leading=12, textColor=GREY, spaceBefore=2, spaceAfter=7)
st_tcell   = S("TCell", fontName="Calibri", fontSize=9.3, leading=12.5, textColor=INK)
st_tcellb  = S("TCellB", fontName="Calibri-B", fontSize=9.3, leading=12.5, textColor=INK)
st_thead   = S("THead", fontName="Calibri-B", fontSize=9.3, leading=12.5, textColor=NAVY)
st_toc     = S("Toc", fontName="Calibri", fontSize=11, leading=18, textColor=INK)

story = []
def H1(t): story.append(Paragraph(t, st_h1))
def H2(t): story.append(Paragraph(t, st_h2))
def H3(t): story.append(Paragraph(t, st_h3))
def P(t, style=st_body): story.append(Paragraph(t, style))
def Note(t): story.append(Paragraph(t, st_note))
def Sp(h=6): story.append(Spacer(1, h))

def Bullets(items, style=st_bullet):
    li = [ListItem(Paragraph(x, style), leftIndent=6, value="•") for x in items]
    story.append(ListFlowable(li, bulletType="bullet", bulletFontName="Calibri",
                              bulletColor=STEEL, leftIndent=14, bulletFontSize=9,
                              spaceBefore=1, spaceAfter=6))

def NumList(items, style=st_bullet):
    li = [ListItem(Paragraph(x, style), leftIndent=4) for x in items]
    story.append(ListFlowable(li, bulletType="1", bulletFontName="Calibri-B",
                              bulletColor=STEEL, leftIndent=16, spaceBefore=1, spaceAfter=6))

def Code(text, caption=None, keep=True):
    lines = text.split("\n")
    while lines and lines[0].strip() == "": lines.pop(0)
    while lines and lines[-1].strip() == "": lines.pop()
    esc = []
    for ln in lines:
        ln = ln.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
        ln = ln.replace(" ", "&nbsp;")
        esc.append(ln)
    para = Paragraph("<br/>".join(esc) if esc else "&nbsp;", st_code)
    tbl = Table([[para]], colWidths=[16.2*cm])
    tbl.setStyle(TableStyle([
        ("BACKGROUND", (0,0), (-1,-1), CODEBG),
        ("BOX", (0,0), (-1,-1), 0.6, LINE),
        ("LEFTPADDING", (0,0), (-1,-1), 8), ("RIGHTPADDING", (0,0), (-1,-1), 8),
        ("TOPPADDING", (0,0), (-1,-1), 6), ("BOTTOMPADDING", (0,0), (-1,-1), 6),
        ("LINEBEFORE", (0,0), (0,-1), 2.5, STEEL),
    ]))
    story.append(KeepTogether(tbl) if (keep and len(esc) < 34) else tbl)
    if caption: story.append(Paragraph(caption, st_codecap))
    else: Sp(7)

def DataTable(rows, col_widths, header=True, caption=None, align=None, keep=True):
    data = []
    for r_i, row in enumerate(rows):
        cells = []
        for c_i, cell in enumerate(row):
            if isinstance(cell, Paragraph): cells.append(cell)
            else:
                stl = st_thead if (header and r_i == 0) else st_tcell
                cells.append(Paragraph(str(cell), stl))
        data.append(cells)
    tbl = Table(data, colWidths=col_widths, repeatRows=1 if header else 0)
    ts = [
        ("GRID", (0,0), (-1,-1), 0.5, LINE), ("VALIGN", (0,0), (-1,-1), "MIDDLE"),
        ("LEFTPADDING", (0,0), (-1,-1), 6), ("RIGHTPADDING", (0,0), (-1,-1), 6),
        ("TOPPADDING", (0,0), (-1,-1), 4), ("BOTTOMPADDING", (0,0), (-1,-1), 4),
        ("ROWBACKGROUNDS", (0,1), (-1,-1), [colors.white, colors.HexColor("#FAFBFC")]),
    ]
    if header:
        ts += [("BACKGROUND", (0,0), (-1,0), HDRBG), ("LINEBELOW", (0,0), (-1,0), 0.8, STEEL)]
    if align:
        for col, a in align.items(): ts.append(("ALIGN", (col,0), (col,-1), a))
    tbl.setStyle(TableStyle(ts))
    story.append(KeepTogether([tbl]) if (keep and len(rows) < 16) else tbl)
    if caption: story.append(Paragraph(caption, st_codecap))
    else: Sp(8)

def Rule():
    story.append(HRFlowable(width="100%", thickness=0.6, color=LINE, spaceBefore=4, spaceAfter=8))

def Callout(title, text):
    inner = [
        Paragraph(f"<b>{title}</b>", S("CalT", fontName="Calibri-B", fontSize=10, leading=13, textColor=NAVY, spaceAfter=2)),
        Paragraph(text, S("CalB", fontName="Calibri", fontSize=9.8, leading=14, textColor=INK, alignment=TA_LEFT)),
    ]
    tbl = Table([[inner]], colWidths=[16.2*cm])
    tbl.setStyle(TableStyle([
        ("BACKGROUND", (0,0), (-1,-1), colors.HexColor("#EEF3F8")),
        ("BOX", (0,0), (-1,-1), 0.5, STEEL),
        ("LEFTPADDING", (0,0), (-1,-1), 10), ("RIGHTPADDING", (0,0), (-1,-1), 10),
        ("TOPPADDING", (0,0), (-1,-1), 7), ("BOTTOMPADDING", (0,0), (-1,-1), 7),
    ]))
    story.append(KeepTogether(tbl)); Sp(8)

# ============================================================================
#  NASLOVNICA
# ============================================================================
Sp(36)
P("Tehnička dokumentacija &nbsp;&middot;&nbsp; drugo, prošireno izdanje (v2)", st_subtitle)
P("Lokalizacija zvuka mikrofonskim nizom", st_title)
P("STM32G474 (akvizicija i DSP)&nbsp;&nbsp;+&nbsp;&nbsp;ESP32 (Wi-Fi 3D vizualizacija)",
  S("Sub3", fontName="Calibri", fontSize=12.5, leading=17, textColor=STEEL, spaceAfter=18))
story.append(HRFlowable(width="100%", thickness=1.2, color=NAVY, spaceAfter=14))

P("Ovo izdanje proširuje prvu verziju s dva temeljita uvodna poglavlja namijenjena "
  "čitatelju koji tek počinje. Prije nego što opišemo kako su stvari riješene u "
  "ovom projektu, najprije <b>od nule</b> objašnjavamo dvije ključne teme i "
  "pokazujemo ih na <b>jednostavnim, ručno izračunatim primjerima</b>:", st_body)
Bullets([
  "<b>Fourierova transformacija i FFT</b> — što znači «frekvencijska domena», "
  "ručni izračun DFT-a na 4 uzorka, kako «čisti ton» izgleda u spektru, te kako "
  "FFT ubrzava račun (leptir / butterfly). Tek nakon toga: kako je FFT iskorišten "
  "u sustavu.",
  "<b>FreeRTOS</b> — što je RTOS, kako radi raspoređivač, te mali primjer "
  "(mjerenje temperature i slanje) na kojem učimo procijeniti veličinu stoga, "
  "odabrati prioritet i odlučiti između reda čekanja (queue), semafora i mutexa. "
  "Tek nakon toga: kako su posloženi taskovi u sustavu.",
])
P("Ostatak dokumenta (arhitektura, akvizicija, GCC-PHAT lokalizacija, UART "
  "protokol, ESP32 vizualizacija, cjeloviti primjer i pojmovnik) zadržan je i "
  "dopunjen iz prve verzije.", st_body)

Sp(8)
Note("Prva verzija dokumenta sačuvana je kao «Dokumentacija_Lokalizacija_Zvuka_v1.pdf».")
story.append(PageBreak())

# ============================================================================
#  SADRŽAJ
# ============================================================================
P("Sadržaj", st_h1)
Rule()
toc = [
    ("1.",  "Pregled sustava — što i zašto"),
    ("2.",  "Arhitektura: dvije ploče, jedan tok podataka"),
    ("3.",  "Hardver i geometrija mikrofonskog niza"),
    ("4.",  "Matematički temelji: Fourierova transformacija i FFT"),
    ("5.",  "FreeRTOS od nule: taskovi, stog, prioriteti, queue/semafor"),
    ("6.",  "STM32 lanac akvizicije: TIM8 → ADC → DMA → ISR"),
    ("7.",  "FreeRTOS u ovom projektu: tri taska i protok podataka"),
    ("8.",  "Procjena veličine stoga u ovom projektu"),
    ("9.",  "Detekcija i lokalizacija zvuka — korak po korak (GCC-PHAT)"),
    ("10.", "UART protokol između STM32 i ESP32"),
    ("11.", "ESP32: Wi-Fi, web-poslužitelj i 3D vizualizacija"),
    ("12.", "Cjeloviti primjer: od pljeska do točke na ekranu"),
    ("13.", "Pojmovnik za početnike"),
]
for num, title in toc:
    story.append(Paragraph(f'<font name="Calibri-B" color="#1F3A5F">{num}</font>&nbsp;&nbsp;{title}', st_toc))
story.append(PageBreak())

# ============================================================================
#  1. PREGLED
# ============================================================================
H1("1.&nbsp;&nbsp;Pregled sustava — što i zašto")
P("Cilj sustava je <b>odrediti smjer iz kojeg dolazi zvuk</b> (npr. pljesak "
  "rukama) i taj smjer prikazati u stvarnom vremenu kao točku u 3D prostoru u "
  "web-pregledniku. Sustav ne prepoznaje <i>što</i> je zvuk, nego <i>odakle</i> "
  "dolazi — njegov azimut (kut u vodoravnoj ravnini) i elevaciju (kut prema gore).")
P("Princip rada temelji se na jednostavnoj fizikalnoj činjenici: zvuk putuje "
  "konačnom brzinom (oko 343 m/s u zraku). Ako su mikrofoni razmaknuti u "
  "prostoru, isti zvučni val do njih stiže u <b>malo različitim trenucima</b>. "
  "Ta razlika u vremenu dolaska zove se <b>TDOA</b> (Time Difference Of Arrival). "
  "Iz nekoliko TDOA mjerenja i poznatih položaja mikrofona moguće je "
  "geometrijski izračunati smjer izvora.")
Callout("Intuicija u jednoj rečenici",
  "Ako mikrofon M2 «čuje» pljesak 0.2 ms prije mikrofona M1, izvor je bliži "
  "mikrofonu M2 — a koliko točno «u stranu», govori nam omjer kašnjenja na svim "
  "parovima mikrofona.")
P("Sustav je podijeljen na dvije ploče s jasno odvojenim ulogama:")
Bullets([
  "<b>STM32G474</b> — «mozak za obradu signala». Uzorkuje 4 mikrofona, "
  "obavlja svu matematiku (FFT, korelacija, geometrija) i kao rezultat šalje "
  "samo tri broja: azimut, elevaciju i jakost.",
  "<b>ESP32</b> — «prozor prema korisniku». Prima te brojeve preko UART-a, "
  "podiže vlastitu Wi-Fi mrežu i poslužuje web-stranicu s 3D prikazom. "
  "Ne radi nikakvu obradu zvuka.",
])
P("Ovakva podjela tipičan je obrazac u ugradbenim sustavima: <b>mikrokontroler "
  "specijaliziran za determinističku obradu u stvarnom vremenu</b> (STM32 s FPU-om "
  "i DSP knjižnicom) odvojen je od <b>mikrokontrolera specijaliziranog za "
  "povezivost</b> (ESP32 s Wi-Fi/TCP-IP stogom). Svaki radi ono u čemu je dobar, "
  "a povezuje ih jednostavan serijski kabel.")

# ============================================================================
#  2. ARHITEKTURA
# ============================================================================
H1("2.&nbsp;&nbsp;Arhitektura: dvije ploče, jedan tok podataka")
P("Cijeli sustav najlakše je razumjeti kao <b>cjevovod</b> (pipeline): podatak "
  "ulazi kao zvučni tlak na mikrofonu, a izlazi kao piksel na ekranu. Na svakom "
  "koraku podatak mijenja oblik i postaje «sažetiji».")
DataTable(
    [["Korak", "Oblik podatka", "Količina"],
     ["Zvučni val", "tlak zraka", "—"],
     ["Mikrofon + pretpojačalo", "analogni napon 0–3.3 V", "4 kanala"],
     ["ADC (TIM8 okida)", "12-bitni uzorci (0–4095)", "256 kS/s ukupno"],
     ["DMA u RAM", "interleaveani uint16 buffer", "8192 uzoraka"],
     ["DSP (FFT/GCC-PHAT)", "kutovi azimut/elevacija", "≈ 5 brojeva"],
     ["UART paket", "10 bajtova (tip 0x03)", "po detekciji"],
     ["ESP32 → WebSocket", "JSON tekst", "po detekciji"],
     ["Web preglednik", "3D točka (WebGL)", "vizualno"]],
    col_widths=[4.6*cm, 7.2*cm, 4.4*cm],
    caption="Tablica 2. Tok podataka — svaki korak smanjuje količinu, a povećava «značenje» podatka."
)
H2("2.1&nbsp;&nbsp;Blok-shema")
Code(
"""
   4 × MIKROFON
   (analogni napon)
        │
        ▼
 ┌─────────────────────────────────────────────────────────────┐
 │  STM32G474RE  (Cortex-M4F @ 170 MHz, FreeRTOS)               │
 │                                                             │
 │  TIM8 (64 kHz)──► ADC1 (4 ranka)──► DMA1 (kružni, HT/TC)    │
 │                                         │                   │
 │                                         ▼  (prekid)         │
 │                       queueDmaEvent ──► [ACQ_Task]          │
 │                                         │ snapshot           │
 │                       queueSnapshot ──► [FFT_Task]          │
 │                                         │  GCC-PHAT + 3D     │
 │                       queueResult   ──► [UART_Task]         │
 │                                         │                   │
 └─────────────────────────────────────── │ ──────────────────┘
                                           ▼  UART4 @115200
                                    [AA BB 03 .. CC DD]
                                           │
 ┌─────────────────────────────────────── │ ──────────────────┐
 │  ESP32                                  ▼                   │
 │                                  UART1 (rx_task)            │
 │                                  parser → JSON              │
 │   Wi-Fi AP "SoundLocalization"   │                         │
 │   HTTP server (port 80) ◄────────┘                         │
 │   WebSocket  /ws  ──────────────► web preglednik (WebGL 3D)│
 └─────────────────────────────────────────────────────────────┘
""",
caption="Slika 1. Blok-shema cijelog sustava: fizički signal → digitalna obrada → mreža → ekran."
)
P("Primijetite tri <b>reda čekanja</b> (queue) unutar STM32: oni razdvajaju "
  "dijelove sustava koji rade različitom brzinom. Prekidna rutina (ISR) i taskovi "
  "nikada ne dijele podatke izravno — uvijek preko reda čekanja. Zašto je to "
  "važno objašnjava poglavlje 5; kako je iskorišteno, poglavlje 7.")

# ============================================================================
#  3. HARDVER I GEOMETRIJA
# ============================================================================
H1("3.&nbsp;&nbsp;Hardver i geometrija mikrofonskog niza")
P("Točnost lokalizacije izravno ovisi o tome koliko <b>precizno poznajemo "
  "položaje mikrofona</b>. Cijela matematika polazi od koordinata zadanih u "
  "datoteci <font name='Consolas'>audio_common.h</font>. Mikrofon M1 je u "
  "ishodištu i služi kao <b>referentni</b> — svi se TDOA mjere u odnosu na njega.")
DataTable(
    [["Mikrofon", "X [cm]", "Y [cm]", "Z [cm]", "ADC rank", "Pin"],
     ["M1 (ref.)", "0.00", "0.00", "0.00", "RANK1", "PB14 / IN5"],
     ["M2", "8.67", "5.00", "0.00", "RANK2", "PC0 / IN6"],
     ["M3", "8.67", "−5.00", "0.00", "RANK3", "PC1 / IN7"],
     ["M4 (vrh)", "5.00", "0.00", "8.00", "RANK4", "PC2 / IN8"]],
    col_widths=[3.0*cm, 2.2*cm, 2.2*cm, 2.2*cm, 2.4*cm, 3.4*cm],
    align={1:"CENTER",2:"CENTER",3:"CENTER",4:"CENTER"},
    caption="Tablica 3. Položaji mikrofona i ADC kanali. M1–M2–M3 čine ~jednakostranični trokut u ravnini, M4 je iznad baze."
)
P("Raspored je odabran promišljeno: tri mikrofona (M1, M2, M3) leže u "
  "vodoravnoj ravnini i daju <b>azimut</b>, dok je četvrti (M4) podignut u "
  "visinu (Z = 8 cm) i tek on omogućuje mjerenje <b>elevacije</b>. Bez mikrofona "
  "izvan ravnine sustav ne bi mogao razlikovati zvuk odozgo od zvuka odozdo.")
H2("3.1&nbsp;&nbsp;Koordinatni sustav i značenje kutova")
Bullets([
  "<b>+X</b> → 0° azimut: «naprijed» (simetrala između M2 i M3).",
  "<b>+Y</b> → +90° azimut: lijevo (strana mikrofona M2).",
  "<b>−Y</b> → −90° azimut: desno (strana mikrofona M3).",
  "<b>−X</b> → ±180° azimut: nazad (strana mikrofona M1).",
  "<b>+Z</b> → +90° elevacija: ravno gore (smjer mikrofona M4).",
])
Callout("Zašto baš ~10 cm razmaka",
  "Veći razmak → veće (lakše mjerljivo) kašnjenje → bolja kutna rezolucija. Ali "
  "ako je razmak veći od pola valne duljine najviših frekvencija, javlja se "
  "prostorni «aliasing» (dvosmislenost smjera). Razmak ~10 cm pri 343 m/s daje "
  "najveće kašnjenje od oko 18.7 uzoraka na 64 kHz — odatle konstanta "
  "TDOA_MAX_SAMPLES = 20.")
P("Računica granice: najveći razmak od referentnog mikrofona je ~10 cm. Vrijeme "
  "da zvuk prijeđe 10 cm je 0.10 m ÷ 343 m/s = 291.5 µs. Pri 64 000 uzoraka/s to "
  "je 291.5 µs × 64 000 = <b>18.66 uzoraka</b>. Zaokruženo naviše s rezervom → 20.")

# ============================================================================
#  4. FOURIER / FFT  (NOVO — UČITELJSKO)
# ============================================================================
H1("4.&nbsp;&nbsp;Matematički temelji: Fourierova transformacija i FFT")
P("Ovo poglavlje gradi razumijevanje FFT-a od nule, na malim primjerima koje "
  "možete pratiti olovkom na papiru. Tek u zadnjem odjeljku (4.9) povezujemo to "
  "s konkretnom implementacijom u ovom projektu. Ako vam je gradivo poznato, "
  "možete preskočiti na 4.9.")

H2("4.1&nbsp;&nbsp;Dvije domene: vrijeme i frekvencija")
P("Signal s mikrofona prirodno «vidimo» u <b>vremenskoj domeni</b>: niz "
  "vrijednosti amplitude u jednakim vremenskim razmacima (uzorci). To odgovara na "
  "pitanje «kolika je amplituda u trenutku t?».")
P("Fourierova transformacija isti signal prikazuje u <b>frekvencijskoj domeni</b>: "
  "kao recept koji kaže <b>od kojih je sinusoida (i kojih jakosti i faza) signal "
  "sastavljen</b>. To odgovara na pitanje «koliko ima koje frekvencije u signalu?». "
  "Oba prikaza nose istu informaciju — samo posloženu drugačije.")
Callout("Analogija: glazbeni akord",
  "Vremenska domena je titranje zraka koje uho prima. Frekvencijska domena je "
  "popis nota u akordu (npr. C, E, G) i koliko je svaka glasna. Fourier je «uho "
  "koje raščlani akord na pojedine note».")

H2("4.2&nbsp;&nbsp;Ideja: svaki signal je zbroj sinusoida")
P("Temeljni Fourierov uvid: gotovo svaki signal može se napisati kao zbroj "
  "sinusa i kosinusa različitih frekvencija. Niska frekvencija opisuje spore "
  "promjene, visoka frekvencija oštre/brze promjene. «Težine» u tom zbroju "
  "(koliko svake frekvencije ima) upravo su ono što transformacija računa.")
P("Budući da računalo radi s konačnim nizom uzoraka (a ne s neprekidnom "
  "funkcijom), koristimo <b>diskretnu</b> Fourierovu transformaciju — DFT.")

H2("4.3&nbsp;&nbsp;DFT — formula i značenje svakog simbola")
P("Za niz od N uzoraka x[0], x[1], …, x[N−1], DFT daje N kompleksnih brojeva "
  "X[0], …, X[N−1]:")
Code(
"""
        N-1
X[k] =  Σ   x[n] · e^(−j·2π·k·n / N)        k = 0, 1, ..., N-1
        n=0
""",
caption="Formula DFT-a. X[k] mjeri «koliko ima» frekvencije koja u prozor stane točno k puta."
)
Bullets([
  "<b>x[n]</b> — n-ti uzorak u vremenu (realan broj kod nas).",
  "<b>X[k]</b> — k-ti «bin» (frekvencijski pretinac), kompleksan broj.",
  "<b>k</b> — koliko punih titraja ta frekvencija napravi unutar prozora od N "
  "uzoraka. k=0 je istosmjerna (DC) komponenta, k=1 je jedan titraj po prozoru, itd.",
  "<b>e^(−j·2π·k·n/N)</b> — jedinična «sonda»: rotirajući kompleksni broj "
  "(fazor) frekvencije k. Pritom je j imaginarna jedinica (j² = −1).",
  "<b>Σ</b> — zbroj po svim uzorcima: množimo signal sa sondom i zbrajamo.",
])
P("Ključ je <b>Eulerova formula</b>, koja kompleksni eksponent pretvara u sinus "
  "i kosinus:")
Code(
"""
e^(−jθ) = cos(θ) − j · sin(θ)

=> X[k] = Σ x[n]·cos(2π·k·n/N)  −  j · Σ x[n]·sin(2π·k·n/N)
          \\_______ realni dio ______/      \\____ imaginarni dio ___/
""",
caption="DFT preko sinusa/kosinusa: realni dio «koliko liči na kosinus», imaginarni «koliko na sinus» te frekvencije."
)
P("Intuitivno: za svaki k uspoređujemo signal s kosinusom i sinusom te "
  "frekvencije. Ako se dobro poklapaju, zbroj je velik → te frekvencije ima "
  "puno. Ako se ne poklapaju, doprinosi se međusobno poništavaju → zbroj je ~0.")

H2("4.4&nbsp;&nbsp;Primjer 1 — DFT na 4 uzorka, ručni izračun")
P("Uzmimo najjednostavniji slučaj N = 4 i signal x = [1, 2, 3, 4]. Za N = 4 "
  "sonda se silno pojednostavi jer je e^(−j·2π/4) = e^(−jπ/2) = −j, pa je "
  "e^(−j·2π·k·n/4) = (−j)^(k·n). Potencije od (−j) kruže: ")
Code(
"""
(−j)^0 = 1     (−j)^1 = −j     (−j)^2 = −1     (−j)^3 = +j   (pa se ponavlja)
""",
caption=None)
P("Sada uvrstimo za svaki k (k·n po modulu 4):")
Code(
"""
X[0] = 1·1 + 2·1   + 3·1   + 4·1    = 1+2+3+4          = 10
X[1] = 1·1 + 2·(−j)+ 3·(−1)+ 4·(+j) = (1−3) + (−2+4)j  = −2 + 2j
X[2] = 1·1 + 2·(−1)+ 3·1   + 4·(−1) = 1−2+3−4          = −2
X[3] = 1·1 + 2·(+j)+ 3·(−1)+ 4·(−j) = (1−3) + (2−4)j   = −2 − 2j
""",
caption="Rezultat: X = [10, −2+2j, −2, −2−2j]. Primijetite X[3] = konjugat od X[1] — tipično za realan ulaz."
)
P("Provjera DC-a: X[0] = 10 je samo zbroj svih uzoraka, tj. N puta prosjek "
  "(prosjek = 2.5). To uvijek vrijedi — <b>bin 0 je istosmjerna komponenta</b>.")
Callout("Zašto je X[3] zrcalna kopija X[1]",
  "Kad je ulaz realan, druga polovica spektra je «konjugirano zrcalo» prve "
  "(X[N−k] = konjugat X[k]). Zato u praksi za realan signal čuvamo samo prvih "
  "N/2+1 binova — ostatak ne nosi novu informaciju. Tu činjenicu koristi i "
  "«realni FFT» (4.9).")

H2("4.5&nbsp;&nbsp;Primjer 2 — čisti ton postaje šiljak u spektru")
P("Uzmimo N = 8 i signal koji je <b>točno jedan</b> puni sinus po prozoru, "
  "x[n] = sin(2π·1·n/8) (dakle frekvencija bina k = 1):")
Code(
"""
n      :   0     1     2     3     4     5     6     7
x[n]   : 0.00  0.71  1.00  0.71  0.00 −0.71 −1.00 −0.71
""",
caption=None)
P("DFT takvog čistog sinusa daje gotovo sve nule, osim dva bina:")
Code(
"""
|X[k]| :   0     4     0     0     0     0     0     4
k      :   0    (1)    2     3     4     5     6    (7)
""",
caption="Sva «energija» je u binu k=1 (i njegovom zrcalu k=7). Drugim riječima: jedna frekvencija → jedan šiljak."
)
P("Ovo je suština: <b>spektar pokazuje koje frekvencije postoje</b>. Sinus jedne "
  "frekvencije pokaže se kao oštar vrh točno na svom binu; složeniji zvuk (npr. "
  "pljesak) raspodijeli energiju po mnogo binova. Iznos 4 = N/2 dolazi od "
  "konvencije normalizacije DFT-a (amplituda 1 → N/2 po svakoj od dvije zrcalne "
  "polovice).")

H2("4.6&nbsp;&nbsp;Magnituda i faza — što nam X[k] govori")
P("Svaki X[k] je kompleksan broj X[k] = a + j·b i ima dvije korisne veličine:")
Bullets([
  "<b>Magnituda</b> |X[k]| = √(a² + b²): <i>koliko</i> ima te frekvencije "
  "(jakost/amplituda).",
  "<b>Faza</b> ∠X[k] = atan2(b, a): <i>s kojim pomakom</i> (kašnjenjem) ta "
  "frekvencija počinje.",
])
Callout("Zašto je faza ključna za ovaj projekt",
  "Kašnjenje signala u vremenu (TDOA!) u frekvencijskoj se domeni očituje kao "
  "<b>linearna promjena faze</b> s frekvencijom. Upravo zato metoda GCC-PHAT "
  "(poglavlje 9) radi s fazom: kašnjenje između dva mikrofona «pročita» iz "
  "razlike faza njihovih spektara. Magnitudu čak namjerno odbacuje.")

H2("4.7&nbsp;&nbsp;Zašto FFT: ista matematika, drastično manje računa")
P("Izravan izračun DFT-a po formuli traži, za svaki od N binova, zbroj od N "
  "članova → <b>N² operacija</b>. Za N = 1024 to je preko milijun množenja po "
  "transformaciji — pretjerano za stvarno vrijeme.")
P("<b>FFT</b> (Fast Fourier Transform) je <i>algoritam</i> koji računa <b>isti "
  "rezultat</b>, ali pametno iskorištava simetrije pa mu treba samo "
  "<b>N·log₂N</b> operacija:")
DataTable(
    [["N (veličina)", "DFT izravno (N²)", "FFT (N·log₂N)", "Ubrzanje"],
     ["8", "64", "24", "≈ 2.7×"],
     ["64", "4 096", "384", "≈ 11×"],
     ["1024", "1 048 576", "10 240", "≈ 102×"],
     ["4096", "16 777 216", "49 152", "≈ 341×"]],
    col_widths=[3.2*cm, 4.6*cm, 4.4*cm, 4.0*cm],
    align={0:"CENTER",1:"CENTER",2:"CENTER",3:"CENTER"},
    caption="Tablica 4. FFT je to isplativiji što je N veći. Zahtijeva da je N potencija broja 2."
)
P("Trik FFT-a (Cooley–Tukey): rastavi N-točkovni problem na dva N/2-točkovna "
  "(uzorci na parnim i na neparnim mjestima), riješi njih, pa rezultate spoji. "
  "To se rekurzivno ponavlja. Osnovna operacija spajanja zove se <b>leptir</b> "
  "(butterfly).")
Code(
"""
        x[n]  (N uzoraka)
        /            \\
  parni indeksi   neparni indeksi      <- podijeli
   (N/2 točaka)    (N/2 točaka)
        |               |
      FFT(N/2)       FFT(N/2)          <- riješi manje (rekurzija)
        \\               /
         spoji leptirima               <- X[k] = E[k] + W^k·O[k]
              |
            X[k]  (N binova)
""",
caption="Slika 2. «Podijeli pa vladaj»: log₂N razina, svaka N/2 leptira → N·log₂N operacija."
)

H2("4.8&nbsp;&nbsp;Primjer 3 — FFT na 4 uzorka preko leptira")
P("Provjerimo da FFT daje isti rezultat kao izravni DFT iz Primjera 1, "
  "x = [1, 2, 3, 4]. Prvo razdvojimo na parne i neparne uzorke:")
Code(
"""
parni    = [x0, x2] = [1, 3]
neparni  = [x1, x3] = [2, 4]

2-točkovni DFT (samo zbroj/razlika):
  E = DFT[1,3] = [1+3, 1−3] = [ 4, −2]      (E[0], E[1])
  O = DFT[2,4] = [2+4, 2−4] = [ 6, −2]      (O[0], O[1])
""",
caption="Korak 1: dva mala 2-točkovna DFT-a. Za N=2 DFT je samo (a+b) i (a−b)."
)
P("Sada spajamo leptirom, uz «twiddle» faktor W = e^(−j·2π/4) = −j, koristeći "
  "X[k] = E[k] + Wᵏ·O[k] za prvu polovicu i X[k+N/2] = E[k] − Wᵏ·O[k] za drugu:")
Code(
"""
X[0] = E[0] + W^0·O[0] = 4 + (1)(6)   = 10
X[1] = E[1] + W^1·O[1] = −2 + (−j)(−2) = −2 + 2j
X[2] = E[0] − W^0·O[0] = 4 − (1)(6)   = −2
X[3] = E[1] − W^1·O[1] = −2 − (−j)(−2) = −2 − 2j
""",
caption="Korak 2: leptiri. Rezultat X = [10, −2+2j, −2, −2−2j] — identičan Primjeru 1, uz manje računa."
)
Callout("Što smo upravo vidjeli",
  "Isti odgovor, ali umjesto 4² = 16 «punih» množenja, FFT je trebao tek "
  "nekoliko zbrajanja i jedno netrivijalno množenje s twiddle faktorom. Na "
  "N = 1024 ta razlika znači obradu u mikrosekundama umjesto milisekundama.")

H2("4.9&nbsp;&nbsp;Realni FFT i pakiranje rezultata (priprema za projekt)")
P("Naš ulaz (uzorci s mikrofona) je <b>realan</b>, pa je druga polovica spektra "
  "zrcalna (vidi 4.4). «Realni FFT» to iskorištava i radi gotovo dvostruko brže "
  "od kompleksnog te vraća samo neredundantne binove (0 … N/2). CMSIS-DSP "
  "funkcija <font name='Consolas'>arm_rfft_fast_f32</font> to pakira ovako:")
Code(
"""
fft[0]       = bin 0  (DC)        — realan
fft[1]       = bin N/2 (Nyquist)  — realan, spremljen u imaginarni slot DC-a
fft[2k]      = realni dio bina k   (k = 1 .. N/2−1)
fft[2k+1]    = imaginarni dio bina k
""",
caption="Pakiranje izlaza realnog FFT-a u CMSIS-DSP. Štedi memoriju: N realnih ulaza → N realnih izlaza."
)

H2("4.10&nbsp;&nbsp;Hann prozor i «curenje» spektra")
P("DFT pretpostavlja da je prozor uzoraka jedan period beskonačno ponavljajućeg "
  "signala. Ako signal na kraju prozora ne «sjedne» glatko na početak, nastaje "
  "oštar skok koji u spektru stvori lažne frekvencije — <b>spektralno curenje</b> "
  "(leakage). Lijek je <b>prozorska funkcija</b> koja rubove glatko spušta na nulu.")
Code(
"""
Hann prozor:   w[n] = 0.5 · (1 − cos(2π·n / (N−1))),   n = 0 .. N−1

signal[n]  ──×──  w[n]  ──►  «omekšani» signal za FFT
                (zvono: 0 na rubovima, 1 u sredini)
""",
caption="Hann prozor množi signal glatkim zvonom prije FFT-a i tako smanjuje curenje."
)

H2("4.11&nbsp;&nbsp;FFT u ovom projektu")
P("Sada možemo precizno opisati kako je FFT iskorišten. Pri pokretanju se "
  "jednom pripreme FFT instanca i tablica Hann prozora "
  "(<font name='Consolas'>GCC_Init</font>), a zatim se za svaki par mikrofona "
  "rade dvije unaprijedne i jedna inverzna transformacija.")
DataTable(
    [["Parametar", "Vrijednost", "Značenje"],
     ["FFT_SIZE (N)", "1024", "uzoraka po kanalu u jednom prozoru"],
     ["Brzina uzorkovanja", "64 000 Hz", "—"],
     ["Trajanje prozora", "1024 / 64000 = 16 ms", "vremenska duljina analize"],
     ["Rezolucija bina", "64000 / 1024 = 62.5 Hz", "razmak između susjednih frekvencija"],
     ["Nyquistova granica", "32 000 Hz", "najviša prikaziva frekvencija (N/2 bin)"],
     ["Knjižnica", "CMSIS-DSP", "arm_rfft_fast_f32 (realni FFT, hardverski FPU)"]],
    col_widths=[3.8*cm, 4.4*cm, 8.0*cm],
    caption="Tablica 5. Parametri FFT-a u projektu (audio_common.h, gcc_phat.c)."
)
Code(
"""
void GCC_Init(void) {
    for (int i = 0; i < FFT_SIZE; i++)
        s_hann[i] = 0.5f*(1.0f - cosf(2.0f*PI*i/(FFT_SIZE-1)));  /* Hann      */
    arm_rfft_fast_init_f32(&s_rfft, FFT_SIZE);                   /* FFT setup */
}

/* po paru mikrofona (vidi poglavlje 9 — GCC-PHAT): */
arm_rfft_fast_f32(&s_rfft, ref, fft_x, 0);   /* 0 = unaprijed (FFT)  */
arm_rfft_fast_f32(&s_rfft, sig, fft_y, 0);
...                                          /* križni spektar + PHAT */
arm_rfft_fast_f32(&s_rfft, fft_c, corr, 1);  /* 1 = inverzno (IFFT)  */
""",
caption="Isječak 1. FFT priprema i poziv (gcc_phat.c). Detaljna uporaba u poglavlju 9."
)
P("Veličina 1024 je kompromis: dovoljno velik prozor da rezolucija (62.5 Hz) i "
  "vremenski raspon (16 ms) pokriju pljesak, a dovoljno malen da cijela obrada "
  "(2 FFT-a + 1 IFFT po paru, 3 para) stane u proračun vremena između dva DMA "
  "prekida (16 ms) bez gubljenja podataka.")

# ============================================================================
#  5. FREERTOS OD NULE  (NOVO — UČITELJSKO)
# ============================================================================
H1("5.&nbsp;&nbsp;FreeRTOS od nule: taskovi, stog, prioriteti, queue/semafor")
P("Ovo poglavlje uvodi FreeRTOS na malom, samostalnom primjeru (mjerenje "
  "temperature i slanje). Na njemu učimo procijeniti stog, odabrati prioritet i "
  "izabrati ispravan mehanizam komunikacije. Kako je to primijenjeno u ovom "
  "projektu, slijedi u poglavljima 7 i 8.")

H2("5.1&nbsp;&nbsp;Zašto RTOS, a ne obična «super-petlja»")
P("Najjednostavniji firmware je jedna velika petlja "
  "(<font name='Consolas'>while(1)</font>) koja redom radi sve poslove. To radi "
  "dok su poslovi kratki i jednako hitni. Problem nastaje kad jedan posao mora "
  "biti <b>brz i točan na vrijeme</b> (npr. preuzeti svjež blok zvuka), a drugi "
  "je <b>spor</b> (npr. slanje 8 KB preko UART-a koje traje 0.7 s).")
P("U super-petlji bi spori posao «zaglavio» cijeli sustav i hitni bi posao "
  "zakasnio. <b>RTOS</b> (Real-Time Operating System) rješava to dijeleći "
  "program na <b>taskove</b> i automatski izmjenjujući ih na procesoru prema "
  "<b>prioritetu</b> — hitan task u svakom trenutku može «preuzeti» procesor od "
  "manje hitnog.")

H2("5.2&nbsp;&nbsp;Task, raspoređivač i stanja")
P("<b>Task</b> je neovisna funkcija koja se vrti u vlastitoj beskonačnoj petlji "
  "i ima <b>vlastiti stog</b>. <b>Raspoređivač</b> (scheduler) je dio RTOS-a koji "
  "odlučuje koji task trenutno koristi procesor. U svakom trenutku task je u "
  "jednom od stanja:")
DataTable(
    [["Stanje", "Značenje"],
     ["Running", "Trenutno se izvršava na procesoru (samo jedan task na jednojezgrenom MCU-u)."],
     ["Ready", "Spreman je i čeka procesor; raspoređivač će ga pokrenuti čim bude najviši po prioritetu."],
     ["Blocked", "Čeka na nešto (npr. podatak iz reda, istek vTaskDelay) i NE troši procesor."],
     ["Suspended", "Ručno zaustavljen; ne sudjeluje u raspoređivanju dok se ne nastavi."]],
    col_widths=[3.0*cm, 13.2*cm],
    caption="Tablica 6. Stanja taska. Ključ za uštedu energije/CPU-a: task koji čeka treba biti Blocked, ne u praznoj petlji."
)
Callout("Najvažnije pravilo za početnike",
  "Task NIKAD ne smije «vrtjeti prazno» čekajući (busy-wait). Umjesto toga "
  "blokira na redu čekanja, semaforu ili vTaskDelay — tada troši 0% procesora i "
  "pušta druge taskove da rade. Blokiranje je dobro, ne loše.")

H2("5.3&nbsp;&nbsp;Prioriteti i preemptivni raspored")
P("Svaki task dobiva <b>prioritet</b> (veći broj = viši prioritet). Pravilo "
  "preemptivnog raspoređivača: <b>uvijek se izvršava task najvišeg prioriteta "
  "koji je Ready</b>. Ako task višeg prioriteta postane spreman (npr. stigne mu "
  "podatak), on odmah «preuzima» procesor od nižeg.")
Code(
"""
Primjer: tri taska, brojevi = prioritet (3 najviši)

  vrijeme ──►
  A(3) hitan   :  ▓▓        ▓▓             ▓▓
  B(2) srednji :     ▓▓▓▓        ▓▓▓▓▓        ▓▓▓
  C(1) spori   :          ▓▓            ▓▓        (radi samo kad su A i B Blocked)

  ▓ = task drži procesor.  Kad A postane Ready, prekida B ili C (preemption).
""",
caption="Slika 3. Viši prioritet uvijek pobjeđuje. Niski task radi tek kad viši «spavaju» (Blocked)."
)

H2("5.4&nbsp;&nbsp;Mali primjer: senzor temperature + slanje")
P("Napravimo minimalan sustav s dva taska. <b>Senzor task</b> svakih 500 ms "
  "očita temperaturu i preda je <b>komunikacijskom tasku</b> koji je formatira i "
  "pošalje. Povezuje ih <b>red čekanja</b> (queue).")
Code(
"""
QueueHandle_t xTempQueue;                       /* veza senzor → komunikacija */

void vSensorTask(void *pv) {                    /* PROIZVOĐAČ podatka          */
    float temp;
    for (;;) {
        temp = read_temperature();              /* očitaj ADC + pretvori u °C */
        xQueueSend(xTempQueue, &temp, 0);       /* stavi vrijednost u red      */
        vTaskDelay(pdMS_TO_TICKS(500));         /* spavaj 500 ms → Blocked     */
    }
}

void vCommTask(void *pv) {                       /* POTROŠAČ podatka            */
    float temp;  char buf[32];
    for (;;) {
        /* blokiraj dok ne stigne vrijednost — 0% CPU dok čekamo */
        if (xQueueReceive(xTempQueue, &temp, portMAX_DELAY) == pdTRUE) {
            int n = snprintf(buf, sizeof(buf), "T=%.1f C\\r\\n", temp);
            uart_send(buf, n);                   /* sporo slanje, nije hitno    */
        }
    }
}

void start(void) {
    xTempQueue = xQueueCreate(4, sizeof(float));        /* do 4 vrijednosti    */
    xTaskCreate(vSensorTask, "SENS", 256, NULL, 3, NULL);  /* viši prioritet   */
    xTaskCreate(vCommTask,   "COMM", 384, NULL, 2, NULL);  /* niži prioritet   */
    vTaskStartScheduler();
}
""",
caption="Isječak 2. Cjelovit mali primjer dva taska + red čekanja. Isti obrazac (proizvođač–potrošač) koristi i ovaj projekt."
)

H2("5.5&nbsp;&nbsp;Kako znamo je li stog dovoljno velik — izračun")
P("Pri kreiranju taska zadajemo veličinu stoga (3. argument "
  "<font name='Consolas'>xTaskCreate</font>) u <b>riječima</b>. Na Cortex-M "
  "jedna riječ = 4 bajta, pa je 256 riječi = 1024 bajta. Procjena ide u tri "
  "koraka: analitička gornja granica → mjerenje → rezerva.")
H3("Korak A — analitička procjena za vCommTask")
P("Zbrojimo sve što task stavlja na stog u svom najdubljem putu izvođenja:")
DataTable(
    [["Doprinos stogu", "Procjena", "Obrazloženje"],
     ["Lokalne varijable", "≈ 40 B", "char buf[32] = 32 B, float temp = 4 B, int n = 4 B"],
     ["Poziv snprintf s %f", "≈ 150–200 B", "formatiranje floata je «stack-žedno» (interni baferi)"],
     ["Okviri ugnježđenih poziva", "≈ 80 B", "spremanje registara kroz nekoliko razina poziva"],
     ["Iznimka/prekid usred taska", "≈ 100 B", "Cortex-M sprema 8 registara (32 B) + lijeni FPU (~68 B)"],
     ["Međuzbroj (vrh potrošnje)", "≈ 400 B", "= 100 riječi"],
     ["Sigurnosna rezerva (~50%)", "≈ 200 B", "za neviđene grane/dublje pozive"],
     ["Preporuka", "≈ 600 B → 256 r", "zaokruženo naviše na 256 riječi (1024 B)"]],
    col_widths=[5.0*cm, 3.4*cm, 7.8*cm],
    caption="Tablica 7. Procjena stoga za vCommTask. Zaključak: 256 riječi je sigurno; 384 (kao u kodu) daje udobnu rezervu."
)
P("Za <b>vSensorTask</b> je račun manji (nema snprintf): par lokalnih varijabli "
  "+ rezerva → 128–256 riječi je dovoljno. Pravilo: task koji zove "
  "<font name='Consolas'>printf/snprintf</font>, plutajući zarez ili rekurziju "
  "treba znatno više stoga od taska koji samo prebacuje cijele brojeve.")
H3("Korak B — mjerenje u radu (high-water mark)")
P("Procjena nije dokaz. FreeRTOS nudi funkciju koja vraća <b>najmanju količinu "
  "ikad slobodnog stoga</b> za task — pustimo sustav da odradi sve rubne "
  "slučajeve, pa očitamo koliko je rezerve stvarno ostalo:")
Code(
"""
/* unutar taska, povremeno ili na kraju testa: */
UBaseType_t slobodno = uxTaskGetStackHighWaterMark(NULL);
/* npr. vrati 180 → ostalo je 180 RIJEČI nikad iskorištenog stoga.
   Ako vrati 5 → stog je gotovo pun, hitno ga povećaj!               */
""",
caption="Isječak 3. Mjerenje «vrha» potrošnje stoga. Cilj: ostaviti barem 25–50% rezerve iznad izmjerenog vrha."
)
H3("Korak C — zaštita (ako ipak pogriješimo)")
P("Uključimo automatsku provjeru prelijevanja. Ako task premaši stog, RTOS "
  "pozove naš «hook» u kojem zaustavimo sustav — lako uočljivo u debuggeru, "
  "umjesto nasumičnog rušenja kasnije:")
Code(
"""
/* FreeRTOSConfig.h */
#define configCHECK_FOR_STACK_OVERFLOW   2   /* provjeri uzorak na dnu stoga */

/* aplikacija: */
void vApplicationStackOverflowHook(TaskHandle_t t, char *name) {
    /* ovdje smo => task 'name' je prešao svoj stog */
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}
""",
caption="Isječak 4. Mreža sigurnosti za stog. Bolje stati ovdje nego se srušiti «negdje drugdje» pola sekunde kasnije."
)

H2("5.6&nbsp;&nbsp;Kako odabrati prioritet")
P("Prioritet se bira prema <b>hitnosti na vrijeme</b> (a ne prema važnosti "
  "posla!). Pitanje glasi: «što se dogodi ako ovaj task zakasni nekoliko "
  "milisekundi?»")
Bullets([
  "<b>Visok prioritet</b> → poslovi kratki i vremenski osjetljivi (reagiraj na "
  "događaj, uzmi svjež uzorak). U primjeru: vSensorTask (3) je viši jer se budi "
  "po tajmeru i mora uzeti uzorak s malim «podrhtavanjem» (jitterom).",
  "<b>Nizak prioritet</b> → poslovi dugi i tolerantni na kašnjenje. U primjeru: "
  "vCommTask (2) je niži jer je slanje sporo i nije strašno ako počne par ms "
  "kasnije.",
  "<b>Kratki na vrhu, dugi na dnu</b> → spriječi da spor task blokira hitan. "
  "Visokoprioritetni taskovi MORAJU često blokirati (vTaskDelay/queue), inače "
  "izgladnjuju niže.",
])
Callout("Česta zamka: prioritetna inverzija",
  "Ako niskoprioritetni task drži resurs (npr. zaključan mutex) koji treba "
  "visokoprioritetni, visoki čeka niskog — kao da su prioriteti zamijenjeni. "
  "Rješenje je <b>mutex s nasljeđivanjem prioriteta</b> (FreeRTOS ga ima): dok "
  "drži mutex, niski task privremeno dobije visoki prioritet da brže oslobodi "
  "resurs.")

H2("5.7&nbsp;&nbsp;Queue, semafor ili mutex — kada koji")
P("Tri su osnovna mehanizma za sinkronizaciju i razmjenu. Lako ih je zamijeniti, "
  "pa evo jasnog kriterija:")
DataTable(
    [["Mehanizam", "Što radi", "Kada ga koristiti"],
     ["Queue (red)", "Prenosi KOPIJU podatka + sinkronizira + razdvaja brzine.",
      "Kad jedan task/ISR proizvodi PODATAK koji drugi treba (npr. izmjerena temperatura)."],
     ["Binarni semafor", "Samo signalizira DOGAĐAJ (0/1), bez podatka.",
      "Kad treba samo «probuditi» task da nešto napravi (npr. «DMA gotov, buffer spreman»)."],
     ["Brojeći semafor", "Broji događaje/slobodne resurse.",
      "Kad ima više jedinica resursa ili se događaji mogu nakupiti (npr. 5 slobodnih mjesta)."],
     ["Mutex", "Štiti zajednički resurs (uz nasljeđivanje prioriteta).",
      "Kad više taskova dijeli istu strukturu/periferiju koju ne smiju mijenjati istovremeno."]],
    col_widths=[3.0*cm, 6.4*cm, 6.8*cm],
    caption="Tablica 8. Izbor mehanizma. Pravilo palca: trebaš li prenijeti PODATAK → queue; samo SIGNAL → semafor; ČUVATI resurs → mutex."
)
H3("Zašto u našem primjeru queue, a ne semafor")
P("Senzor tasku treba prenijeti <b>stvarnu vrijednost</b> temperature "
  "komunikacijskom tasku. Queue to radi izravno: kopira float, usput budi "
  "potrošača i razdvaja brzine (proizvođač 2 Hz, potrošač svojim tempom). "
  "Binarni semafor sam po sebi <b>ne nosi podatak</b> — morali bismo dodati "
  "dijeljenu globalnu varijablu i zasebno je štititi, što je više koda i "
  "podložnije greškama (utrka, torn read).")
H3("Kada bi semafor bio bolji")
P("Da senzor task samo treba reći «ima novih podataka u već dogovorenom "
  "bufferu» (bez prenošenja vrijednosti), binarni semafor bio bi lakši i brži. "
  "Tipičan slučaj je ISR koji javlja tasku «periferija je gotova» — nema "
  "vrijednosti za prenijeti, samo signal. Mutex pak ne služi za prenošenje nego "
  "isključivo za zaštitu: npr. dvije niti koje pišu u istu listu.")
Callout("Most prema projektu",
  "U ovom projektu DMA prekid šalje u queue broj 0 ili 1 (koja je polovica "
  "buffera gotova). Budući da nosi <b>podatak</b> (0/1), queue je ispravan izbor; "
  "da nije bilo razlike između polovica, dostajao bi binarni semafor. Na ESP32 "
  "strani lista WebSocket klijenata zaštićena je <b>mutexom</b> jer joj pristupa "
  "više niti. Oba primjera iz prakse vidjet ćete u poglavljima 7 i 11.")

# ============================================================================
#  6. AKVIZICIJA
# ============================================================================
H1("6.&nbsp;&nbsp;STM32 lanac akvizicije: TIM8 → ADC → DMA → ISR")
P("Prije bilo kakve matematike treba <b>pretvoriti zvuk u niz brojeva</b> "
  "pouzdanom, ravnomjernom brzinom. Ovaj lanac to radi gotovo bez sudjelovanja "
  "procesora — hardver sam «puni» memoriju, a procesor se javlja tek kad je "
  "blok podataka spreman. To je ključ za rad u stvarnom vremenu.")
H2("6.1&nbsp;&nbsp;TIM8 — generator takta uzorkovanja")
P("Tajmer TIM8 postavljen je da se preljeva (overflow) točno 64 000 puta u "
  "sekundi. Pri svakom preljevu šalje interni okidač (TRGO) prema ADC-u. Time je "
  "<b>brzina uzorkovanja potpuno hardverska i jednolika</b> — ne ovisi o tome "
  "koliko je procesor zauzet.")
Code(
"""
TIM_InitStruct.Prescaler  = 0;
TIM_InitStruct.Autoreload = 2655;     /* ARR */
LL_TIM_SetTriggerOutput(TIM8, LL_TIM_TRGO_UPDATE);   /* TRGO na svaki update */

/* Frekvencija = f_tim / (ARR + 1) = 170 MHz / 2656 = 64 006 Hz  ≈ 64 kHz
   Perioda = 1 / 64000 = 15.625 µs po uzorku */
""",
caption="Isječak 5. Konfiguracija TIM8 (timer_driver.c / main.c). ARR = 2655 daje ≈ 64 kHz."
)
H2("6.2&nbsp;&nbsp;ADC — jedan pretvarač, četiri kanala redom")
P("Sustav koristi <b>jedan</b> ADC (ADC1) koji u «scan» načinu pretvara četiri "
  "kanala jedan za drugim pri svakom okidaču tajmera. Budući da kanali nisu "
  "uzorkovani istovremeno nego redom, svaki sljedeći kasni za prethodnim za "
  "vrijeme jedne konverzije. To unosi sustavnu pogrešku u TDOA koju kod kasnije "
  "ispravlja (vidi 9.6).")
Code(
"""
/* ADC takt = PCLK / 4 = 170 MHz / 4 = 42.5 MHz  →  perioda 23.53 ns
   Po kanalu: 2.5 (uzorkovanje) + 12.5 (konverzija) = 15 ciklusa
            = 15 × 23.53 ns = 352.9 ns                                   */
#define CH_DELAY_S   352.9e-9f       /* M1=+0, M2=+1×, M3=+2×, M4=+3× */
""",
caption="Isječak 6. Kanalni offset zbog sekvencijalne konverzije (audio_common.h)."
)
H2("6.3&nbsp;&nbsp;DMA — hardver puni memoriju umjesto procesora")
P("DMA (Direct Memory Access) premješta svaki rezultat ADC-a izravno u RAM "
  "<b>bez prekidanja procesora za svaki uzorak</b>. Konfiguriran je kao "
  "<b>kružni</b>: kad napuni kraj buffera, automatski se vrati na početak. "
  "Buffer je dvostruke veličine i koristi se kao <b>dvostruki spremnik</b>:")
Bullets([
  "Dok DMA puni <b>drugu</b> polovicu buffera, procesor smije čitati <b>prvu</b> "
  "— i obrnuto. Nikad ne pišu i ne čitaju isti dio.",
  "DMA javlja dva prekida: <b>HT</b> (Half Transfer) i <b>TC</b> (Transfer "
  "Complete).",
])
Code(
"""
#define SAMPLES_PER_CHANNEL  1024
#define NUM_CH               4
#define HALF_BUFFER  (NUM_CH * SAMPLES_PER_CHANNEL)   /* 4096 uzoraka  */
#define FULL_BUFFER  (HALF_BUFFER * 2)                /* 8192 uzoraka  */
uint16_t adc_buffer[FULL_BUFFER];     /* 8192 × 2 B = 16 KB u RAM-u    */

/* INTERLEAVED raspored (kanali isprepleteni):
     adc_buffer[s*4 + 0]=M1   [s*4+1]=M2   [s*4+2]=M3   [s*4+3]=M4      */
""",
caption="Isječak 7. Buffer i interleaveani raspored (adc_driver.c, audio_common.h)."
)
P("Jedna «polovica» = 1024 vremenska trenutka × 4 kanala. Pri 64 kHz, 1024 "
  "uzorka po kanalu traje 1024 ÷ 64000 = <b>16 ms</b>. Dakle svakih 16 ms stigne "
  "jedan HT ili TC prekid — to je «otkucaj srca» cijelog sustava.")
H2("6.4&nbsp;&nbsp;ISR — kratak prekid koji samo javlja «gotovo»")
P("Prekidna rutina ne radi nikakvu obradu — samo pošalje broj (0 za HT, 1 za "
  "TC) u red čekanja i, ako je time probudila task višeg prioriteta, zatraži "
  "preraspodjelu procesora. Sva «teška» obrada događa se izvan prekida, u tasku.")
Code(
"""
void DMA1_Channel1_IRQHandler(void) {
    BaseType_t woken = pdFALSE;  uint32_t msg;
    if (LL_DMA_IsActiveFlag_HT1(DMA1)) {            /* prva polovica gotova */
        LL_DMA_ClearFlag_HT1(DMA1);  msg = 0u;
        xQueueSendFromISR(queueDmaEventHandle, &msg, &woken);
    }
    if (LL_DMA_IsActiveFlag_TC1(DMA1)) {            /* druga polovica gotova */
        LL_DMA_ClearFlag_TC1(DMA1);  msg = 1u;
        xQueueSendFromISR(queueDmaEventHandle, &msg, &woken);
    }
    portYIELD_FROM_ISR(woken);   /* probudi ACQ_Task ako treba */
}
""",
caption="Isječak 8. DMA prekidna rutina (stm32g4xx_it.c). Pravilo: u ISR-u radi minimum, ostalo prepusti tasku."
)

# ============================================================================
#  7. TASKOVI U PROJEKTU
# ============================================================================
H1("7.&nbsp;&nbsp;FreeRTOS u ovom projektu: tri taska i protok podataka")
P("Sada primjenjujemo načela iz poglavlja 5. Sustav koristi tri taska povezana "
  "s tri reda čekanja po obrascu <b>proizvođač–potrošač</b>: svaki task uzima "
  "posao iz ulaznog reda, obradi ga i preda u sljedeći.")
DataTable(
    [["Task", "Prioritet", "Stog", "Uloga"],
     ["ACQ_Task", "Realtime (najviši)", "256 riječi", "Kopira svježu polovicu buffera u snapshot"],
     ["FFT_Task", "High", "1024 riječi", "GCC-PHAT + 3D geometrija (sva matematika)"],
     ["UART_Task", "Low (najniži)", "256 riječi", "Šalje rezultat (i sirove uzorke) na ESP32"]],
    col_widths=[2.7*cm, 3.2*cm, 2.5*cm, 7.8*cm],
    caption="Tablica 9. Tri taska, prioriteti i stogovi (task_manager.c). Usporedite logiku prioriteta s odjeljkom 5.6."
)
P("Logika prioriteta (po pravilu iz 5.6 — hitnost na vrijeme): <b>akvizicija je "
  "najhitnija</b> jer propušten svjež podatak iz DMA-a nestaje zauvijek. Obrada "
  "(FFT) je važna ali smije malo pričekati. Slanje na UART je najmanje hitno "
  "(prikaz toleriran je da kasni desetke ms), pa ima najniži prioritet.")
H2("7.1&nbsp;&nbsp;Tri reda čekanja")
Code(
"""
queueDmaEventHandle  : DMA ISR  → ACQ_Task   (dubina 8, uint32 događaj HT/TC)
queueSnapshotHandle  : ACQ_Task → FFT_Task   (dubina 2, indeks buffera uint8)
queueResultHandle    : FFT_Task → UART_Task  (dubina 4, loc3d_result_t)
""",
caption="Isječak 9. Tri reda čekanja (task_manager.h/.c). Svaki nosi PODATAK → zato queue, ne semafor (vidi 5.7)."
)
H2("7.2&nbsp;&nbsp;ACQ_Task — siguran prijenos svježe polovice")
P("ACQ_Task čeka događaj iz DMA prekida (0 = HT, 1 = TC), tu polovicu kopira u "
  "jedan od <b>tri</b> «snapshot» buffera i indeks pošalje dalje. Tri buffera i "
  "kopiranje sprječavaju da DMA (koji nikad ne staje) prepiše podatke dok ih "
  "FFT_Task još obrađuje.")
Code(
"""
static void StartACQTask(void const *argument) {
    uint8_t write_idx = 0;
    for (;;) {
        osEvent evt = osMessageGet(queueDmaEventHandle, osWaitForever);
        if (evt.status != osEventMessage) continue;
        /* ako FFT zaostaje i red je pun — preskoči, ne prepisuj buffer
           koji se možda još čita (sprječava «torn read»)              */
        if (uxQueueSpacesAvailable(queueSnapshotHandle) == 0u) continue;
        const uint16_t *src = (evt.value.v == 0u) ? &adc_buffer[0]
                                                  : &adc_buffer[HALF_BUFFER];
        memcpy(acq_snapshot[write_idx], src, HALF_BUFFER*sizeof(uint16_t));
        xQueueSend(queueSnapshotHandle, &write_idx, 0u);
        write_idx = (write_idx + 1u) % ACQ_NUM_BUFFERS;   /* 0→1→2→0 */
    }
}
""",
caption="Isječak 10. ACQ_Task (task_manager.c). Trostruki spremnik + provjera praznine reda = nema oštećenih podataka."
)
Callout("Pojam: torn read (poderano čitanje)",
  "Ako jedan task čita niz dok ga drugi (ili DMA) istovremeno mijenja, dobije "
  "mješavinu starih i novih vrijednosti — besmislen podatak. Rješenje: kopiraj u "
  "zaseban buffer i nikad ne prepisuj onaj koji je još «u opticaju».")
H2("7.3&nbsp;&nbsp;FFT_Task — klizni prozor i sva matematika")
P("FFT_Task nadovezuje svježu polovicu na prethodnu, tvoreći <b>klizni prozor</b> "
  "od 2×1024 = 2048 trenutaka. Razlog: pljesak može pasti na granicu dviju "
  "polovica; s dvostrukim prozorom uvijek imamo cijeli događaj na jednom mjestu.")
Code(
"""
static void StartFFTTask(void const *argument) {
    loc3d_result_t result;  uint8_t read_idx;
    for (;;) {
        if (xQueueReceive(queueSnapshotHandle, &read_idx, portMAX_DELAY) != pdTRUE)
            continue;
        memcpy(&sliding_buf[0],          &sliding_buf[HALF_BUFFER], HALF_BUFFER*2);
        memcpy(&sliding_buf[HALF_BUFFER], acq_snapshot[read_idx],   HALF_BUFFER*2);
        if (!capture_ready) {
            if (LOC3D_Process(sliding_buf, &result)) {   /* ← sva obrada ovdje */
                capture_ready = 1;
                xQueueSend(queueResultHandle, &result, 0);
            }
        }
    }
}
""",
caption="Isječak 11. FFT_Task (task_manager.c). LOC3D_Process je srce DSP-a — razrađeno u poglavlju 9."
)
H2("7.4&nbsp;&nbsp;UART_Task i mehanizam «capture_ready»")
P("UART_Task šalje rezultat na ESP32, a opcionalno i sirove uzorke (8 KB) za "
  "analizu. Slanje 8 KB na 115200 bps traje ~0.7 s — vječnost za sustav koji "
  "okida svakih 16 ms. Zastavica <font name='Consolas'>capture_ready</font> "
  "«zamrzne» obradu dok traje slanje, da se sirovi buffer ne prepiše usred "
  "prijenosa. To je jednostavan ručni «handshake» između dva taska.")
Code(
"""
static void StartUARTTask(void const *argument) {
    loc3d_result_t r;
    for (;;) {
        if (xQueueReceive(queueResultHandle, &r, portMAX_DELAY) == pdTRUE) {
            UART_SendAngle3DPacket(r.az_tenth, r.el_tenth, r.strength);
            if (capture_ready) {
                UART_SendRawCapture(dbg_raw_ch0, dbg_raw_ch1,
                                    dbg_raw_ch2, dbg_raw_ch3);
                capture_ready = 0;     /* obrada se nastavlja */
            }
        }
    }
}
""",
caption="Isječak 12. UART_Task (task_manager.c)."
)

# ============================================================================
#  8. STACK SIZING U PROJEKTU
# ============================================================================
H1("8.&nbsp;&nbsp;Procjena veličine stoga u ovom projektu")
P("Primijenimo postupak iz odjeljka 5.5 na stvarne taskove. Ključna projektna "
  "odluka je da <b>veliki nizovi NE žive na stogu</b> — deklarirani su kao "
  "<font name='Consolas'>static</font> (u BSS segmentu), pa stog ostaje malen.")
Code(
"""
/* Veliki radni nizovi su 'static' → u BSS-u, NE na stogu FFT_Task-a: */
static float s_ch0[1024], s_ch1[1024], s_ch2[1024], s_ch3[1024];  /* 16 KB */
static float s_corr[1024];                                        /*  4 KB */
/* Unutar GCC_PHAT — također 'static': */
static float fft_x[1024], fft_y[1024], fft_c[1024];               /* 12 KB */
""",
caption="Isječak 13. Veliki nizovi su 'static' (sound_loc_3d.c, gcc_phat.c) — zato FFT_Task treba samo ~4 KB stoga."
)
P("Da su ti nizovi bili obične lokalne varijable, jedan poziv "
  "<font name='Consolas'>LOC3D_Process</font> tražio bi preko 30 KB stoga — "
  "višestruko više od dodijeljenog. Ovako na stogu ostaju samo skalari "
  "(pokazivači, brojači, međurezultati float).")
P("Slijedom koraka A–C iz 5.5: za FFT_Task analitička procjena (skalari + "
  "interni okviri CMSIS-DSP-a + FPU/prekid rezerva) staje u nekoliko stotina "
  "bajta; uz velikodušnu rezervu dodijeljeno je 1024 riječi (4 KB). ACQ i UART "
  "task rade samo s memcpy/UART pozivima → 256 riječi svaki. Vrijednosti se "
  "potvrđuju mjerenjem (high-water mark) i štite hookom za prelijevanje.")
Callout("Zašto je configENABLE_FPU = 1 obavezan ovdje",
  "FFT_Task intenzivno koristi plutajući zarez (asinf, atan2f, množenja matrice). "
  "Ako raspoređivač prekine task usred float-operacije, a druga nit dotakne FPU, "
  "bez čuvanja FPU konteksta vrijednosti se nepovratno pokvare. Uz FPU=1 "
  "FreeRTOS koristi «lijeno» spremanje FPU registara — čuva ih samo kad task "
  "stvarno koristi FPU.")
H2("8.1&nbsp;&nbsp;Bilanca FreeRTOS heap-a")
P("Osim stogova, FreeRTOS ima zajednički <b>heap</b> iz kojeg alocira stogove "
  "taskova, redove čekanja i druge objekte. Postavljen je na 24 KB:")
DataTable(
    [["Objekt", "Procjena"],
     ["ACQ_Task stog (256 riječi)", "1.0 KB"],
     ["FFT_Task stog (1024 riječi)", "4.0 KB"],
     ["UART_Task stog (256 riječi)", "1.0 KB"],
     ["3 reda čekanja + RTOS overhead", "≈ 0.6 KB"],
     ["Ukupno zauzeto", "≈ 6.6 KB"],
     ["configTOTAL_HEAP_SIZE", "24 KB"],
     ["Slobodno za budući rast", "≈ 17 KB"]],
    col_widths=[9.0*cm, 7.2*cm],
    align={1:"CENTER"},
    caption="Tablica 10. Bilanca heap-a (FreeRTOSConfig.h). STM32G474RE ima 128 KB RAM-a — 24 KB je ~19%."
)
P("Napomena: veliki nizovi (adc_buffer 16 KB, snapshoti 24 KB, klizni prozor "
  "16 KB, DSP nizovi ~32 KB) <b>ne dolaze iz heap-a</b> — statički su globalni "
  "(BSS) i linker ih smješta zasebno. Heap služi isključivo RTOS objektima.")

# ============================================================================
#  9. DETEKCIJA — KORAK PO KORAK
# ============================================================================
H1("9.&nbsp;&nbsp;Detekcija i lokalizacija zvuka — korak po korak (GCC-PHAT)")
P("Ovo je srž projekta — funkcija <font name='Consolas'>LOC3D_Process()</font>, "
  "koju FFT_Task zove za svaku novu polovicu (svakih 16 ms). Oslanja se na FFT "
  "iz poglavlja 4. Svaki korak prati <b>brojčani primjer</b> za jedan zamišljeni "
  "pljesak.")
Callout("Postavka primjera koji pratimo kroz cijelo poglavlje",
  "Pljesak dolazi iz smjera azimut +30°, elevacija +20°, dovoljno glasan. Kroz "
  "korake vidimo kako iz sirovih uzoraka ispadnu upravo ti kutovi.")
H2("9.1&nbsp;&nbsp;Korak 0 — Cooldown (sprječavanje višestrukih okidanja)")
P("Pljesak odzvanja desetke ms i prelijeva se preko više prozora. Bez kočnice "
  "sustav bi za jedan pljesak ispalio 5–10 detekcija. Nakon svake uspješne "
  "detekcije slijedi «hlađenje» od 19 prozora (≈ 304 ms).")
Code(
"""
#define DETECTION_COOLDOWN_FRAMES  19      /* 19 × 16 ms ≈ 304 ms */
if (s_cooldown > 0) { s_cooldown--; return 0; }   /* preskoči, jeftino */
""",
caption="Isječak 14. Cooldown (sound_loc_3d.c)."
)
H2("9.2&nbsp;&nbsp;Korak 1 — Pronalazak energetskog vrha")
P("Algoritam pronađe gdje <b>unutar</b> kliznog prozora leži najglasniji dio i "
  "ondje centrira analizu. Koristi klizeći prozor energije (16 uzoraka) koji se "
  "pomiče inkrementalno — oduzme uzorak koji «izlazi», doda onaj koji «ulazi» "
  "(O(N) umjesto O(N×W)).")
Code(
"""
int32_t v = (int32_t)s[ch] - 2048;     /* 2048 = sredina 12-bitnog raspona */
win_e += (v * v) >> 6;                  /* >>6 sprječava preljev            */
win_e -= (v_old*v_old) >> 6;            /* klizni pomak: skini stari rub    */
win_e += (v_new*v_new) >> 6;            /*               dodaj novi rub     */
if (win_e > best_e) { best_e = win_e; best_frame = f + PROBE/2; }
""",
caption="Isječak 15. find_peak_offset (sound_loc_3d.c). Rezultat: frame_offset = početak 1024-prozora centriranog na pljesak."
)
Callout("Primjer 9.2",
  "Ako je pljesak najglasniji oko 1200-tog trenutka (od 0–2047), algoritam vrati "
  "frame_offset ≈ 1200 − 512 = 688 → analizira uzorke [688 … 1711], s događajem "
  "u sredini.")
H2("9.3&nbsp;&nbsp;Korak 2 — Deinterleave, uklanjanje DC-a i Hann prozor")
P("Iz interleaveanog buffera izdvajamo 4 kanala i pripremamo ih za FFT "
  "(usporedi 4.10):")
Bullets([
  "<b>Uklanjanje DC-a.</b> Mikrofon «sjedi» na ~1.65 V (ADC ~2048). Za "
  "korelaciju nas zanima samo promjena oko sredine, pa oduzmemo prosjek kanala.",
  "<b>Hann prozor.</b> Glatko zvono na rubovima → 0 smanjuje spektralno curenje "
  "(objašnjeno u 4.10).",
])
Code(
"""
float w = s_hann[s];                  /* unaprijed izračunato zvono */
ch0[s] = ((float)r0 - dc0) * w;       /* (uzorak − DC) × prozor     */
""",
caption="Isječak 16. GCC_ExtractChannels (gcc_phat.c)."
)
Callout("Primjer 9.3",
  "Uzorak M1 = 2100, dc0 = 2048, Hann w = 0.95 → (2100 − 2048)×0.95 = 49.4. "
  "Tako «očišćen» signal ide u FFT.")
H2("9.4&nbsp;&nbsp;Korak 3 — RMS prag (je li bilo dovoljno glasno)")
P("RMS (Root Mean Square) je efektivna amplituda — mjera glasnoće. Pretih "
  "najglasniji kanal → tišina/šum → izlaz. Drugi (blaži) prag traži da ni "
  "najtiši kanal nije posve mrtav.")
Code(
"""
#define MIN_RMS_THRESHOLD    10.0f
#define MIN_RMS_PER_CHANNEL  (MIN_RMS_THRESHOLD * 0.2f)   /* = 2.0 */
if (rms_max < MIN_RMS_THRESHOLD)    return 0;   /* pretiho */
if (rms_min < MIN_RMS_PER_CHANNEL)  return 0;   /* jedan mik mrtav */
""",
caption="Isječak 17. RMS prag (sound_loc_3d.c). RMS = sqrt(prosjek kvadrata uzoraka)."
)
Callout("Primjer 9.4",
  "Pljesak rms = [42, 38, 35, 30]: rms_max=42&gt;10 ✓ i rms_min=30&gt;2 ✓ → prolazi. "
  "Tiha soba rms = [3,2,3,2]: rms_max=3&lt;10 → izlaz.")
H2("9.5&nbsp;&nbsp;Korak 4 — GCC-PHAT: kašnjenje između parova")
P("Za svaki par (M1–M2, M1–M3, M1–M4) mjerimo <b>za koliko uzoraka jedan signal "
  "kasni za drugim</b>. Naivni pristup je <b>križna korelacija</b>: kliži jedan "
  "signal preko drugog i traži pomak najboljeg poklapanja. GCC-PHAT je njena "
  "izoštrena inačica koja radi preko FFT-a iz poglavlja 4.")
H3("Zašto PHAT, a ne obična korelacija")
P("Obična korelacija daje širok, zaobljen vrh — teško je točno locirati "
  "maksimum, osobito uz jeku. <b>PHAT</b> (Phase Transform) u frekvencijskoj "
  "domeni <b>odbaci amplitudu i zadrži samo fazu</b>. Rezultat je vrlo oštar "
  "vrh točno na pravom kašnjenju — jer kašnjenje je linearni nagib faze po "
  "frekvenciji (sjetite se 4.6).")
NumList([
  "Izračunaj FFT oba signala: X = FFT(ref), Y = FFT(sig).",
  "Križni spektar po frekvenciji: C = konj(X) · Y.",
  "PHAT normalizacija: podijeli svaki C s njegovim iznosom |C| (ostaje samo faza).",
  "Inverzni FFT od C → korelacija u vremenu; položaj vrha je kašnjenje.",
])
Code(
"""
arm_rfft_fast_f32(&s_rfft, ref, fft_x, 0);   /* X = FFT(ref) */
arm_rfft_fast_f32(&s_rfft, sig, fft_y, 0);   /* Y = FFT(sig) */
float cre = re1*re2 + im1*im2;     /* Re{ konj(X)·Y } */
float cim = re1*im2 - im1*re2;     /* Im{ konj(X)·Y } */
float mag = sqrtf(cre*cre + cim*cim);
float w   = (mag > 1e-9f) ? 1.0f/(mag + 1e-9f) : 0.0f;   /* PHAT: /|C| */
fft_c[ire] = cre*w;  fft_c[iim] = cim*w;
arm_rfft_fast_f32(&s_rfft, fft_c, corr, 1);  /* IFFT → korelacija */
""",
caption="Isječak 18. GCC_PHAT (gcc_phat.c). Konj(X)·Y daje vrh na pozitivnom kašnjenju kad sig kasni za ref."
)
Callout("Primjer 9.5 (predznak govori smjer)",
  "Za par M1–M2: vrh na lagu +5 uzoraka znači da M2 kasni 5 uzoraka za M1 → val "
  "je do M1 stigao prije → izvor je bliži M1-strani. Predznak i iznos sva tri "
  "kašnjenja zajedno jednoznačno kodiraju smjer.")
H2("9.6&nbsp;&nbsp;Korak 5 — Točan vrh i korekcija ADC offseta")
P("Korelacija je uzorkovana u cijelim uzorcima, ali pravo kašnjenje rijetko "
  "pada točno na uzorak. <b>Parabolička interpolacija</b> kroz vrh i dva susjeda "
  "daje pod-uzorčano kašnjenje. Pretraga je ograničena na ±20 uzoraka (granica "
  "iz poglavlja 3).")
Code(
"""
delta = 0.5f * (c0 - c2) / (c0 - 2*c1 + c2);   /* tjeme parabole       */
float frac = (float)max_idx + delta;            /* npr. 5 + 0.3 = 5.3   */
return delay_samp * SAMPLE_PERIOD_S;            /* uzorci → sekunde     */
""",
caption="Isječak 19. GCC_FindTDOA (gcc_phat.c)."
)
P("Korekcija sekvencijalnog ADC offseta: GCC-PHAT izmjeri <b>prividno</b> "
  "kašnjenje koje uključuje hardverski pomak (6.2). Stvarno <b>akustičko</b> "
  "kašnjenje = izmjereno + (j−1)×CH_DELAY:")
Code(
"""
float tau12 = tau12_meas + 1.0f * CH_DELAY_S;   /* M2 = 2. rank */
float tau13 = tau13_meas + 2.0f * CH_DELAY_S;   /* M3 = 3. rank */
float tau14 = tau14_meas + 3.0f * CH_DELAY_S;   /* M4 = 4. rank */
""",
caption="Isječak 20. Korekcija kanalnog offseta (sound_loc_3d.c)."
)
Callout("Primjer 9.6 (naši brojevi)",
  "Za +30°/+20° prava akustička kašnjenja iz geometrije: τ12 ≈ −274 µs "
  "(−17.5 uzoraka), τ13 ≈ −137 µs (−8.8), τ14 ≈ −198 µs (−12.7).")
H2("9.7&nbsp;&nbsp;Korak 6 — Od kašnjenja do smjera (geometrija)")
P("Tri kašnjenja i poznata geometrija jednoznačno određuju smjer. Veza je "
  "linearna: ako složimo baseline vektore u matricu <b>D</b> (reci = Mj − M1), "
  "vrijedi <b>D · s = −c · τ</b>, gdje je <b>s</b> jedinični vektor prema "
  "izvoru, a <b>c</b> brzina zvuka.")
Code(
"""
/* jednom na startu: */  M_geom = c · inv(D),  D = [M2−M1; M3−M1; M4−M1]
/* po detekciji:      */  u = M_geom · τ        (smjer propagacije)
ux = M_geom[0][0]*τ12 + M_geom[0][1]*τ13 + M_geom[0][2]*τ14;   /* itd. */
/* smjer PREMA izvoru = −u, normaliziran: */
sx=−ux; sy=−uy; sz=−uz;  norm=sqrt(sx²+sy²+sz²);  s/=norm;
""",
caption="Isječak 21. Geometrijsko rješenje (LOC3D_Init + LOC3D_Process, sound_loc_3d.c)."
)
Callout("Zašto inverzija samo jednom",
  "Matrica D ovisi samo o položajima mikrofona, koji se ne mijenjaju. Skupu "
  "inverziju napravimo jednom (LOC3D_Init) i spremimo; u vrućoj petlji ostaje "
  "samo jeftino množenje matrice s vektorom. Tipičan DSP obrazac: "
  "«pretproračunaj sve što je konstantno».")
H2("9.8&nbsp;&nbsp;Korak 7 — Kutovi azimut i elevacija")
Code(
"""
float az_rad = atan2f(sy, sx);     /* azimut: [−180°, +180°]  */
float el_rad = asinf(sz);          /* elevacija: [−90°, +90°]  */
out->az_tenth = (int16_t)(az_rad * 1800.0f / PI);   /* radijani → 0.1° */
out->el_tenth = (int16_t)(el_rad * 1800.0f / PI);
""",
caption="Isječak 22. Pretvorba vektora u kutove (sound_loc_3d.c)."
)
Callout("Primjer 9.8 (zatvaranje kruga)",
  "Geometrija vrati s ≈ (0.814, 0.470, 0.342). Tada atan2(0.470, 0.814) = 30.0° "
  "→ az_tenth = 300, a asin(0.342) = 20.0° → el_tenth = 200. Točno smjer iz "
  "kojeg smo «pustili» pljesak.")
H2("9.9&nbsp;&nbsp;Korak 8 — Jakost signala")
Code(
"""
float str_f = 20.0f * log10f(rms_max + 1.0f);    /* dB-slično, log ljestvica */
if (str_f < 1.0f) str_f = 1.0f;  if (str_f > 100.0f) str_f = 100.0f;
out->strength = (uint8_t)str_f;   /* RMS 20→~26, 100→~40, 500→~54 */
""",
caption="Isječak 23. Izračun jakosti (sound_loc_3d.c)."
)
P("Na kraju se postavi cooldown i funkcija vrati 1 (valjana detekcija); FFT_Task "
  "rezultat šalje u red prema UART tasku.")

# ============================================================================
#  10. UART PROTOKOL
# ============================================================================
H1("10.&nbsp;&nbsp;UART protokol između STM32 i ESP32")
P("Ploče komuniciraju serijski (UART, 115200 bps, 8N1) jednostavnim "
  "<b>binarnim paketnim protokolom</b>. Svaki paket počinje s 0xAA 0xBB i "
  "završava s 0xCC 0xDD; između je bajt tipa i korisni teret. Start/end markeri "
  "omogućuju prijemniku da se «uhvati» na početak paketa i nakon izgubljenog bajta.")
DataTable(
    [["Tip", "Naziv", "Duljina", "Sadržaj"],
     ["0x02", "2D kut", "8 B", "AZ(2) + jakost(1)"],
     ["0x03", "3D kut", "10 B", "AZ(2) + EL(2) + jakost(1)"],
     ["0x04", "Sirovi capture", "~8 KB", "NCH + N + NCH×N×uint16 (big-endian)"]],
    col_widths=[1.8*cm, 3.0*cm, 2.4*cm, 9.0*cm],
    caption="Tablica 11. Tipovi paketa (uart_driver.c na obje strane). Primarno se koristi 0x03."
)
Code(
"""
3D paket (tip 0x03), 10 bajtova:
 ┌─────┬─────┬─────┬──────┬──────┬──────┬──────┬─────┬─────┬─────┐
 │ AA  │ BB  │ 03  │ AZ_H │ AZ_L │ EL_H │ EL_L │ STR │ CC  │ DD  │
 └─────┴─────┴─────┴──────┴──────┴──────┴──────┴─────┴─────┴─────┘
   start  start tip   azimut(BE)   elevacija(BE)  jak.  end   end
 AZ, EL = int16 big-endian u desetinkama °  (npr. 0x012C = 300 = 30.0°)
""",
caption="Isječak 24. Raspored 3D paketa. «BE» = big-endian (viši bajt prvi)."
)

# ============================================================================
#  11. ESP32
# ============================================================================
H1("11.&nbsp;&nbsp;ESP32: Wi-Fi, web-poslužitelj i 3D vizualizacija")
P("ESP32 je «prezentacijski» dio. Pri pokretanju "
  "(<font name='Consolas'>app_main</font>) redom inicijalizira: NVS, Wi-Fi "
  "(Access Point), HTTP poslužitelj, UART i procesor primljenih paketa.")
Code(
"""
void app_main(void) {
    nvs_flash_init();           /* trajna pohrana (Wi-Fi treba NVS)    */
    wifi_manager_init();        /* podigni vlastitu Wi-Fi mrežu (AP)   */
    web_server_init();          /* HTTP + WebSocket na portu 80        */
    uart_driver_init();         /* UART1: RX=GPIO16, TX=GPIO17, 115200 */
    sound_loc_processor_init(); /* pokreni rx_task (parser paketa)     */
}
""",
caption="Isječak 25. Redoslijed inicijalizacije na ESP32 (main.c)."
)
H2("11.1&nbsp;&nbsp;Wi-Fi Access Point")
P("ESP32 ne spaja se na postojeću mrežu, nego <b>stvara vlastitu</b> — sustav "
  "radi samostalno, bez routera. Korisnik se spoji na tu mrežu i otvori adresu "
  "poslužitelja.")
DataTable(
    [["Parametar", "Vrijednost"],
     ["SSID (ime mreže)", "SoundLocalization"],
     ["Lozinka", "soundloc123"],
     ["IP poslužitelja", "192.168.4.1"],
     ["Maks. klijenata", "4"]],
    col_widths=[5.5*cm, 10.7*cm],
    align={1:"CENTER"},
    caption="Tablica 12. Wi-Fi AP postavke (wifi_manager.c). U pregledniku otvoriti http://192.168.4.1"
)
H2("11.2&nbsp;&nbsp;Parser paketa — konačni automat (state machine)")
P("UART podaci stižu kao <b>tok bajtova</b> bez jasnih granica. "
  "<font name='Consolas'>rx_task</font> ih sklapa <b>konačnim automatom</b>: za "
  "svaki bajt prelazi u sljedeće stanje (SOF1 → SOF2 → tip → … → EOF). Tek uz "
  "ispravan end-marker paket se smatra valjanim.")
Code(
"""
switch (state) {
  case S_SOF1: if (b==0xAA) state=S_SOF2;  break;   /* čekaj start */
  case S_SOF2: state=(b==0xBB)?S_TYPE:S_SOF1; break;
  case S_TYPE: pkt_type=b;
               state=(b==0x03)?S_AZ_H:(b==0x04?S_RAW_NCH:S_SOF1); break;
  ...
  case S_EOF2: if (b==0xDD) {            /* paket potpun i ispravan */
                 int16_t az=(az_h<<8)|az_l;
                 web_server_send_data(az/10.0f, el/10.0f, str_val);
               } state=S_SOF1; break;
}
""",
caption="Isječak 26. Parser kao konačni automat (sound_loc_processor.c). Otporno na izgubljene bajtove."
)
Callout("Zašto automat, a ne «pročitaj 10 bajtova»",
  "Ako se izgubi jedan bajt, naivno čitanje fiksne duljine ostalo bi zauvijek "
  "pomaknuto. Automat se uvijek iznova sinkronizira na 0xAA 0xBB, pa se nakon "
  "greške sam oporavi na sljedećem paketu.")
H2("11.3&nbsp;&nbsp;WebSocket i mutex (veza s 5.7)")
P("Za podatke koji stižu spontano (svaki pljesak) prikladniji je <b>WebSocket</b> "
  "od klasičnog HTTP-a — trajna veza preko koje poslužitelj <b>sam</b> gurne "
  "podatak čim ga ima. Za svaku detekciju ESP32 svim preglednicima pošalje JSON.")
Code(
"""
{"azimuth":30.0,"polar":20.0,"strength":42}
""",
caption="Isječak 27. JSON poruka preko WebSocket-a (web_server.c)."
)
P("Lista spojenih klijenata čuva se uz <b>mutex</b> "
  "(<font name='Consolas'>ws_fds_mutex</font>) jer joj pristupaju dvije niti — "
  "ona koja prima nove veze i ona koja šalje podatke. To je upravo slučaj iz "
  "odjeljka 5.7 «mutex za zaštitu zajedničkog resursa»: bez zaključavanja lista "
  "bi se mogla oštetiti; mrtve veze automatski se uklanjaju.")
H2("11.4&nbsp;&nbsp;3D vizualizacija (WebGL)")
P("Web-stranica ugrađena je kao tekst u firmware ESP32 i poslužuje se na «/». "
  "Crta minimalnu 3D scenu (WebGL):")
Bullets([
  "Plava kocka u središtu = mikrofonski niz (slušatelj).",
  "Tri prozirne ravnine (XY, XZ, YZ) daju osjećaj prostora.",
  "Svaka detekcija doda <b>svjetleću kuglicu</b> na izračunatom smjeru, koja "
  "kroz 5 s polako izblijedi.",
  "Mišem/dodirom scena se zakreće i zumira (orbit kamera).",
])
Callout("Pretvorba kuta u 3D položaj kuglice",
  "Iz azimuta (a) i polara (p): x = 8·sin(a)·cos(p), y = 8·sin(p), "
  "z = −8·cos(a)·cos(p). Tako kut iz STM32 izračuna postaje točka koju oko vidi.")

# ============================================================================
#  12. CJELOVITI PRIMJER
# ============================================================================
H1("12.&nbsp;&nbsp;Cjeloviti primjer: od pljeska do točke na ekranu")
P("Spojimo sve u jedan slijed za <b>jedan pljesak</b> iz smjera azimut +30°, "
  "elevacija +20°:")
NumList([
  "<b>t = 0 ms.</b> Val stiže na mikrofone u malo različitim trenucima.",
  "<b>Kontinuirano.</b> TIM8 okida ADC svakih 15.6 µs; ADC redom čita M1–M4; "
  "DMA puni adc_buffer bez procesora.",
  "<b>Svakih 16 ms.</b> DMA javi HT/TC → ISR pošalje događaj u red → ACQ_Task "
  "kopira polovicu u snapshot i javi FFT_Task-u.",
  "<b>FFT_Task.</b> Sastavi klizni prozor (2048), pozove LOC3D_Process: nađe "
  "energetski vrh, izdvoji kanale (DC + Hann), provjeri RMS prag.",
  "<b>GCC-PHAT × 3.</b> Izmjeri τ12 ≈ −274 µs, τ13 ≈ −137 µs, τ14 ≈ −198 µs "
  "(nakon korekcije offseta).",
  "<b>Geometrija.</b> u = M_geom·τ, pa s = −u/|u| ≈ (0.814, 0.470, 0.342).",
  "<b>Kutovi.</b> azimut = 30.0°, elevacija = 20.0°, jakost ≈ 42.",
  "<b>UART.</b> UART_Task pošalje [AA BB 03 01 2C 00 C8 2A CC DD] na ESP32.",
  "<b>ESP32.</b> rx_task automatom sklopi paket, izračuna 30.0°/20.0°, pošalje "
  "JSON preko WebSocket-a.",
  "<b>Preglednik.</b> Doda svjetleću kuglicu na (x,y,z) za 30°/20°; HUD ispiše "
  "«Azimuth: 30.0° | Polar: 20.0° | Strength: 42».",
  "<b>Cooldown.</b> STM32 idućih ~304 ms ignorira nove detekcije.",
])
P("0x012C = 300 = 30.0° (azimut), 0x00C8 = 200 = 20.0° (elevacija), 0x2A = 42 "
  "(jakost) — bajtovi u paketu iz koraka 8.")

# ============================================================================
#  13. POJMOVNIK
# ============================================================================
H1("13.&nbsp;&nbsp;Pojmovnik za početnike")
DataTable(
    [["Pojam", "Objašnjenje"],
     ["ADC", "Analog-to-Digital Converter — pretvara napon u broj (ovdje 12-bitni, 0–4095)."],
     ["DMA", "Direct Memory Access — sklop koji premješta podatke u/iz memorije bez procesora."],
     ["ISR", "Interrupt Service Routine — kratka rutina koja se izvrši na hardverski prekid."],
     ["RTOS", "Real-Time OS — sustav koji dijeli procesor na taskove s prioritetima."],
     ["Task", "Neovisna nit izvođenja s vlastitim stogom; vrti se u petlji."],
     ["Scheduler", "Raspoređivač — dio RTOS-a koji bira koji task trenutno radi."],
     ["Preemption", "Oduzimanje procesora nižem tasku čim viši postane spreman."],
     ["Stack", "Stog — memorija za lokalne varijable i povratne adrese jednog taska."],
     ["High-water mark", "Najmanja ikad zabilježena slobodna rezerva stoga; mjeri stvarnu potrošnju."],
     ["Heap", "Gomila — zajednička memorija iz koje RTOS alocira objekte."],
     ["Queue", "Red čekanja — prenosi podatak među taskovima/ISR-om i sinkronizira ih."],
     ["Semafor", "Signalizira događaj (binarni) ili broji resurse (brojeći); ne nosi podatak."],
     ["Mutex", "Brava za zaštitu zajedničkog resursa, s nasljeđivanjem prioriteta."],
     ["DFT", "Discrete Fourier Transform — rastav niza uzoraka na frekvencijske binove."],
     ["FFT", "Fast Fourier Transform — brzi algoritam za DFT (N·log₂N umjesto N²)."],
     ["Bin", "Frekvencijski pretinac DFT-a; razmak = fs/N (kod nas 62.5 Hz)."],
     ["Magnituda / faza", "Iznos i kut kompleksnog X[k]: «koliko» i «s kojim pomakom» frekvencije."],
     ["Twiddle faktor", "Kompleksni množitelj e^(−j2πk/N) u leptiru FFT-a."],
     ["Hann prozor", "Glatko «zvono» kojim se množi signal da se smanji spektralno curenje."],
     ["TDOA", "Time Difference Of Arrival — razlika vremena dolaska zvuka na dva mikrofona."],
     ["Korelacija", "Mjera sličnosti dvaju signala u ovisnosti o njihovom pomaku."],
     ["GCC-PHAT", "Izoštrena križna korelacija (samo faza) za precizno mjerenje TDOA."],
     ["RMS", "Root Mean Square — efektivna amplituda (mjera glasnoće) signala."],
     ["Azimut / elevacija", "Vodoravni / okomiti kut smjera izvora."],
     ["Interleaved", "Isprepleteni raspored uzoraka: M1 M2 M3 M4 M1 M2…"],
     ["Big-endian", "Zapis broja s višim bajtom prvim (npr. 300 = 0x01 0x2C)."],
     ["WebSocket", "Trajna dvosmjerna veza preglednik–poslužitelj za «push» podatke."],
     ["Access Point", "Način rada u kojem ESP32 sam stvara Wi-Fi mrežu."]],
    col_widths=[3.4*cm, 12.8*cm], keep=False
)
Sp(10)
story.append(HRFlowable(width="100%", thickness=0.8, color=NAVY, spaceBefore=6, spaceAfter=6))
P("Kraj dokumenta (v2). Svi isječci koda i konstante preuzeti su izravno iz "
  "izvornog koda projekta (mape «Sound Localization» i «ESP32_Visualization»). "
  "Primjeri u poglavljima 4 i 5 namjerno koriste jednostavne, izmišljene "
  "vrijednosti radi učenja.", st_note)

# ============================================================================
#  IZGRADNJA PDF-a
# ============================================================================
def on_page(canvas, doc):
    canvas.saveState()
    w, h = A4
    if doc.page > 1:
        canvas.setFont("Calibri", 8); canvas.setFillColor(GREY)
        canvas.drawString(2.0*cm, h - 1.25*cm,
                          "Lokalizacija zvuka mikrofonskim nizom — STM32 + ESP32  (v2)")
        canvas.setStrokeColor(LINE); canvas.setLineWidth(0.4)
        canvas.line(2.0*cm, h - 1.35*cm, w - 2.0*cm, h - 1.35*cm)
    canvas.setFont("Calibri", 8.5); canvas.setFillColor(GREY)
    canvas.drawCentredString(w/2.0, 1.1*cm, str(doc.page))
    canvas.restoreState()

OUT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..",
                      "Dokumentacija_Lokalizacija_Zvuka_v2.pdf"))
doc = BaseDocTemplate(
    OUT, pagesize=A4,
    leftMargin=2.0*cm, rightMargin=2.0*cm, topMargin=1.7*cm, bottomMargin=1.6*cm,
    title="Lokalizacija zvuka mikrofonskim nizom — STM32 + ESP32 (v2)",
    author="Projekt dokumentacija",
)
frame = Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="main")
doc.addPageTemplates([PageTemplate(id="all", frames=[frame], onPage=on_page)])
doc.build(story)
print("PDF napisan:", OUT)
