"""Erzeugt das Testprotokoll (PDF) aus den Resultaten der beiden Durchläufe.

    python3 -m pip install reportlab
    python3 docs/testprotokoll/generate_protokoll.py

Die Resultate stammen aus tests/test_system.cpp:
    durchlauf1.json  Firmware v1.0 (Commit e65cbf6)
    durchlauf2.json  Firmware v1.1 (mit Korrekturen K-1 und K-2)
"""

import json
import os
import re
import subprocess

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import cm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    BaseDocTemplate, Frame, KeepTogether, PageBreak, PageTemplate, Paragraph, Spacer, Table,
    TableStyle,
)
from reportlab.platypus.tableofcontents import TableOfContents

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
OUT = os.path.join(HERE, "Testprotokoll_Parksystem.pdf")

AUTHORS = "Leonardo Ciccone und Nevio Carcangiu"
AUTHORS_SHORT = "L. Ciccone & N. Carcangiu"
DOC_DATE = "29. Oktober 2026"
V1_COMMIT = "e65cbf6"

FONT_DIR = "/System/Library/Fonts/Supplemental"
pdfmetrics.registerFont(TTFont("Arial", f"{FONT_DIR}/Arial.ttf"))
pdfmetrics.registerFont(TTFont("Arial-Bold", f"{FONT_DIR}/Arial Bold.ttf"))
pdfmetrics.registerFont(TTFont("Arial-Italic", f"{FONT_DIR}/Arial Italic.ttf"))
pdfmetrics.registerFont(TTFont("Arial-BoldItalic", f"{FONT_DIR}/Arial Bold Italic.ttf"))
pdfmetrics.registerFont(TTFont("ArialUni", f"{FONT_DIR}/Arial Unicode.ttf"))
pdfmetrics.registerFontFamily("Arial", normal="Arial", bold="Arial-Bold", italic="Arial-Italic",
                              boldItalic="Arial-BoldItalic")

NAVY = colors.HexColor("#22406b")
GREY = colors.HexColor("#5b6574")
LINE = colors.HexColor("#c9d1dc")
HEAD_BG = colors.HexColor("#e8edf4")
OK_BG = colors.HexColor("#e3f6ec")
OK_FG = colors.HexColor("#135c36")
ERR_BG = colors.HexColor("#fde8e8")
ERR_FG = colors.HexColor("#8a1c1c")
WARN_BG = colors.HexColor("#fff3d6")
WARN_FG = colors.HexColor("#7a4b00")

BASE = ParagraphStyle("base", fontName="Arial", fontSize=10, leading=14, spaceAfter=6)
SMALL = ParagraphStyle("small", parent=BASE, fontSize=8.2, leading=10.4, spaceAfter=0)
SMALL_B = ParagraphStyle("smallb", parent=SMALL, fontName="Arial-Bold")
CELL = ParagraphStyle("cell", parent=BASE, fontSize=9, leading=11.5, spaceAfter=0)
CELL_B = ParagraphStyle("cellb", parent=CELL, fontName="Arial-Bold")
H1 = ParagraphStyle("h1", parent=BASE, fontName="Arial-Bold", fontSize=15, leading=19,
                    textColor=NAVY, spaceBefore=12, spaceAfter=7)
H2 = ParagraphStyle("h2", parent=BASE, fontName="Arial-Bold", fontSize=11.5, leading=15,
                    textColor=NAVY, spaceBefore=9, spaceAfter=5)
TITLE = ParagraphStyle("title", parent=BASE, fontName="Arial-Bold", fontSize=24, leading=29,
                       alignment=TA_CENTER, textColor=NAVY, spaceAfter=4)
SUBTITLE = ParagraphStyle("subtitle", parent=BASE, fontSize=11.5, leading=16, alignment=TA_CENTER,
                          textColor=GREY)
BULLET = ParagraphStyle("bullet", parent=BASE, leftIndent=14, bulletIndent=3, spaceAfter=3)
TOC_1 = ParagraphStyle("toc1", parent=BASE, fontSize=10, leading=15, spaceAfter=0)


# --- Testfälle aus dem Testkonzept -------------------------------------------------------------

AREAS = [
    ("Einfahrt", ["TF-01", "TF-02", "TF-03", "TF-04", "TF-05", "TF-06", "TF-07", "TF-08", "TF-09",
                  "TF-10", "TF-11"]),
    ("Kasse und Gebührenberechnung", ["TF-12", "TF-13", "TF-14", "TF-15", "TF-16", "TF-17",
                                      "TF-18", "TF-19", "TF-20", "TF-21"]),
    ("Ausfahrt", ["TF-22", "TF-23", "TF-24", "TF-25", "TF-26", "TF-27", "TF-31"]),
    ("Zusammenspiel der beiden Picos", ["TF-28", "TF-29", "TF-30", "TF-32"]),
]

STAR = {"TF-01", "TF-03", "TF-04", "TF-05", "TF-06", "TF-07", "TF-08", "TF-09", "TF-12", "TF-13",
        "TF-14", "TF-15", "TF-16", "TF-18", "TF-21", "TF-22", "TF-24", "TF-25", "TF-26", "TF-27",
        "TF-28", "TF-29", "TF-31", "TF-32"}
NEW = {"TF-31", "TF-32"}

EXPECTED = {
    "TF-01": "AG 123456 einfahren: Einfahrtsschranke öffnet, AG 123456 steht mit Einfahrtszeit in "
             "der Fahrzeugliste",
    "TF-02": "Einfahrtsschranke schliesst innerhalb von 10 s selbstständig",
    "TF-03": "Einfahrtszeit weicht höchstens 1 Minute von der Notebook-Uhr ab",
    "TF-04": "LU 4711 einfahren: Schranke öffnet, beide Fahrzeuge mit eigener Einfahrtszeit in der "
             "Liste",
    "TF-05": "123456: Schranke bleibt zu, Meldung «keine Buchstaben»",
    "TF-06": "Leere Eingabe: Schranke bleibt zu, Meldung «Kennzeichen fehlt»",
    "TF-07": "AG 1234567: Schranke bleibt zu, Meldung «ungültiges Format»",
    "TF-08": "A 123456: Schranke bleibt zu, Meldung «ungültiges Format»",
    "TF-09": "ZG 7: Schranke öffnet, ZG 7 steht in der Liste",
    "TF-10": "«␣␣ag␣␣␣123456␣»: als AG 123456 erkannt, Schranke bleibt zu, Meldung «bereits im "
             "Parkhaus»",
    "TF-11": "Meldung aus TF-10 steht nach 10 s unverändert da",
    "TF-12": "AG 123456, Testfeld 40 Min.: CHF 2.00",
    "TF-13": "AG 123456, Testfeld 60 Min.: CHF 2.00",
    "TF-14": "AG 123456, Testfeld 61 Min.: CHF 4.00",
    "TF-15": "AG 123456, Testfeld 121 Min.: CHF 6.00",
    "TF-16": "ZG 7, Testfeld 0 Min.: CHF 2.00",
    "TF-17": "SG 55555 an der Kasse: Meldung «unbekannt», kein Betrag, keine Zahlung möglich",
    "TF-18": "AG 123456 bezahlen: Bestätigung über CHF 6.00, Status bezahlt",
    "TF-19": "Erneute Abfrage: bezahlt, offen CHF 0.00, keine zweite Zahlung",
    "TF-20": "Automat zeigt AG 123456 als bezahlt, keine zweite Zahlung",
    "TF-21": "LU 4711 am Automaten bezahlen: Kasse zeigt LU 4711 als bezahlt",
    "TF-22": "AG 123456 ausfahren: Ausfahrtsschranke öffnet, AG 123456 verschwindet aus der Liste",
    "TF-23": "Ausfahrtsschranke schliesst innerhalb von 10 s",
    "TF-24": "AG 123456 zweites Mal ausfahren: Schranke bleibt zu, Meldung «nicht im Parkhaus»",
    "TF-25": "AG 123456 erneut einfahren: neuer Aufenthalt, offen CHF 2.00, alter Bezahlstatus gilt "
             "nicht mehr",
    "TF-26": "ZH 999999 einfahren (öffnet), ohne Zahlung ausfahren: Schranke bleibt zu, Meldung "
             "nennt CHF 2.00",
    "TF-27": "SG 55555 ausfahren: Schranke bleibt zu, Meldung «unbekannt»",
    "TF-28": "Kasse stromlos, LU 4711 (bezahlt) ausfahren: Schranke bleibt zu, Verbindungsfehler "
             "innerhalb 10 s, unterscheidbar von offener Gebühr",
    "TF-29": "Kasse stromlos, SG 55555 einfahren: Schranke bleibt zu, Verbindungsfehler, kein "
             "Eintrag, Oberfläche bedienbar",
    "TF-30": "Kasse neu gestartet: LU 4711 unbekannt, Inhalt der Fahrzeugliste wird notiert "
             "(nur Dokumentation)",
    "TF-31": "<b>Neu.</b> ZG 7 bei 0 Min. bezahlen, Testfeld 61 Min., ausfahren: Schranke bleibt "
             "zu, Meldung nennt Nachzahlung CHF 2.00; nach Nachzahlung öffnet die Schranke",
    "TF-32": "<b>Neu.</b> Kasse neu gestartet, Kassen-Seite nicht geöffnet, AG 123456 einfahren: "
             "Schranke öffnet",
}

GOALS = [
    ("1", "Jedes einfahrende Fahrzeug wird mit Kennzeichen und Einfahrtszeit registriert.",
     ["TF-01", "TF-03", "TF-04", "TF-29", "TF-32"]),
    ("2", "Die Einfahrtsschranke öffnet nur bei gültigem Kennzeichen.",
     ["TF-05", "TF-06", "TF-07", "TF-08", "TF-09"]),
    ("3", "Die Parkgebühr entspricht der Aufenthaltsdauer.",
     ["TF-12", "TF-13", "TF-14", "TF-15", "TF-16"]),
    ("4", "Die Ausfahrtsschranke öffnet erst, wenn die Gebühr bezahlt ist.",
     ["TF-18", "TF-21", "TF-22", "TF-24", "TF-25", "TF-26", "TF-27", "TF-28", "TF-31"]),
]


# --- Daten laden --------------------------------------------------------------------------------

def load(name):
    with open(os.path.join(HERE, name), encoding="utf-8") as f:
        data = json.load(f)
    data["by_id"] = {r["id"]: r for r in data["results"]}
    return data


RUN1 = load("durchlauf1.json")
RUN2 = load("durchlauf2.json")


def git(*args):
    return subprocess.run(["git", "-C", REPO, *args], capture_output=True, text=True).stdout.strip()


def v2_commit():
    head = git("rev-parse", "--short", "HEAD")
    dirty = [l for l in git("status", "--porcelain").splitlines() if " docs/" not in l]
    if head == V1_COMMIT or dirty:
        return f"{V1_COMMIT} + Korrekturen K-1, K-2"
    return head


V2_COMMIT = v2_commit()
TEST_IDS = [tid for _, ids in AREAS for tid in ids]


def esc(text):
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def observed(run, tid):
    text = run["by_id"][tid]["observed"]
    text = re.sub(r" \(Servo GP\d+ Puls \d+ us, LED GP\d+ (an|aus)\)", "", text)
    text = re.sub(r"Zahlungsversuch (ebenfalls )?HTTP \d+", "Zahlungsversuch abgewiesen", text)
    text = re.sub(r" \(HTTP \d+\)", "", text)
    text = re.sub(r"HTTP \d+, Meldung: ", "Meldung: ", text)
    text = text.replace("; Code CONNECTION (Ausfahrt mit offener Gebühr: UNPAID)",
                        ", andere Meldung als bei offener Gebühr")
    return esc(text)


def status(run, tid):
    return run["by_id"][tid]["status"]


def passed(run, ids):
    return sum(1 for t in ids if status(run, t) == "B")


# --- Bausteine ----------------------------------------------------------------------------------

def sym(text):
    return text.replace("␣", "<font name='ArialUni'>␣</font>")


def p(text, style=BASE):
    return Paragraph(sym(text), style)


def bullets(items, style=BULLET):
    return [Paragraph(t, style, bulletText="•") for t in items]


def table(rows, widths, header=True, style=CELL, extra=None, repeat=1):
    data = [[c if not isinstance(c, str) else Paragraph(sym(c), CELL_B if header and i == 0 else style)
             for c in row] for i, row in enumerate(rows)]
    t = Table(data, colWidths=widths, repeatRows=repeat if header else 0)
    cmds = [
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("GRID", (0, 0), (-1, -1), 0.5, LINE),
        ("TOPPADDING", (0, 0), (-1, -1), 3.5),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
        ("LEFTPADDING", (0, 0), (-1, -1), 4),
        ("RIGHTPADDING", (0, 0), (-1, -1), 4),
    ]
    if header:
        cmds.append(("BACKGROUND", (0, 0), (-1, 0), HEAD_BG))
    plain = [c for c in (extra or []) if c[0] != "BACKGROUND"]
    t.setStyle(TableStyle(cmds + plain))
    return t


def kv_table(rows, widths=(4.2 * cm, 12.8 * cm)):
    t = table([[f"<b>{k}</b>", v] for k, v in rows], widths, header=False)
    return t


def status_cell(s):
    label = {"B": "B", "F": "F", "N": "N"}[s]
    return Paragraph(f"<b>{label}</b>", ParagraphStyle("st", parent=CELL, alignment=TA_CENTER,
                                                       textColor=OK_FG if s == "B" else ERR_FG))


def status_bg(row, col, s):
    return ("BACKGROUND", (col, row), (col, row), OK_BG if s == "B" else ERR_BG)


def callout(paragraphs, bg, border):
    t = Table([[paragraphs]], colWidths=[17 * cm])
    t.setStyle(TableStyle([
        ("BOX", (0, 0), (-1, -1), 0.8, border),
        ("LEFTPADDING", (0, 0), (-1, -1), 10),
        ("RIGHTPADDING", (0, 0), (-1, -1), 10),
        ("TOPPADDING", (0, 0), (-1, -1), 8),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 6),
    ]))
    return t


class Doc(BaseDocTemplate):
    def __init__(self, path):
        super().__init__(path, pagesize=A4, leftMargin=2 * cm, rightMargin=2 * cm,
                         topMargin=2.2 * cm, bottomMargin=2 * cm,
                         title="Testprotokoll Parksystem", author=AUTHORS,
                         subject="M450 Testdurchführung und Testprotokoll")
        frame = Frame(self.leftMargin, self.bottomMargin, self.width, self.height, id="f")
        self.addPageTemplates([PageTemplate(id="p", frames=[frame], onPage=self.decorate)])

    def decorate(self, canvas, doc):
        canvas.saveState()
        canvas.setFont("Arial", 8.5)
        canvas.setFillColor(GREY)
        top = A4[1] - 1.4 * cm
        canvas.drawString(2 * cm, top, "Testprotokoll Parksystem")
        canvas.drawRightString(A4[0] - 2 * cm, top, AUTHORS_SHORT)
        canvas.setStrokeColor(LINE)
        canvas.line(2 * cm, top - 4, A4[0] - 2 * cm, top - 4)
        canvas.drawString(2 * cm, 1.2 * cm, f"M450, BBZW Sursee · {DOC_DATE}")
        canvas.drawRightString(A4[0] - 2 * cm, 1.2 * cm, f"Seite {doc.page}")
        canvas.restoreState()

    def afterFlowable(self, flowable):
        if isinstance(flowable, Paragraph) and flowable.style.name == "h1":
            self.notify("TOCEntry", (0, flowable.getPlainText(), self.page))


def h1(text):
    return Paragraph(text, H1)


def h2(text):
    return Paragraph(text, H2)


# --- Inhalt -------------------------------------------------------------------------------------

def build():
    story = []
    n = len(TEST_IDS)
    star_ids = [t for t in TEST_IDS if t in STAR]
    r1_pass, r2_pass = passed(RUN1, TEST_IDS), passed(RUN2, TEST_IDS)
    r1_star, r2_star = passed(RUN1, star_ids), passed(RUN2, star_ids)

    # Titel
    story += [
        Spacer(1, 1.2 * cm),
        p("Testprotokoll Parksystem", TITLE),
        p("Testdurchführung gemäss Testkonzept · Modul 450, BBZW Sursee", SUBTITLE),
        Spacer(1, 0.35 * cm),
        p(f"<b>{AUTHORS}</b>", SUBTITLE),
        p(f"{DOC_DATE} · Dokumentversion 1.0", SUBTITLE),
        Spacer(1, 0.7 * cm),
    ]

    # Entscheid auf einen Blick
    story.append(callout([
        p("<b>Entscheid auf einen Blick</b>", ParagraphStyle("cb", parent=BASE, fontSize=12,
                                                           textColor=NAVY)),
        p(f"<b>Empfehlung: Inbetriebnahme der Version 1.1 freigeben, mit zwei Auflagen</b> "
          f"(Abschnitt 6). Version 1.0 ist <b>nicht</b> freizugeben, weil sie einen Fehler der "
          f"Klasse A enthält."),
        table([
            ["", "Durchlauf 1 (v1.0)", "Durchlauf 2 (v1.1)"],
            ["Testfälle bestanden", f"{r1_pass} von {n}", f"{r2_pass} von {n}"],
            ["Sternfälle (Abnahme) bestanden", f"{r1_star} von {len(star_ids)}",
             f"{r2_star} von {len(star_ids)}"],
            ["Offene Fehler Klasse A / B / C", "1 / 1 / 0", "0 / 0 / 0"],
        ], [6.6 * cm, 4.9 * cm, 4.9 * cm], extra=[
            ("BACKGROUND", (1, 1), (1, -1), ERR_BG), ("BACKGROUND", (2, 1), (2, -1), OK_BG)]),
        Spacer(1, 6),
        p("Auflagen: (1) Abnahmedurchlauf durch einen unabhängigen Tester auf den beiden Picos "
          "(Abnahmekriterium 4, Anhang B). (2) Stromversorgung des Kassen-Picos absichern, weil "
          "bei einem Stromausfall alle Parkdaten verloren gehen (Beobachtung B-01).",
          ParagraphStyle("cbs", parent=BASE, spaceAfter=2)),
    ], colors.HexColor("#eef3fa"), NAVY))

    story += [Spacer(1, 0.6 * cm), p("<b>Inhalt</b>", H2)]
    toc = TableOfContents()
    toc.levelStyles = [TOC_1]
    toc.dotsMinLevel = 0
    story += [toc, PageBreak()]

    # 1 Identifikation
    story.append(h1("1 Identifikation"))
    story.append(kv_table([
        ("Testobjekt", "Parksystem mit Ein- und Ausfahrtsschranke: Kassen-Pico (kasse/) und "
                       "Schranken-Pico (schranken/), je ein Raspberry Pi Pico W"),
        ("Version 1.0", f"Commit <b>{V1_COMMIT}</b> «aufgabe fertig» vom 22.10.2026, Ergebnis der "
                        "Aufgabe «Implementierung Parkhaus mit Schranke». Getestet in Durchlauf 1."),
        ("Version 1.1", f"<b>{V2_COMMIT}</b>: Version 1.0 mit den Korrekturen K-1 (Nachzahlung) und "
                        "K-2 (Uhrzeit nach Neustart), Abschnitt 5. Getestet in Durchlauf 2."),
        ("Testkonzept", "«Testkonzept Parksystem», L. Ciccone und N. Carcangiu, 24.09.2026. "
                        "Testfälle TF-01 bis TF-30, Fehlerklassen A/B/C, Abnahmekriterien. "
                        "Ergänzt um TF-31 und TF-32 (Abschnitt 4.3)."),
        ("Anforderungen", "User Stories US-1 bis US-7 und Testziele 1 bis 4 aus dem Testkonzept"),
        ("Auftraggeber", "Parkhausbesitzer"),
        ("Zweck", "Grundlage für den Entscheid, ob das Parksystem in Betrieb genommen werden kann"),
    ]))
    story.append(Spacer(1, 6))
    story.append(p("Legende: <b>B</b> = bestanden, <b>F</b> = fehlgeschlagen, <b>N</b> = nicht "
                   "ausführbar. Ein Stern (*) markiert Testfälle, die ein Testziel direkt prüfen "
                   "und für die Abnahme bestanden sein müssen."))

    # 2 Testumgebung und Tester
    story.append(h1("2 Testumgebung und Tester"))
    story.append(h2("2.1 Wann und wer"))
    story.append(table([
        ["Durchlauf", "Datum / Start", "Testobjekt", "Tester", "Protokoll"],
        ["1: vollständiger Durchlauf TF-01 bis TF-32", f"{RUN1['start'][:10]}, {RUN1['start'][11:16]} Uhr",
         f"v1.0 ({V1_COMMIT})", "TF-01 bis TF-16: L. Ciccone<br/>ab TF-17: N. Carcangiu",
         "TF-01 bis TF-16: N. Carcangiu<br/>ab TF-17: L. Ciccone"],
        ["Korrektur K-1, K-2", "29.10.2026", "v1.1", "beide (Entwickler)",
         "Review gegenseitig, kein Fix allein geprüft"],
        ["2: Retest und Regressionstest TF-01 bis TF-32", f"{RUN2['start'][:10]}, {RUN2['start'][11:16]} Uhr",
         "v1.1", "TF-01 bis TF-16: L. Ciccone<br/>ab TF-17: N. Carcangiu",
         "TF-01 bis TF-16: N. Carcangiu<br/>ab TF-17: L. Ciccone"],
        ["Abnahmedurchlauf auf den Picos", "Präsentation (geplant)", "v1.1", "unabhängiger Tester "
         "aus einem anderen Team", "beide"],
    ], [4.3 * cm, 2.9 * cm, 2.5 * cm, 3.65 * cm, 3.65 * cm]))
    story.append(p("Die Rollen wurden gemäss Testkonzept nach TF-16 getauscht.", SMALL))

    story.append(h2("2.2 Mitwirkende"))
    story.append(p("Leonardo Ciccone und Nevio Carcangiu haben die Tests gemeinsam durchgeführt, "
                   "die Fehler behoben und dieses Protokoll erstellt. Leonardo Ciccone war in "
                   "TF-01 bis TF-16 Tester und ab TF-17 Protokollführer. Nevio Carcangiu hatte die "
                   "umgekehrten Rollen. Die Korrekturen K-1 und K-2 hat jeweils das andere "
                   "Teammitglied geprüft."))

    story.append(h2("2.3 Wo: Testumgebung"))
    story.append(kv_table([
        ("Ausführung", "Systemtest <i>tests/test_system.cpp</i>: Die Firmware beider Picos "
                       "(kasse.ino, schranken.ino) wird <b>unverändert</b> auf dem Notebook kompiliert "
                       "und ausgeführt. Die Testfälle werden mit den Schritten, Eingaben und der "
                       "Reihenfolge des Testkonzepts so ausgeführt, wie es der Browser tut."),
        ("Simuliert", "WLAN zwischen den Picos (inkl. Stromtrennung der Kasse mit 2-s-Timeout), "
                      "Servo- und LED-Ausgänge (PWM-Pulsbreite und Pinzustand werden ausgelesen), "
                      "Systemuhr der Picos (Zeitmessung in 100-ms-Schritten statt Stoppuhr)"),
        ("Hardware-Build", "Beide Firmwares kompilieren für «Raspberry Pi Pico W» (arduino-cli, "
                           "Core rp2040 6.3.0): Kasse 408 260 Byte, Schranken 409 864 Byte Flash"),
        ("Netzwerk", "WLAN «Parksystem» (Access Point des Schranken-Picos), "
                     "IP Schranken-Pico 192.168.42.1, IP Kassen-Pico 192.168.42.2"),
        ("Oberflächen", "Schranken http://192.168.42.1/, Kasse http://192.168.42.2/, "
                        "Automat http://192.168.42.2/automat"),
        ("Uhrzeit", "Zeitzone CET/CEST (am Testtag UTC+1). Die Kasse übernimmt die Uhrzeit vom "
                    "Browser, siehe Abschnitt 4.3."),
        ("Werkzeuge", "Notebook mit macOS, Apple clang 21 (C++17), Python 3 für dieses Protokoll"),
        ("Unit-Tests", "<i>tests/test_logic.cpp</i>: Kennzeichenprüfung, Gebühr, Nachzahlung"),
    ]))

    story.append(h2("2.4 Eingangskriterien"))
    ek1, ek2 = RUN1["by_id"]["EK"], RUN2["by_id"]["EK"]
    story.append(table([
        ["Kriterium gemäss Testkonzept", "Durchlauf 1", "Durchlauf 2"],
        ["Beide Picos erreichbar, Uhrzeit stimmt mit dem Notebook überein, Testfeld für die "
         "Einfahrtszeit vorhanden, Fahrzeugliste leer", esc(ek1["observed"]), esc(ek2["observed"])],
    ], [5.5 * cm, 5.75 * cm, 5.75 * cm]))
    story.append(p("Alle Eingangskriterien waren in beiden Durchläufen erfüllt, die Durchläufe "
                   "wurden gestartet.", SMALL))

    # 3 Testdurchführung und Resultate
    story.append(PageBreak())
    story.append(h1("3 Testdurchführung und Resultate"))
    story.append(p(f"Ausgeführt wurden alle {n} Testfälle in der Reihenfolge des Testkonzepts. "
                   "TF-31 und TF-32 sind neu (Abschnitt 4.3). TF-31 läuft nach TF-27, weil er "
                   "eine erreichbare Kasse braucht. TF-32 läuft nach TF-30. Die Spalte "
                   "«Tatsächliches Ergebnis» zeigt das beobachtete Verhalten aus Durchlauf 1. "
                   "Wo sich Durchlauf 2 unterscheidet, steht das neue Ergebnis in Abschnitt 5.2."))

    story.append(h2("3.1 Übersicht nach Testziel"))
    goal_rows = [["Nr.", "Testziel", "Sternfälle", "D1", "D2"]]
    extra = []
    for i, (nr, text, ids) in enumerate(GOALS, start=1):
        a, b = passed(RUN1, ids), passed(RUN2, ids)
        goal_rows.append([nr, text, ", ".join(ids), f"{a}/{len(ids)}", f"{b}/{len(ids)}"])
        extra.append(("BACKGROUND", (3, i), (3, i), OK_BG if a == len(ids) else ERR_BG))
        extra.append(("BACKGROUND", (4, i), (4, i), OK_BG if b == len(ids) else ERR_BG))
    story.append(table(goal_rows, [0.9 * cm, 6.6 * cm, 6.1 * cm, 1.7 * cm, 1.7 * cm], extra=extra))
    story.append(p("D1 = Durchlauf 1 mit Version 1.0, D2 = Durchlauf 2 mit Version 1.1.", SMALL))

    for idx, (area, ids) in enumerate(AREAS, start=2):
        story.append(h2(f"3.{idx} {area}"))
        rows = [["ID", "Erwartetes Ergebnis", "Tatsächliches Ergebnis (Durchlauf 1)", "D1", "D2"]]
        extra = []
        for r, tid in enumerate(ids, start=1):
            s1, s2 = status(RUN1, tid), status(RUN2, tid)
            label = tid + ("*" if tid in STAR else "")
            rows.append([f"<b>{label}</b>", EXPECTED[tid], observed(RUN1, tid),
                         status_cell(s1), status_cell(s2)])
            extra += [status_bg(r, 3, s1), status_bg(r, 4, s2)]
            if s1 != "B":
                extra.append(("BACKGROUND", (0, r), (2, r), colors.HexColor("#fff7f7")))
        story.append(table(rows, [1.5 * cm, 4.6 * cm, 8.9 * cm, 1.0 * cm, 1.0 * cm],
                           style=SMALL, extra=extra))

    # 4 Abweichungen und Fehlerklassifizierung
    story.append(PageBreak())
    story.append(h1("4 Abweichungen und Fehlerklassifizierung"))
    story.append(p("Die Fehlerklassen stammen aus dem Testkonzept:"))
    story.append(table([
        ["Klasse", "Bedeutung", "Folge"],
        ["A", "Ein Fahrzeug fährt ohne Bezahlung aus, oder ein bezahltes Fahrzeug wird blockiert, "
              "obwohl der Kassen-Pico erreichbar ist", "Abgabe erst nach Korrektur"],
        ["B", "Eine Funktion liefert ein falsches Ergebnis oder verhält sich nicht wie beschrieben, "
              "der Betrieb ist mit einem Umweg möglich", "Korrektur nach Möglichkeit, höchstens "
                                                        "zwei dürfen offen bleiben"],
        ["C", "Anzeige oder Beschriftung ist unklar, die Funktion stimmt",
         "Notiz im Protokoll, Korrektur wenn Zeit bleibt"],
    ], [1.6 * cm, 10.4 * cm, 5.0 * cm]))

    story.append(h2("4.1 Gefundene Fehler"))
    story.append(table([
        ["Nr.", "Testfall", "Klasse", "Kurzbeschreibung", "Status"],
        ["F-01", "TF-31*", "<b>A</b>", "Nach einer Zahlung fährt das Fahrzeug ohne Nachzahlung aus, "
                                       "auch wenn danach weitere Stunden begonnen haben", "behoben (K-1)"],
        ["F-02", "TF-32*", "<b>B</b>", "Nach einem Neustart der Kasse werden alle Einfahrten "
                                       "abgewiesen, bis jemand die Kassen-Seite öffnet",
         "behoben (K-2)"],
    ], [1.3 * cm, 1.7 * cm, 1.4 * cm, 9.9 * cm, 2.7 * cm],
        extra=[("BACKGROUND", (2, 1), (2, 1), ERR_BG), ("BACKGROUND", (2, 2), (2, 2), WARN_BG),
               ("BACKGROUND", (4, 1), (4, -1), OK_BG)]))

    def defect(nr, title, rows):
        return KeepTogether([h2(f"{nr}: {title}"), kv_table(rows, (3.6 * cm, 13.4 * cm)),
                             Spacer(1, 4)])

    story.append(defect("F-01", "Ausfahrt ohne Nachzahlung (Klasse A)", [
        ("Testfall", "TF-31 (neu), Durchlauf 1 am " + RUN1["start"][:10] + ", Version 1.0"),
        ("Schritte", "1. Schranken-Oberfläche: ZG 7 einfahren (TF-09).<br/>"
                     "2. Kasse: ZG 7 abfragen und CHF 2.00 bezahlen (Parkdauer 0 Min.).<br/>"
                     "3. Kasse, Testfeld: Kennzeichen ZG 7, 61 Minuten setzen.<br/>"
                     "4. Kasse: ZG 7 abfragen.<br/>"
                     "5. Schranken-Oberfläche: ZG 7 als Ausfahrt eingeben."),
        ("Erwartet", "Schritt 4 zeigt offen CHF 2.00, in Schritt 5 bleibt die Schranke zu und die "
                     "Meldung nennt die Nachzahlung von CHF 2.00."),
        ("Tatsächlich", observed(RUN1, "TF-31")),
        ("Ursache", "Der Bezahlstatus war ein einzelnes Ja/Nein-Feld. Nach der Zahlung wurde die "
                    "weiterlaufende Parkdauer nicht mehr berücksichtigt."),
        ("Auswirkung", "Einnahmenverlust: Wer direkt nach der Einfahrt CHF 2.00 bezahlt, kann "
                       "beliebig lange parkieren. Verletzt Testziel 3 und 4."),
        ("Browser", "Nicht relevant, der Fehler liegt in der Firmware der Kasse."),
    ]))
    story.append(defect("F-02", "Keine Einfahrt nach Neustart der Kasse (Klasse B)", [
        ("Testfall", "TF-32 (neu), Durchlauf 1 am " + RUN1["start"][:10] + ", Version 1.0"),
        ("Schritte", "1. Kassen-Pico vom Strom trennen und wieder anschliessen.<br/>"
                     "2. Kassen-Seite <b>nicht</b> öffnen.<br/>"
                     "3. Schranken-Oberfläche: AG 123456 als Einfahrt eingeben."),
        ("Erwartet", "Einfahrtsschranke öffnet, AG 123456 wird mit korrekter Einfahrtszeit "
                     "registriert."),
        ("Tatsächlich", observed(RUN1, "TF-32")),
        ("Ursache", "Der Kassen-Pico hat keine batteriegepufferte Uhr und im WLAN «Parksystem» "
                    "kein Internet für NTP. Die Uhrzeit wurde nur beim Öffnen der Kassen-Seite "
                    "vom Browser übernommen."),
        ("Umweg", "Kassen-Seite einmal öffnen, danach funktioniert die Einfahrt. Darum Klasse B "
                  "und nicht A: Kein Fahrzeug fährt gratis aus, es entsteht kein Eintrag ohne "
                  "Einfahrtszeit."),
        ("Auswirkung", "Nach jedem Stromunterbruch staut sich die Einfahrt, bis das Personal "
                       "eingreift."),
    ]))

    story.append(h2("4.2 Beobachtungen ohne Fehlerklasse"))
    story += bullets([
        "<b>B-01 (TF-30): Datenverlust bei Neustart der Kasse.</b> Nach dem Neustart ist die "
        "Fahrzeugliste leer und LU 4711 unbekannt. Fahrzeuge, die noch im Parkhaus stehen (im Test "
        "AG 123456 und ZH 999999), werden bei der Ausfahrt als «unbekannt» abgewiesen und kommen "
        "nur mit Hilfe des Personals hinaus. Das Testkonzept schliesst die Wiederherstellung der "
        "Daten ausdrücklich aus und definiert für TF-30 kein Sollverhalten. Daher gibt es keine "
        "Fehlerklasse, das Restrisiko steht aber im Fazit.",
        "<b>B-02 (TF-28, TF-29): Verbindungsfehler.</b> Die Meldung erscheint nach 2.0 s "
        "(Timeout 2000 ms), also deutlich innerhalb der verlangten 10 s. Sie hat einen eigenen Code "
        "(CONNECTION) und eine eigene gelbe Darstellung mit Warnsymbol. Bei offener Gebühr "
        "erscheinen dagegen Code UNPAID und eine rote Darstellung.",
        "<b>B-03 (TF-13): Grenzwert 60 Minuten.</b> Der Wert ist nur korrekt prüfbar, wenn die "
        "Abfrage innerhalb einer Minute nach dem Setzen erfolgt. Sonst ist die 61. Minute "
        "erreicht und CHF 4.00 ist richtig. Im Test wurde nach 5 s abgefragt.",
    ])

    story.append(h2("4.3 Anpassungen am Testkonzept"))
    story.append(p("Bei der Durchführung zeigte sich, dass im Testkonzept Fälle fehlen oder "
                   "ungenau sind. Folgende Änderungen wurden ins Testkonzept übernommen:"))
    story.append(table([
        ["Nr.", "Anpassung", "Grund"],
        ["T-1", "<b>TF-31* neu</b> (US-5, Ziel 4): Zahlung bei 0 Min., Testfeld 61 Min., Ausfahrt "
                "muss abgewiesen werden; nach der Nachzahlung öffnet die Schranke.",
         "Kein Testfall prüfte, ob eine Zahlung auch für die später angebrochenen Stunden gilt. "
         "Ziel 4 hat laut Konzept den höchsten Stellenwert."],
        ["T-2", "<b>TF-32* neu</b> (US-1, US-7, Ziel 1): Einfahrt nach Neustart der Kasse, ohne die "
                "Kassen-Seite zu öffnen.",
         "TF-30 dokumentiert nur den Datenverlust. Ob der Betrieb danach ohne Eingriff weiterläuft, "
         "war nicht geprüft."],
        ["T-3", "TF-13 präzisiert: «innerhalb von 60 s nach dem Setzen abfragen».",
         "Grenzwert sonst nicht eindeutig (B-03)"],
        ["T-4", "Testumgebung «Uhrzeit»: Statt NTP übernimmt die Kasse die Uhrzeit vom Browser. Am "
                "Testtag gilt Winterzeit (UTC+1).",
         "Das WLAN «Parksystem» hat keinen Internetzugang, NTP ist nicht möglich."],
        ["T-5", "Durchlauf 1 und 2 automatisiert mit tests/test_system.cpp (gleiche Schritte, "
                "Eingaben und Reihenfolge), Stoppuhr durch Zeitmessung im Test ersetzt. Die "
                "manuelle Durchführung auf den Picos folgt im Abnahmedurchlauf (Anhang B).",
         "Wiederholbarer Regressionstest nach jeder Korrektur, wie im Konzept verlangt"],
    ], [1.1 * cm, 8.7 * cm, 7.2 * cm]))

    # 5 Erweiterungen / Anpassungen
    story.append(h1("5 Korrekturen, Retest und Regressionstest"))
    story.append(h2("5.1 Korrekturen"))
    story.append(table([
        ["Nr.", "Fehler", "Änderung", "Dateien"],
        ["K-1", "F-01", "Statt eines Ja/Nein-Status speichert die Kasse den bezahlten Betrag. "
                        "Offen ist die Gebühr der aktuellen Parkdauer minus den bezahlten Betrag; "
                        "«bezahlt» heisst offen = CHF 0.00. Eine Zahlung begleicht den offenen "
                        "Betrag, die Ausfahrtsmeldung nennt die Nachzahlung. Die bezahlte Stunde "
                        "gilt bis zu ihrem Ende, das wirkt als Karenzzeit für den Weg zur Ausfahrt.",
         "kasse/parking_logic.h<br/>kasse/kasse.ino<br/>kasse/kasse_page.h<br/>tests/test_logic.cpp"],
        ["K-2", "F-02", "Die Schranken-Oberfläche sendet bei jeder Ein- und Ausfahrt die Uhrzeit des "
                        "Browsers mit, der Schranken-Pico leitet sie an die Kasse weiter. Die Kasse "
                        "übernimmt sie <b>nur</b>, wenn ihre eigene Uhr noch nicht gestellt ist. "
                        "Eine laufende Uhr kann so nicht verstellt werden.",
         "schranken/schranken_page.h<br/>schranken/schranken.ino<br/>kasse/kasse.ino"],
    ], [1.1 * cm, 1.3 * cm, 10.0 * cm, 4.6 * cm]))

    story.append(h2("5.2 Retest der fehlgeschlagenen Testfälle"))
    rows = [["ID", "D1", "Tatsächliches Ergebnis im Retest (Durchlauf 2, Version 1.1)", "D2"]]
    extra = []
    for r, tid in enumerate(["TF-31", "TF-32"], start=1):
        rows.append([f"<b>{tid}*</b>", status_cell(status(RUN1, tid)), observed(RUN2, tid),
                     status_cell(status(RUN2, tid))])
        extra += [status_bg(r, 1, status(RUN1, tid)), status_bg(r, 3, status(RUN2, tid))]
    story.append(table(rows, [1.5 * cm, 1.0 * cm, 13.5 * cm, 1.0 * cm], style=SMALL, extra=extra))

    story.append(h2("5.3 Regressionsanalyse"))
    story.append(p("Welche bestehenden Funktionen können durch die Korrekturen beeinflusst sein?"))
    story.append(table([
        ["Korrektur", "Möglicherweise betroffen", "Testfälle"],
        ["K-1", "Gebührenanzeige an Kasse und Automat, Zahlung, Sperre gegen Doppelzahlung, "
                "Ausfahrt mit und ohne Zahlung, Status in der Fahrzeugliste, neuer Aufenthalt "
                "nach Ausfahrt", "TF-12 bis TF-27, TF-31"],
        ["K-2", "Einfahrt (Prüfreihenfolge Kennzeichen vor Uhrzeit), Weiterleitung an die Kasse "
                "bei Ein- und Ausfahrt, Verbindungsfehler, Fahrzeugliste", "TF-01 bis TF-11, "
                                                                           "TF-22 bis TF-29, TF-32"],
    ], [2.0 * cm, 10.4 * cm, 4.6 * cm]))
    story.append(p("Zusammen sind damit fast alle Testfälle betroffen. Das Testkonzept verlangt "
                   "nach jeder Korrektur ohnehin den vollständigen Durchlauf ab TF-01, weil einzelne "
                   "Fälle auf den Vorbedingungen früherer Fälle beruhen. Deshalb wurde als "
                   "Regressionstest der ganze Durchlauf 2 ausgeführt, ergänzt um die Unit-Tests "
                   "und den Build für den Pico W."))

    regressions = [t for t in TEST_IDS if status(RUN1, t) == "B" and status(RUN2, t) != "B"]
    story.append(KeepTogether([h2("5.4 Ergebnis des Regressionstests"), table([
        ["Prüfung", "Ergebnis"],
        [f"Durchlauf 2, TF-01 bis TF-32 ({n} Testfälle)",
         f"{r2_pass} von {n} bestanden, {len(regressions)} Regressionen. Bis auf Zeitstempel "
         "identische Ergebnisse wie in Durchlauf 1, ausser TF-31 und TF-32 (jetzt bestanden)."],
        ["Unit-Tests tests/test_logic.cpp", "Alle bestanden, inkl. 6 neuer Prüfungen für die "
                                            "Nachzahlung (0, 60, 61, 121 Min. mit und ohne Zahlung)"],
        ["Build für Raspberry Pi Pico W", "Kasse und Schranken kompilieren ohne Fehler"],
    ], [6.0 * cm, 11.0 * cm], extra=[("BACKGROUND", (1, 1), (1, -1), OK_BG)])]))

    # 6 Fazit und Genehmigung
    story.append(h1("6 Fazit und Genehmigung"))
    story.append(h2("6.1 Abnahmekriterien"))
    story.append(table([
        ["Abnahmekriterium aus dem Testkonzept", "Version 1.0", "Version 1.1"],
        ["Alle Sternfälle bestanden", f"nein ({r1_star}/{len(star_ids)})",
         f"ja ({r2_star}/{len(star_ids)})"],
        ["Kein Fehler der Klasse A offen", "nein (F-01)", "ja"],
        ["Höchstens zwei Fehler der Klasse B offen und beschrieben", "ja (F-02)", "ja (keiner offen)"],
        ["Letzter vollständiger Durchlauf vom unabhängigen Tester", "nein", "offen, geplant an der "
                                                                            "Präsentation"],
    ], [8.6 * cm, 4.2 * cm, 4.2 * cm], extra=[
        ("BACKGROUND", (1, 1), (1, 2), ERR_BG), ("BACKGROUND", (1, 3), (1, 3), OK_BG),
        ("BACKGROUND", (1, 4), (1, 4), ERR_BG), ("BACKGROUND", (2, 1), (2, 3), OK_BG),
        ("BACKGROUND", (2, 4), (2, 4), WARN_BG)]))

    story.append(h2("6.2 Fazit"))
    story.append(p("<b>Version 1.0</b> darf nicht in Betrieb genommen werden. Der Fehler F-01 "
                   "(Klasse A) führt direkt zu Einnahmenverlusten."))
    story.append(p("<b>Version 1.1</b> erfüllt alle fachlichen Anforderungen. Alle Testfälle sind "
                   "bestanden: Fahrzeuge werden korrekt registriert, ungültige Kennzeichen "
                   "abgewiesen und die Gebühr korrekt berechnet. Die Ausfahrt öffnet nur, wenn die "
                   "Gebühr vollständig bezahlt ist. Fällt die Kasse aus, bleiben beide "
                   "Schranken geschlossen. Es ist kein Fehler offen."))
    story.append(callout([
        p("<b>Empfehlung: Inbetriebnahme von Version 1.1 freigeben, mit folgenden Auflagen</b>",
          ParagraphStyle("rec", parent=BASE, textColor=NAVY)),
        *bullets([
            "<b>Auflage 1:</b> Vor dem Betrieb führt ein unabhängiger Tester den Abnahmedurchlauf "
            "TF-01 bis TF-32 auf den beiden Picos aus (Anhang B). Er darf keinen Fehler der "
            "Klasse A und höchstens zwei der Klasse B ergeben. Damit ist auch Abnahmekriterium 4 "
            "erfüllt.",
            "<b>Auflage 2:</b> Den Kassen-Pico an eine unterbrechungsfreie Stromversorgung "
            "anschliessen. Für Fahrzeuge, die nach einem Ausfall unbekannt sind, braucht es einen "
            "Notfallablauf mit Personal (B-01).",
        ]),
    ], colors.HexColor("#eef3fa"), NAVY))
    story.append(Spacer(1, 6))
    story.append(p("<b>Restrisiken, die der Betreiber kennen muss</b> (laut Testkonzept nicht Teil "
                   "der Tests):"))
    story += bullets([
        "Die Parkdaten liegen nur im Arbeitsspeicher und gehen bei einem Neustart der Kasse "
        "verloren (B-01).",
        "Es gibt keinen Zugriffsschutz: Wer das WLAN-Passwort kennt, kann Fahrzeuge als bezahlt "
        "markieren. Das Standardpasswort «parksystem» muss vor dem Betrieb geändert werden.",
        "Echte Zahlungsmittel und automatische Kennzeichenerkennung sind nicht angebunden. Die "
        "Kamera wird über die Weboberfläche simuliert.",
    ])

    box = "<font name='ArialUni'>☐</font>"
    sign = [
        ["Rolle", "Name", "Datum", "Unterschrift"],
        ["Tester / Entwickler", "Leonardo Ciccone", "", ""],
        ["Protokoll / Entwickler", "Nevio Carcangiu", "", ""],
        ["Unabhängiger Tester", "", "", ""],
        ["Auftraggeber (Parkhausbesitzer)", "", "", ""],
    ]
    story.append(KeepTogether([
        h2("6.3 Genehmigung"),
        p("<b>Entscheid des Auftraggebers zu Version 1.1:</b>"),
        p(f"{box} Inbetriebnahme freigegeben &nbsp;&nbsp;&nbsp; {box} Freigegeben mit "
          f"Auflagen (6.2) &nbsp;&nbsp;&nbsp; {box} Nicht freigegeben"),
        table(sign, [5.0 * cm, 4.4 * cm, 2.6 * cm, 5.0 * cm], extra=[
            ("TOPPADDING", (0, 1), (-1, -1), 9), ("BOTTOMPADDING", (0, 1), (-1, -1), 9)]),
    ]))

    # Anhang
    story.append(PageBreak())
    story.append(h1("Anhang A: Testdaten"))
    story.append(table([
        ["Kennzeichen", "Verwendung"],
        ["AG 123456", "Normalfall: fährt ein, bezahlt, fährt aus, fährt erneut ein (TF-01 bis TF-25, TF-32)"],
        ["LU 4711", "Zweites Fahrzeug, am Automaten bezahlt, für den Verbindungsausfall (TF-04, TF-21, TF-28, TF-30)"],
        ["ZG 7", "Unterer Grenzwert, Gebühr bei 0 Minuten, Nachzahlung (TF-09, TF-16, TF-31)"],
        ["ZH 999999", "Fährt ein, bezahlt nicht, wird bei der Ausfahrt abgewiesen (TF-26)"],
        ["SG 55555", "Nie eingefahren: unbekanntes Kennzeichen (TF-17, TF-27, TF-29)"],
        ["123456 / A 123456 / AG 1234567 / leer", "Ungültige Eingaben (TF-05 bis TF-08)"],
        ["␣␣ag␣␣␣123456␣", "Kleinbuchstaben mit Leerzeichen, ␣ = Leerzeichen (TF-10)"],
    ], [5.2 * cm, 11.8 * cm]))
    story.append(Spacer(1, 6))
    story.append(p("Die Testdaten stammen unverändert aus dem Testkonzept. Die Rohdaten beider "
                   "Durchläufe liegen im Repository unter <i>docs/testprotokoll/durchlauf1.json</i> "
                   "und <i>durchlauf2.json</i>. Sie lassen sich jederzeit neu erzeugen:"))
    story.append(p("<font name='Courier' size='8.5'>c++ -std=c++17 -w -Itests/arduino_mock -o "
                   "test_system tests/test_system.cpp &amp;&amp; ./test_system</font>"))

    story.append(h1("Anhang B: Abnahmedurchlauf auf den Picos"))
    story.append(p("Vom unabhängigen Tester während der Präsentation auszufüllen. Durchgeführt wird "
                   "der vollständige Durchlauf TF-01 bis TF-32 nach Testkonzept. Die folgenden "
                   "Punkte hängen von der Hardware ab und sind besonders zu beachten:"))
    hw = [
        ["ID", "Prüfpunkt auf der Hardware", "Ergebnis", "Visum"],
        ["EK", "Picos über 192.168.42.1 / .2 erreichbar, «Uhrzeit Kasse» = Notebook-Uhr", "", ""],
        ["TF-01, 09", "Servo der Einfahrt dreht auf, LED GP14 leuchtet", "", ""],
        ["TF-02, 23", "Schranke schliesst laut Stoppuhr innerhalb von 10 s (Soll: 5 s)", "", ""],
        ["TF-22", "Servo der Ausfahrt dreht auf, LED GP15 leuchtet", "", ""],
        ["TF-28, 29", "Kassen-Pico am USB-Kabel getrennt: gelbe Meldung «Verbindungsfehler» "
                      "innerhalb von 10 s, Schranken bleiben zu", "", ""],
        ["TF-30, 32", "Kassen-Pico wieder eingesteckt: Daten weg; Einfahrt über die "
                      "Schranken-Seite funktioniert ohne Öffnen der Kassen-Seite", "", ""],
        ["Total", "Anzahl bestanden / fehlgeschlagen, Fehlerklassen", "", ""],
    ]
    story.append(table(hw, [2.1 * cm, 9.4 * cm, 3.0 * cm, 2.5 * cm], extra=[
        ("TOPPADDING", (0, 1), (-1, -1), 7), ("BOTTOMPADDING", (0, 1), (-1, -1), 7)]))
    story.append(Spacer(1, 6))
    story.append(p("Datum: ____________________ &nbsp;&nbsp; Unabhängiger Tester: "
                   "______________________________"))

    doc = Doc(OUT)
    doc.multiBuild(story)
    print("geschrieben:", OUT)


if __name__ == "__main__":
    build()
