# shake-cursor
Shake your mouse to find the cursor in Ubuntu

Eng: This application runs as a service in Ubuntu 24 and temporarily increases the size of the mouse cursor for easy detection (you just need to move the mouse left and right).

How it use

1. Build:
   sudo apt update
   sudo apt install build-essential pkg-config libglib2.0-dev libevdev-dev
   make
3. Install as user-service: make install
4. Check status: systemctl --user status shake-cursor.service
5. Remove: make uninstall

Rus: Это приложение запускается как сервис в Убунту 24 и временно увеличивает размер курсора мыши для его простого обнаружения (вам просто нужно помотать мышью вправо-влево).

Как пользоваться

1. Сборка:
   sudo apt update
   sudo apt install build-essential pkg-config libglib2.0-dev libevdev-dev
   make
3. Установка + автозапуск как user-сервис: make install
4. Проверка статуса: systemctl --user status shake-cursor.service
5. Удаление: make uninstall
