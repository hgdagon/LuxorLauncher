mkdir /s /q staging
mkdir staging

copy "build\windows\x86\release\LuxorLauncher.exe" "staging\Luxor.exe"
copy "build\windows\x86\release\LuxorLauncher.exe" "staging\Luxor AR.exe"
copy "build\windows\x86\release\LuxorLauncher.exe" "staging\Luxor2.exe"
copy "build\windows\x86\release\LuxorLauncher.exe" "staging\Luxor3.exe"
copy "build\windows\x86\release\LuxorLauncher.exe" "staging\LUXOR - Quest for the Afterlife.exe"
copy "build\windows\x86\release\LuxorLauncher.exe" "staging\LUXOR - 5th Passage.exe"
copy "build\windows\x86\release\LuxorLauncher.exe" "staging\LUXOR - Mahjong.exe"
copy "build\windows\x86\release\LuxorLauncher.pdb" "staging\launcher.pdb"