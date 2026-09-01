 python -m venv .venv
 ./.venv\Scripts\Activate.ps1
 #pip install --no-cache-dir nltk
 #pip install --no-cache-dir g2p-en
 python ./multinet_g2p_lite.py

$currentfileaddr ="commands_en.txt"
$destfileaddr = "../components/espressif__esp-sr\model\multinet_model\fst\commands_en.txt"
Copy-Item -Path $currentfileaddr -Destination $destfileaddr -Force

#$null = [System.Console]::ReadKey($true)
#exit
