@echo off
chcp 65001 > nul

REM 실행 중인 서버/클라이언트를 모두 종료한다.
REM 창을 하나씩 닫는 것보다 빠르고, 좀비 프로세스가 남지 않는다.

taskkill /IM TPSGameServer.exe /F > nul 2>&1
taskkill /IM TPSGame.exe /F > nul 2>&1

echo 종료 완료.
timeout /t 1 /nobreak > nul
