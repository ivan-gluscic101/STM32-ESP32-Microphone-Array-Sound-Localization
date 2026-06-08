# -*- coding: utf-8 -*-
"""
Generator tehničke dokumentacije (PDF) za projekt
STM32 + ESP32 lokalizacije zvuka mikrofonskim nizom.

Font: Calibri / Consolas (pune hrvatske dijakritike, Unicode).
Paleta: namjerno suzdržana (tamno plava za naslove, siva za kod/tablice).
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
# Boje (suzdržana paleta)
# ----------------------------------------------------------------------------
NAVY   = colors.HexColor("#1F3A5F")   # naslovi
STEEL  = colors.HexColor("#2E5E8C")   # podnaslovi / akcenti
INK    = colors.HexColor("#1A1A1A")   # glavni tekst
GREY   = colors.HexColor("#666666")   # sporedni tekst
LIGHT  = colors.HexColor("#F2F2F2")   # pozadina koda
LINE   = colors.HexColor("#C9C9C9")   # linije tablica
CODEBG = colors.HexColor("#F4F5F7")
HDRBG  = colors.HexColor("#E4EAF1")

# ----------------------------------------------------------------------------
# Stilovi
# ----------------------------------------------------------------------------
styles = getSampleStyleSheet()

def S(name, **kw):
    base = kw.pop("parent", styles["Normal"])
    return ParagraphStyle(name, parent=base, **kw)

st_title   = S("Title2", fontName="Calibri-B", fontSize=26, leading=31,
               textColor=NAVY, alignment=TA_LEFT, spaceAfter=6)
st_subtitle= S("Sub2", fontName="Calibri", fontSize=13, leading=18,
               textColor=GREY, spaceAfter=2)
st_h1      = S("H1", fontName="Calibri-B", fontSize=17, leading=21,
               textColor=NAVY, spaceBefore=16, spaceAfter=7)
st_h2      = S("H2", fontName="Calibri-B", fontSize=13.5, leading=17,
               textColor=STEEL, spaceBefore=11, spaceAfter=4)
st_h3      = S("H3", fontName="Calibri-BI", fontSize=11.5, leading=15,
               textColor=INK, spaceBefore=8, spaceAfter=3)
st_body    = S("Body2", fontName="Calibri", fontSize=10.5, leading=15.5,
               textColor=INK, alignment=TA_JUSTIFY, spaceAfter=6)
st_body_l  = S("BodyL", parent=st_body, alignment=TA_LEFT)
st_note    = S("Note", fontName="Calibri-I", fontSize=9.5, leading=13.5,
               textColor=GREY, alignment=TA_LEFT, spaceAfter=6)
st_bullet  = S("Bul", fontName="Calibri", fontSize=10.5, leading=15,
               textColor=INK, alignment=TA_LEFT)
st_code    = S("Code2", fontName="Consolas", fontSize=8.6, leading=11.6,
               textColor=INK)
st_codecap = S("CodeCap", fontName="Calibri-I", fontSize=9, leading=12,
               textColor=GREY, spaceBefore=2, spaceAfter=7)
st_tcell   = S("TCell", fontName="Calibri", fontSize=9.3, leading=12.5,
               textColor=INK)
st_tcellb  = S("TCellB", fontName="Calibri-B", fontSize=9.3, leading=12.5,
               textColor=INK)
st_thead   = S("THead", fontName="Calibri-B", fontSize=9.3, leading=12.5,
               textColor=NAVY)
st_toc     = S("Toc", fontName="Calibri", fontSize=11, leading=19, textColor=INK)
st_toc_h   = S("TocH", fontName="Calibri-B", fontSize=11, leading=19, textColor=NAVY)

# ----------------------------------------------------------------------------
# Pomoćne funkcije za gradnju sadržaja
# ----------------------------------------------------------------------------
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
                              bulletColor=STEEL, leftIndent=16, spaceBefore=1,
                              spaceAfter=6))

def Code(text, caption=None):
    # escape za XML
    lines = text.split("\n")
    # ukloni vodeću/završnu prazninu
    while lines and lines[0].strip() == "":
        lines.pop(0)
    while lines and lines[-1].strip() == "":
        lines.pop()
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
        ("LEFTPADDING", (0,0), (-1,-1), 8),
        ("RIGHTPADDING", (0,0), (-1,-1), 8),
        ("TOPPADDING", (0,0), (-1,-1), 6),
        ("BOTTOMPADDING", (0,0), (-1,-1), 6),
        ("LINEBEFORE", (0,0), (0,-1), 2.5, STEEL),
    ]))
    story.append(KeepTogether(tbl) if len(esc) < 30 else tbl)
    if caption:
        story.append(Paragraph(caption, st_codecap))
    else:
        Sp(7)

def DataTable(rows, col_widths, header=True, caption=None, align=None):
    data = []
    for r_i, row in enumerate(rows):
        cells = []
        for c_i, cell in enumerate(row):
            if isinstance(cell, Paragraph):
                cells.append(cell)
            else:
                stl = st_thead if (header and r_i == 0) else st_tcell
                cells.append(Paragraph(str(cell), stl))
        data.append(cells)
    tbl = Table(data, colWidths=col_widths, repeatRows=1 if header else 0)
    ts = [
        ("GRID", (0,0), (-1,-1), 0.5, LINE),
        ("VALIGN", (0,0), (-1,-1), "MIDDLE"),
        ("LEFTPADDING", (0,0), (-1,-1), 6),
        ("RIGHTPADDING", (0,0), (-1,-1), 6),
        ("TOPPADDING", (0,0), (-1,-1), 4),
        ("BOTTOMPADDING", (0,0), (-1,-1), 4),
        ("ROWBACKGROUNDS", (0,1), (-1,-1), [colors.white, colors.HexColor("#FAFBFC")]),
    ]
    if header:
        ts += [("BACKGROUND", (0,0), (-1,0), HDRBG),
               ("LINEBELOW", (0,0), (-1,0), 0.8, STEEL)]
    if align:
        for col, a in align.items():
            ts.append(("ALIGN", (col,0), (col,-1), a))
    tbl.setStyle(TableStyle(ts))
    story.append(KeepTogether([tbl]) if len(rows) < 16 else tbl)
    if caption:
        story.append(Paragraph(caption, st_codecap))
    else:
        Sp(8)

def Rule():
    story.append(HRFlowable(width="100%", thickness=0.6, color=LINE,
                            spaceBefore=4, spaceAfter=8))

def Callout(title, text):
    inner = []
    inner.append(Paragraph(f"<b>{title}</b>", S("CalT", fontName="Calibri-B",
                  fontSize=10, leading=13, textColor=NAVY, spaceAfter=2)))
    inner.append(Paragraph(text, S("CalB", fontName="Calibri", fontSize=9.8,
                  leading=14, textColor=INK, alignment=TA_LEFT)))
    tbl = Table([[inner]], colWidths=[16.2*cm])
    tbl.setStyle(TableStyle([
        ("BACKGROUND", (0,0), (-1,-1), colors.HexColor("#EEF3F8")),
        ("BOX", (0,0), (-1,-1), 0.5, STEEL),
        ("LEFTPADDING", (0,0), (-1,-1), 10),
        ("RIGHTPADDING", (0,0), (-1,-1), 10),
        ("TOPPADDING", (0,0), (-1,-1), 7),
        ("BOTTOMPADDING", (0,0), (-1,-1), 7),
    ]))
    story.append(KeepTogether(tbl))
    Sp(8)

# ============================================================================
#  NASLOVNICA
# ============================================================================
Sp(40)
P("Tehnička dokumentacija", st_subtitle)
P("Lokalizacija zvuka mikrofonskim nizom", st_title)
P("STM32G474 (akvizicija i DSP)&nbsp;&nbsp;+&nbsp;&nbsp;ESP32 (Wi-Fi 3D vizualizacija)",
  S("Sub3", fontName="Calibri", fontSize=12.5, leading=17, textColor=STEEL, spaceAfter=18))
story.append(HRFlowable(width="100%", thickness=1.2, color=NAVY, spaceAfter=14))

P("Ovaj dokument detaljno opisuje arhitekturu sustava, raspodjelu posla na "
  "FreeRTOS taskove, način procjene veličine stoga (stack), te cjelokupni "
  "lanac obrade signala — od mikrofona do kuta prikazanog u web-pregledniku. "
  "Pisan je tako da ga može razumjeti i osoba koja tek počinje s ugradbenim "
  "(embedded) sustavima i digitalnom obradom signala (DSP): svaki korak je "
  "objašnjen i popraćen konkretnim brojčanim primjerom.", st_body)

Sp(10)
DataTable(
    [["Stavka", "Vrijednost"],
     ["Mikrokontroler (DSP)", "STM32G474RE — Cortex-M4F @ 170 MHz, 128 KB RAM"],
     ["Mikrofoni", "4 analogna (elektret + pretpojačalo), niz u 3D rasporedu"],
     ["Brzina uzorkovanja", "64 kHz po kanalu (4 kanala, isti ADC)"],
     ["Algoritam smjera", "GCC-PHAT (TDOA) → geometrijsko 3D rješenje"],
     ["Komunikacija", "UART @ 115200 bps, binarni paketni protokol"],
     ["Vizualizacija", "ESP32 Wi-Fi Access Point + WebSocket + WebGL 3D"],
     ["RTOS", "FreeRTOS (CMSIS-OS v1 omotač) na STM32; FreeRTOS na ESP32"]],
    col_widths=[5.2*cm, 11.0*cm],
    caption="Tablica 1. Ključne značajke sustava."
)
Sp(6)
P("Napomena o oznakama: kut azimuta dan je u rasponu [-180&deg;, +180&deg;], a "
  "vrijednosti se prenose kao «desetinke stupnja» (tenths) — npr. 305 znači 30.5°.",
  st_note)

story.append(PageBreak())

# ============================================================================
#  SADRŽAJ
# ============================================================================
P("Sadržaj", st_h1)
Rule()
toc = [
    ("1.", "Pregled sustava — što i zašto", False),
    ("2.", "Arhitektura: dvije ploče, jedan tok podataka", False),
    ("3.", "Hardver i geometrija mikrofonskog niza", False),
    ("4.", "STM32 lanac akvizicije: TIM8 → ADC → DMA → ISR", False),
    ("5.", "FreeRTOS taskovi i protok podataka", False),
    ("6.", "Procjena veličine stoga (stack sizing)", False),
    ("7.", "Detekcija i lokalizacija zvuka — korak po korak", False),
    ("8.", "UART protokol između STM32 i ESP32", False),
    ("9.", "ESP32: Wi-Fi, web-poslužitelj i 3D vizualizacija", False),
    ("10.", "Cjeloviti primjer: od pljeska do točke na ekranu", False),
    ("11.", "Pojmovnik za početnike", False),
]
for num, title, sub in toc:
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

P("Ovakva podjela je tipičan obrazac u ugradbenim sustavima: <b>mikrokontroler "
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
    caption="Tablica 2. Tok podataka kroz sustav — svaki korak smanjuje količinu, a povećava «značenje» podatka."
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
caption="Slika 1. Blok-shema cijelog sustava. Lijevo-desno: fizički signal → digitalna obrada → mreža → ekran."
)

P("Primijetite tri <b>reda čekanja</b> (engl. <i>queue</i>) unutar STM32: oni "
  "razdvajaju dijelove sustava koji rade različitom brzinom. Prekidna rutina "
  "(ISR) i taskovi nikada ne dijele podatke izravno — uvijek preko reda čekanja. "
  "To je temelj sigurnog višezadaćnog rada i objašnjeno je detaljno u poglavlju 5.")

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
    caption="Tablica 3. Položaji mikrofona i pridruženi ADC kanali. M1–M2–M3 čine ~jednakostranični trokut u ravnini, M4 je iznad baze."
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

Callout("Zašto baš ~10 cm razmaka?",
  "Razmak određuje dvije suprotstavljene stvari. Veći razmak → veće (lakše "
  "mjerljivo) kašnjenje → bolja kutna rezolucija. Ali ako je razmak veći od "
  "pola valne duljine najviših frekvencija, javlja se prostorni «aliasing» "
  "(dvosmislenost smjera). Razmak od ~10 cm pri brzini zvuka 343 m/s daje "
  "najveće kašnjenje od oko 18.7 uzoraka na 64 kHz — odatle konstanta "
  "TDOA_MAX_SAMPLES = 20 koja ograničava pretragu vrha korelacije.")

P("Računica gornje granice (zašto 20): najveći razmak od referentnog mikrofona "
  "je ~10 cm. Vrijeme da zvuk prijeđe 10 cm iznosi 0.10 m ÷ 343 m/s = 291.5 µs. "
  "Pri 64 000 uzoraka u sekundi to je 291.5 µs × 64 000 = <b>18.66 uzoraka</b>. "
  "Zaokruženo naviše i s malom rezervom → 20.")

# ============================================================================
#  4. AKVIZICIJA
# ============================================================================
H1("4.&nbsp;&nbsp;STM32 lanac akvizicije: TIM8 → ADC → DMA → ISR")

P("Prije bilo kakve matematike treba <b>pretvoriti zvuk u niz brojeva</b> "
  "pouzdanom, ravnomjernom brzinom. Ovaj lanac to radi gotovo bez sudjelovanja "
  "procesora — hardver sam «puni» memoriju, a procesor se javlja tek kad je "
  "blok podataka spreman. To je ključ za rad u stvarnom vremenu.")

H2("4.1&nbsp;&nbsp;TIM8 — generator takta uzorkovanja")
P("Tajmer TIM8 postavljen je da se preljeva (overflow) točno 64 000 puta u "
  "sekundi. Pri svakom preljevu šalje interni okidač (TRGO) prema ADC-u. "
  "Time je <b>brzina uzorkovanja potpuno hardverska i jednolika</b> — ne ovisi "
  "o tome koliko je procesor zauzet.")
Code(
"""
TIM_InitStruct.Prescaler  = 0;
TIM_InitStruct.Autoreload = 2655;     /* ARR */
LL_TIM_SetTriggerOutput(TIM8, LL_TIM_TRGO_UPDATE);   /* TRGO na svaki update */

/* Frekvencija = f_tim / (ARR + 1) = 170 MHz / (2655 + 1)
              = 170 000 000 / 2656 = 64 006 Hz  ≈ 64 kHz
   Perioda = 1 / 64000 = 15.625 µs po uzorku */
""",
caption="Isječak 1. Konfiguracija TIM8 (timer_driver.c / main.c). ARR = 2655 daje ≈ 64 kHz."
)

H2("4.2&nbsp;&nbsp;ADC — jedan pretvarač, četiri kanala redom")
P("Sustav koristi <b>jedan</b> ADC (ADC1) koji u «scan» načinu pretvara četiri "
  "kanala jedan za drugim pri svakom okidaču tajmera. Redoslijed (rank) određuje "
  "i raspored podataka u memoriji.")
P("Bitan detalj koji početnici lako previde: budući da kanali nisu uzorkovani "
  "<b>istovremeno</b> nego redom, svaki sljedeći kanal kasni za prethodnim za "
  "vrijeme jedne konverzije. To unosi sustavnu pogrešku u TDOA koju kod kasnije "
  "ispravlja (vidi 7.6).")
Code(
"""
/* ADC takt = PCLK / 4 = 170 MHz / 4 = 42.5 MHz  →  perioda 23.53 ns
   Po kanalu: 2.5 (uzorkovanje) + 12.5 (konverzija) = 15 ciklusa
            = 15 × 23.53 ns = 352.9 ns                                   */
#define CH_DELAY_S   352.9e-9f

/* Posljedica sekvencijalnog čitanja:
     M1 uzorkovan u  t + 0×352.9 ns
     M2 uzorkovan u  t + 1×352.9 ns
     M3 uzorkovan u  t + 2×352.9 ns
     M4 uzorkovan u  t + 3×352.9 ns                                      */
""",
caption="Isječak 2. Kanalni offset zbog sekvencijalne konverzije (audio_common.h)."
)

H2("4.3&nbsp;&nbsp;DMA — hardver puni memoriju umjesto procesora")
P("DMA (Direct Memory Access) je sklop koji premješta svaki rezultat ADC-a "
  "izravno u RAM <b>bez prekidanja procesora za svaki uzorak</b>. Konfiguriran "
  "je kao <b>kružni</b> (circular): kad napuni kraj buffera, automatski se vrati "
  "na početak. Buffer je dvostruke veličine (8192 uzoraka) i koristi se kao "
  "<b>dvostruki spremnik</b> (double buffer):")
Bullets([
  "Dok DMA puni <b>drugu</b> polovicu buffera, procesor smije čitati "
  "<b>prvu</b> polovicu — i obrnuto. Nikad ne pišu i ne čitaju isti dio.",
  "DMA javlja dva prekida: <b>HT</b> (Half Transfer, popunjena prva polovica) "
  "i <b>TC</b> (Transfer Complete, popunjena druga polovica).",
])
Code(
"""
#define SAMPLES_PER_CHANNEL  1024
#define NUM_CH               4
#define HALF_BUFFER  (NUM_CH * SAMPLES_PER_CHANNEL)   /* 4096 uzoraka  */
#define FULL_BUFFER  (HALF_BUFFER * 2)                /* 8192 uzoraka  */

uint16_t adc_buffer[FULL_BUFFER];     /* 8192 × 2 B = 16 KB u RAM-u    */

/* Raspored u memoriji je INTERLEAVED (kanali isprepleteni):
     adc_buffer[s*4 + 0] = M1     adc_buffer[s*4 + 2] = M3
     adc_buffer[s*4 + 1] = M2     adc_buffer[s*4 + 3] = M4              */
""",
caption="Isječak 3. Buffer i njegov interleaveani raspored (adc_driver.c, audio_common.h)."
)
P("Jedna «polovica» = 4096 uzoraka = 1024 vremenska trenutka × 4 kanala. Pri "
  "64 kHz, 1024 uzorka po kanalu traje 1024 ÷ 64000 = <b>16 ms</b>. Dakle svakih "
  "16 ms stigne jedan HT ili TC prekid — to je «otkucaj srca» cijelog sustava.")

H2("4.4&nbsp;&nbsp;ISR — kratak prekid koji samo javlja «gotovo»")
P("Prekidna rutina mora biti <b>što kraća</b>. Ova ne radi nikakvu obradu — "
  "samo pošalje broj (0 za HT, 1 za TC) u red čekanja i, ako je time probudila "
  "task višeg prioriteta, zatraži preraspodjelu procesora. Sva «teška» obrada "
  "događa se izvan prekida, u tasku.")
Code(
"""
void DMA1_Channel1_IRQHandler(void) {
    BaseType_t woken = pdFALSE;
    uint32_t   msg;
    if (LL_DMA_IsActiveFlag_HT1(DMA1)) {            /* prva polovica gotova */
        LL_DMA_ClearFlag_HT1(DMA1);
        msg = 0u;
        xQueueSendFromISR(queueDmaEventHandle, &msg, &woken);
    }
    if (LL_DMA_IsActiveFlag_TC1(DMA1)) {            /* druga polovica gotova */
        LL_DMA_ClearFlag_TC1(DMA1);
        msg = 1u;
        xQueueSendFromISR(queueDmaEventHandle, &msg, &woken);
    }
    portYIELD_FROM_ISR(woken);   /* probudi ACQ_Task ako treba */
}
""",
caption="Isječak 4. DMA prekidna rutina (stm32g4xx_it.c). Pravilo: u ISR-u radi minimum, ostalo prepusti tasku."
)

# ============================================================================
#  5. TASKOVI
# ============================================================================
H1("5.&nbsp;&nbsp;FreeRTOS taskovi i protok podataka")

P("FreeRTOS je <b>operativni sustav za stvarno vrijeme</b> (RTOS). Omogućuje da "
  "se posao podijeli na neovisne «niti» (taskove) koje scheduler izmjenjuje na "
  "procesoru prema <b>prioritetu</b>. Svaki task ima vlastiti stog (stack) i "
  "vrti se u beskonačnoj petlji, najčešće čekajući na nekom redu čekanja.")

P("Sustav koristi tri taska, povezana s tri reda čekanja, po načelu "
  "<b>proizvođač–potrošač</b>: svaki task uzima posao iz svog ulaznog reda, "
  "obradi ga i rezultat preda u sljedeći red.")

DataTable(
    [["Task", "Prioritet", "Stog", "Uloga"],
     ["ACQ_Task", "Realtime (najviši)", "256 riječi", "Kopira svježu polovicu buffera u snapshot"],
     ["FFT_Task", "High", "1024 riječi", "GCC-PHAT + 3D geometrija (sva matematika)"],
     ["UART_Task", "Low (najniži)", "256 riječi", "Šalje rezultat (i sirove uzorke) na ESP32"]],
    col_widths=[2.7*cm, 3.2*cm, 2.5*cm, 7.8*cm],
    caption="Tablica 4. Tri taska, njihovi prioriteti i stogovi (task_manager.c, app_tasks_init)."
)

P("Logika prioriteta: <b>akvizicija je najvažnija</b> jer ako propustimo svjež "
  "podatak iz DMA-a, izgubili smo ga zauvijek. Obrada (FFT) je važna ali smije "
  "malo pričekati. Slanje na UART je najmanje hitno (ESP32 prikaz toleriran je "
  "kasni desetke milisekundi), pa ima najniži prioritet.")

H2("5.1&nbsp;&nbsp;Tri reda čekanja")
Code(
"""
queueDmaEventHandle  : DMA ISR  → ACQ_Task   (dubina 8, uint32 događaj HT/TC)
queueSnapshotHandle  : ACQ_Task → FFT_Task   (dubina 2, indeks buffera uint8)
queueResultHandle    : FFT_Task → UART_Task  (dubina 4, loc3d_result_t)
""",
caption="Isječak 5. Tri reda čekanja povezuju ISR i tri taska (task_manager.h / .c)."
)

H2("5.2&nbsp;&nbsp;ACQ_Task — siguran prijenos svježe polovice")
P("ACQ_Task čeka na događaj iz DMA prekida. Kad dobije 0 (HT) zna da je gotova "
  "prva polovica buffera, kad dobije 1 (TC) druga. Tu polovicu kopira u jedan od "
  "<b>tri</b> «snapshot» buffera i indeks pošalje dalje. Zašto tri buffera i "
  "kopiranje? Da DMA (koji nikad ne staje) ne prepiše podatke dok ih FFT_Task "
  "još obrađuje.")
Code(
"""
static void StartACQTask(void const *argument) {
    uint8_t write_idx = 0;
    for (;;) {
        osEvent evt = osMessageGet(queueDmaEventHandle, osWaitForever);
        if (evt.status != osEventMessage) continue;

        /* Ako FFT_Task zaostaje i red je pun — preskoči, ne prepisuj buffer
           koji se možda još čita (sprječava «torn read»).               */
        if (uxQueueSpacesAvailable(queueSnapshotHandle) == 0u) continue;

        const uint16_t *src = (evt.value.v == 0u) ? &adc_buffer[0]
                                                  : &adc_buffer[HALF_BUFFER];
        memcpy(acq_snapshot[write_idx], src, HALF_BUFFER * sizeof(uint16_t));

        xQueueSend(queueSnapshotHandle, &write_idx, 0u);
        write_idx = (write_idx + 1u) % ACQ_NUM_BUFFERS;   /* 0→1→2→0 */
    }
}
""",
caption="Isječak 6. ACQ_Task (task_manager.c). Trostruki spremnik + provjera praznine reda = nema oštećenih podataka."
)
Callout("Pojam: torn read (poderano čitanje)",
  "Ako jedan task čita niz dok ga drugi (ili DMA) istovremeno mijenja, dobije "
  "mješavinu starih i novih vrijednosti — besmislen podatak. Rješenje ovdje: "
  "ACQ kopira podatak u zaseban buffer i nikad ne prepisuje onaj koji je još «u "
  "opticaju» (u redu čekanja ili u obradi).")

H2("5.3&nbsp;&nbsp;FFT_Task — klizni prozor i sva matematika")
P("FFT_Task uzima indeks svježe polovice i nadovezuje je na prethodnu, tvoreći "
  "<b>klizni prozor</b> (sliding window) od 2×1024 = 2048 vremenskih trenutaka. "
  "Razlog: pljesak može pasti na granicu dviju polovica; s dvostrukim prozorom "
  "uvijek imamo cijeli događaj na jednom mjestu, pa ga algoritam može «centrirati».")
Code(
"""
static void StartFFTTask(void const *argument) {
    loc3d_result_t result;
    uint8_t read_idx;
    for (;;) {
        if (xQueueReceive(queueSnapshotHandle, &read_idx, portMAX_DELAY) != pdTRUE)
            continue;

        /* pomakni stari «desni» pola u «lijevi», pa dodaj novu polovicu desno */
        memcpy(&sliding_buf[0],          &sliding_buf[HALF_BUFFER],
               HALF_BUFFER*sizeof(uint16_t));
        memcpy(&sliding_buf[HALF_BUFFER], acq_snapshot[read_idx],
               HALF_BUFFER*sizeof(uint16_t));

        if (!capture_ready) {
            if (LOC3D_Process(sliding_buf, &result)) {   /* ← sva obrada ovdje */
                capture_ready = 1;
                xQueueSend(queueResultHandle, &result, 0);
            }
        }
    }
}
""",
caption="Isječak 7. FFT_Task (task_manager.c). LOC3D_Process je srce DSP-a — razrađeno u poglavlju 7."
)

H2("5.4&nbsp;&nbsp;UART_Task i mehanizam «capture_ready»")
P("UART_Task čeka rezultat i šalje ga na ESP32. Dodatno, nakon kuta opcionalno "
  "šalje i <b>sirove uzorke</b> (8 KB) za analizu u MATLAB-u. Slanje 8 KB na "
  "115200 bps traje ~0.7 s — pravu vječnost za sustav koji okida svakih 16 ms. "
  "Zato zastavica <font name='Consolas'>capture_ready</font> «zamrzne» obradu "
  "dok traje slanje, da se sirovi buffer ne prepiše usred prijenosa.")
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
caption="Isječak 8. UART_Task (task_manager.c). capture_ready je jednostavan «handshake» između FFT i UART taska."
)

# ============================================================================
#  6. STACK SIZING
# ============================================================================
H1("6.&nbsp;&nbsp;Procjena veličine stoga (stack sizing)")

P("Svaki FreeRTOS task ima <b>vlastiti stog</b> — područje RAM-a na koje se "
  "spremaju lokalne varijable, argumenti funkcija i adrese povratka pri svakom "
  "ugnježđenom pozivu. Ako task potroši više od dodijeljenog stoga, dolazi do "
  "<b>prelijevanja stoga</b> (stack overflow) — jedne od najčešćih i "
  "najpodmuklijih grešaka u ugradbenim sustavima, jer se očituje kao naizgled "
  "slučajno rušenje.")

Callout("Jedinica: «riječ» (word), ne bajt",
  "U FreeRTOS-u se veličina stoga zadaje u <b>riječima</b>. Na Cortex-M4 jedna "
  "riječ = 4 bajta. Dakle 256 riječi = 1024 bajta = 1 KB, a 1024 riječi = 4 KB.")

H2("6.1&nbsp;&nbsp;Što sve troši stog")
Bullets([
  "<b>Lokalne varijable</b> svake funkcije u lancu poziva (najveći potrošač su "
  "velika lokalna polja).",
  "<b>Spremanje registara</b> pri pozivu funkcije i, na Cortex-M4F, "
  "<b>FPU registara</b> kod operacija s pomičnim zarezom.",
  "<b>Ugnježđivanje poziva</b> — svaki dublji poziv dodaje novi «okvir».",
  "<b>Kontekst prekida</b> — prekid koji se dogodi dok task radi nakratko "
  "koristi stog (ovisno o portu).",
])

H2("6.2&nbsp;&nbsp;Kako se procjenjuje u praksi")
P("Procjena ide kombinacijom <b>analize</b> i <b>mjerenja</b>. Ključ je shvatiti "
  "da <b>velika polja podataka NE žive na stogu</b> u ovom projektu — namjerno su "
  "deklarirana kao <font name='Consolas'>static</font> (u BSS segmentu), pa ne "
  "opterećuju stog taska. To je svjesna projektna odluka koja drži stogove malima.")
Code(
"""
/* Ova polja su 'static' → u BSS-u, NE na stogu FFT_Task-a: */
static float s_ch0[1024], s_ch1[1024], s_ch2[1024], s_ch3[1024];  /* 16 KB */
static float s_corr[1024];                                        /*  4 KB */

/* Unutar GCC_PHAT — također 'static', dijele se kroz pozive: */
static float fft_x[1024], fft_y[1024], fft_c[1024];               /* 12 KB */
""",
caption="Isječak 9. Veliki radni nizovi su 'static' (sound_loc_3d.c, gcc_phat.c) — zato FFT_Task treba samo ~4 KB stoga."
)

P("Da su ti nizovi bili obične lokalne varijable, samo jedan poziv "
  "<font name='Consolas'>LOC3D_Process</font> tražio bi preko 30 KB stoga — "
  "višestruko više od dodijeljenog. Ovako na stogu ostaju samo skalari "
  "(pokazivači, brojači petlji, međurezultati tipa <font name='Consolas'>float</font>).")

H3("Postupak u tri koraka")
NumList([
  "<b>Analitička gornja procjena.</b> Prođi kroz najdublji lanac poziva taska i "
  "zbroji lokalne varijable + rezervu za registre/FPU/prekid. Za FFT_Task "
  "najdublji put je LOC3D_Process → GCC_PHAT → arm_rfft_fast_f32; budući da su "
  "veliki nizovi static, ostaje par stotina bajta skalara + CMSIS-DSP interni "
  "okviri. S velikodušnom rezervom → 1024 riječi (4 KB).",
  "<b>Mjerenje u radu (high-water mark).</b> FreeRTOS nudi "
  "<font name='Consolas'>uxTaskGetStackHighWaterMark()</font> koji vraća "
  "<i>najmanju</i> količinu slobodnog stoga ikad zabilježenu za taj task. "
  "Pustiš sustav da radi (puno pljeskanja, rubni slučajevi), pa očitaš koliko je "
  "stoga stvarno ostalo neiskorišteno.",
  "<b>Sigurnosna rezerva.</b> Ostavi barem 25–50% slobodnog stoga iznad "
  "izmjerenog vrha, za nepredviđene dublje putove (npr. drugačiji ulaz koji "
  "aktivira drugu granu koda).",
])

H2("6.3&nbsp;&nbsp;Mreža sigurnosti: dvije zaštite")
P("Procjena nikad nije savršena, pa kod ima dvije aktivne zaštite:")
Code(
"""
/* FreeRTOSConfig.h */
#define configENABLE_FPU       1   /* čuva FPU registre pri context switchu —
                                      nužno jer FFT_Task koristi float/asinf   */
#define configCHECK_FOR_STACK_OVERFLOW ...  /* (preko hook-a niže)             */

/* main.c — poziva se automatski ako task prijeđe svoj stog: */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    __disable_irq();
    while (1) { }        /* zaustavi sve — lako uočljivo u debuggeru */
}
""",
caption="Isječak 10. Zaštite vezane uz stog (FreeRTOSConfig.h, main.c)."
)
Callout("Zašto je configENABLE_FPU = 1 obavezan ovdje",
  "FFT_Task intenzivno koristi pomični zarez (asinf, atan2f, množenja matrice). "
  "Ako scheduler prekine task usred float-operacije, a druga nit dotakne FPU, bez "
  "čuvanja FPU konteksta vrijednosti se nepovratno pokvare. Uz FPU=1 FreeRTOS "
  "koristi «lijeno» (lazy) spremanje FPU registara — čuva ih samo kad task "
  "stvarno koristi FPU, štedeći stog i vrijeme.")

H2("6.4&nbsp;&nbsp;Heap (gomila) cijelog RTOS-a")
P("Osim stogova pojedinih taskova, FreeRTOS ima i zajednički <b>heap</b> iz "
  "kojeg se alociraju stogovi taskova, redovi čekanja i drugi objekti. "
  "Postavljen je na 24 KB, što je s rezervom dovoljno:")
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
    caption="Tablica 5. Bilanca FreeRTOS heap-a (FreeRTOSConfig.h). STM32G474RE ima 128 KB RAM-a, pa je 24 KB tek ~19%."
)
P("Napomena: veliki nizovi (adc_buffer 16 KB, snapshoti 24 KB, klizni prozor "
  "16 KB, DSP nizovi ~32 KB) <b>ne dolaze iz heap-a</b> — oni su statički "
  "globalni (BSS) i linker ih smješta zasebno. Heap služi isključivo RTOS "
  "objektima.")

# ============================================================================
#  7. DETEKCIJA — KORAK PO KORAK
# ============================================================================
H1("7.&nbsp;&nbsp;Detekcija i lokalizacija zvuka — korak po korak")

P("Ovo je srž projekta. Cijeli postupak izvodi funkcija "
  "<font name='Consolas'>LOC3D_Process()</font>, koju FFT_Task zove jednom za "
  "svaku novu polovicu buffera (svakih 16 ms). Niže je svaki korak objašnjen i "
  "popraćen <b>brojčanim primjerom</b> za jedan zamišljeni pljesak.")

Callout("Postavka primjera koji pratimo kroz cijelo poglavlje",
  "Pretpostavimo da pljesak dolazi iz smjera azimut +30°, elevacija +20°, "
  "dovoljno glasan. Kroz korake ćemo vidjeti kako iz sirovih uzoraka ispadnu "
  "upravo ti kutovi.")

H2("7.1&nbsp;&nbsp;Korak 0 — Cooldown (sprječavanje višestrukih okidanja)")
P("Jedan pljesak traje i odzvanja nekoliko desetaka milisekundi i prelijeva se "
  "preko više prozora. Bez kočnice sustav bi za jedan pljesak ispalio 5–10 "
  "detekcija. Zato nakon svake uspješne detekcije slijedi «hlađenje» od 19 "
  "prozora (≈ 304 ms) tijekom kojih se obrada preskače.")
Code(
"""
#define DETECTION_COOLDOWN_FRAMES  19      /* 19 × 16 ms ≈ 304 ms */
static uint16_t s_cooldown = 0;

if (s_cooldown > 0) { s_cooldown--; return 0; }   /* preskoči, jeftino */
""",
caption="Isječak 11. Cooldown (sound_loc_3d.c). Primjer: detekcija u t=0 → sljedeća moguća tek nakon ~304 ms."
)

H2("7.2&nbsp;&nbsp;Korak 1 — Pronalazak energetskog vrha (gdje je pljesak)")
P("Pljesak je kratak, a prozor traje 32 ms. Algoritam prvo pronađe gdje "
  "<b>unutar</b> kliznog prozora leži najglasniji dio i ondje centrira analizu. "
  "Koristi se <b>klizeći prozor energije</b> širine 16 uzoraka koji se "
  "inkrementalno pomiče — umjesto da za svaki položaj iznova zbraja, oduzme "
  "uzorak koji «izlazi» i doda onaj koji «ulazi». To je O(N) umjesto O(N×W).")
Code(
"""
/* energija = suma kvadrata (uzorak − 2048), 2048 = sredina 12-bitnog raspona */
int32_t v = (int32_t)s[ch] - 2048;
win_e += (v * v) >> 6;                 /* >>6 sprječava preljev akumulatora */

/* klizni pomak: skini stari rub, dodaj novi */
win_e -= (v_old*v_old) >> 6;
win_e += (v_new*v_new) >> 6;
if (win_e > best_e) { best_e = win_e; best_frame = f + PROBE/2; }
""",
caption="Isječak 12. find_peak_offset (sound_loc_3d.c). Rezultat: frame_offset = početak 1024-uzorčanog prozora centriranog na pljesak."
)
Callout("Primjer 7.2",
  "Recimo da je pljesak najglasniji oko 1200-tog trenutka u kliznom prozoru "
  "(0–2047). Algoritam vrati frame_offset ≈ 1200 − 512 = 688, tj. analizirat će "
  "uzorke [688 … 1711] gdje je događaj lijepo u sredini.")

H2("7.3&nbsp;&nbsp;Korak 2 — Deinterleave, uklanjanje DC-a i Hann prozor")
P("Iz interleaveanog buffera treba izdvojiti <b>4 zasebna kanala</b>. Pritom se "
  "obavljaju dvije pripreme nužne za korelaciju:")
Bullets([
  "<b>Uklanjanje istosmjerne komponente (DC).</b> Mikrofon «sjedi» na nekom "
  "naponu (npr. ~1.65 V → ADC ~2048). Za korelaciju nas zanima samo "
  "<i>promjena</i> oko te sredine, pa od svakog uzorka oduzmemo prosjek kanala.",
  "<b>Hann prozor.</b> Množenje signala glatkim «zvonom» (na rubovima → 0) "
  "smanjuje artefakte FFT-a (spektralno curenje) koji nastaju jer FFT "
  "pretpostavlja da je signal periodičan.",
])
Code(
"""
float w = s_hann[s];                      /* unaprijed izračunato zvono */
ch0[s] = ((float)r0 - dc0) * w;           /* (uzorak − DC) × prozor     */
...
/* Hann: w[i] = 0.5 · (1 − cos(2π·i / (N−1))),   N = 1024                */
""",
caption="Isječak 13. GCC_ExtractChannels (gcc_phat.c). Pretvara uint16 → float, makne DC, primijeni Hann."
)
Callout("Primjer 7.3",
  "Uzorak M1 = 2100, prosjek kanala dc0 = 2048, a Hann na tom mjestu w = 0.95. "
  "Rezultat: (2100 − 2048) × 0.95 = 52 × 0.95 = 49.4. Tako «očišćen» signal "
  "ide u FFT.")

H2("7.4&nbsp;&nbsp;Korak 3 — RMS prag (je li uopće bilo dovoljno glasno)")
P("RMS (Root Mean Square) je mjera «glasnoće» — efektivna amplituda signala. "
  "Ako je najglasniji kanal tiši od praga, riječ je o tišini/šumu i obrada "
  "se prekida. Drugi (blaži) prag traži da ni najtiši kanal ne bude posve mrtav, "
  "čime se odbacuju situacije kad samo jedan mikrofon nešto uhvati.")
Code(
"""
#define MIN_RMS_THRESHOLD    10.0f
#define MIN_RMS_PER_CHANNEL  (MIN_RMS_THRESHOLD * 0.2f)   /* = 2.0 */

if (rms_max < MIN_RMS_THRESHOLD)    return 0;   /* pretiho — izađi   */
if (rms_min < MIN_RMS_PER_CHANNEL)  return 0;   /* jedan mik mrtav   */
""",
caption="Isječak 14. RMS prag (sound_loc_3d.c). RMS = sqrt(prosjek kvadrata uzoraka)."
)
Callout("Primjer 7.4",
  "Pljesak: rms = [42, 38, 35, 30]. rms_max = 42 > 10 ✓ i rms_min = 30 > 2 ✓ → "
  "prolazi. Tiha soba: rms = [3, 2, 3, 2] → rms_max = 3 < 10 → odmah izlaz.")

H2("7.5&nbsp;&nbsp;Korak 4 — GCC-PHAT: mjerenje kašnjenja između parova")
P("Ovo je matematička jezgra. Za svaki par (M1–M2, M1–M3, M1–M4) treba "
  "izmjeriti <b>za koliko uzoraka jedan signal kasni za drugim</b>. Naivni "
  "pristup je <b>križna korelacija</b>: kliži jedan signal preko drugog i traži "
  "pomak pri kojem se najbolje poklapaju. GCC-PHAT je njena «izoštrena» inačica.")

H3("Zašto PHAT, a ne obična korelacija")
P("Obična korelacija daje širok, zaobljen vrh — teško je točno odrediti gdje je "
  "maksimum, osobito uz jeku i šum. <b>PHAT</b> (Phase Transform) u "
  "frekvencijskoj domeni <b>odbaci informaciju o amplitudi i zadrži samo fazu</b> "
  "svake frekvencije. Rezultat je vrlo oštar, gotovo «šiljast» vrh točno na "
  "pravom kašnjenju — jer kašnjenje je u biti linearni nagib faze po frekvenciji.")

P("Postupak po paru izvodi se preko brzog FFT-a (CMSIS-DSP "
  "<font name='Consolas'>arm_rfft_fast_f32</font>):")
NumList([
  "Izračunaj FFT oba signala: X = FFT(ref), Y = FFT(sig).",
  "Za svaku frekvenciju izračunaj <b>križni spektar</b> C = konj(X) · Y "
  "(konj = kompleksno konjugiranje).",
  "<b>PHAT normalizacija</b>: podijeli svaki C s njegovim iznosom |C|, tako da "
  "ostane samo faza (jedinični iznos).",
  "Inverzni FFT od normaliziranog C → korelacijska sekvenca u vremenu. Položaj "
  "njenog vrha je traženo kašnjenje.",
])
Code(
"""
arm_rfft_fast_f32(&s_rfft, ref, fft_x, 0);   /* X = FFT(ref)            */
arm_rfft_fast_f32(&s_rfft, sig, fft_y, 0);   /* Y = FFT(sig)            */

/* za svaki frekvencijski bin k: */
float cre = re1*re2 + im1*im2;     /* Re{ konj(X)·Y } */
float cim = re1*im2 - im1*re2;     /* Im{ konj(X)·Y } */
float mag = sqrtf(cre*cre + cim*cim);
float w   = (mag > 1e-9f) ? 1.0f/(mag + 1e-9f) : 0.0f;   /* PHAT: /|C|  */
fft_c[ire] = cre*w;  fft_c[iim] = cim*w;

arm_rfft_fast_f32(&s_rfft, fft_c, corr, 1);  /* IFFT → korelacija       */
""",
caption="Isječak 15. GCC_PHAT (gcc_phat.c). Konj(X)·Y daje vrh na pozitivnom kašnjenju kad sig kasni za ref."
)
Callout("Primjer 7.5 (predznak govori smjer)",
  "Za par M1–M2: ako vrh korelacije ispadne na lagu +5 uzoraka, to znači da M2 "
  "kasni 5 uzoraka za M1 → val je do M1 stigao prije → izvor je bliži M1-strani. "
  "Predznak i iznos svih triju kašnjenja zajedno jednoznačno kodiraju smjer.")

H2("7.6&nbsp;&nbsp;Korak 5 — Točan vrh: pretraga + parabolička interpolacija")
P("Korelacija je uzorkovana u cijelim uzorcima, ali pravo kašnjenje rijetko "
  "pada točno na uzorak. Zato se nakon pronalaska najvišeg uzorka radi "
  "<b>parabolička interpolacija</b>: kroz vrh i dva susjeda provuče se parabola "
  "i njen tjeme daje <b>pod-uzorčano</b> (sub-sample) kašnjenje. Pretraga je "
  "ograničena na ±20 uzoraka (fizička granica iz poglavlja 3).")
Code(
"""
/* najveći APSOLUTNI vrh u rasponu ±TDOA_MAX_SAMPLES (hvata i negativan
   polaritet ako je neki mikrofon obrnuto spojen)                        */
delta = 0.5f * (c0 - c2) / (c0 - 2*c1 + c2);   /* tjeme parabole         */
float frac = (float)max_idx + delta;            /* npr. 5 + 0.3 = 5.3     */
return delay_samp * SAMPLE_PERIOD_S;            /* uzorci → sekunde       */

/* Usput: 'qual' = vrh / prosjek|korelacije| — visok za pravi signal,
   ~1 za šum. Trenutno je prag kvalitete 0 (gate isključen radi ugađanja).*/
""",
caption="Isječak 16. GCC_FindTDOA (gcc_phat.c). Pretvara položaj vrha u kašnjenje u sekundama."
)

H3("Korekcija sekvencijalnog ADC offseta")
P("Sjetimo se da kanali nisu uzorkovani istovremeno (poglavlje 4.2). GCC-PHAT "
  "izmjeri <b>prividno</b> kašnjenje koje uključuje i taj hardverski pomak. "
  "Stvarno <b>akustičko</b> kašnjenje dobije se dodavanjem (j−1)×CH_DELAY:")
Code(
"""
float tau12 = tau12_meas + 1.0f * CH_DELAY_S;   /* M2 = 2. rank */
float tau13 = tau13_meas + 2.0f * CH_DELAY_S;   /* M3 = 3. rank */
float tau14 = tau14_meas + 3.0f * CH_DELAY_S;   /* M4 = 4. rank */
""",
caption="Isječak 17. Korekcija kanalnog offseta (sound_loc_3d.c)."
)
Callout("Primjer 7.6 (naši brojevi)",
  "Za smjer +30°/+20° prava akustička kašnjenja izračunata iz geometrije su: "
  "τ12 ≈ −274 µs (−17.5 uzoraka), τ13 ≈ −137 µs (−8.8), τ14 ≈ −198 µs (−12.7). "
  "GCC-PHAT izmjeri približno te vrijednosti umanjene za kanalni pomak; "
  "korak korekcije ih vrati na navedene «prave» iznose.")

H2("7.7&nbsp;&nbsp;Korak 6 — Geometrijsko rješenje: od kašnjenja do smjera")
P("Tri kašnjenja (τ12, τ13, τ14) i poznata geometrija jednoznačno određuju "
  "smjer. Veza je linearna: ako složimo baseline vektore u matricu "
  "<b>D</b> (reci = Mj − M1), vrijedi <b>D · s = −c · τ</b>, gdje je <b>s</b> "
  "jedinični vektor prema izvoru, a <b>c</b> brzina zvuka.")
P("Rješavanje po s svodi se na množenje unaprijed izračunatom matricom "
  "<font name='Consolas'>M_geom = c · inv(D)</font>. Inverzija 3×3 računa se "
  "<b>jednom</b> pri pokretanju (LOC3D_Init), pa je po detekciji potrebno samo "
  "9 množenja i 6 zbrajanja — vrlo jeftino.")
Code(
"""
/* jednom na startu: */
M_geom = c · inv(D),   D = [ M2−M1 ; M3−M1 ; M4−M1 ]   (3×3)

/* po detekciji: u = smjer propagacije = M_geom · τ */
ux = M_geom[0][0]*τ12 + M_geom[0][1]*τ13 + M_geom[0][2]*τ14;
uy = M_geom[1][0]*τ12 + M_geom[1][1]*τ13 + M_geom[1][2]*τ14;
uz = M_geom[2][0]*τ12 + M_geom[2][1]*τ13 + M_geom[2][2]*τ14;

/* smjer PREMA izvoru je suprotan smjeru propagacije: s = −u, pa normaliziraj */
sx = −ux; sy = −uy; sz = −uz;   norm = sqrt(sx²+sy²+sz²);  s /= norm;
""",
caption="Isječak 18. Geometrijsko rješenje (LOC3D_Init + LOC3D_Process, sound_loc_3d.c)."
)
Callout("Zašto inverzija samo jednom",
  "Matrica D ovisi isključivo o položajima mikrofona, koji se ne mijenjaju u "
  "radu. Skupu operaciju (inverziju) napravimo jednom i spremimo rezultat; u "
  "vrućoj petlji ostaje samo jeftino množenje matrice s vektorom. Tipičan DSP "
  "obrazac: «pretproračunaj sve što je konstantno».")

H2("7.8&nbsp;&nbsp;Korak 7 — Kutovi azimut i elevacija")
P("Iz jediničnog vektora smjera s = (sx, sy, sz) kutovi se dobiju izravno "
  "trigonometrijom. Rezultat se sprema u desetinkama stupnja (cijeli broj) radi "
  "kompaktnog prijenosa.")
Code(
"""
float az_rad = atan2f(sy, sx);     /* azimut: [−180°, +180°]  */
float el_rad = asinf(sz);          /* elevacija: [−90°, +90°]  */

out->az_tenth = (int16_t)(az_rad * 1800.0f / PI);   /* radijani → 0.1°   */
out->el_tenth = (int16_t)(el_rad * 1800.0f / PI);
""",
caption="Isječak 19. Pretvorba vektora u kutove (sound_loc_3d.c)."
)
Callout("Primjer 7.8 (zatvaranje kruga)",
  "Za naš pljesak geometrija vrati s ≈ (0.814, 0.470, 0.342). Tada je "
  "atan2(0.470, 0.814) = 30.0° → az_tenth = 300, a asin(0.342) = 20.0° → "
  "el_tenth = 200. Točno smjer iz kojeg smo «pustili» pljesak na početku "
  "poglavlja.")

H2("7.9&nbsp;&nbsp;Korak 8 — Jakost signala")
P("Za vizualizaciju je korisna i «jakost» 0–100. Računa se logaritamski "
  "(slično decibelima) jer ljudski doživljaj glasnoće i raspon pljeskova "
  "(RMS ≈ 30–500) najbolje opisuje log-ljestvica.")
Code(
"""
float str_f = 20.0f * log10f(rms_max + 1.0f);    /* dB-slično */
if (str_f < 1.0f)   str_f = 1.0f;
if (str_f > 100.0f) str_f = 100.0f;
out->strength = (uint8_t)str_f;
/* RMS 20 → ~26,  RMS 100 → ~40,  RMS 500 → ~54 */
""",
caption="Isječak 20. Izračun jakosti (sound_loc_3d.c)."
)
P("Na kraju, ako je sve prošlo, postavi se cooldown i funkcija vrati 1 "
  "(valjana detekcija). FFT_Task tada rezultat šalje u red prema UART tasku.")

# ============================================================================
#  8. UART PROTOKOL
# ============================================================================
H1("8.&nbsp;&nbsp;UART protokol između STM32 i ESP32")

P("Dvije ploče komuniciraju serijski (UART, 115200 bps, 8N1) jednostavnim "
  "<b>binarnim paketnim protokolom</b>. Svaki paket počinje s dva «start» bajta "
  "(0xAA 0xBB) i završava s dva «end» bajta (0xCC 0xDD), a između ima bajt tipa "
  "i korisni teret. Start/end markeri omogućuju prijemniku da se «uhvati» na "
  "početak paketa čak i ako propusti koji bajt.")

DataTable(
    [["Tip", "Naziv", "Duljina", "Sadržaj"],
     ["0x02", "2D kut", "8 B", "AZ(2) + jakost(1)"],
     ["0x03", "3D kut", "10 B", "AZ(2) + EL(2) + jakost(1)"],
     ["0x04", "Sirovi capture", "~8 KB", "NCH + N + NCH×N×uint16 (big-endian)"]],
    col_widths=[1.8*cm, 3.0*cm, 2.4*cm, 9.0*cm],
    caption="Tablica 6. Tipovi paketa (uart_driver.c na obje strane). Sustav primarno koristi 0x03."
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
caption="Isječak 21. Raspored 3D paketa. «BE» = big-endian (viši bajt prvi)."
)
P("Brojevi se šalju kao <b>desetinke stupnja</b> u cjelobrojnom tipu int16 "
  "(npr. 30.5° → 305) jer je cijele brojeve jednostavnije i pouzdanije prenijeti "
  "od decimalnih, a desetinka stupnja je više nego dovoljna preciznost.")

# ============================================================================
#  9. ESP32
# ============================================================================
H1("9.&nbsp;&nbsp;ESP32: Wi-Fi, web-poslužitelj i 3D vizualizacija")

P("ESP32 je «prezentacijski» dio sustava. Pri pokretanju "
  "(<font name='Consolas'>app_main</font>) redom inicijalizira: NVS (pohrana za "
  "Wi-Fi), Wi-Fi u Access Point modu, HTTP poslužitelj, UART i procesor "
  "primljenih paketa.")
Code(
"""
void app_main(void) {
    nvs_flash_init();          /* trajna pohrana (Wi-Fi treba NVS)        */
    wifi_manager_init();       /* podigni vlastitu Wi-Fi mrežu (AP)       */
    web_server_init();         /* HTTP + WebSocket na portu 80            */
    uart_driver_init();        /* UART1: RX=GPIO16, TX=GPIO17, 115200     */
    sound_loc_processor_init();/* pokreni rx_task (parser paketa)         */
}
""",
caption="Isječak 22. Redoslijed inicijalizacije na ESP32 (main.c)."
)

H2("9.1&nbsp;&nbsp;Wi-Fi Access Point")
P("ESP32 ne spaja se na postojeću mrežu, nego <b>stvara vlastitu</b>. To znači "
  "da sustav radi samostalno, bez routera. Korisnik se telefonom ili laptopom "
  "spoji na tu mrežu i otvori adresu poslužitelja.")
DataTable(
    [["Parametar", "Vrijednost"],
     ["SSID (ime mreže)", "SoundLocalization"],
     ["Lozinka", "soundloc123"],
     ["IP poslužitelja", "192.168.4.1"],
     ["Maks. klijenata", "4"]],
    col_widths=[5.5*cm, 10.7*cm],
    caption="Tablica 7. Wi-Fi AP postavke (wifi_manager.c). U pregledniku otvoriti http://192.168.4.1"
)

H2("9.2&nbsp;&nbsp;Parser paketa — konačni automat (state machine)")
P("UART podaci stižu kao <b>tok bajtova</b>, bez jasnih granica. Da iz toka "
  "izvuče pakete, <font name='Consolas'>rx_task</font> koristi <b>konačni "
  "automat</b>: za svaki bajt prelazi u sljedeće stanje (čekam SOF1 → SOF2 → "
  "tip → AZ_H → … → EOF). Tek kad uredno stigne do kraja s ispravnim end-markerom, "
  "paket se smatra valjanim i prosljeđuje vizualizaciji.")
Code(
"""
switch (state) {
  case S_SOF1: if (b==0xAA) state=S_SOF2;  break;   /* čekaj start    */
  case S_SOF2: state=(b==0xBB)?S_TYPE:S_SOF1; break;
  case S_TYPE: pkt_type=b;
               state=(b==0x03)?S_AZ_H:(b==0x04?S_RAW_NCH:S_SOF1); break;
  case S_AZ_H: az_h=b; state=S_AZ_L; break;
  ...
  case S_EOF2: if (b==0xDD) {            /* paket potpun i ispravan    */
                 int16_t az=(az_h<<8)|az_l;
                 web_server_send_data(az/10.0f, el/10.0f, str_val);
               } state=S_SOF1; break;
}
""",
caption="Isječak 23. Parser kao konačni automat (sound_loc_processor.c). Otporno na izgubljene/iskrivljene bajtove."
)
Callout("Zašto automat, a ne «pročitaj 10 bajtova»",
  "Ako se izgubi samo jedan bajt, naivno čitanje fiksne duljine zauvijek bi "
  "ostalo pomaknuto. Automat se uvijek iznova sinkronizira na start-marker "
  "0xAA 0xBB, pa se nakon greške sam oporavi na sljedećem paketu.")

H2("9.3&nbsp;&nbsp;WebSocket — gurni podatke u preglednik čim stignu")
P("Klasični HTTP traži da preglednik <i>pita</i> poslužitelj. Za podatke koji "
  "stižu spontano (svaki pljesak) prikladniji je <b>WebSocket</b> — trajna veza "
  "preko koje poslužitelj može <b>sam</b> poslati podatak čim ga ima. Za svaku "
  "detekciju ESP32 svim spojenim preglednicima pošalje kratki JSON.")
Code(
"""
/* ESP32 → preglednik, za svaku detekciju: */
{"azimuth":30.0,"polar":20.0,"strength":42}
""",
caption="Isječak 24. JSON poruka preko WebSocket-a (web_server.c, web_server_send_data)."
)
P("Lista spojenih klijenata čuva se uz <b>mutex</b> (uzajamno isključenje) jer "
  "joj pristupaju dvije niti — ona koja prima nove veze i ona koja šalje podatke. "
  "Bez zaključavanja moglo bi doći do oštećenja liste; mrtve veze (zatvoreni "
  "preglednik) automatski se uklanjaju.")

H2("9.4&nbsp;&nbsp;3D vizualizacija (WebGL)")
P("Sama web-stranica ugrađena je kao tekst u firmware ESP32 i poslužuje se na "
  "korijenskoj adresi «/». Crta minimalnu 3D scenu pomoću WebGL-a:")
Bullets([
  "Plava kocka u središtu predstavlja mikrofonski niz (slušatelja).",
  "Tri prozirne ravnine (XY, XZ, YZ) daju osjećaj prostora i orijentacije.",
  "Svaka detekcija doda <b>svjetleću kuglicu</b> na izračunatom smjeru "
  "(azimut + elevacija na zamišljenoj sferi), koja kroz 5 s polako izblijedi.",
  "Mišem/dodirom scena se može slobodno zakretati i zumirati (orbit kamera).",
])
Callout("Pretvorba kuta u 3D položaj kuglice",
  "Iz azimuta (a) i elevacije/polara (p) položaj na sferi polumjera 8 računa se "
  "kao x = 8·sin(a)·cos(p), y = 8·sin(p), z = −8·cos(a)·cos(p). Tako kut iz "
  "STM32 izračuna postaje konkretna točka koju oko vidi.")

# ============================================================================
#  10. CJELOVITI PRIMJER
# ============================================================================
H1("10.&nbsp;&nbsp;Cjeloviti primjer: od pljeska do točke na ekranu")

P("Spojimo sve u jedan slijed događaja za <b>jedan pljesak</b> iz smjera "
  "azimut +30°, elevacija +20°:")
NumList([
  "<b>t = 0 ms.</b> Zvučni val stiže na mikrofone u malo različitim trenucima "
  "(M2 i M3 ranije/kasnije od M1, ovisno o smjeru).",
  "<b>Kontinuirano.</b> TIM8 okida ADC svakih 15.6 µs; ADC redom čita M1–M4; "
  "DMA puni adc_buffer bez procesora.",
  "<b>Svakih 16 ms.</b> DMA javi HT ili TC prekid → ISR pošalje događaj u red → "
  "ACQ_Task kopira svježu polovicu u snapshot i javi FFT_Task-u.",
  "<b>FFT_Task.</b> Sastavi klizni prozor (2048), pozove LOC3D_Process: nađe "
  "energetski vrh, izdvoji kanale (DC + Hann), provjeri RMS prag.",
  "<b>GCC-PHAT × 3.</b> Izmjeri kašnjenja τ12 ≈ −274 µs, τ13 ≈ −137 µs, "
  "τ14 ≈ −198 µs (nakon korekcije kanalnog offseta).",
  "<b>Geometrija.</b> u = M_geom·τ, pa s = −u/|u| ≈ (0.814, 0.470, 0.342).",
  "<b>Kutovi.</b> azimut = atan2(0.470, 0.814) = 30.0°, elevacija = "
  "asin(0.342) = 20.0°, jakost ≈ 42.",
  "<b>UART.</b> UART_Task pošalje 10-bajtni paket "
  "[AA BB 03 01 2C 00 C8 2A CC DD] na ESP32.",
  "<b>ESP32.</b> rx_task automatom sklopi paket, izračuna 30.0°/20.0°, pošalje "
  "JSON preko WebSocket-a.",
  "<b>Preglednik.</b> Doda svjetleću kuglicu na (x,y,z) koji odgovara "
  "30°/20°; HUD ispiše «Azimuth: 30.0° | Polar: 20.0° | Strength: 42».",
  "<b>Cooldown.</b> STM32 idućih ~304 ms ignorira nove detekcije da isti "
  "pljesak ne ispali više točaka.",
])
P("0x012C = 300 = 30.0° (azimut), 0x00C8 = 200 = 20.0° (elevacija), 0x2A = 42 "
  "(jakost) — bajtovi u paketu iz koraka 8.")

# ============================================================================
#  11. POJMOVNIK
# ============================================================================
H1("11.&nbsp;&nbsp;Pojmovnik za početnike")
DataTable(
    [["Pojam", "Objašnjenje"],
     ["ADC", "Analog-to-Digital Converter — pretvara napon u broj (ovdje 12-bitni, 0–4095)."],
     ["DMA", "Direct Memory Access — sklop koji premješta podatke u/iz memorije bez procesora."],
     ["ISR", "Interrupt Service Routine — kratka rutina koja se izvrši na hardverski prekid."],
     ["RTOS", "Real-Time OS — sustav koji dijeli procesor na taskove s prioritetima."],
     ["Task", "Neovisna nit izvođenja s vlastitim stogom; vrti se u petlji."],
     ["Stack", "Stog — memorija za lokalne varijable i povratne adrese jednog taska."],
     ["Heap", "Gomila — zajednička memorija iz koje RTOS alocira objekte."],
     ["Queue", "Red čekanja — sigurna cijev za predaju podataka među taskovima/ISR-om."],
     ["TDOA", "Time Difference Of Arrival — razlika vremena dolaska zvuka na dva mikrofona."],
     ["Korelacija", "Mjera sličnosti dvaju signala u ovisnosti o njihovom međusobnom pomaku."],
     ["GCC-PHAT", "Izoštrena križna korelacija (samo faza) za precizno mjerenje TDOA."],
     ["FFT", "Fast Fourier Transform — brzi prelazak signala u frekvencijsku domenu."],
     ["DC offset", "Istosmjerna (srednja) razina signala; uklanja se prije korelacije."],
     ["Hann prozor", "Glatko «zvono» kojim se množi signal da se smanje FFT artefakti."],
     ["RMS", "Root Mean Square — efektivna amplituda (mjera glasnoće) signala."],
     ["Azimut", "Vodoravni kut smjera (lijevo/desno/naprijed/nazad)."],
     ["Elevacija", "Okomiti kut smjera (gore/dolje)."],
     ["Interleaved", "Isprepleteni raspored: uzorci kanala poredani M1 M2 M3 M4 M1 M2…"],
     ["Big-endian", "Zapis broja s višim bajtom prvim (npr. 300 = 0x01 0x2C)."],
     ["WebSocket", "Trajna dvosmjerna veza preglednik–poslužitelj za «push» podatke."],
     ["Access Point", "Način rada u kojem ESP32 sam stvara Wi-Fi mrežu."]],
    col_widths=[3.2*cm, 13.0*cm],
    caption=None
)

Sp(10)
story.append(HRFlowable(width="100%", thickness=0.8, color=NAVY, spaceBefore=6, spaceAfter=6))
P("Kraj dokumenta. Svi isječci koda i konstante preuzeti su izravno iz izvornog "
  "koda projekta (mape «Sound Localization» i «ESP32_Visualization»).", st_note)

# ============================================================================
#  IZGRADNJA PDF-a (s brojevima stranica)
# ============================================================================
def on_page(canvas, doc):
    canvas.saveState()
    w, h = A4
    # zaglavlje (osim na naslovnici)
    if doc.page > 1:
        canvas.setFont("Calibri", 8)
        canvas.setFillColor(GREY)
        canvas.drawString(2.0*cm, h - 1.25*cm,
                          "Lokalizacija zvuka mikrofonskim nizom — STM32 + ESP32")
        canvas.setStrokeColor(LINE)
        canvas.setLineWidth(0.4)
        canvas.line(2.0*cm, h - 1.35*cm, w - 2.0*cm, h - 1.35*cm)
    # podnožje
    canvas.setFont("Calibri", 8.5)
    canvas.setFillColor(GREY)
    canvas.drawCentredString(w/2.0, 1.1*cm, str(doc.page))
    canvas.restoreState()

OUT = os.path.join(os.path.dirname(__file__), "..",
                   "Dokumentacija_Lokalizacija_Zvuka.pdf")
OUT = os.path.abspath(OUT)

doc = BaseDocTemplate(
    OUT, pagesize=A4,
    leftMargin=2.0*cm, rightMargin=2.0*cm,
    topMargin=1.7*cm, bottomMargin=1.6*cm,
    title="Lokalizacija zvuka mikrofonskim nizom — STM32 + ESP32",
    author="Projekt dokumentacija",
)
frame = Frame(doc.leftMargin, doc.bottomMargin,
              doc.width, doc.height, id="main")
doc.addPageTemplates([PageTemplate(id="all", frames=[frame], onPage=on_page)])
doc.build(story)
print("PDF napisan:", OUT)
