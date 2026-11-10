# Historial de cambios de JIANT FM

Firmware personalizado para el M-VAVE FM-1, de Juan Jiant. Primero la versión más nueva.

**Estado de cada versión**

| Versión | Fecha | Estado |
| --- | --- | --- |
| **V10.7.17** | 2026-11-10 | **Pública** |
| **V10.7.16** | 2026-11-10 | **Pública** |
| V10.7.7 … V10.7.15 | 2026-11-09 … 11-10 | Beta |
| **V10.7.6** | 2026-11-09 | **Pública** |
| V10.6d … V10.7.5 (menos V10.6z2) | 2026-11-06 … 11-09 | Beta |
| **V10.6z2** | 2026-11-07 | **Pública** |
| **V10.6c** | 2026-11-03 | **Pública** (la última pública antes de este ciclo) |

Si venís de la **V10.6c**, leé todo hasta la V10.6c. Si venís de la **V10.6z2**, leé hasta la V10.6z3.
Antes de instalar, revisá los apartados **Compatibilidad**.

> **Sobre los números de versión.** Antes de la V10.6c las versiones iban V10.1, V10.2 … V10.61. Con la V10.6c arrancó
> otra numeración: V10.6c, V10.6d … V10.6z, V10.6z2 … V10.6z10, después V10.7, V10.7.1 … V10.7.5. La **V10.6** y la
> **V10.7** del 31 de octubre de 2026 son versiones mucho más viejas con el mismo nombre. Están en "Versiones anteriores",
> al final.

---

## V10.7.17 — estabilidad de MY LOOP (2026-11-10)

- MY LOOP ya no se bloquea cuando una pista melódica conserva golpes de DRUM-X sin notas.
- TRACKS 4 excluye T5/T6 ocultas de la detección del loop, el análisis tonal y la asignación de roles. Cambiar la cantidad de pistas activas reinicia la reutilización del loop.
- La publicación pública ejecuta las regresiones de MY LOOP, generación de canciones, borradores y GEN antes del despliegue.

---

## Problemas conocidos en la V10.7.16

- **Velocity en la fila de arriba.** Las teclas C#5..G5 no tienen sensor de presión. Tocan con el promedio de lo que
  venís tocando y no tienen aftertouch. Velocity y aftertouch funcionan en F3..C5.
- **AIR es ruidoso.** Como fuente de modulación y para SWARM, AIR (los dispositivos Bluetooth que hay alrededor) salta
  más que PROX. Si podés, usá **PROX** (tu propio celular, calibrado con FAR / NEAR).
- **VEL en proyectos viejos.** El ajuste nuevo VEL (EDIT > VOICE 2, KNOB 3) se guarda donde MUTATE guardaba su
  configuración. Un proyecto que todavía tenga ajustes viejos de MUTATE puede abrir con **VEL por debajo del 100 %** en
  algunas pistas. Cuando abras un proyecto viejo, revisá VEL en cada pista. Si una pista suena más plana que antes,
  volvé a ponerla en 100 %.
- **Instalar desde Android se puede trabar.** Si podés, instalá desde Chrome o Edge en una computadora. Con un celular
  y un cable OTG puede andar, pero con la compu vas más seguro.
- **GLO > SYSTEM 2 solo muestra valores.** No se editan ahí. La página te dice dónde se cambian (MENU > BLUETOOTH).

---

## V10.7.16 — 2026-11-10 — pública

Todo lo nuevo desde la V10.7.6 (incluye las betas V10.7.7 … V10.7.15).

- **GENE › FORM MY LOOP**: tu loop hecho canción entera. Queda como está (el drop); GEN escucha su tonalidad y sus
  acordes (seis modos, también los carriles con nota de DRUM-X), rearmoniza cada semilla con acordes que contienen tus
  notas, agrega bajo, acordes y contramelodía solo en pistas vacías, y el género arma las otras secciones (su batería con
  tu kit, su bajo y sus partes). OCT+ otra vez para otra canción, UNDO vuelve al loop. EXPLORE elige un género por semilla.
- **Se acabaron las notas colgadas**: teclas, teclas de SCENE, LATCH, el arpegio y las notas MIDI entrantes apagan
  siempre lo que encendieron. **PÁNICO**: mantené PLAY unos 0,7 s.
- **VOICE** con palabras más claras y fluidas (consonantes, vocales abiertas, sin pozos de volumen).
- **Step components: TRIG** (trig condition estilo Elektron: TRIG 4:4 suena solo en la 4.ª vuelta).
- **Kits de DRUM-X secos** (sin envíos por defecto), las teclas FX de EXTENDED ya no fuerzan delay / reverb; pre-delay
  del reverb sin aliasing (ROOM / SPRING con PRE > 0).
- Sonidos al prender: ACOUSTIC, ANALOG PLUCK, PHASE INIT SAW, FM BASS, CHANT, WHISTLE (secos).
- Arreglos pedidos por usuarios: REC y PLAY graban el loop después de una canción de GEN; el EQ y el TAPE del master se
  guardan en todas las secciones; audio USB en macOS 13–15; la perilla PRESETS dice LIST <tipo> si hay filtro; el scope
  de KEY SPLIT ya no se congela; la espera del velocity tope 2,2 ms (ninguna en baterías); BLUETOOTH dice MISSING (antes
  NO SEG) con aviso para reinstalar.
- GENE: güiro y kit LATIN PERC, CUMBIA RKT, REGGAETON más duro, BAILE FUNK minimal, EBM / MINIMAL TECHNO / PHONK según
  temas de referencia. EDIT › SOUND sin la lista de parámetros al costado; barras térmicas en PROX y SWARM.

**Compatibilidad**
- Los proyectos en los formatos más viejos (JIANT 0.1–0.5, Felucca 1.0/1.1) ya no se leen: abrilos y guardalos de nuevo
  con la V10.7.15 o anterior antes de actualizar. Los presets de usuario muy viejos cargan tal cual; un motor retirado en
  una pista carga como ANALOG.
- ADD muestra la criatura genérica.
- Un proyecto con TRIG abierto en un firmware anterior toca ese paso al revés (no rompe nada).

## V10.7.6 — 2026-11-09 — pública

**Arreglado**
- SEQ > STRETCH > NUDGE (OCT+ dos veces, el paso): una nota sostenida se mueve con sus ligaduras y la vecina conserva su
  largo; en el paso 1 y en LEN el paso queda quieto (las ligaduras no dan la vuelta). Locks y automatización lo
  acompañan; UNDO lo devuelve.
- SEQ > COMP: la tarjeta MOVE se lee entera ("-63 ST").
- Editor web: los tiempos de delay incluyen los con puntillo (1/4D 1/8D 1/16D) en el orden correcto.
- Las pruebas automáticas vuelven a correr hasta el final (desde el 8/11 se cortaban antes), así los errores vuelven a aparecer.

---

## V10.7.5 — 2026-11-09 — beta

**Nuevo**
- GENE **CUMBIA** rehecha a partir de la referencia del autor y de sus propios loops:
  - Bajos sincopados: la tónica en el 1, la quinta anticipando el 3 y un cierre con cuarta o séptima.
  - Bombo en 1 y 3, acentos de llamador y conga, y el cencerro en el 2 (y en el 4).
- El güiro va en la "y" en las tiradas sonideras (como en los loops del autor) y en el tiempo en las villeras y
  digitales.
- Una canción de cumbia generada tiene un corte en su compás de fill.

## V10.7.4 — 2026-11-09 — beta

**Cambiado**
- SEQ > STRETCH, NUDGE tiene un tercer modo: apretando OCT+ otra vez movés el paso seleccionado un paso a la izquierda
  o a la derecha. Quedan así:
  - OCT- mueve toda la secuencia.
  - OCT+ mueve el MICRO del paso.
  - OCT+ otra vez mueve el paso.

## V10.7.3 — 2026-11-09 — beta

**Nuevo**
- **VEL** es cuánto le afecta la velocity a cada sonido (EDIT > VOICE 2, KNOB 3, 0..100 %). En 0 % todas las notas
  suenan a nivel fijo y los acentos siguen andando.

**Cambiado**
- Los bajos de fábrica quedan con VEL en 0 %, a nivel parejo. Los bajos de GENE quedan en 75 %.

**Compatibilidad**
- Los proyectos viejos abren con VEL en 100 % y suenan como antes. La excepción está en "VEL en proyectos viejos"
  (Problemas conocidos).

## V10.7.2 — 2026-11-08 — beta

**Cambiado**
- GENE junta la variedad de la V10.6z10 (abajo) con la mezcla de la V10.7 y las reglas de los géneros latinos.
- Las tiradas de cada género latino:
  - **CUMBIA**: sonidera, villera o digital.
  - **REGGAETON**: clásico, romántico o perreo.
  - **DEMBOW**: clásico o rápido.
- Cada estilo sigue sus reglas: güiro, bajo tónica-quinta, el riddim de dembow, el 808 en tresillo y los chops.

## V10.6z10 — 2026-11-08 — beta (hecha en paralelo, incluida en la V10.7.2)

**Cambiado**
- GENE: si tirás dos veces el mismo género, ya no sale la misma canción.
  - El estilo de bajo y los riffs cambian en cada tirada. JAZZ tiene un tercer estilo de walking bass.
  - Hay más variedad de sonidos: un tercio de las veces sale otro preset del mismo tipo. Nunca repite opción y nunca
    usa un preset INIT. Drive, chorus y detune se abren un poco más. Phonk y baile funk siempre usan el 808 COWBELL.
  - El tempo se mueve dentro de un tercio del rango del género, alrededor del punto que marca ENERGY.
  - Hay más variedad de batería:
    - Los bombos extra propios de cada género y la percusión euclidiana.
    - Ghost notes en el redoblante y un primer compás un poco variado.
    - Dos breaks más para IDM: FUNKY y THINK.
  - Las progresiones salen de una familia de armonías. Un tercio de las veces es la del propio género.
  - Las secciones que siguen desarrollan la primera en vez de repetirla: varían el bajo, los acordes, el arpegio y el
    motivo del lead.
  - OCT+ nunca repite una de las últimas ocho canciones del género. La misma semilla sigue dando la misma canción.

## V10.7.1 — 2026-11-08 — beta

**Arreglado**
- Velocity y aftertouch vuelven a funcionar como en la V10.6z9. La velocity más pareja de la V10.7 dejaba mudos los
  golpes fuertes y el aftertouch quedaba débil. La mezcla de GENE y los manuales nuevos de la V10.7 siguen.

## V10.7 — 2026-11-08 — beta (preparada como versión pública, reemplazada por la V10.7.1)

**Nuevo**
- Las canciones de GENE salen mezcladas como un disco:
  - Niveles de cada parte según el género y ajuste de nivel de cada sonido.
  - Ancho estéreo y sonidos que no cambian solos.
- DnB tiene el break Amen y un loop de cuatro acordes. Los géneros de club tienen ganchos de arpegio.
- Manuales nuevos (inglés y español), README y machetes.

**Cambiado**
- La velocity es más pareja y la fila de arriba sigue tu dinámica. *(Se deshizo en la V10.7.1.)*
- GENE reemplaza SOFT PAD y BRASS, y ya no toca al azar los ajustes de lista.

## V10.6z9 — 2026-11-08 — beta

**Arreglado**
- DRUM-X vuelve a tener pegada:
  - Los bombos recuperan el cuerpo de antes de la V10.6q. NOISE viene apagado y, si lo subís, agrega un golpe corto.
  - Los hi-hats vuelven a ser brillantes: LOFI DUST, HIP HOP, 808, ACOUSTIC, DUB ROOTS, los kits suaves de GENE y el
    ride de JAZZ.
  - Redoblantes, palmas y cencerros no cambian.

## V10.6z8 — 2026-11-08 — beta

**Cambiado**
- Si cambiás un sonido mientras suena una canción, la canción respeta tu cambio. La pista lo mantiene hasta una tirada
  nueva de GENE, una carga, un salto o EDIT > RECALL. Las secciones siguen cambiando los pasos, la mezcla y los mutes.
- La calibración de PROX (FAR / NEAR) ahora toma la lectura del medio de 2,5 segundos. Quedate quieto con el celular
  mientras mide.
- AIR está suavizado y salta menos.
- GLO > SYSTEM 2 te dice dónde se editan sus valores.

## V10.6z7 — 2026-11-08 — beta

**Nuevo**
- **Velocity en las teclas.** F3..C5 suenan según la fuerza con que tocás y se calibran solas mientras tocás.
  - MENU > CONTROL > VELOCITY elige la curva: SOFT, NORMAL o HARD. Con OFF todas las notas suenan a 100, como antes.
  - VEL MIN pone el nivel mínimo.
  - La fila de arriba toca con tu promedio reciente.
- **Aftertouch.** Si apretás más fuerte una tecla que ya está sostenida, sirve como fuente de modulación (AT) y sale
  por MIDI. Los sonidos nuevos traen el aftertouch abriendo el filtro.
- MENU > CONTROL > AFTERTOUCH y VEL LEARN.
- En el editor web se ve la velocity en vivo.

## V10.6z6 — 2026-11-08 — beta

**Nuevo**
- SWARM toca una melodía según tu cercanía (primero PROX, después AIR), a tempo y en la escala de la pista. Con la
  escala cromática toca blues con blue notes.
- SWARM tiene un morph FAR / NEAR como DRUM-X: RATE, MOVE, TONE, NOISE y MOD de cada lado (EDIT > SWARM / SWARM 2).
- Cada dispositivo Bluetooth cercano suma modulación. CHAOS pone el máximo.

## V10.6z5 — 2026-11-08 — beta

**Nuevo**
- Un modo de prueba de velocity en las teclas, apagado de fábrica (el primer paso hacia la V10.6z7). En el editor web
  se ve en vivo.

**Cambiado**
- Más espacio libre en el firmware para lo que viene. No cambia ningún sonido.

## V10.6z4 — 2026-11-07 — beta

**Nuevo**
- Con una nota sonando:
  - Si mantenés **OCT-**, suena vibrato y entra de a poco, como una rueda de modulación.
  - Si mantenés **OCT+**, la nota sube con un bend.
- MENU > AUDIO > OCT BEND elige el rango: OFF, 1, 2, 3, 4, 5, 7 o 12 semitonos (2 de fábrica).
- Los dos efectos se graban y salen con MIDI OUT ALL.

**Cambiado**
- REC respeta cómo tocaste:
  - Guarda el largo de cada nota y graba el legato como SLIDE.
  - Pone a tiempo las notas en legato que entraron tarde.
  - Graba la velocity sin acentos falsos y pone la primera nota en el paso 1.
- Las pistas nuevas arrancan con GATE en 100 %.

## V10.6z3 — 2026-11-07 — beta

**Nuevo**
- Una conexión estable para las apps compañeras (versión 1.0). Las apps pueden preguntar qué versión tiene el equipo y
  seguir el transporte, los efectos punch-in y los mutes.
- Todos los parámetros automatizables de pista y los globales se controlan por MIDI de alta resolución (NRPN) y se
  graban. Con MIDI OUT ALL, mover una perilla manda NRPN.

## V10.6z2 — 2026-11-07 — **PÚBLICA**

**Arreglado**
- Algunos equipos se colgaban con el Bluetooth prendido. Ahora el segundo núcleo del procesador arranca solo con la
  alimentación normal, y no después de un SAFE START.

## V10.6z — 2026-11-07 — beta

**Arreglado**
- Los **FM-1 más nuevos** (vendidos desde fines de 2026, con el logo en el frente) traen otro chip de memoria. En esos
  equipos no andaban el guardado, GENE ni las actualizaciones. Ahora sí andan.

**Compatibilidad**
- Si tenés uno de los equipos nuevos, instalá la V10.6z o una posterior. En las anteriores no vas a poder guardar.

## V10.6y — 2026-11-07 — beta

**Arreglado**
- VOICE ya no distorsiona en vocales abiertas como "AH". Su nivel queda a la par de ANALOG.

**Nuevo**
- WORDS, KNOB 3 **CONS**: largo de las consonantes, de 50 a 400 %. Con LANG prendido, POS pasa a EDIT 1.

## V10.6x — 2026-11-07 — beta

**Cambiado**
- GENE JAZZ va en cualquier tempo, de balada a bebop según la energía de la tirada. El swing se endereza a medida que
  sube el tempo. La pantalla muestra la mitad del tempo que se siente.

## V10.6w — 2026-11-07 — beta

**Nuevo**
- GENE **HIP HOP**: cada tirada sale trap, drill, pluggnb, boom bap o lofi.
- GENE **JAZZ**: el patrón del ride y hi-hat en 2 y 4, walking bass, séptimas ii–V–I, comping de piano Charleston y
  swing.
- GENE **TECHNO ROCK**: sale rock o synth ochentoso.

**Cambiado**
- Baterías más brillantes y un 808 menos oscuro.
- El piano roll, las miniaturas de compás y los rieles de LIVE se pintan por nota, en los colores térmicos.

## V10.6v — 2026-11-07 — beta

**Nuevo**
- El componente de paso **RAMP**: desde un paso, un parámetro va deslizando hasta un destino durante 1–32 pasos o 1–16
  compases. La curva se elige: LIN, EXP, LOG, S, STEPS o ZIGZAG. Al final, y con STOP, vuelve deslizando a lo que marca
  la perilla.
- SWARM:
  - La mitad de arriba de OCTAVE camina por la escala.
  - Cada dispositivo Bluetooth nuevo cambia el timbre y tiene el suyo propio.
  - Perilla nueva **CHAOS** (reemplaza a SELF).

**Quitado**
- El componente de paso JUMP (lo reemplaza RAMP).

## V10.6u — 2026-11-07 — beta

**Cambiado**
- Los géneros de GENE se basan en los loops de referencia del autor:
  - CUMBIA a 76–92 BPM, con el güiro en la "y" y un kit más brillante.
  - EBM tiene una tirada a medio tiempo (148–156) con bajo en semicorcheas sin parar.
  - BAILE FUNK tiene sus palmas, el tamborzão y el mandelão (128–136).
  - PHONK tiene una tirada de phonk brasileño (130–156).
  - HOUSE, DEEP HOUSE y TECH HOUSE van más rápido.

## V10.6t — 2026-11-07 — beta

**Arreglado**
- Las canciones de GENE vuelven a sonar llenas:
  - Las intros tienen más partes.
  - Vuelven el giro y los hi-hats de DnB y breakbeat.
  - EXPLORE vuelve a ser aventurero.
- Los bombos de DRUM-X tienen más cuerpo.
- Se emparejó el volumen de los presets que sonaban muy fuerte (INIT SAW/PWM, ORGAN, TINE EP, WHISTLE, OCARINA y otros).

## V10.6s — 2026-11-07 — beta

**Nuevo**
- **SAVE > SONG > CLIPS** es una grilla de 6 pistas × 4 partes.
  - KNOB 1 elige la pista y KNOB 2 la parte.
  - OCT+ lanza esa celda en el compás siguiente y OCT- para la pista.
  - F3..B3 lanzan una parte entera.
  - REC graba cada lanzamiento como una fila de la canción.

**Compatibilidad**
- **Se retiraron formatos viejos.**
  - Los proyectos de las primeras versiones de JIANT (anteriores al formato 1.0.3) se rechazan y quedan sin tocar.
  - Los sonidos viejos PHYS MODEL, PERC y de batería GM ya no se convierten.
  - Un sonido DIGITAL carga como FM6 INIT.

## V10.6r — 2026-11-07 — beta

**Nuevo**
- DRUM-X **KEY MAP EXTENDED** viene de fábrica.
  - OCT +1 toca fills de verdad: redobles, bajadas de toms, flams, 3-3-2 y un crash en el próximo tiempo fuerte.
  - OCT -1 toca cualquier sonido cromático: KEY MAP 2..9 y BELL para melodías de cencerro.
- SCL > SCENE **DRUM GEN**: GENE escribe la parte del kit según el género. Cada tecla toca BASE, DENSE, BREAK o FILL.

## V10.6q — 2026-11-07 — beta

**Cambiado**
- DRUM-X suena mejor, sin funciones nuevas: ruido más limpio, menos aliasing, mejor palma 808 y perillas más suaves.
- Un cencerro clásico, estilo analógico.

## V10.6p — 2026-11-07 — beta

**Cambiado**
- GENE **PHONK**, **BAILE FUNK** y **EBM** reescritos para que suenen como los géneros de verdad:
  - Phonk: el preset nuevo **808 COWBELL** y sus riffs.
  - Baile funk: el tamborzão.
  - EBM: bajos analógicos en semicorcheas y baterías pesadas.

**Compatibilidad**
- **6 canciones en vez de 8.** Las canciones 7 y 8 quedan reservadas para un futuro banco de samples de batería. Un
  proyecto guardado en la 7 o la 8 abre en la 6. Si tenían música, te avisa una sola vez.
- **Hacé un backup antes de instalar.** El instalador ahora te pide el backup primero. El backup lee 8 o 6
  canciones. Al restaurar, te dice qué canciones quedaron en el archivo.

## V10.6o — 2026-11-07 — beta

**Nuevo**
- **SAVE > SONG > TIMELINE** muestra tu canción como bloques de colores.
  - Abrís una fila pista por pista: MUTE, FROM otra parte y su variación.
  - PLAY repite la fila y otro PLAY sigue la canción desde ahí.
  - OCT+ copia la fila y OCT- la borra.
- **SAVE > SONG > PARTS** muestra las cuatro partes como tarjetas. Elegís una para repetirla desde el compás siguiente.
  KNOB 4 cambia entre LOOP y SONG.

**Arreglado**
- Un STRUM largo grabado se reproducía demasiado rápido.

**Quitado**
- DRUM-X ACOUST.

## V10.6n — 2026-11-07 — beta

**Cambiado**
- La lista de géneros de GENE la armó el autor: 25 géneros, más EXPLORE y RANDOM. Los nuevos son HARD BASS, TRIBAL,
  CUMBIA, REGGAETON, DEMBOW, IDM GLITCH / AMBIENT / WEIRD, FUGA, ELECTRO METAL, TECHNO ROCK, DREAM POP, PHONK y BAILE
  FUNK.
- EXPLORE hace música más rara, pero musical.
- El dado de SCL > SCENE elige una tonalidad (casi siempre modos menores).

**Quitado**
- Los géneros que no quedaron en la lista nueva.

## V10.6m — 2026-11-06 — beta

**Cambiado**
- Los géneros de GENE mantienen su propia batería. EBM tiene muchos más patrones y bajos. Los fills van solo al final
  de una frase.
- Los géneros de club se arman como los toca un DJ:
  - Intro y groove.
  - Un break y un build de 4 compases (redoble, riser, high-pass y crash).
  - El drop.

**Nuevo**
- SCL > SCENE, patrón **GEN** de BASS / LEAD: GENE le escribe a la pista una línea en la tonalidad, y cada acorde la
  mueve.

## V10.6l — 2026-11-06 — beta

**Nuevo**
- Un logo nuevo "jiant fm" al encender, escrito en el mismo orden en que lo escribe a mano el autor.

**Arreglado**
- La primera vez que apretabas GENE después de encender salía siempre la misma canción. Ahora sale una nueva.

**Cambiado**
- EXPLORE se mueve (el filtro respira) en vez de tirar pasos para cualquier lado.

## V10.6k — 2026-11-06 — beta

**Cambiado**
- Más espacio libre para GENE. No cambia ningún sonido.
- El patrón BASS / LEAD y el estilo DRUM de SCL > SCENE se guardan con el proyecto.

**Compatibilidad**
- Los proyectos viejos abren con el primer patrón y el primer estilo.

## V10.6j — 2026-11-06 — beta

**Nuevo**
- Cada nota tiene su color en KEY, CHORD, ARP y SCENE: C es la más fría y B la más caliente.
- SCL > SCENE:
  - BASS y LEAD tocan un patrón sobre el acorde (KNOB 4).
  - El rol DRUM escribe un ritmo por tecla (KNOB 4 STYLE).

## V10.6i — 2026-11-06 — beta

**Cambiado**
- Se rehicieron los presets de fábrica de todos los motores para que se muevan: LFOs, ENV 2, modulación y filtro. El
  navegador muestra primero los sonidos normales, después los experimentales y al final los INIT.
- ANIMAL queda con 29 presets.

**Compatibilidad**
- Los números de preset se mantienen: los proyectos conservan sus sonidos y PROGRAM CHANGE sigue andando.
- Al cargar un sonido ya no se mantienen los slots de modulación viejos de MUTATE.

## V10.6h — 2026-11-06 — beta

**Nuevo**
- FX > DLY 3 TYPE **REVERSE**: cada repetición toca al revés lo último que sonó.

**Arreglado**
- Los números de PROGRAM CHANGE vuelven a ser los de antes de que existiera MATH.
- Las pistas viejas LOFI PULSE LD cargan como un órgano ANALOG.
- Los sonidos chip convertidos conservan su filtro.
- CC74 / CC71 mueven la página FILTER en los motores que no tienen su propio control de brillo.
- SWARM ya no confunde su propia voz con un dispositivo.

## V10.6g / V10.6f — 2026-11-06 — beta

**Cambiado**
- Se ahorró espacio dentro del firmware. Las palabras, los sonidos y los dibujos son iguales que antes.

## V10.6e — 2026-11-06 — beta

**Cambiado**
- **LOFI ahora es MATH**, solo con fórmulas (BYTE, FLOAT). Los sonidos MATH arrancan quietos y afinados.
- La banda nueva al encender: DRUM-X ACOUSTIC, ANALOG FAT BASS, PHASE GLASS PAD, FM6 TINE EP, VOICE CHOIR AAH y ANIMAL
  WHISTLE.
- Las voces tonales de ANIMAL tocan la nota que apretás.

**Compatibilidad**
- Un sonido chip viejo de LOFI, sea de un proyecto o de un preset de usuario, carga como el sonido ANALOG más
  parecido.

## V10.6d — 2026-11-06 — beta (incluye las versiones de desarrollo de la V10.6c de abajo)

**Nuevo**
- **SWARM**: si no hay nada enchufado al motor INPUT y BLUETOOTH AIR está escuchando, cada dispositivo que hay alrededor
  se vuelve una voz (hasta ocho). SOURCE elige entre SWARM y la entrada de audio, y los dos pueden sonar a la vez en
  dos pistas.
- INPUT pasa a llamarse **EXT**.
- GEN pasa a llamarse **GENE**, y su página muestra una doble hélice girando.
- De las versiones de desarrollo de la V10.6c (4 y 5 de noviembre):
  - **SAVE SONG AS**: copia una canción entera (A–D y el arreglo) a otro proyecto.
  - **FIT / SPAN** (SEQ > LENGTH > RHYTHM): metés cualquier cantidad de pasos en el largo de un loop, por ejemplo 18
    pasos contra 16.
  - **KEY SPLIT** (GLO > SYSTEM > KEY SPLIT): dos pistas en el teclado y en el MIDI externo.
  - Borrador y borrado:
    - Las canciones generadas quedan en un borrador hasta que las guardás como SONG o LOOP.
    - **ERASE** borra un proyecto o una sección.
  - Modulación:
    - Fuentes nuevas: envolventes de pista y seguidores.
    - Destinos más claros.
  - Mapa **EXTENDED** de DRUM-X: un fill por tecla, bombo y tom cromáticos, y efectos por golpe.
  - VOICE en más idiomas.
  - Los presets USER guardan la letra de VOICE.

**Quitado**
- La consola serie de desarrollo. El puerto USB ahora funciona como MENU > USB STEREO.

## V10.6c — 2026-11-03 — **PÚBLICA**

**Arreglado**
- La entrada de audio por USB (USB > HOST con una interfaz de audio, o USB IN+OUT) anda bien. Se arreglaron dos errores
  en cómo se recibía y remuestreaba el audio que entra. Se probó en el equipo.

**Cambiado** (de la V10.61, incluido acá)
- La página PROX:
  - KNOB 1 **MIN** y KNOB 2 **MAX** achican el campo, así no tenés que alejar tanto el celular.
  - KNOB 3 **SPEED** (1–5) elige qué tan rápido responde PROX.

---

## Versiones anteriores (resumen)

- **V10.47 – V10.61 (2 y 3 de noviembre de 2026)**:
  - El motor **INPUT** (audio por USB en una pista) y la fuente FOLLOW. USB HOST acepta interfaces de audio, también a
    48 kHz.
  - STEREO IMAGE en los sintes, con su propia página, y DLY R TIME.
  - El roll sigue al cursor de reproducción y REPEAT anda en tempos lentos.
  - El EQ y el TAPE del master ya no se resetean solos.
  - Un SAFE START para los equipos que quedaban sin imagen después de instalar.
  - Los sonidos de GEN se arreglaron después del cambio de presets.
- **V10.20 – V10.46 (1 y 2 de noviembre)**:
  - USB HOST para teclados MIDI.
  - Emparejamiento de Bluetooth MIDI que anda en Windows y Mac.
  - Clock MIDI de salida estable y BLUETOOTH AIR.
  - LFO DRIFT, ENV 2, el filtro LP24 y los intervalos de ANALOG PLS3 / SAW3.
  - Kit nuevo de DRUM-X HIP HOP.
  - Presets de fábrica más distintos entre sí.
  - Muchas páginas con dibujos que muestran qué hace cada una.
  - PROX más suave.
- **V10.1 – V10.19 (30 y 31 de octubre)**:
  - Llega el Bluetooth: BLE-MIDI de entrada y salida, y **PROX**, la distancia de tu celular como fuente de modulación.
  - SEQ > EUCLID y MODES reemplazan al primer MUTATE.
  - ERASE de secciones.
  - El aspecto del TIMELINE y el orden del menú SAVE.
- **V10 (30 de octubre)**:
  - Base nueva: TIMELINE, VAR y el motor ADD.
  - Se puede ir y volver al firmware oficial V15 / V16 de M-VAVE.
- **0.9.x (17 al 26 de octubre)**:
  - Los motores STRING y ADD.
  - El EQ de 4 bandas del master y TAPE.
  - El generador de canciones rediseñado y VOICE.
  - Stems por USB limpios, y la app JIANT Stems.
- **0.7 – 0.8 (15 y 16 de octubre)**: stems multipista por USB, mejoras en el secuenciador y en REC, y los machetes.
- **0.6.x (10 al 15 de octubre)**:
  - Seis pistas, SLICER, MUTATE y los componentes de paso.
  - MIDI OUT y PROGRAM CHANGE.
  - Audio en dos núcleos.
  - INIT SYSTEM (arrancar de cero al encender) y FM6 editable.
- **0.3 – 0.5 (9 de octubre)**:
  - DRUM-X FM, efectos punch-in, reverb y grain delay.
  - Los macros de HOME y el dado de SELECT.
  - El nivelador del master y un filtro en cada motor.
- **Antes de la 0.3 (2 al 8 de octubre)**: las betas de Felucca (0.4 – 1.1.5), el instalador web y el editor web.
