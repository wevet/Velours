@echo off
setlocal

REM バッチファイルがある場所から、プロジェクトルート(1つ上)を定義
set "PROJ_DIR=%~dp0.."
set "UPROJECT_PATH=%PROJ_DIR%\Velours.uproject"

REM エンジンの RunUAT パス
set "RUN_UAT=C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat"

REM 実行 (引数の前後の余計なスペースを削除しています)
call "%RUN_UAT%" BuildCookRun ^
 -project="%UPROJECT_PATH%" ^
 -target="Velours" ^
 -platform=Win64 ^
 -clientconfig=Shipping ^
 -cook -stage -pak -iostore -compressed -build ^
 -package -archive -archivedirectory="%PROJ_DIR%\Builds" ^
 -utf8output -CrashForUAT -iterate

pause

