"""
Enregistreur CSV pour propulseur_distance.ino
Usage :  python enregistreur_csv.py COM3        (Windows)
         python enregistreur_csv.py /dev/ttyACM0 (Linux)
Dépendance : pip install pyserial
Chaque START crée un nouveau fichier essai_AAAAMMJJ_HHMMSS.csv
Ferme le moniteur série de l'IDE Arduino avant de lancer.
"""
import os
import sys
from datetime import datetime

import serial

port = sys.argv[1] if len(sys.argv) > 1 else "COM3"
ser = serial.Serial(port, 115200, timeout=1)
print(f"Connecté sur {port}. Ctrl+C pour quitter.")

fichier = None
try:
    while True:
        ligne = ser.readline().decode(errors="ignore").strip()
        if not ligne:
            continue
        if ligne.startswith("#START"):
            nom = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                               datetime.now().strftime("essai_%Y%m%d_%H%M%S.csv"))
            fichier = open(nom, "w", newline="")
            print(f">>> START — enregistrement dans {nom}")
        elif ligne.startswith("#STOP"):
            if fichier:
                fichier.close()
                fichier = None
            print(f">>> {ligne}")
        elif ligne.startswith("#"):
            print(ligne)
        elif fichier:
            fichier.write(ligne + "\n")
            fichier.flush()
            print(ligne)
except KeyboardInterrupt:
    pass
finally:
    if fichier:
        fichier.close()
    ser.close()
