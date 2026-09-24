# HIGGS: механики, API и горячие места

Теги: higgs, planck, vrik, api, производительность, исследование

Предмет — исходники HIGGS в `D:\Skyrim VR Modding\HIGGS` (ветка `research/trinity-synergy`,
версия 1.10.10.0, `include/version.h:7-10`). Установленная сборка — та же версия
(`MO2:HIGGS - Enhanced VR Interaction/meta.ini`: `version=1.10.10.0`). Разбор только читающий.

Условные обозначения путей:

- `src/…`, `include/…` — репозиторий HIGGS;
- `PLANCK:src/…` — `D:\Skyrim VR Modding\PLANCK`;
- `MO2:…` — `D:\Skyrim VR Modding\MO2\ModOrganizer\Skyrim VR\mods\…`;
- `VRIK-API:…` — `D:\Skyrim VR Modding\Claude Skyrim VR\local\vrik-re\api\vrikinterface001.h`.

Всё, что помечено **вывод**, — умозаключение из кода, а не прочитанный факт.

---

## 0. Главное

1. HIGGS — владелец «физической руки»: тела рук и тел оружия в Havok, хватание и удержание,
   гравитационные перчатки, двуручный хват, сгибание пальцев под геометрию предмета, тактильный
   отклик на столкновения. Плюс набор исправлений движка, к руке не относящихся (шаг физики под
   частоту кадров, мерцание теней, клон тела оружия, столкновения игрока с мелочью).
2. API (C++ `IHiggsInterface001`, 41 виртуальная функция; Papyrus — 27 натив и 7 событий)
   хорошо отвечает на вопрос «что держит рука», но почти не даёт **управлять** рукой совместно:
   все выключатели — одиночные `bool` без владельца и счётчика (`include/pluginapi.h:95-104`),
   `ForceWeaponCollisionEnabled` — защёлка без отмены (`src/pluginapi.cpp:351-354`), нет
   «отпусти предмет», нет событий о смене тела оружия, у событий столкновения нет ни тела, ни
   точки контакта (`include/higgsinterface001.h:37`).
3. PLANCK уже строится на HIGGS и именно поэтому упирается в эти пробелы: «уронить предмет»
   он изображает парой `DisableHand`/`EnableHand` (`PLANCK:src/main.cpp:3764-3782`), навсегда
   защёлкивает `ForceWeaponCollisionEnabled` обеих рук (`PLANCK:src/main.cpp:8262-8263`) и
   правит общий слой 56 через `SetHiggsLayerBitfield` (`PLANCK:src/main.cpp:8257-8260`).
4. С VRIK HIGGS договаривается порядком хуков, а не контрактом: пальцы HIGGS перезаписывает
   после VRIK (`src/hooks.cpp:527-548`), хотя у VRIK есть `setFingerRange/restoreFingers`
   (`include/vrikinterface001.h:38-40`), которыми HIGGS не пользуется; будущую позу руки и
   оружия VRIK HIGGS **предсказывает сам** (`src/hand.cpp:2765-2782`). Зоны «за плечо» у HIGGS
   (`src/hand.cpp:1927-1935`) и кобуры на плечах у VRIK (`MO2:VRIK Player Avatar/SKSE/Plugins/vrikslots.ini:5`)
   живут независимо.
5. Горячие места: ~7 поисков узла по имени на руку за кадр (+15 на анимирующую пальцы руку),
   перезапуск обхода сцены с корня при сборе связанных тел — квадратичный и выполняется каждый
   кадр при удержании (в установленной сборке `ForcePhysicsGrab = 1`, значит всегда),
   три-четыре запроса к миру Havok на руку за кадр в простое, аллокации `std::set` каждый кадр.

---

## 1. Механики

| # | Механика | Где живёт | Что делает | Замечания |
|---|---|---|---|---|
| 1 | Выбор предмета вблизи | `src/hand.cpp:320-422`, `src/hand.cpp:2452-2717` | Сфера `g_pickSphere` протягивается от ладони (`linearCast`), ближайшее к лучу тело становится выбранным | Сфера — чужая (прицел игры), HIGGS временно меняет ей фильтр, радиус и позицию и возвращает (`src/hand.cpp:327-335`, `src/hand.cpp:417-419`) |
| 2 | Выбор издалека (гравиперчатки, фаза выбора) | `src/hand.cpp:425-519` | Луч + протяжка сферы до точки попадания, фильтр по конусу взгляда | Отключение мародёрства живых/не упавших — `src/hand.cpp:487-496` |
| 3 | Подсветка выбранного | `src/effects.cpp:218-355`, `src/hooks.cpp:221-248`, формы `0x6F00/0x6F01` из `higgs_vr.esp` (`src/main.cpp:915-935`) | Шейдер на выбранных геометриях, красный для чужого | Хуки конструктора `ShaderReferenceEffect` и `SetEffectShaderData` (`src/hooks.cpp:28-29`, `src/hooks.cpp:80-87`) |
| 4 | Луч выбора | `src/hooks.cpp:487-501`, `src/hand.cpp:4852` | Растягивает `SpellOrigin` к выбранному предмету | Патч `0x6AC012` — 7 NOP, чтобы игра не прятала узел (`src/hooks.cpp:1424-1428`) |
| 5 | Хват: кинематический («Held») и физический («HeldBody») | `src/hand.cpp:1145-1158`, `src/hand.cpp:1301-1709`, `src/hand.cpp:3746-3807`, `src/hand.cpp:3809-4040`, `src/RE/havok.cpp:126` | Кинематический — тело переводится в KEYFRAMED и ведётся за рукой; физический — `bhkGroupConstraint` между телом руки и предметом | Физический обязателен для актёров и предметов со связями (`src/hand.cpp:1151-1152`); установлено `ForcePhysicsGrab = 1` (`MO2:HIGGS - Enhanced VR Interaction/SKSE/Plugins/higgs_vr.ini:328`) — все хваты физические |
| 6 | Изменение держимого тела | `src/hand.cpp:1350-1358`, `src/hand.cpp:1636-1670`, `src/hand.cpp:1673-1677` | Тип движения, качество коллизии, инерция (ограничение отношения осей), `contactPointCallbackDelay=0`, контактный слушатель на все связанные тела | Состояние сохраняется и возвращается при отпускании; двуручное удержание одного тела согласовано между руками (`src/hand.cpp:1639-1656`) |
| 7 | Пальцы под геометрию | `src/hand.cpp:1384-1541`, `src/math_utils.cpp:1436-1440`, `src/math_utils.cpp:2054-2062`, `src/finger_curves.cpp:469-503` | При хвате треугольники предмета (с CPU-скиннингом) пересекаются с дугами пальцев; из таблиц кривых берётся степень сгиба 0..1 | Альтернативная кривая большого пальца (`src/hand.cpp:1508-1518`); минимум 0.2 (`src/hand.cpp:1532`) |
| 8 | Анимация пальцев | `src/finger_animator.cpp:55-176`, вызов `src/hooks.cpp:532-539` | Интерполирует локальные трансформы 15 костей пальцев к цели; при отпускании возвращает к позе игры/VRIK | Узлы берутся в скелете 3-го лица при VRIK, 1-го — без (`src/finger_animator.cpp:40`); пишет **после** VRIK |
| 9 | Кривые пальцев (генератор) | `src/finger_curves.cpp:70-420` | Инструмент разработчика: снимает кривые с модели | Выключен (закомментирован вызов `src/hand.cpp:2350`); читает `handSize` VRIK без проверки на null (`src/finger_curves.cpp:72`) |
| 10 | Гравиперчатки: притяжение | `src/hand.cpp:3186-3316`, `src/hand.cpp:3455-3522`, `src/hand.cpp:2063` | Рывок рукой при удержании курка — баллистическая скорость к игроку, временное игнорирование столкновений с игроком | Событие Pulled (`src/hand.cpp:3269`, `src/hand.cpp:3446`) |
| 11 | Снятие брони с тел | `src/hand.cpp:3199-3216`, `src/hand.cpp:3317-3358`, `src/hand.cpp:3402-3453`, `src/hand.cpp:1980-2018` | Выбор надетой вещи на теле, снятие рывком или второй рукой | Владелец вещи меняется на игрока (`src/hand.cpp:3382`, `src/hand.cpp:3417`) |
| 12 | Передача из руки в руку | `src/hand.cpp:2855-2863`, `src/hand.cpp:2393-2401` | Вторая рука просит первую отпустить и забирает | Событие Dropped подавляется, пока другая рука держит то же тело (`src/hand.cpp:2955-2958`, `src/hand.cpp:3138-3142`) |
| 13 | Бросок | `src/hand.cpp:3011-3084`, `src/main.cpp:122-204` | Скорость предмета из пиков скорости контроллера + скорость комнаты; временно без столкновения с рукой | Скорости контроллеров — из `WaitGetPoses` (`src/main.cpp:710-721`) |
| 14 | Спрятать за плечо / съесть у рта | `src/hand.cpp:1908-1978`, `src/hand.cpp:3085-3151` | Отпускание у плеча — `Activate` (в инвентарь), у рта — `EquipItem`/чтение книги | События Stashed/Consumed вызываются **до** активации (`src/hand.cpp:3090`, `src/hand.cpp:3115`) |
| 15 | Шум и разрушение брошенного | `src/physics.cpp:360-382`, `src/physics.cpp:464-529` | Брошенное тело при ударе поднимает событие обнаружения (шум по массе) и наносит урон разрушаемым | Свойство Havok `056614` помечает брошенные тела (`src/physics.cpp:360`) |
| 16 | Тело руки | `src/hand.cpp:559-688` | Кинематический бокс на слое 56, группа игрока, биты рэгдолла руки | Выключается при физическом хвате и двуручном хвате (`src/hand.cpp:658-666`) |
| 17 | Тело оружия | `src/hand.cpp:720-896` | Клон формы из `VRMeleeData.collisionNode`, масштаб по руке VRIK, слой 56 | Пересоздаётся при смене формы или размера руки (`src/hand.cpp:848-856`); выключается: оружие убрано, API, 0.5 с после удара (`src/hand.cpp:863-868`) |
| 18 | Двуручный хват | `src/hand.cpp:1712-1884`, `src/hand.cpp:3524-3744`, `src/hand.cpp:4372-4424` | Вторая рука берётся за оружие первой; HIGGS пишет узлы оружия, смещения, коллизии и жезла | Узел жезла двигается ради ванильного блока (`src/hand.cpp:3705-3711`) |
| 19 | Блок снарядов оружием | `src/hooks.cpp:101-193` | Попадание снаряда в тело руки/оружия HIGGS перенаправляется в ванильное тело ближнего боя и выдаётся за блок щитом | Временно выставляет граф-переменную `IsBlocking` (`src/hooks.cpp:143-152`) |
| 20 | Тактильный отклик | `src/physics.cpp:556-602`, `src/physics.cpp:660-779`, `src/hand.cpp:53-66`, `src/haptics.cpp:8-75` | Импульсы при столкновениях рук/оружия/держимого и при хвате | Свой поток на каждую руку (`include/haptics.h:20-23`) |
| 21 | Звуки физики | `src/hand.cpp:899-983` | Звук по материалу предмета при хвате/броске | На время воспроизведения обнуляет затухание **общего** `BGSSoundDescriptorForm` (`src/hand.cpp:972-977`) |
| 22 | «Пространство игрока» | `src/main.cpp:339-517` | Тела, связанные с игроком (руки, оружие, держимое), получают скорость/телепорт вместе с комнатой | Учитывает смещение камеры VRIK (`src/main.cpp:400-411`) |
| 23 | Гашение соседних тел при хвате | `src/hand.cpp:154-201`, `src/main.cpp:282-336` | Соседние тела временно получают сильное демпфирование | — |
| 24 | Замедление от веса | `src/main.cpp:232-269`, `src/hooks.cpp:747-768` | Масса держимого снижает `SpeedMult` и высоту прыжка | Правит AV игрока `SpeedMult` и дёргает `CarryWeight` (`src/utils.cpp:1084-1093`) |
| 25 | Игрок против мелочи | `src/main.cpp:839-898`, `src/hooks.cpp:794-842`, `src/physics.cpp:534-549`, `src/main.cpp:1071-1075` | Лёгкое не толкается игроком, тяжёлое ведёт себя как статика | Пишет `fMoveLimitMass` |
| 26 | «Havok fix» | `src/hooks.cpp:607-625`, `src/main.cpp:1065-1069` | Длительность шага физики под текущий кадр | Пишет `fMaxTime`/`fMaxTimeComplex` каждый кадр |
| 27 | Исправление теней | `src/physics.cpp:309-358`, `src/main.cpp:207-222`, `src/hooks.cpp:571-579` | Засыпающие острова вызывают внеочередное обновление теней | Пишет `iShadowUpdateFrameDelay` |
| 28 | Клон тела ближнего боя | `src/hooks.cpp:628-696`, `src/hooks.cpp:1446-1450` | Правильный `bhkRigidBodyT` для тела оружия игры | Патч размера аллокации 0x40→0x60 |
| 29 | Подпись на руке и кнопка активации | `src/hooks.cpp:344-473`, `src/hooks.cpp:551-604`, `src/hand.cpp:4589-4850` | Перенос `WSActivateRollover` на тыл руки, свой текст и кнопка | Пишет `fActivateRumbleIntensity:VRInput` |
| 30 | Разбор ввода курка/хвата | `src/hand.cpp:4131-4304` | Задерживает, подавляет и «досылает» нажатия в `VRControllerState` | Приоритет 66 (`src/main.cpp:1167`) |
| 31 | Руки двойной точности, задержка руки | `src/hooks.cpp:845-863`, `src/hand.cpp:2235-2259`, `src/hand.cpp:2437-2450` | Своя `UpdateClavicleToTransformHand` в double | — |
| 32 | Спрятать заклинание в руке | `src/hand.cpp:4050-4072` | Узлы магии опускаются на −10000 | — |
| 33 | Ванильный хват выключен | `src/hooks.cpp:1430-1434` | `RET` в `PlayerCharacter::StartGrabObject` | По настройке |
| 34 | Руки «принадлежат игроку» для триггеров | `src/hooks.cpp:196-218`, `src/hooks.cpp:1436-1444` | Коллизия руки отдаётся как узел игрока; триггеры видят игрока | По настройкам |
| 35 | Физика содержимого | `src/hand.cpp:3847-3857`, `src/physics.cpp:1000` | Тела внутри держимого контейнера учитываются в массе и движении | — |

---

## 2. Когда и где исполняется код (точки кадра и потоки)

| Точка | Где установлена | Что делает HIGGS | Поток |
|---|---|---|---|
| `PlayerCharacter::Update`, перед VRIK (`0x6ABCBF`) | `src/hooks.cpp:46-47`, `src/hooks.cpp:1240-1275` | Весь `Update()` рук: выбор, хват, события API (`src/hooks.cpp:519-524`) | Главный |
| После VRIK, обновление корня 3-го лица (`0x6C6A7D`) | `src/hooks.cpp:49`, `src/hooks.cpp:1413-1416` | Пальцы, `PostVrikUpdate` (`src/hooks.cpp:527-548`) | Главный |
| `UpdateHands` (`0x6ABCAF`) | `src/hooks.cpp:770-779` | Снимок анимированных трансформов руки и оружия | Главный |
| `ApplyMovementDelta` (`0x5E08C2`) | `src/hooks.cpp:781-791` | Пространство игрока, чистка игнорируемых тел (`src/main.cpp:811-836`) | Главный |
| Перед `hkpWorld::stepDeltaTime` (`0xDFB722`) | `src/hooks.cpp:704-717` | Колбэки API; свои `Pre/PostPhysicsStep` пусты (`src/main.cpp:354-362`) | Главный |
| После обновления жезлов, `Main::Draw` (`0x13233AA`) | `src/hooks.cpp:39-40`, `src/hooks.cpp:476-516` | Подпись, луч, скрытие второй руки без VRIK | Главный |
| `CompareFilterInfo` (`0xE2BA10`) | `src/hooks.cpp:1358-1411` | Только цепочка колбэков API | Потоки Havok, сотни раз за кадр (`include/higgsinterface001.h:79`) |
| Контакт-слушатель мира | `src/main.cpp:594`, `src/physics.cpp:660-719` | Регистрация столкновений, тактильный отклик, колбэк Collision | Потоки Havok |
| Пост-симуляция мира | `src/main.cpp:595`, `src/physics.cpp:721-779` | Фронты столкновений, чистка | Havok |
| `RegisterForPoses`, `RegisterForControllerState` | `src/main.cpp:1167-1168` | Скорости контроллеров, разбор ввода | Поток OpenVR игры |
| Потоки тактильного отклика | `include/haptics.h:20-23`, `src/haptics.cpp:36-75` | Опрос очереди раз в 5 мс | Два своих потока |

Порядок относительно VRIK HIGGS описывает только комментариями у адресов: «прямо перед хуком
VRIK» (`src/hooks.cpp:46`), «после хука VRIK updateUI» (`src/hooks.cpp:479`). **Вывод:** это
неявный контракт — если VRIK сдвинет свои точки, HIGGS об этом не узнает.

---

## 3. Ресурсы, которые HIGGS захватывает

Имена — в общем формате с префиксами, чтобы сводить с PLANCK и VRIK.

### 3.1. Хуки и патчи кода

| Ресурс | Доступ | Доказательство |
|---|---|---|
| `hook:PlayerCharacter::Update→UpdateVRBlocking call @0x6ABCBF` | hook | `src/hooks.cpp:46-47`, `src/hooks.cpp:1272` |
| `hook:PlayerCharacter::Update→NiNode::Update(3rd root) call @0x6C6A7D` | hook | `src/hooks.cpp:49`, `src/hooks.cpp:1414` |
| `hook:Main::Draw post-wand call 0xDCF900 @0x13233AA` | hook | `src/hooks.cpp:39-40`, `src/hooks.cpp:1235` |
| `hook:bhkWorld::Update→hkpWorld::stepDeltaTime call @0xDFB722` | hook | `src/hooks.cpp:89`, `src/hooks.cpp:1419` |
| `hook:bhkCollisionFilter::CompareFilterInfo entry @0xE2BA10` | hook (Write6Branch) | `src/hooks.cpp:68`, `src/hooks.cpp:1408` |
| `hook:physics-times call 0xDFB3C0 @0x5BBAEF` | hook (при EnableHavokFix) | `src/hooks.cpp:65-66`, `src/hooks.cpp:1319-1353` |
| `hook:UpdateVRMeleeData→bhkRigidBody_ctor @0x6B0843` | hook | `src/hooks.cpp:55-56`, `src/hooks.cpp:1314` |
| `hook:vtbl PlayerCharacter::Update slot 0xAF @0x16E27A8` | hook | `src/hooks.cpp:721`, `src/hooks.cpp:1452-1455` |
| `hook:vtbl MissileProjectile::UpdateImpactFromCollector @0x16FE4F0` | hook | `src/hooks.cpp:104`, `src/hooks.cpp:1457-1458` |
| `hook:vtbl ArrowProjectile::UpdateImpactFromCollector @0x16F99A0` | hook | `src/hooks.cpp:105`, `src/hooks.cpp:1460-1461` |
| `hook:ArrowProjectile::AddImpact→GetRigidBody @0x75CF10` | hook | `src/hooks.cpp:95`, `src/hooks.cpp:1464` |
| `hook:Actor::Jump→SetJumping @0x5DA6FB` | hook | `src/hooks.cpp:758`, `src/hooks.cpp:1469` |
| `hook:PlayerCharacter::VRUpdate→UpdateHands @0x6ABCAF` | hook | `src/hooks.cpp:770`, `src/hooks.cpp:1475` |
| `hook:Actor::UpdateMovement→GetCharacterController @0x5E08C2` | hook | `src/hooks.cpp:781`, `src/hooks.cpp:1481` |
| `hook:bhkLinearCaster::linearCast→hkpWorld::linearCast @0x3AFD84` | hook | `src/hooks.cpp:820`, `src/hooks.cpp:1487` |
| `hook:UpdateHands→UpdateClavicleToTransformHand @0x6AC765/@0x6AC88C` | hook | `src/hooks.cpp:845-846`, `src/hooks.cpp:1493-1498` |
| `hook:vtbl hkpCachingShapePhantom::setPositionAndLinearCast @0x17B6890` | hook | `src/hooks.cpp:796`, `src/hooks.cpp:1503-1504` |
| `hook:UpdateShadows call @0x1321DA7` | hook | `src/hooks.cpp:572`, `src/hooks.cpp:1508` |
| `hook:ShaderReferenceEffect ctor call 0x564DD0 @0x2AE3E8` | hook | `src/hooks.cpp:28-29`, `src/hooks.cpp:964` |
| `hook:CrosshairPickData::Pick→LinearCast @0x3BA3D7` | hook | `src/hooks.cpp:35-36`, `src/hooks.cpp:995` |
| `hook:BSLightingShaderProperty::SetEffectShaderData @0x2292AE` | hook | `src/hooks.cpp:80-81`, `src/hooks.cpp:1023` |
| `hook:TESEffectShader::SetEffectShaderData @0x56761E` | hook | `src/hooks.cpp:84-85`, `src/hooks.cpp:1095` |
| `hook:shader flags check @0x229243` | hook | `src/hooks.cpp:87`, `src/hooks.cpp:1056` |
| `hook:GetActivateText @0x6D3337` | hook | `src/hooks.cpp:70`, `src/hooks.cpp:1140` |
| `hook:RefreshActivateButtonArt @0x53F18E` | hook | `src/hooks.cpp:72`, `src/hooks.cpp:1196` |
| `hook:GetRefFromCollidable→GetNodeFromCollidable @0x3B495B` | hook (по настройке) | `src/hooks.cpp:91`, `src/hooks.cpp:1436-1439` |
| `hook:TriggerEntry::RegisterOverlap→IsInRagdollState @0x3B6E23` | hook (по настройке) | `src/hooks.cpp:93`, `src/hooks.cpp:1441-1444` |
| `patch:0x6AC012 7×NOP (не прятать SpellOrigin)` | patch | `src/hooks.cpp:58`, `src/hooks.cpp:1424-1428` |
| `patch:0x6CC000 RET (PlayerCharacter::StartGrabObject)` | patch | `src/hooks.cpp:60`, `src/hooks.cpp:1430-1434` |
| `patch:0x6B0828 mov ecx,0x60 (тело ближнего боя 0x40→0x60)` | patch | `src/hooks.cpp:62`, `src/hooks.cpp:1446-1450` |

Объявлены, но не ставятся: `pickHookLoc`, `preVRIKMainThreadHookLoc`, `playerCharacterUpdateHookLoc`,
`pickRayCastHookLoc`, `worldUpdateHookLoc`, `physicsUpdateHookLoc`, два адреса
`hkpCharacterProxy_integrateImplementation` (`src/hooks.cpp:31`, `src/hooks.cpp:42-43`,
`src/hooks.cpp:51`, `src/hooks.cpp:74-78`, `src/hooks.cpp:97-98`; установки для них нет в
`PerformHooks`, `src/hooks.cpp:919-1519`).

### 3.2. Havok и коллизии

| Ресурс | Доступ | Доказательство |
|---|---|---|
| `collision:layer 56 L_HIGGSCOLLISION` (руки, оружие) | own | `src/physics.cpp:831-839`; маска по умолчанию `0x01053343161b7fff` — `include/pluginapi.h:101-102` |
| `havok:bhkCollisionFilter.layerBitfields[0..63] бит 56` | write | `src/physics.cpp:852-863`, пересинхронизация при расхождении `src/physics.cpp:841-850` |
| `collision:filterInfo bit14 (выключить коллизию) у тел рук/оружия` | own | `src/hand.cpp:656-676`, `src/hand.cpp:870-887` |
| `collision:filterInfo bit15 + ragdoll bits 8-12 (LeftHand=5, RightHand=3)` | own | `src/hand.cpp:576-580`, `include/physics.h:210-222` |
| `collision:player collision group (из тела NPC COM)` | read | `src/hand.cpp:2323-2336` |
| `collision:CompareFilterInfo override chain` | own | `src/pluginapi.cpp:208-218` |
| `havok:hand rigid bodies (keyframed box)` | own | `src/hand.cpp:559-614` |
| `havok:weapon rigid bodies (клон формы VRMeleeData)` | own | `src/hand.cpp:720-791` |
| `havok:grab constraint bhkGroupConstraint hand↔held` | own | `src/hand.cpp:1616-1619` |
| `havok:held body motionType/quality/inertia/contactPointCallbackDelay` | write | `src/hand.cpp:1350-1358`, `src/hand.cpp:1644-1669`, `src/hand.cpp:1673-1677` |
| `havok:world contact listener` | own | `src/main.cpp:594` |
| `havok:world post-simulation listener` | own | `src/main.cpp:595` |
| `havok:world island activation listener` | own | `src/main.cpp:597-599` |
| `havok:player hkpCharacterProxy listener (обнуляет скорость поверхностей)` | own | `src/main.cpp:547-560`, `src/physics.cpp:534-549` |
| `havok:entity contact listener на держимых/брошенных` | write | `src/hand.cpp:1637`, `src/physics.cpp:362-382` |
| `havok:property 056614 (брошено HIGGS)` | own | `src/physics.cpp:360-371` |
| `havok:g_pickSphere (сфера прицела игры) — фильтр, радиус, позиция` | write (временно) | `src/hand.cpp:2374`, `src/hand.cpp:327-335`, `src/hand.cpp:417-419` |
| `havok:velocity/transform тел пространства игрока` | write | `src/main.cpp:432-499` |
| `havok:damping соседних тел` | write | `src/main.cpp:290-336` |
| `havok:player proxy cast hits (удаление попаданий мелочи)` | write | `src/main.cpp:839-898`, `src/hooks.cpp:797-842` |

### 3.3. Узлы, настройки, ввод, формы, события

| Ресурс | Доступ | Доказательство |
|---|---|---|
| `node:NPC R/L Finger00..42 local transform (3-е лицо при VRIK)` | write, после VRIK | `src/finger_animator.cpp:37-53`, `src/finger_animator.cpp:81-107`, `src/hooks.cpp:537-542` |
| `node:1st-person hand bone + clavicle world transform` | write | `src/hand.cpp:2235-2259`, `src/hand.cpp:4100-4127` |
| `node:WEAPON/SHIELD fp, WeaponOffsetNode, CollisionOffsetNode, Left/RightWandNode local` | write (двуручный хват) | `src/hand.cpp:3678-3711`, откат `src/hand.cpp:3722-3737` |
| `node:Primary/SecondaryMagicOffsetNode, MagicAimNode` | write | `src/hand.cpp:4050-4072` |
| `node:SpellOrigin visibility/transform` | write | `src/hooks.cpp:487-501` |
| `node:WSActivateRollover transform/scale/alpha` | write | `src/hooks.cpp:360-471` |
| `node:Left/RightControllerNode scale (без VRIK)` | write | `src/hooks.cpp:503-515` |
| `node:VRMeleeData.collisionNode` | read + `UpdateVRMeleeCollision` | `include/utils.h:62`, `src/hand.cpp:726-752`, `src/hand.cpp:3693-3696` |
| `node:NPC COM [COM ]` | read | `src/hand.cpp:2323-2324` |
| `anim:IsBlocking (подмена на время удара снаряда)` | write | `src/hooks.cpp:143-152`, `src/hooks.cpp:169` |
| `setting:fMaxTime:HAVOK`, `setting:fMaxTimeComplex:HAVOK` | write каждый кадр | `src/hooks.cpp:607-625` |
| `setting:uMaxNumPhysicsStepsPerUpdate(+Complex)` | write при загрузке | `src/main.cpp:1065-1069` |
| `setting:fMoveLimitMass` | write | `src/main.cpp:1071-1075` |
| `setting:fActivateRumbleIntensity:VRInput` | write | `src/main.cpp:1053-1057`, `src/hooks.cpp:397-402`, `src/hooks.cpp:430-433` |
| `setting:bAlwaysShowHands:VR` | write | `src/main.cpp:1059-1063` |
| `setting:iShadowUpdateFrameDelay` | write | `src/main.cpp:207-222` |
| `setting:higgs_vr.ini [Settings]` | own | `src/config.cpp:196-548`, путь `src/config.cpp:576-590` |
| `input:trigger/grip (VRControllerState: подавление и досылка)` | write | `src/hand.cpp:4131-4304`, `src/main.cpp:1167` |
| `input:controller poses (скорости)` | read | `src/main.cpp:122-204`, `src/main.cpp:1168` |
| `haptics:OpenVR controller L/R` | write, свой поток на руку | `src/haptics.cpp:8-75`, `include/haptics.h:20-28` |
| `form:higgs_vr.esp 0x6F00/0x6F01 (TESEffectShader)` | own | `src/main.cpp:915-935` |
| `form:BGSSoundDescriptorForm dbAttenuation (мягкие звуки)` | write временно | `src/hand.cpp:972-977` |
| `form:ActorValue SpeedMult/CarryWeight (игрок)` | write | `src/utils.cpp:1084-1093`, `src/main.cpp:249-269` |
| `event:TESHitEvent` | sink | `src/main.cpp:770-793`, `src/main.cpp:942-945` |
| `event:MenuOpenCloseEvent` | sink | `src/main.cpp:937-940` |
| `event:SKSE mod message 0xF9279A57 (выдача API)` | own | `src/pluginapi.cpp:11-35`, `src/main.cpp:1096-1099` |
| `api:VRIK getFinalCameraOffsettingAmount/getFinalSmoothingOffsettingAmount` | call | `src/main.cpp:400-401` |
| `api:VRIK getBuildNumber` (требует ≥80400) | call | `src/main.cpp:1102-1109` |
| `api:VRIK getSettingDouble("handSize")` | call (только генератор кривых) | `src/finger_curves.cpp:72` |

---

## 4. C++ API: `IHiggsInterface001`

Получение: сообщение SKSE `0xF9279A57` в «HIGGS» (`src/higgsinterface001.cpp:3-30`), слушатель
регистрируется на `kMessage_PostLoad` (`src/main.cpp:1096-1099`). Ревизия интерфейса — одна,
`1` (`src/pluginapi.cpp:21-26`). Номер сборки `1*1000000+10*10000+10*100+0 = 1101000`
(`src/pluginapi.cpp:38`).

### 4.1. Функции

| Функция | Что даёт | Чего не хватает тому, кто строит на HIGGS | Доказательство |
|---|---|---|---|
| `GetBuildNumber` | Номер сборки | Нет карты «сборка → какие функции есть»; единственный способ узнать о функции — сравнение с числом | `include/higgsinterface001.h:16`, `PLANCK:src/main.cpp:8247-8250` |
| `AddPulledCallback(isLeft, refr)` | Рука начала притягивать | Нет отписки; нет тела/узла; вызов в главном потоке внутри `Update` | `include/higgsinterface001.h:19-20`, `src/hand.cpp:3269`, `src/hand.cpp:3446` |
| `AddGrabbedCallback(isLeft, refr)` | Предмет взят | Нет узла, тела, режима (keyframed/constraint), признака «из другой руки»; нет отписки | `include/higgsinterface001.h:22-23`, `src/hand.cpp:1702` |
| `AddDroppedCallback(isLeft, refr)` | Отпущено | Не приходит, если та же вещь осталась в другой руке; не различает бросок и отпускание; нет скорости | `src/hand.cpp:2955-2958`, `src/hand.cpp:3138-3142` |
| `AddStashedCallback / AddConsumedCallback` | Спрятано / съедено (только базовая форма) | Нельзя отменить (вызывается до `Activate`, но результата не ждёт) | `include/higgsinterface001.h:28-34`, `src/hand.cpp:3090-3093`, `src/hand.cpp:3115-3126` |
| `AddCollisionCallback(isLeft, mass, separatingVelocity)` | Столкновение руки/оружия/держимого | Нет тела, ссылки, точки, нормали, признака «рука/оружие/держимое»; зовётся из потоков Havok без оговорки; для двуручного — дважды | `include/higgsinterface001.h:37-38`, `src/physics.cpp:556-602` |
| `GrabObject(refr, isLeft)` | Запрос хвата | Асинхронно, без результата: при `!CanGrabObject` молча ничего; нет задания узла, трансформа, режима | `src/pluginapi.cpp:250-256`, `src/hand.cpp:2387-2391`, `src/hand.cpp:2020-2039` |
| `GetGrabbedObject(isLeft)` | Держимая ссылка | Проверка состояния без блокировки, «somewhat thread-safe» | `src/pluginapi.cpp:258-274` |
| `IsHandInGrabbableState / CanGrabObject` | Готова ли рука | Нет причины отказа (оружие, API, состояние) | `src/hand.cpp:4503-4535` |
| `DisableHand / EnableHand / IsDisabled` | Выключить хват рукой | Один `atomic<bool>` на руку: два мода выключают — первый включивший снимает запрет второго | `include/pluginapi.h:95-96`, `src/pluginapi.cpp:291-319` |
| `DisableWeaponCollision / EnableWeaponCollision / IsWeaponCollisionDisabled` | Выключить тело оружия | Тот же одиночный `bool`; `IsWeaponCollisionDisabled` отвечает только про флаг API, а не про фактическое состояние (оружие убрано, окно после удара) | `include/pluginapi.h:98-99`, `src/hand.cpp:863-887` |
| `ForceWeaponCollisionEnabled(isLeft)` | Отключить «выключение после удара» | Защёлка без отмены; по тексту заголовка — «держать включённой», по коду — только гасит окно после удара | `include/higgsinterface001.h:100-101`, `src/pluginapi.cpp:351-354`, `src/hand.cpp:864-868` |
| `IsTwoHanding` | Двуручный хват идёт | Нет: какая рука главная, какое оружие | `src/pluginapi.cpp:356-359` |
| `AddStart/StopTwoHandingCallback()` | Начало/конец двуручного | Без аргументов | `include/higgsinterface001.h:63-68`, `src/hand.cpp:1880`, `src/hand.cpp:3179`, `src/hand.cpp:3739` |
| `AddCollisionFilterComparisonCallback` | Решать, сталкиваются ли две `filterInfo` | Первый не-`Continue` побеждает, порядок = порядок регистрации (порядок загрузки); нет приоритета и отписки | `src/pluginapi.cpp:208-218` |
| `AddPrePhysicsStepCallback(world)` | Перед шагом Havok | Нет пост-шага | `src/hooks.cpp:706-717` |
| `Get/SetHiggsLayerBitfield` | Маска слоя 56 | Общая изменяемая маска без владельца: кто последний записал, тот и прав; применяется при следующем кадре | `src/pluginapi.cpp:388-396`, `src/physics.cpp:841-850`, `PLANCK:src/main.cpp:8257-8260` |
| `GetHandRigidBody / GetWeaponRigidBody` | Сырые `bhkRigidBody` | Без ссылки и срока жизни: тело оружия пересоздаётся при смене оружия/размера руки и удаляется при смене мира — события об этом нет | `src/pluginapi.cpp:367-377`, `src/hand.cpp:848-861`, `src/main.cpp:562-609` |
| `GetGrabbedRigidBody` | Тело, за которое держат | Одно тело; связанных тел (рэгдолл) API не даёт | `src/pluginapi.cpp:379-386`, `src/hand.cpp:1549-1559` |
| `IsHoldingObject` | Держит ли | — | `src/pluginapi.cpp:410-414` |
| `GetFingerValues(isLeft, values[5])` | Сгиб пальцев | Отдаёт **целевые значения последнего хвата** `grabbedFingerValues`, а не текущую позу; после отпускания — устаревшие; нет признака «HIGGS сейчас ведёт пальцы» | `src/pluginapi.cpp:416-422`, `src/hand.cpp:1524-1539`, `include/finger_animator.h:38` |
| `AddPre/PostVrikPre/PostHiggsCallback` | Четыре точки кадра вокруг VRIK и HIGGS | Хорошая основа; но гарантии держатся на соседстве хуков с VRIK | `src/hooks.cpp:519-548` |
| `Deprecated1/2(std::string_view&, std::string&)` | Старые настройки | В ABI виртуальной таблицы висят типы STL — зависимость от версии компилятора потребителя | `include/higgsinterface001.h:115-117` |
| `Get/SetGrabTransform` | Рука→узел держимого | Можно писать в любом состоянии, без проверки; значение живёт до следующего хвата | `src/pluginapi.cpp:398-408`, `src/hand.cpp:2262-2271` |
| `Get/SetSettingDouble(name)` | Любая числовая настройка | 16 векторных/табличных ключей не доступны; перезагрузка ini молча затирает то, что записал мод; уведомления нет | `src/config.cpp:154-194`, `src/config.cpp:204-229`, `src/config.cpp:550-574` |
| `GetGrabbedNodeName` | Имя узла держимого | — | `src/pluginapi.cpp:276-283` |

### 4.2. Сквозные пробелы

1. **Владение выключателями.** Каждый выключатель — одно значение на руку
   (`include/pluginapi.h:95-104`). Пример из жизни: PLANCK, чтобы «уронить» держимое, зовёт
   `DisableHand`, а когда рука опустела — `EnableHand` (`PLANCK:src/main.cpp:3764-3782`); если
   в этот момент руку выключил другой мод, PLANCK снимет и его запрет. Нужны токены владельца
   или счётчик: `AcquireHandDisable(owner) → handle`, `Release(handle)`, `GetDisableOwners`.
2. **Нет команды «отпустить».** Отпускание — только через ввод или запрет руки
   (`src/hand.cpp:2940-2950`). Нужны `ReleaseObject(isLeft, velocity?)` и `ThrowObject`.
3. **Нет состояния руки.** Внутренний автомат из 15 состояний (`include/hand.h:127-144`)
   наружу не выдан; есть лишь четыре булевых среза. Нужен `GetHandState(isLeft)` и событие
   смены состояния.
4. **Нет событий о телах.** Смена/удаление `weaponBody` и `handBody`, смена мира Havok
   (`src/main.cpp:562-609`) — без уведомления, хотя потребители держат сырые указатели.
5. **Столкновения бедны.** Колбэк несёт массу и скорость; тело, ссылку, точку и нормаль
   HIGGS знает (`src/physics.cpp:660-719`), но не передаёт. Потребитель, которому нужно «рука
   ударила актёра X в точке Y», вынужден ставить свой контакт-слушатель мира — второй такой
   же, как у HIGGS.
6. **Потоки и время не описаны.** Колбэки хранятся в `std::vector`; добавление — под
   мьютексом, чтение — без (`src/pluginapi.cpp:61-143` против `src/pluginapi.cpp:146-248`).
   **Вывод:** регистрировать безопасно только до начала игры, но заголовок этого не говорит.
   Колбэк столкновения приходит из потоков Havok, фильтровый — сотни раз за кадр, остальные —
   из главного; в заголовке оговорена только частота фильтрового (`include/higgsinterface001.h:79`).
7. **Готовность.** Руки создаются на `kMessage_DataLoaded` и только если `higgs_vr.esp`
   активен (`src/main.cpp:902-913`, `src/main.cpp:1016-1017`); функции API разыменовывают
   `g_leftHand/g_rightHand` без проверки (`src/pluginapi.cpp:252`, `src/pluginapi.cpp:260`).
   Нужен `IsReady()`.
8. **Физическая рука против логической.** `isLeft` в HIGGS — физическая рука; вторичная рука
   вычисляется как `*g_leftHandedMode != isLeft` (`src/hand.cpp:843`, `src/hand.cpp:4054`).
   VRIK в кобурах принимает `secondaryHand` (`VRIK-API:57-60`), в слотах — `0/1/2`. Нужна
   явная пара функций перевода или везде одна семантика.
9. **Версионирование.** Новые функции дописывались в конец той же `IHiggsInterface001`,
   ревизия осталась `1` (`src/pluginapi.cpp:21-26`). **Вывод:** старый потребитель на новом
   HIGGS работает, новый на старом упадёт при вызове отсутствующей записи таблицы; защита —
   только ручная проверка номера сборки, как у PLANCK.
10. **Нет отписки** ни от одного колбэка (все `Add*`, `include/higgsinterface001.h:19-113`).

---

## 5. Papyrus API (`HiggsVR`)

| Группа | Функции | Доказательство | Пробел |
|---|---|---|---|
| Настройки | `GetSetting` (−2.71828 при отсутствии), `SetSetting` | `src/papyrusapi.cpp:7-15`, `MO2:…/Source/Scripts/HiggsVR.psc:8-11` | Магическое значение вместо признака |
| Хват | `GrabObject`, `GetGrabbedObject`, `GetGrabbedNodeName`, `CanGrabObject` | `src/papyrusapi.cpp:17-31` | Нет `IsHoldingObject`, `GetFingerValues`, `Get/SetGrabTransform` |
| Выключатели | `Disable/EnableHand`, `IsDisabled`, `Disable/EnableWeaponCollision`, `IsWeaponCollisionDisabled` | `src/papyrusapi.cpp:33-55` | Те же одиночные флаги, что и в C++ |
| Двуручный | `IsTwoHanding` | `src/papyrusapi.cpp:57-59` | — |
| События | `OnObjectPulled/Grabbed/Dropped/Stashed/Consumed`, `OnStart/StopTwoHanding` | `src/papyrusapi.cpp:61-283`, `HiggsVR.psc:59-87` | Нет события столкновения |
| Регистрация | `RegisterFor…Event(Form)` ×7 + `Unregister…` | `src/papyrusapi.cpp:304-323` | Регистрации не сериализуются: интерфейс сериализации не запрашивается (`src/main.cpp:1144-1196`). **Вывод:** подписка живёт до выхода из игры и не чистится при загрузке другого сохранения |

---

## 6. INI как API

- Около 239 ключей регистрируются в картах и доступны через `Get/SetSettingDouble`
  (`src/config.cpp:196-548`, `Register*`); 16 ключей — векторы и таблицы (`PalmPosition`,
  `HandCollisionBoxHalfExtents`, `RightShoulderHmdOffset`, `MouthHmdOffset`, …) — читаются
  через `ReadVector/ReadFloatMap` и через API **недоступны** (`src/config.cpp:204-229`).
- Поиск идёт по четырём `std::map` подряд (`src/config.cpp:154-194`).
- Какая настройка действует после изменения на лету, не описано: заголовок говорит «зависит от
  того, когда читается» (`include/higgsinterface001.h:124-125`).
- `reloadConfigIfModified` перечитывает весь ini по дате файла (`src/config.cpp:550-574`) и
  затирает значения, выставленные модами; в установленной сборке выключено
  (`MO2:HIGGS - Enhanced VR Interaction/SKSE/Plugins/higgs_vr.ini:587`), в отладочной сборке
  включено всегда (`src/main.cpp:538-545`).

Установленные значения, важные для разбора тройки
(`MO2:HIGGS - Enhanced VR Interaction/SKSE/Plugins/higgs_vr.ini`): `EnableWeaponCollision = 1`
(291), `useVrikWeaponTransform = 1` (294), `WeaponCollisionDisableOnHitTime = 0.5` (298),
`ForcePhysicsGrab = 1` (328), `EnableHavokFix = 1` (331), `minPhysicsFrameRate = 70.0` (335),
`EnableShadowUpdateFix = 1` (341), `doPhysicsGrabPlayerMovementCompensation = 1` (416),
`DisableVanillaGrab = 1` (466), `treatHandCollisionAsBelongingToPlayer = 1` (469),
`allowAllPlayerCollisionForTriggers = 1` (471), `spoofProjectileWeaponHitsAsIfBlocked = 1` (579),
`doDoublePrecision = 1` (582), `handleVrikOffsetting = 1` (584).

---

## 7. Стыки с VRIK и PLANCK

### 7.1. Что HIGGS берёт у VRIK и где изобретает своё

| Стык | Как сейчас | Доказательство | Что было бы «одним целым» |
|---|---|---|---|
| Пальцы | HIGGS пишет локальные трансформы пальцев после VRIK, VRIK не спрашивает; обратный переход — интерполяция к позе, оставленной VRIK | `src/hooks.cpp:532-542`, `src/finger_animator.cpp:77-107` | Один владелец пальцев с арбитражем: HIGGS заявляет «держу, вот цель» через API VRIK (`setFingerRange`, `VRIK-API:41-43`) или VRIK отдаёт слой поверх себя |
| Будущая поза руки и оружия | HIGGS сам пересчитывает, где VRIK поставит оружие в этом кадре | `src/hand.cpp:2765-2782` | VRIK публикует решённые трансформы кадра (или колбэк после решения) |
| Смещение камеры | HIGGS вычитает/прибавляет смещение VRIK вокруг своего `Update` | `src/hand.cpp:4100-4127`, `src/main.cpp:400-411` | Уже через API VRIK — пример правильного стыка |
| Размер руки | Читается масштаб узла руки 3-го лица, а не настройка VRIK | `src/hand.cpp:2194-2199`, `src/hand.cpp:747-750` | Годится; но смена масштаба пересоздаёт тело оружия без события |
| Плечи | HIGGS: «за плечо — в инвентарь» по своим сферам; VRIK: кобуры 11/12 на плечах | `src/hand.cpp:1927-1935`; `MO2:VRIK Player Avatar/SKSE/Plugins/vrikslots.ini:5`, `vrikslots.ini:352-366` | Одна карта зон тела; HIGGS спрашивает `GetHolsterSlotInReach` (`VRIK-API:57`) прежде, чем прятать |
| Копия контракта VRIK | В HIGGS лежит старая версия без кобур | `include/vrikinterface001.h:12-50` против `VRIK-API:57-74` | Общий заголовок из одного места |
| Скрытие второго контроллера | Без VRIK HIGGS прячет узел контроллера сам | `src/hooks.cpp:503-515` | Дело VRIK, когда он есть; HIGGS это и учитывает (`g_isVrikPresent`, `src/main.cpp:951`) |
| Тактильный отклик | Свой поток на руку; VRIK тоже вибрирует (`vrikslots.ini:13`) | `src/haptics.cpp:36-75` | Один диспетчер вибрации (очередь с приоритетами) |

### 7.2. Что PLANCK берёт у HIGGS (по коду PLANCK)

| Вызов PLANCK | Зачем (вывод по контексту) | Доказательство | Пробел API HIGGS, который он обходит |
|---|---|---|---|
| `AddCollisionFilterComparisonCallback` | Свои правила столкновений рук/оружия с рэгдоллами | `PLANCK:src/main.cpp:8252` | Порядок разрешения конфликтов не задан |
| `AddPrePhysicsStepCallback` | Своя работа перед шагом | `PLANCK:src/main.cpp:8253` | Нет пост-шага |
| `AddPreVrikPreHiggsCallback` | Кадровая точка до HIGGS | `PLANCK:src/main.cpp:8254` | — |
| `AddGrabbedCallback` | Реакция на хват (в т.ч. актёра) | `PLANCK:src/main.cpp:8255` | Нет узла/тела/режима в событии |
| `Get/SetHiggsLayerBitfield` | Добавить `Biped`/`BipedNoCC` в маску слоя 56 | `PLANCK:src/main.cpp:8257-8260` | Общая маска без владельца |
| `ForceWeaponCollisionEnabled(false/true)` | Выключить «гашение после удара» HIGGS — удары ведёт PLANCK | `PLANCK:src/main.cpp:8262-8263` | Защёлка без отмены |
| `DisableHand/EnableHand` | «Уронить» держимое | `PLANCK:src/main.cpp:3764-3782` | Нет `ReleaseObject` |
| `GetHand/Weapon/GrabbedRigidBody`, `GetGrabbedObject`, `GetGrabTransform` | Опознать тела HIGGS в своих контактах | `PLANCK:src/main.cpp:4263-4304`, `PLANCK:src/main.cpp:4057` | Нет события смены тел; нет запроса «чьё это тело» |
| `IsTwoHanding` | Учесть двуручный хват | `PLANCK:src/main.cpp:2245` | Нет «какая рука ведёт» |

**Вывод:** «попадание по актёру» в тройке сейчас распределено так: тела оружия и рук — HIGGS,
решение «сталкиваться ли с рэгдоллом» — PLANCK через фильтровый колбэк HIGGS и через маску
слоя 56, выключение тела оружия после удара — отнято у HIGGS защёлкой. Разделение верное по
смыслу, но держится на побочных эффектах API, а не на явном контракте.

---

## 8. Что добавить в API (предложение, `IHiggsInterface002`)

Описано как направление, а не как готовое решение; всё ниже — **предложение**.

| Предложение | Какой пробел закрывает | Кто выиграет |
|---|---|---|
| `GetHandState(isLeft) → enum` + `AddHandStateChangedCallback` | 4.2 п.3 | PLANCK, моды интерфейса, VRIK-жесты |
| `ReleaseObject(isLeft, optional velocity)` | 4.2 п.2 | PLANCK (замена DisableHand/EnableHand) |
| Токены владельца: `AcquireHandDisable/ReleaseHandDisable`, `AcquireWeaponCollisionOverride(mode)` с отменой | 4.2 п.1, защёлка | Любые два мода одновременно |
| `AddCollisionCallbackEx(struct{isLeft, source=hand/weapon/held, otherBody, otherRefr, point, normal, separatingVelocity, mass})` с явным «поток Havok» | 4.2 п.5 | PLANCK (не нужен второй контакт-слушатель мира) |
| `AddBodiesChangedCallback(isLeft, kind, oldBody, newBody)` | 4.2 п.4 | Все, кто держит указатели на тела |
| `GetGrabbedRigidBodies(isLeft, out[], n)` | связанные тела рэгдолла | PLANCK |
| `SetFingerOverride(isLeft, values, owner)` / `ClearFingerOverride` и `GetCurrentFingerPose` | пальцы, 7.1 | Моды оружия и жестов; стык с VRIK |
| `QueueHaptic(isLeft, start, end, duration, priority)` | 7.1, два диспетчера вибрации | Все |
| `IsReady()`; `GetInterfaceRevision()`; новая ревизия при каждом расширении | 4.2 п.7 и 9 | Все |
| Отписка `Remove*Callback`; явная фраза о потоке и допустимом моменте регистрации в заголовке | 4.2 п.6 и 10 | Все |
| Papyrus: `IsHoldingObject`, `GetFingerValues`, событие столкновения, сериализация регистраций | раздел 5 | Скриптовые моды |
| Векторные настройки в `Get/SetSetting` (по компонентам `PalmPositionX`…) | раздел 6 | Моды калибровки |

---

## 9. Производительность

| Место | Что происходит | Цена | Что можно сделать |
|---|---|---|---|
| `src/hand.cpp:2323-2324` | `GetObjectByName("NPC COM [COM ]")` от корня скелета — каждый кадр, на каждую руку | Обход скелета ×2 за кадр | Кешировать узел, сбрасывать при смене 3D игрока |
| `src/hand.cpp:4427-4453` (`PostVrikUpdate`) | `GetThirdPersonHandNode` (`src/hand.cpp:2081-2090`), `GetWeaponNode(true/false)` (`src/hand.cpp:2174-2181`), ещё раз `GetThirdPersonHandNode` из `GetHandSize` (`src/hand.cpp:2194-2199`) | 4 поиска по имени на руку за кадр | Кеш по имени с проверкой родителя |
| `src/hand.cpp:4462-4471`, `src/hand.cpp:4086-4097` | Ещё по `GetWeaponNode(false)` на руку за кадр в `PreUpdateHandsUpdate` и `PreUpdate` | +2 поиска на руку | То же |
| `src/finger_animator.cpp:37-60` | 15 поисков узлов пальцев каждый кадр, пока рука анимирует пальцы (при хвате — всегда) | 15 поисков, первые 5 — от корня | Заполнить один раз, обновлять при смене скелета |
| `src/finger_animator.cpp:24-35` | Левая рука: кватернион→матрица→Эйлер→матрица на каждую фалангу каждый кадр | 15 тригонометрических преобразований | Зеркальные кривые посчитать один раз |
| `src/hooks.cpp:374-376` | Поиск `WSActivateRollover` от узла комнаты каждый кадр | Обход поддерева | Кеш |
| `src/hooks.cpp:397` | `GetINISetting("fActivateRumbleIntensity:VRInput")` каждый кадр, пока показана подпись | Строковый поиск настройки | Получить указатель один раз (как в `src/main.cpp:1055`) |
| `src/hand.cpp:2452-2476`, `src/hand.cpp:320-519`, `src/hand.cpp:275-317` | В простое на каждую руку: протяжка сферы вблизи; если пусто — луч + протяжка вдаль; при двуручном оружии — ещё протяжка по **всему миру**, чтобы найти одно известное тело (TODO в коде, `src/hand.cpp:300`); каждый запрос берёт блокировку мира отдельно (`src/hand.cpp:347`, `src/hand.cpp:437-439`, `src/hand.cpp:462`) | До 4 запросов к миру на руку за кадр | Прямой запрос к форме оружия; объединить блокировки |
| `src/utils.cpp:1132-1186` | `CollectAllConnectedRigidBodies`: помощник возвращается сразу после первой добавленной пары, внешний цикл перезапускает обход **с корня** | O(число тел × число узлов); для рэгдолла — десятки проходов по сотням узлов | Один проход по связям тел (BFS по `constraints`) |
| `src/hand.cpp:3834-3836`, `src/hand.cpp:3482-3484` | Этот сбор вызывается каждый кадр в `HeldBody` и `Pulled`; при `ForcePhysicsGrab = 1` любой хват — `HeldBody` | Квадратичный обход каждый кадр удержания | Считать один раз при хвате, пересчитывать при изменении связей |
| `src/utils.cpp:1211-1219` | `CollectRelevantNonMoveableRigidBodies` копирует `std::set` | Аллокации каждый кадр | Вектор-буфер |
| `src/utils.cpp:1265-1303`, вызов `src/hand.cpp:3863-3866` | `ForEachAdjacentBody`: сбор всех связей обходом сцены + копия `std::set` на каждую «волну», каждый кадр при удержании актёра | Обход + аллокации | Граф связей рэгдолла кешировать на хват |
| `src/main.cpp:274-276`, `src/main.cpp:416-418`, `src/main.cpp:501-505` | `std::set<NiPointer>` + два `unordered_set` пересобираются каждый кадр | Аллокации узлов каждый кадр | Плоский вектор с `reserve` |
| `src/main.cpp:228-236`, `src/main.cpp:642` | `unordered_set` масс чистится и наполняется каждый кадр | Аллокации | Вектор с сортировкой/флагом кадра |
| `src/physics.cpp:660-719` | Слушатель контактов на **всём** мире: каждый контакт мира проходит `IsHandOrWeaponOrHeld` для двух тел | Дёшево на контакт, но на каждый контакт мира | Слушатели на сущностях HIGGS вместо мира (**вывод**: потребует отдельной обработки держимого) |
| `src/physics.cpp:611-627` | `find` в `unordered_map` без блокировки, вставка под блокировкой — из нескольких потоков Havok | Гонка данных (UB), не только цена | Блокировка на чтение или по-потоковые буферы |
| `src/hooks.cpp:1358-1411` | Хук `CompareFilterInfo` ставится всегда, даже без колбэков; на каждое сравнение — сохранение регистров и вызов | Сотни вызовов за кадр | Ставить только при первой регистрации колбэка |
| `src/haptics.cpp:36-75` | Два потока постоянно просыпаются раз в 5 мс и берут мьютекс даже без событий | Фоновая нагрузка | Ожидание на условной переменной |
| `src/main.cpp:538-545`, `src/config.cpp:550-574` | При `reloadConfigIfModified` — `fs::last_write_time` каждый кадр | Системный вызов за кадр (в установленной сборке выключено) | Проверять раз в секунду |
| `src/hand.cpp:1384-1405` | При каждом хвате — CPU-скиннинг всей геометрии предмета (для актёра — всего тела) с записью времени в журнал | Пик в кадр хвата | Ограничить узлами у точки хвата (для книг так и делается, `src/hand.cpp:1386-1390`) |
| `src/main.cpp:839-898` | На каждое попадание прокси игрока — мьютекс и линейный `std::find` по игнорируемым телам (`src/hand.cpp:4564-4581`) | Несколько раз за кадр | Множество с флагом кадра |
| `src/hooks.cpp:242-243` | `count` + `operator[]` — два хеш-поиска на каждый `SetEffectShaderData` шейдера HIGGS | Мелочь | Один `find` |
| `src/main.cpp:354-362` | Пустые `PrePhysicsStep/PostPhysicsStep` на каждом шаге | Ничтожно | Убрать |
| `src/main.cpp:169-196`, `src/hand.cpp:2356-2358` | `std::deque` из 5 элементов: `pop_back/push_front` каждый кадр | Возможны аллокации блоков | Кольцевой буфер фиксированного размера |

---

## 10. Побочные находки (корректность)

| Место | Суть | Доказательство |
|---|---|---|
| Подпись на руке | `rolloverNode` может быть `nullptr` (поиск от узла комнаты), но разыменовывается без проверки | `src/hooks.cpp:376`, `src/hooks.cpp:421`, `src/hooks.cpp:437`, `src/hooks.cpp:451` |
| Генератор кривых | `g_vrikInterface` без проверки на null | `src/finger_curves.cpp:72` |
| API до загрузки данных | Разыменование `g_leftHand/g_rightHand`, созданных только на `DataLoaded` и только при активном esp | `src/pluginapi.cpp:252`, `src/main.cpp:902-913`, `src/main.cpp:1016-1017` |
| Колбэки после старта | Чтение векторов колбэков без блокировки при добавлении под блокировкой | `src/pluginapi.cpp:61-143`, `src/pluginapi.cpp:146-248` |
| Общий звук | Временное обнуление затухания общей формы звука — видно всем, кто играет её в это же время | `src/hand.cpp:972-977` |
| Дескриптор ревизии | Функции дописываются в ревизию 1 | `src/pluginapi.cpp:21-26` |
