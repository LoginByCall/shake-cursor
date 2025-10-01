# Shake Cursor

**EN:** Shake the mouse or touchpad left–right quickly to temporarily enlarge the cursor (like on macOS).  
**ES:** Mueve rápidamente el ratón o el touchpad de izquierda a derecha para agrandar temporalmente el cursor (como en macOS).  
**RU:** Быстро потрясите мышкой или тачпадом «влево–вправо», чтобы временно увеличить курсор (как в macOS).

---

## Features / Características / Особенности

- System tray icon with settings menu  
- Works with both mouse (REL_X) and touchpad (ABS_X)  
- Adjustable parameters:
  - Max cursor size
  - Duration
  - Sensitivity thresholds
  - Flip count
  - Polling interval

---

## Installation / Instalación / Установка

### Dependencies / Dependencias / Зависимости

```bash
sudo apt update
sudo apt install -y build-essential pkg-config \
  libevdev-dev libglib2.0-dev libgtk-3-dev \
  libayatana-appindicator3-dev
```

Build & Install / Compilar e instalar / Сборка и установка
``` bash
make
make install
```

After installation, the service will run automatically under your user session.
Después de la instalación, el servicio se ejecutará automáticamente en tu sesión de usuario.
После установки сервис будет автоматически запускаться в вашей пользовательской сессии.

Usage / Uso / Использование

* Shake the mouse/touchpad → cursor enlarges for N seconds.
* Tray icon → right-click → "Preferences…" to configure parameters.
* Config file: ~/.config/shake-cursor/config.ini
* To stop:

```bash
make uninstall
```

License / Licencia / Лицензия

MIT
