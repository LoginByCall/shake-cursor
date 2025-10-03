````markdown
<p align="center">
  <img src="https://img.shields.io/badge/Ubuntu-24.04%2B-brightgreen" alt="Ubuntu 24.04+">
  <img src="https://img.shields.io/badge/Wayland%2FX11-supported-blue" alt="Wayland/X11">
  <img src="https://img.shields.io/badge/license-MIT-lightgrey" alt="License">
</p>

# Shake Cursor

**EN:** Shake the mouse or touchpad left–right to temporarily enlarge the cursor (like on macOS).  
**ES:** Mueve rápidamente el ratón o el touchpad de izquierda a derecha para agrandar temporalmente el cursor (como en macOS).  
**RU:** Быстро потрясите мышью или тачпадом «влево–вправо», чтобы временно увеличить курсор (как в macOS).

---

## Table of Contents
- [EN — Features](#en--features)
- [EN — Install from source](#en--install-from-source)
- [EN — Install .deb](#en--install-deb)
- [EN — Configuration](#en--configuration)
- [ES — Características](#es--características)
- [ES — Instalación desde el código fuente](#es--instalación-desde-el-código-fuente)
- [ES — Instalar .deb](#es--instalar-deb)
- [ES — Configuración](#es--configuración)
- [RU — Особенности](#ru--особенности)
- [RU — Установка из исходников](#ru--установка-из-исходников)
- [RU — Установка из .deb](#ru--установка-из-deb)
- [RU — Настройка](#ru--настройка)
- [License / Licencia / Лицензия](#license--licencia--лицензия)
- [Troubleshooting](#troubleshooting)

---

## EN — Features

- Works with mouse (**REL\_X**) and touchpad (**ABS\_X**)
- Lightweight: epoll/GIO loop, minimal CPU/RAM
- User tray icon (Ayatana AppIndicator) with Preferences
- Configurable:
  - Max cursor size (`BIG_CURSOR_SIZE`)
  - Hold duration (`ENLARGE_DURATION_S`)
  - Sensitivity thresholds for mouse/touchpad (`MIN_DX`, `MIN_DX_ABS`)
  - Direction flip interval (`MAX_INTERVAL_S`) and count (`REQUIRED_FLIPS`)
  - Poll interval (`TICK_MS`)
- Multi-monitor aware; forces cursor reload on all heads

### EN — Install from source

**Dependencies**
```bash
sudo apt update
sudo apt install -y build-essential pkg-config \
  libevdev-dev libglib2.0-dev libgtk-3-dev libayatana-appindicator3-dev
````

**Build & run**

```bash
make
./shake-cursor
```

**Install as user service**

```bash
make install
systemctl --user status shake-cursor.service
```

## EN — Install .deb

```bash
sudo dpkg -i shake-cursor_1.0-1_amd64.deb
systemctl --user enable --now shake-cursor.service
```

## EN — Configuration

* Tray → **Preferences…**
* Config file: `~/.config/shake-cursor/config.ini`

## EN - Build .deb / Compilar .deb / Сборка .deb

**From the project root (where the `debian/` folder lives):**

```bash
dpkg-buildpackage -b -us -uc
```

**Inspect the output:**

```bash
dpkg -c ../shake-cursor_1.0-1_amd64.deb
dpkg -I ../shake-cursor_1.0-1_amd64.deb
```

**Install and enable the user service:**

```bash
sudo dpkg -i ../shake-cursor_1.0-1_amd64.deb
systemctl --user enable --now shake-cursor.service
```

**Uninstall:**

```bash
sudo apt remove shake-cursor
```

> Note: the build phase must not run `systemctl`. Service is enabled **after** installing the package, by the user.

---

## ES — Características

* Compatible con ratón (**REL_X**) y touchpad (**ABS_X**)
* Ligero: bucle epoll/GIO, uso mínimo de CPU/RAM
* Icono en la bandeja (Ayatana AppIndicator) con Preferencias
* Parámetros ajustables:

  * Tamaño máx. del cursor (`BIG_CURSOR_SIZE`)
  * Duración de retención (`ENLARGE_DURATION_S`)
  * Sensibilidad para ratón/touchpad (`MIN_DX`, `MIN_DX_ABS`)
  * Intervalo de cambio de dirección (`MAX_INTERVAL_S`) y número (`REQUIRED_FLIPS`)
  * Intervalo de sondeo (`TICK_MS`)
* Soporte multimonitor; fuerza recarga del cursor en todas las pantallas

### ES — Instalación desde el código fuente

```bash
sudo apt update
sudo apt install -y build-essential pkg-config \
  libevdev-dev libglib2.0-dev libgtk-3-dev libayatana-appindicator3-dev

make
./shake-cursor
```

**Servicio de usuario**

```bash
make install
systemctl --user status shake-cursor.service
```

## ES — Instalar .deb

```bash
sudo dpkg -i shake-cursor_1.0-1_amd64.deb
systemctl --user enable --now shake-cursor.service
```

## ES — Configuración

* Bandeja → **Preferencias…**
* Archivo: `~/.config/shake-cursor/config.ini`

## ES - Desde la raíz del proyecto (donde está la carpeta `debian/`):

```bash
dpkg-buildpackage -b -us -uc
```

**Verificar el contenido:**

```bash
dpkg -c ../shake-cursor_1.0-1_amd64.deb
dpkg -I ../shake-cursor_1.0-1_amd64.deb
```

**Instalar y habilitar el servicio de usuario:**

```bash
sudo dpkg -i ../shake-cursor_1.0-1_amd64.deb
systemctl --user enable --now shake-cursor.service
```

**Desinstalar:**

```bash
sudo apt remove shake-cursor
```

> Nota: durante la **compilación** no se debe ejecutar `systemctl`. Se habilita el servicio **después** de instalar el paquete.

---

## RU — Особенности

* Работает с мышью (**REL_X**) и тачпадом (**ABS_X**)
* Лёгкий демон: epoll/GIO, минимальные CPU/RAM
* Иконка в трее (Ayatana AppIndicator) и окно настроек
* Настраивается:

  * Максимальный размер курсора (`BIG_CURSOR_SIZE`)
  * Длительность удержания (`ENLARGE_DURATION_S`)
  * Чувствительность мышь/тачпад (`MIN_DX`, `MIN_DX_ABS`)
  * Интервал и число смен направления (`MAX_INTERVAL_S`, `REQUIRED_FLIPS`)
  * Частота проверки (`TICK_MS`)
* Корректно обновляет курсор на всех мониторах

### RU — Установка из исходников

```bash
sudo apt update
sudo apt install -y build-essential pkg-config \
  libevdev-dev libglib2.0-dev libgtk-3-dev libayatana-appindicator3-dev

make
./shake-cursor
```

**Поставить как user-service**

```bash
make install
systemctl --user status shake-cursor.service
```

## RU — Установка из .deb

```bash
sudo dpkg -i shake-cursor_1.0-1_amd64.deb
systemctl --user enable --now shake-cursor.service
```

## RU — Настройка

* Трей → **Настройки…**
* Файл: `~/.config/shake-cursor/config.ini`

## RU - Из корня проекта (где находится папка `debian/`):

```bash
dpkg-buildpackage -b -us -uc
```

**Проверить содержимое пакета:**

```bash
dpkg -c ../shake-cursor_1.0-1_amd64.deb
dpkg -I ../shake-cursor_1.0-1_amd64.deb
```

**Установка и включение сервиса пользователем:**

```bash
sudo dpkg -i ../shake-cursor_1.0-1_amd64.deb
systemctl --user enable --now shake-cursor.service
```

**Удаление:**

```bash
sudo apt remove shake-cursor
```

> Важно: на этапе **сборки** нельзя вызывать `systemctl`. Сервис включается **после** установки пакета.

---

## License / Licencia / Лицензия

MIT — see [`LICENSE`](./LICENSE).

---

## Troubleshooting

* **No tray icon in GNOME:**
  Install & enable the AppIndicator extension:

  ```bash
  sudo apt install gnome-shell-extension-appindicator
  gnome-extensions enable ubuntu-appindicators@ubuntu.com
  # log out & log in
  ```

* **No input events / needs permissions:**
  Add your user to the `input` group and re-login:

  ```bash
  sudo usermod -aG input "$USER"
  ```

* **Multi-monitor size not updating on one screen:**
  The app forces a brief cursor theme toggle to reload cursors across all heads. If you still see issues, switch between monitors once and report a bug with `journalctl --user -u shake-cursor.service -b`.

* **Service logs:**

  ```bash
  journalctl --user -u shake-cursor.service -f
  ```
  
