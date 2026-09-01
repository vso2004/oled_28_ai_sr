from g2p_en import G2p
# import argparse
import numpy as np
import nltk
# import pandas
from pathlib import Path


def spit_number_text(textin):
    char_to_find = " "
    pos = textin.find(char_to_find)
    if pos != -1:
        # Разделяем строку на две части (символ-разделитель исключен)
        part1 = textin[:pos]
        part2 = textin[pos+1:]
        return part1, part2
    else:
        return "", ""


def english_g2p(text, alphabet=None):
    g2p = G2p()
    out = ""

    if alphabet is None:
        alphabet = {"AE1": "a", "N": "N", " ": " ", "OW1": "b", "V": "V", "AH0": "c", "L": "L", "F": "F", "EY1": "d", "S": "S", "B": "B", "R": "R", "AO1": "e", "D": "D", "AH1": "c", "EH1": "f", "OW0": "b", "IH0": "g", "G": "G", "HH": "h", "K": "K", "IH1": "g", "W": "W", "AY1": "i", "T": "T", "M": "M", "Z": "Z", "DH": "j", "ER0": "k", "P": "P", "NG": "l", "IY1": "m", "AA1": "n", "Y": "Y", "UW1": "o", "IY0": "m",
                    "EH2": "f", "CH": "p", "AE0": "a", "JH": "q", "ZH": "r", "AA2": "n", "SH": "s", "AW1": "t", "OY1": "u", "AW2": "t", "IH2": "g", "AE2": "a", "EY2": "d", "ER1": "k", "TH": "v", "UH1": "w", "UW2": "o", "OW2": "b", "AY2": "i", "UW0": "o", "AH2": "c", "EH0": "f", "AW0": "t", "AO2": "e", "AO0": "e", "UH0": "w", "UH2": "w", "AA0": "n", "AY0": "i", "IY2": "m", "EY0": "d", "ER2": "k", "OY2": "u", "OY0": "u"}

    text_list = text.split(";")
    for item in text_list:
        item = item.split(",")
        for phrase in item:
            labels = g2p(phrase)
            for char in labels:
                if char not in alphabet:
                    print("skip %s, not found in alphabet")
                    continue
                else:
                    out += alphabet[char]
            if phrase != item[-1]:
                out += ','
    return text, out

# example : CONFIG_EN_SPEECH_COMMAND_ID0="LfFT PaNcL"


def gogogo():

    cwd_path = Path.cwd()
    file_in_abs = str(cwd_path) + '/' + "commands_en_in.txt"
    file_out_abs = str(cwd_path) + '/' + "commands_en_v5.txt"
    Path(file_out_abs).unlink(missing_ok=True)
    Path(file_out_abs).touch()

    with open(file_in_abs, 'r', encoding='utf-8') as file:
        for line in file:
          
            cleaned_line = line.strip()
            part1, part2 = spit_number_text(cleaned_line)
          
            ret = english_g2p(part2)
            uppercase_text = str(ret[0]).upper()
            out_string_7 = part1 + "," + ret[0] + "," + ret[1] + '\n'
            out_string_5 = 'CONFIG_EN_SPEECH_COMMAND_ID'+ str(part1) + '="' + str(ret[1]) + '"' + '\n'
            out_string = out_string_5
            print(out_string)

            with open(file_out_abs, "a") as f:
                f.write(out_string)
            f.close()



if __name__ == "__main__":
   
    gogogo()
    exit(0)

    index = 0
    print("\n")
    for item in voice_command_array:
        ret = english_g2p(item)
        uppercase_text = str(ret[0]).upper()
        print(f'{index+45},{uppercase_text},{str(ret[1])}                  ')
        index += 1

