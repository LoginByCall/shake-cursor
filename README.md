# shake-cursor
Shake your mouse to find the cursor in Ubuntu

Eng: This application runs as a service in Ubuntu 24 and temporarily increases the size of the mouse cursor for easy detection (you just need to move the mouse left and right).

How it use

1. Build: make
2. Install as user-service: make install
3. Check status: systemctl --user status shake-cursor.service
4. Remove: make uninstall

Rus: Это приложение запускается как сервис в Убунту 24 и временно увеличивает размер курсора мыши для его простого обнаружения (вам просто нужно помотать мышью вправо-влево).

Как пользоваться

1. Сборка: make
2. Установка + автозапуск как user-сервис: make install
3. Проверка статуса: systemctl --user status shake-cursor.service
4. Удаление: make uninstall
5. 
