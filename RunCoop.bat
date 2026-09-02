@echo off
chcp 65001 > nul
setlocal

REM ============================================================
REM  TPSGame 코옵 테스트 실행
REM    데디케이티드 서버 1 + 클라이언트 N
REM
REM  사용법:
REM    RunCoop.bat        클라 2개 (기본)
REM    RunCoop.bat 1      클라 1개 (netprofile 측정용)
REM    RunCoop.bat 4      클라 4개
REM ============================================================

set SERVER_EXE=D:\Ex\TPSGame_Package_Server\WindowsServer\TPSGameServer.exe
set CLIENT_EXE=D:\Ex\TPSGame_Package\Windows\TPSGame.exe

set MAP=ThirdPersonMap
set PORT=7777
set RESX=1280
set RESY=720

REM 클라 수 (인자 없으면 2)
set CLIENTS=%1
if "%CLIENTS%"=="" set CLIENTS=2

REM ---------- 경로 확인 ----------
if not exist "%SERVER_EXE%" (
    echo [오류] 서버 실행파일이 없습니다:
    echo        %SERVER_EXE%
    echo        서버 패키징이 끝났는지 확인하세요.
    pause
    exit /b 1
)
if not exist "%CLIENT_EXE%" (
    echo [오류] 클라이언트 실행파일이 없습니다:
    echo        %CLIENT_EXE%
    pause
    exit /b 1
)

REM ---------- 서버 ----------
echo [서버] %MAP% 로드, 포트 %PORT%
start "TPSGame Server" "%SERVER_EXE%" %MAP% -log -port=%PORT%

REM 서버가 맵을 로드할 시간. 느리면 늘릴 것.
timeout /t 5 /nobreak > nul

REM ---------- 클라이언트 ----------
setlocal enabledelayedexpansion
for /l %%i in (1,1,%CLIENTS%) do (
    echo [클라 %%i] 127.0.0.1:%PORT% 접속

    REM 창이 겹치지 않도록 계단식 배치
    set /a WINX=%%i*60
    set /a WINY=%%i*60

    start "TPSGame Client %%i" "%CLIENT_EXE%" 127.0.0.1:%PORT% ^
        -game -log -WINDOWED -ResX=%RESX% -ResY=%RESY% -WinX=!WINX! -WinY=!WINY!

    timeout /t 2 /nobreak > nul
)
endlocal

echo.
echo ============================================================
echo  서버 1 + 클라 %CLIENTS% 실행됨
echo.
echo  클라 콘솔(~)에서 쓸 명령:
echo    NetEmulation.PktLag 300        지연 걸기 (편도 ms)
echo    TPS.LagCompensation.Debug 1    랙 보상 A/B 측정 로그
echo    TPS.LagCompensation 0          랙 보상 끄기 (전/후 비교)
echo.
echo  서버 콘솔에서 쓸 명령:
echo    netprofile enable / disable    대역폭 측정
echo    Log TPSLog Verbose             상세 로그
echo.
echo  종료: StopCoop.bat
echo ============================================================
pause
