# Diagnóstico de velocidad de las teclas (JIANT V10.6z5)

> **Desde la V10.6z7 la velocidad y el aftertouch ya son parte del firmware**: MENU > CONTROL > **VELOCITY**, **VEL
> MIN**, **AFTERTOUCH** y **VEL LEARN** (ver el [manual](MANUAL.es.md#velocidad-y-aftertouch-de-las-teclas-v106z7)). La
> misma página muestra ahora, en *En vivo*, la velocidad de cada nota y la calibración de cada tecla. El diagnóstico de
> abajo (la primera etapa, V10.6z5) sigue disponible para medir un equipo y mandarnos el archivo.

Este diagnóstico **mide** cuánta presión le das a cada tecla, para saber si tu FM-1 puede tener velocidad
(notas más fuertes cuanto más fuerte tocás). Es la primera etapa: **no cambia el sonido ni cómo suenan las
teclas**. Las notas siguen sonando igual que siempre; el diagnóstico solo escucha y anota.

- Está **apagado** siempre que encendés el FM-1. Solo se prende cuando vos lo pedís desde la página (abajo), y se
  apaga con el botón *Desactivar* o apagando el FM-1.
- Mide las **20 teclas de F3 a C5**. Las 7 teclas de arriba de todo (C#5 a G5) no se pueden medir en este hardware.
- La perilla MASTER sigue funcionando mientras mide.
- La forma de leer las teclas sigue el trabajo de Charles Vestal ("Velo", github.com/charlesvestal/fm1-velocity,
  GPL-3.0), que descubrió cómo medir la presión de las teclas del FM-1.

## Qué necesitás

- El FM-1 con **JIANT V10.6z5 o posterior** instalado.
- El cable USB que usás para el editor.
- Una **computadora con Google Chrome** (o Microsoft Edge), o un **teléfono Android con Chrome** y un adaptador
  USB OTG (USB-C o micro-USB a USB-A, el mismo tipo que sirve para conectar un pendrive al teléfono).
- La página del diagnóstico: **https://juanjiant-bit.github.io/JIANT/webapp/vel/**
  (si todavía no está publicada, bajá el archivo `web/vel.html` del repositorio, con el botón *Download raw file*
  en GitHub, y abrilo con doble clic en Chrome).

## Paso a paso desde una computadora (Chrome)

1. **Cerrá el editor web de JIANT** y cualquier programa de música que use MIDI (DAW, otros sitios con MIDI). En
   Windows, el puerto MIDI lo puede usar un solo programa a la vez.
2. Conectá el FM-1 por USB a la computadora y encendelo.
3. Abrí la página **https://juanjiant-bit.github.io/JIANT/webapp/vel/** en Chrome.
4. Tocá **"1. Conectar el FM-1"**. Chrome pregunta si la página puede *controlar y reprogramar tus dispositivos
   MIDI*: elegí **Permitir** (hace falta para hablar con el FM-1; la página no instala nada).
   Arriba tiene que aparecer *Conectado: JIANT …* con la versión instalada (V10.6z5 o posterior).
5. Dejá **Drive** en **2 (normal)** y tocá **"2. Activar la medición"**.
6. Probá una tecla: en la pantalla del FM-1 aparece un mensaje como `VEL C4 D412 P188 2.2MS`, y en la página
   aparece la misma línea en grande y en el registro. Si eso pasa, está todo funcionando.
7. Tocá la rutina de la sección siguiente. **Antes de cada grupo, apretá en la página el botón de la marca**
   (*suave*, *normal*, *fuerte*, *otras*), así después se sabe qué era cada grupo.
8. Al terminar, tocá **"Guardar .txt"**. Chrome guarda un archivo `jiant-velocidad-AAAA-MM-DD-HH-MM.txt` en
   *Descargas*.
9. Tocá **"Desactivar"** (o apagá el FM-1).
10. **Mandá el archivo** `.txt` (por mail, WhatsApp, Telegram o adjunto en un issue de GitHub). Si no podés
    adjuntar archivos, usá **"Copiar todo"** y pegá el texto en el mensaje.

## Paso a paso desde un teléfono Android

1. Conectá el FM-1 al teléfono con el adaptador OTG y el cable USB. El teléfono le da corriente al FM-1; si el FM-1
   no enciende, probá con el FM-1 cargado o con otro adaptador.
2. Si Android muestra un aviso sobre el uso del USB, no hace falta cambiar nada (el FM-1 se presenta como un
   instrumento MIDI).
3. Abrí **Chrome** (no otro navegador) y entrá a **https://juanjiant-bit.github.io/JIANT/webapp/vel/**.
4. Seguí los pasos 4 a 9 de arriba (*Conectar*, *Permitir*, *Activar*, la rutina con las marcas, *Guardar .txt*,
   *Desactivar*). El archivo queda en *Descargas* (la app *Archivos*).
5. Mandalo desde *Archivos* → mantené apretado el archivo → *Compartir* (WhatsApp, Gmail, etc.). También sirve
   *Copiar todo* y pegarlo en el mensaje.

Si Chrome en tu Android no encuentra el FM-1, usá la computadora: algunos teléfonos no dan acceso MIDI por USB.

## Qué tocar

Tocá **una tecla por vez**, soltándola del todo entre toque y toque (más o menos **un segundo** entre cada uno), y
mantenela apretada **medio segundo** cada vez. No hace falta que suene nada en especial: cualquier sonido sirve,
incluso con el volumen bajo.

1. Apretá la marca **suave** y tocá **10 veces suave** cada una de estas teclas: **F3** (la primera de la izquierda),
   **C4** y **G4**. Suave = lo más suave que podés tocar y que igual suene la nota.
2. Apretá la marca **normal** y tocá **10 veces normal** cada una: **F3, C4, G4**. Normal = como tocás siempre.
3. Apretá la marca **fuerte** y tocá **10 veces fuerte** cada una: **F3, C4, G4**. Fuerte = un golpe firme (sin
   lastimarte ni golpear el equipo).
4. Apretá la marca **otras** y tocá **5 veces normal** cada una: **A3, E4, A4 y C5**. Después, en **C4**: **3 veces
   muy despacio** (bajando la tecla lentamente, en un segundo o más), **3 roces** (apenas tocar la tecla, sin que
   llegue a sonar) y **3 veces apretando fuerte y sosteniendo 2 segundos**.

En total son unos 120 toques; lleva menos de 10 minutos. Si te equivocás o se te escapa un toque, no pasa nada:
seguí, y si querés anotalo con una marca.

## Cómo leer lo que aparece

Cada toque da una línea como `C4 D412 P188 2.2MS`:

- **C4**: la tecla.
- **D412**: la lectura más baja con la fila "empujada" (*drive*). **Cuanto más bajo, más fuerte tocaste.** Es el
  número que más importa: si entre *suave* y *fuerte* hay una diferencia clara (por ejemplo 650 contra 450), tu
  FM-1 puede dar velocidad.
- **P188**: la lectura más baja de la forma normal (con *pull-up*). Casi no cambia con la fuerza; sirve de control.
- **2.2MS**: el tiempo desde que la tecla empieza a tocar el contacto hasta que queda apoyada (en milisegundos;
  se mide en pasos de 2,2 ms). **GRAZE** = un roce: la tecla tocó el contacto pero no llegó a apoyarse.

La tabla *Por tecla* muestra, para cada tecla, cuántos toques hubo y la D más baja, la media y la más alta. En el
archivo guardado, además de esas líneas, están las **capturas**: las 56 lecturas crudas alrededor de cada toque
(8 antes del contacto y 48 después, cada 2,2 ms; las que empiezan con `d` son las lecturas con *drive*). Son para
el análisis, no hace falta mirarlas.

## Si algo no anda

- *"Sin acceso MIDI"*: usá Chrome o Edge y, cuando pregunte, elegí **Permitir**. Si lo rechazaste, tocá el
  candado a la izquierda de la dirección → *Dispositivos MIDI* → *Permitir*, y recargá la página.
- *"No encontré el FM-1"*: cerrá el editor web y otros programas MIDI, desenchufá y volvé a enchufar el USB,
  recargá la página.
- *"Este firmware no tiene el diagnóstico de velocidad"*: el FM-1 tiene una versión anterior; instalá JIANT
  V10.6z5 o posterior con el instalador web.
- En la página dice *Perdidos* con un número mayor que 0: algunos toques no se alcanzaron a leer (por ejemplo si
  tocaste muy rápido o la pestaña estuvo en segundo plano). No es grave; dejá la página visible mientras tocás.
- Si notás algo raro en el FM-1 mientras mide (una tecla o un botón que se aprieta solo, el OCT que cambia sin
  tocarlo), tocá **Desactivar** o apagá el FM-1, y contalo en el mensaje. Al encenderlo de nuevo el diagnóstico
  está apagado.

## Calibración (JIANT V10.6z6: la velocidad de verdad)

Desde V10.6z6 el FM-1 **toca con velocidad** (MENU > CONTROL > VELOCITY, NORMAL por defecto; OFF deja todo como
antes) usando lo que midió este diagnóstico, y con **aftertouch** (MENU > CONTROL > AFTERTOUCH). Lo que salió de tus
dos capturas:

- La medida que sirve es la **lectura D más baja en los primeros milisegundos** del toque (más baja = más fuerte). En
  tu FM-1, con *drive* 2: suave ~675, normal ~594, fuerte ~560 (la primera captura, con notas sostenidas, dio ~690 /
  655 / 610: otro día, otra mano; por eso la calibración se ajusta sola). Medir la pendiente del golpe no resultó más
  confiable.
- **Cada tecla es distinta** (D4 lee ~77 más alto que el promedio, A#4 ~31 más bajo), así que cada tecla tiene su
  propia calibración: tres referencias, su golpe FUERTE, NORMAL y SUAVE. La velocidad sale **continua** de la lectura
  pasando por esas tres (FUERTE 112, NORMAL 76, SUAVE 36, interpolando entre medio y con la mitad de la pendiente más
  allá de las puntas), y después la curva (SOFT / NORMAL / HARD) y el piso VEL MIN (20 por defecto).
- **Arranca** de tus medianas (las de cada tecla donde había datos: G4, A#4, D4, D#4, F4, C4; el promedio en las
  demás) y **aprende sola** mientras tocás (VEL LEARN ON): en cada toque cada referencia da un pasito hacia tu propio
  percentil 15 (FUERTE), mediana (NORMAL) y percentil 85 (SUAVE) de esa tecla; las primeras 24 veces de cada tecla
  más rápido. Un toque raro la mueve un pasito, nada más. Se guarda en la configuración medio minuto después de que
  dejás de tocar. **VEL LEARN RESET** (o el botón *Volver a la calibración de fábrica* de la página) vuelve al
  arranque.
- **Latencia**: la nota espera el final de su golpe como mucho una lectura de su tecla (2,2 ms; 1,7 ms en promedio en
  la prueba `tests/vel_test.c`). No se nota.
- **Presión (aftertouch)**: con la tecla abajo, la lectura sigue bajando si apretás más: eso es la presión, 0..127
  (MID: 2 por cuenta, con 8 cuentas de zona muerta para que un dedo apoyado no haga nada), suavizada, 0 al soltar.
- La fila de arriba (C#5 a G5) sigue sin poder medirse: toca el promedio de tus últimas velocidades.

En la página, la sección **En vivo** muestra la velocidad de cada nota que tocás, la lectura de la que salió, la
calibración de cada tecla (FUERTE / NORMAL / SUAVE) y la presión. Si una tecla suena siempre muy fuerte o muy suave,
mirá ahí su calibración y mandá una captura.

## Para quien analiza los datos

El registro es texto plano, una línea por evento:

```
# JIANT V10.6z5  <fecha ISO>  <navegador>
# vel on, drive 2
# marca suave
vel F3 pre 1023 1023 1023 1023 1023 1023 1023 1023
vel F3 post 812 d655 402 d431 230 d428 ...
vel 1 F3 D428 P230 4.4MS
...
# por tecla: tecla toques Dmin Dmedia Dmax
# F3 30 412 520 690
```

- Lecturas del ADC de 10 bits (1023 = abierta). Muestreo: cada tecla de las filas PA7 / PA8 una vez cada dos
  cuadros del escaneo (~2,2 ms); una tecla tocada alterna una lectura con *pull-up* y una con la fila empujada en
  alto (`d`, fuerza *drive* − 1 en HD0 / HD), como Velo.
- Umbrales: tocada < 900, apoyada < 400 (con *pull-up*), soltada: dos lecturas ≥ 600 (≥ 900 si nunca se apoyó).
- `D` = mínimo con *drive* durante todo el toque; `P` = mínimo con *pull-up*; el tiempo, del contacto (< 900) a
  apoyada (< 400), en décimas de ms.
- Protocolo: comando SysEx 81 `VEL` del editor (`web/EDITOR_PROTOCOL.md`, versión 1.1); código:
  `firmware/src/vel_diag.c`, `firmware/hal/fm1_input.h`; prueba: `tests/vel_diag_test.c`.
