# FELUCCA TONIC — DRUM-X

Motor de batería de JIANT con la idea de Microtonic: un oscilador afinado con envolvente de pitch y FM, un
generador de ruido filtrado, sus envolventes, dos patches (A y B) por sonido y un MORPH entre ellos.
Reemplaza a DRUM por etapas, dentro del mismo motor (número 10). Así se conservan la grilla de 8 lanes, el
mapa GM, los patrones y los proyectos.

## El sonido (`firmware/src/drumx_voice.c`)

Un sonido (un lane del kit) ocupa 11 bytes (`dx_lane_t`): un byte de modo y cinco valores para A y cinco para B.

| Valor | Qué hace |
| --- | --- |
| PITCH | oscilador, MIDI 24–119 (33 Hz – 7,9 kHz), 3/4 de semitono por paso |
| PMOD | envolvente de pitch: hasta +4 octavas en el golpe, cae con 1/6 del decay (1–40 ms) |
| DECAY | caída de amplitud, 3 ms – 1,5 s (exponencial) |
| NOISE | mezcla: 0 solo oscilador, 127 solo ruido |
| COLOR | índice de FM del oscilador (en SINE, su drive) y corte del filtro del ruido (30 Hz – 16 kHz) |

El modo elige la onda (SINE, FM 1:1, METAL 1:1,47, BELL 1:2,76), el filtro del ruido (LP, BP, HP; V10.7.8 GR, el
güiro: un BP cuyo golpe es una pasada de ticks a 24–64 Hz según PITCH, sostenida el SCRAPE (el lugar de PMOD, 0–173
ms) por la cuarta potencia de la velocidad: acento la pasada larga, golpe suave el "chk") y la envolvente del ruido (la
del oscilador, o SNAP: 1/4 de su caída).

El MORPH (0 = A, 127 = B) interpola valor por valor y se lee en cada bloque, así que se puede mover mientras
el golpe suena. Todo es punto fijo con productos de 32 bits.

## Kit por sección

Un kit de 8 sonidos ocupa 88 B. Va dentro de cada sección, que tiene 192 B libres (decisión del usuario): cada
variación A–D puede tener su propio kit.

## Etapas

- **Fase 1 — HECHO.** DRUM toca el kit de fábrica `DX_KIT_DEF`. Lo cubre `tests/drumx_test.c`:
  cada lane suena y termina, el MORPH funciona (también con el golpe sonando), el PITCH, el NOIS, el choke de hats y
  los mutes por grupo, y deja demos en `build/drumx_demo/`.
- **Los kits de Felucca, retirados (JIANT 0.1).** STD y los kits de modelo (80 10 66 55 77, `drum_voice.c`) ya no
  existen: DRUM es DRUM-X. Las perillas de DRUM (mismo lugar en el proyecto): **MRPH** (donde estaba KIT; KNOB 1),
  TUNE, TONE (COLOR), DECY (DECAY), **NOIS** (donde estaba SNAP: mueve el ruido de todos los lanes), ACC (acento,
  hasta +3,5 dB; en JIANT 0.3 su lugar es **FM**: FM armónica en cada sonido, 8 bandas de ratio, 0 = el patch como
  está), DRV. El lugar de KICK queda libre. Un proyecto o sonido de Felucca con DRUM suena con DRUM-X.
- **Fase 2.** El kit se guarda en la sección (88 B en el proyecto FUNA, conversión desde FUN9):
  - **hecho:** una página de edición por lane (SOUND / SOUND 2: lado A/B, los 5 valores y el modo);
  - MORPH (MRPH) y PITCH lockeables por paso (los locks de parámetros genéricos ya los cubren);
  - DRIVE y PAN por sonido;
  - **hecho:** grupos KICK (bombo) / SNARE (snare y clap) / HAT (los dos hats) / PERC (tom, rim, bell) con mute en
    vivo: GLO sostenido + teclas negras 5–8 (D#4 F#4 G#4 A#4), LED apagado = muteado, C4 los desmutea con los tracks.
    Un golpe nuevo de un grupo muteado no suena; lo que suena se apaga en ~6 ms
    (sin click). Se guardan con la sección (byte 3208 del proyecto FUNA);
  - más kits de fábrica de DRUM-X para elegir.
