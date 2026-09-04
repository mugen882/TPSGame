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
set CLIENT_EXE=D:\Ex\TPSGame_Package_Client\Windows\TPSGame.exe

set MAP=ThirdPersonMap
set PORT=7777
set RESX=1280
set RESY=720

REM ------------------------------------------------------------
REM  서버 콘솔 명령
REM
REM  데디케이티드 서버에는 게임 콘솔(~)이 없다. 뷰포트가 없기 때문이다.
REM  랙 보상 디버그처럼 서버에서 실행되어야 하는 명령은 여기에 넣는다.
REM  여러 개면 쉼표로 구분한다.
REM
REM  예) set SERVER_CMDS=-ExecCmds="TPS.LagCompensation.Debug 1"
REM ------------------------------------------------------------
set SERVER_CMDS=

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
start "TPSGame Server" "%SERVER_EXE%" %MAP% -log -port=%PORT% %SERVER_CMDS%

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
echo  클라 게임 콘솔(~):
echo    NetEmulation.PktLag 300        지연 걸기 (편도 ms)
echo    showdebug abilitysystem        어트리뷰트 / 태그 확인
echo.
echo  서버 (-log 창 입력란 또는 위의 SERVER_CMDS):
echo    TPS.LagCompensation 0 / 1      랙 보상 토글
echo    TPS.LagCompensation.Debug 1    A/B 측정 로그
echo    netprofile enable / disable    대역폭 측정
echo.
echo  종료: StopCoop.bat
echo ============================================================
pause
