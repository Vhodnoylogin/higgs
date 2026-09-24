# HIGGS: опись среды исполнения (runtime inventory)

Предмет: что HIGGS делает с движком Skyrim VR во время игры — какие перехваты ставит, какие байты
переписывает, где и в каком порядке работает каждый кадр, какое состояние движка пишет и что читает
у других модов (VRIK, PLANCK).

- Исходник: `D:\Skyrim VR Modding\HIGGS`, ветка `research/trinity-synergy`, версия 1.10.10.0
  (`include/version.h:7-10`). Установленная сборка — та же 1.10.10
  (`MO2 mods/HIGGS - Enhanced VR Interaction/meta.ini:5`), поэтому опись относится к тому, что реально работает.
- Ссылки вида `src/hooks.cpp:123` — от корня репозитория HIGGS. Ссылки на установленный ini —
  `higgs_vr.ini:NNN` = `MO2 mods/HIGGS - Enhanced VR Interaction/SKSE/Plugins/higgs_vr.ini`.
  Ссылки на PLANCK — `PLANCK src/main.cpp:NNN` от корня `D:\Skyrim VR Modding\PLANCK`.
- Все адреса — смещения RelocAddr/RelocPtr от базы SkyrimVR.exe 1.4.15 (`src/main.cpp:1136`).
- **Вывод** помечен словом «вывод»; всё остальное прочитано в коде.
- Имена ресурсов в таблицах — с префиксами общей схемы (`hook:`, `patch:`, `node:`, `havok:`, `collision:`,
  `setting:`, `input:`, `form:`, `event:`, `papyrus:`, `api:`, `equip:`, `anim:`, `haptics:`, `frame:`),
  чтобы опись можно было соединить с описями VRIK и PLANCK.

---

## 1. Главное

1. **HIGGS — хозяин кадра для всей тройки.** Три точки кадра PLANCK исполняются внутри перехватов HIGGS:
   `PreVrikPreHiggs` (перед обновлением рук HIGGS, `src/hooks.cpp:521`), `PrePhysicsStep`
   (`src/hooks.cpp:708`) и сравнение фильтров коллизий (`src/hooks.cpp:699-702`). PLANCK подписывается
   на все три (`PLANCK src/main.cpp:8252-8254`). Порядок относительно VRIK задан не договорённостью,
   а выбором адресов вызова: перехват `0x6ABCBF` стоит «прямо перед хуком VRIK», `0x6C6A7D` — «после
   обновления VRIK» (комментарии `src/hooks.cpp:46`, `src/hooks.cpp:49`). Если VRIK сдвинет свою точку,
   порядок молча сломается.
2. **28 точек перехвата в релизе** (+1 вызов только в `_DEBUG`): 11 переходов на сгенерированный
   Xbyak-код, 1 обход входа функции (6 байт), 12 заменённых вызовов (`call rel32`), 4 записи в таблицы
   виртуальных функций (три из 28 — `0x3B495B`, `0x3B6E23`, `0x5BBAEF` — зависят от ini и в сборке включены), плюс подмена таблицы у отдельных объектов `ShaderReferenceEffect` и у новых тел ближнего боя.
   Три байтовых патча. Ещё 8 адресов объявлены, но не используются (мёртвый код, раздел 4).
3. **HIGGS пишет в узлы, которыми владеет VRIK.** Руки первого лица и ключицы — до хука VRIK
   (`src/hand.cpp:2235-2259`, `src/hand.cpp:3915`, `src/hand.cpp:4100-4127`), пальцы — после VRIK прямой
   записью локальных трансформ скелета третьего лица (`src/finger_animator.cpp:40`, `:107`). Готовым
   API VRIK для пальцев (`setFingerRange`/`restoreFingers`) HIGGS не пользуется — в исходнике нет ни
   одного вызова (grep по `src/`).
4. **HIGGS вводит свой слой коллизий 56 `L_HIGGSCOLLISION`** и каждый кадр принудительно восстанавливает
   его битовую маску из собственного API (`src/physics.cpp:831-850`, `src/main.cpp:592`, `:611`). Руки и
   оружие HIGGS живут на слое 56 в группе игрока с битом 15 и «ragdoll-битами» 3/5 в поле номера части
   (`src/hand.cpp:576-582`, `include/physics.h:211-222`) — это и есть протокол, по которому PLANCK
   отличает левую руку от правой в тряпичных куклах NPC.
5. **Равноправные дубли механик:** пальцы (VRIK и HIGGS пишут одни узлы), тела оружия (движок держит
   своё тело ближнего боя `VRMeleeData`, HIGGS клонирует его на свой слой, `src/hand.cpp:720-791`),
   фильтрация столкновений капсулы игрока (HIGGS и PLANCK перехватывают **один и тот же** вызов
   `0x3AFD84`, `src/hooks.cpp:820`, `PLANCK src/main.cpp:7345`), зоны у плеч (стэш HIGGS и кобуры VRIK
   11/12 — вывод, раздел 11).

---

## 2. Порядок работы в кадре

Порядок внутри одной ветви (B) прочитан из кода и комментариев автора; взаимный порядок ветвей
(B, C, D) исходник не фиксирует — это вывод, отмечен.

| # | Точка кадра (ресурс) | Где/как | Что HIGGS делает здесь | Доказательство |
|---|---|---|---|---|
| A | `frame:main-thread after SKSE ProcessEvents` | перехват 0x5BABB5 объявлен, **не установлен** | ничего; по комментарию здесь стоит хук VRIK на главном потоке | `src/hooks.cpp:42-43` |
| B0 | `frame:Job_Non_render_safe_AI→PlayerCharacter::Update` | 0x649FD3 → 0x6C6910 (объявлено, не установлено) | контекст для B1–B5 | `src/hooks.cpp:51-52` |
| B1 | `frame:PlayerCharacter::VRUpdate pre-UpdateHands` | call 0x6ABCAF | `PreUpdateHandsUpdate`: запомнить мировые трансформы руки 1-го лица и WEAPON до обновления рук | `src/hooks.cpp:770-779`, `src/hand.cpp:4462-4471` |
| B2 | `frame:UpdateHands clavicle L/R` | call 0x6AC765 / 0x6AC88C | своя (double-точность) `UpdateClavicleToTransformHand` вместо движковой | `src/hooks.cpp:845-863` |
| B3 | `frame:pre-VRIK player update` | branch 0x6ABCBF (вместо вызова `UpdateVRBlocking`) | колбэки `PreVrikPreHiggs` (PLANCK) → **`Update()` HIGGS** (обе руки: PreUpdate/Update/PostUpdate, тела рук и оружия, хват, двуручие, скорость игрока) → колбэки `PreVrikPostHiggs` → оригинальный `UpdateVRBlocking` | `src/hooks.cpp:519-524`, `:1240-1275`, `src/main.cpp:520-660` |
| B4 | *(VRIK)* | хук VRIK в `PlayerCharacter::Update` сразу после 0x6ABCBF | VRIK двигает тело третьего лица по рукам 1-го лица, которые HIGGS только что переписал (вывод из порядка и `src/hand.cpp:2765`) | `src/hooks.cpp:46` |
| B5 | `frame:post-VRIK 3rd-person root update` | call 0x6C6A7D (вызов `NiNode::Update` корня 3-го лица) | колбэки `PostVrikPreHiggs` → **пальцы** (`FingerAnimator::Update`) → сам `NiAVObject_UpdateNode(корень 3-го лица)` → `Hand::PostVrikUpdate` (снимок трансформ VRIK, размер руки) → колбэки `PostVrikPostHiggs` | `src/hooks.cpp:527-548`, `src/hand.cpp:4427-4454` |
| B6 | `frame:PlayerCharacter::Update vtbl` | vtbl 0x16E27A8 (слот 0xAF) | после оригинала: только отладочный показ контроллеров (`debugDrawControllers=0` в сборке) | `src/hooks.cpp:719-744`, `higgs_vr.ini:576` |
| C | `frame:Actor::UpdateMovement (player)` | call 0x5E08C2 (`GetCharacterController`) | для игрока: чистка списка игнорируемых тел, **`SimulatePlayerSpace`**: перенос тел «пространства игрока» и **перемещение тел рук/оружия** (жёсткий ключ кадра) | `src/hooks.cpp:781-791`, `src/main.cpp:811-836`, `:371-517` |
| C1 | `frame:player char-proxy integrate` | vtbl 0x17B6890 и call 0x3AFD84 | фильтрация попаданий линейного каста капсулы игрока; `processConstraintsCallback` обнуляет скорость поверхностей | `src/hooks.cpp:797-842`, `src/physics.cpp:534-549` |
| D0 | `frame:bhkWorld::Update physics time` | branch 0x5BBAEF (если `EnableHavokFix=1`, в сборке 1) | пишет `fMaxTime`/`fMaxTimeComplex` под текущий кадр | `src/hooks.cpp:607-625`, `:1319-1356`, `higgs_vr.ini:331` |
| D1 | `frame:pre/post physics step` | call 0xDFB722 (`hkpWorld::stepDeltaTime`) | колбэки `PrePhysicsStep` (PLANCK) → `PrePhysicsStep` (пусто) → шаг → `PostPhysicsStep` (пусто) | `src/hooks.cpp:704-717`, `src/main.cpp:354-362` |
| D2 | `frame:havok step callbacks` | внутри шага | сравнение фильтров (0xE2BA10), слушатели контактов мира и тел, пост-симуляция, деактивация островов | `src/physics.cpp:660-779`, `:384-395`, `src/hooks.cpp:699-702` |
| E | `frame:Main::Draw post-wand-update` | branch 0x13233AA | «после хука VRIK updateUI»: плашка `WSActivateRollover`, луч выбора `SpellOrigin`, скрытие узла контроллера второй руки без VRIK | `src/hooks.cpp:476-516`, комментарий `:39`, `:479` |
| E1 | `frame:UpdateShadows` | call 0x1321DA7 | задержка обновления теней (фикс мерцания) | `src/hooks.cpp:571-579`, `src/main.cpp:207-222` |
| F | `frame:OpenVR poses callback` | `RegisterForPoses(…, 66, …)` | скорости контроллеров из OpenVR | `src/main.cpp:1168`, `:710-721`, `:122-204` |
| G | `frame:OpenVR controller state callback` | `RegisterForControllerState(…, 66, …)` | чтение/подавление/форсирование курка и хвата | `src/main.cpp:1167`, `:724-744`, `src/hand.cpp:4131-4304` |
| H | `frame:haptics thread ×2` | `std::thread` на каждую руку, цикл 5 мс | вибрация | `include/haptics.h:20-23`, `src/haptics.cpp:36-74` |

Вывод о взаимном порядке B и C: `SimulatePlayerSpace` потребляет и очищает список тел, собранный в
`Update()` (`src/main.cpp:501-505` против `src/hand.cpp:685-687`, `:893-895`), и двигает тела рук по
трансформе, посчитанной из руки после B (`src/hand.cpp:524-525`), поэтому по замыслу C идёт после B3
в том же кадре. Прямого доказательства порядка в исходнике нет.

Вывод о потоках: `ControllerStateCB` и `WaitPosesCB` вызываются SKSE VR из вызовов OpenVR игры; колбэки
контактов Havok могут идти с рабочих потоков физики — на это указывают мьютексы
`PhysicsListener::handLocks` (`src/physics.cpp:554`, `:616`) и отправка работы через
`g_taskInterface->AddTask` из колбэка контакта (`src/physics.cpp:498`).

---

## 3. Перехваты движка (hook:)

Все устанавливаются в `PerformHooks()` при загрузке плагина (`src/main.cpp:1189`, `src/hooks.cpp:919-1519`).
Трамплины: 1024 байта из пула SKSE на ветвления и столько же на код (`src/main.cpp:83-119`).

### 3.1. Переходы на сгенерированный код (Write5Branch/Write6Branch + Xbyak)

| Ресурс | Адрес | Условие | Что делает HIGGS | Доказательство |
|---|---|---|---|---|
| `hook:ShaderReferenceEffect ctor call@0x2AE3E8` | 0x2AE3E8 → 0x564DD0 | всегда | ловит указатель только что созданного `ShaderReferenceEffect`, чтобы подменить ему таблицу | `src/hooks.cpp:28-29`, `:221-227`, `:930-967` |
| `hook:CrosshairPickData::Pick hkpWorld::LinearCast call@0x3BA3D7` | 0x3BA3D7 → 0xAB5EC0 | всегда | если рука HIGGS показывает плашку — **пропускает** движковый каст прицела и пишет выбранный HIGGS объект в `CrosshairPickData`; иначе запоминает `leftHandle1/rightHandle1` | `src/hooks.cpp:35-36`, `:253-274`, `:969-998` |
| `hook:BSLightingShaderProperty::SetEffectShaderData call@0x2292AE` | 0x2292AE → 0x22A280 | всегда | для шейдеров выделения HIGGS разрешает эффект только на геометрии из своего набора | `src/hooks.cpp:80-81`, `:232-248`, `:1000-1026` |
| `hook:shader check flags vcall [rax+0x1C8]@0x229243` | 0x229243 (6 байт) | всегда | сохраняет текущую геометрию (`rdx`) для фильтра выше | `src/hooks.cpp:87`, `:1028-1059` |
| `hook:TESEffectShader::SetEffectShaderData call@0x56761E` | 0x56761E → 0x2291F0 | всегда | ставит/сбрасывает `g_shaderReference` вокруг вызова | `src/hooks.cpp:84-85`, `:1061-1098` |
| `hook:GetActivateText vcall [rax+0x260]@0x6D3337` | 0x6D3337 (6 байт) | всегда (действует при `OverrideActivateText=1`, в сборке 1) | заменяет текст плашки активации своим | `src/hooks.cpp:70`, `:555-568`, `:1100-1143`, `higgs_vr.ini:486` |
| `hook:RefreshActivateButtonArt vcall [rax+0xB8]@0x53F18E` | 0x53F18E (6 байт) | всегда | подменяет иконку кнопки активации («grip»/«trigger»/движение) | `src/hooks.cpp:72`, `:582-604`, `:1145-1199` |
| `hook:Main::Draw post-wand-update call@0x13233AA` | 0x13233AA → 0xDCF900 | всегда | точка E (раздел 2) | `src/hooks.cpp:39-40`, `:476-516`, `:1201-1238` |
| `hook:PlayerCharacter::Update UpdateVRBlocking call@0x6ABCBF` | 0x6ABCBF → 0x6AA060 | всегда | точка B3: колбэки PreVrik* и `Update()` HIGGS | `src/hooks.cpp:46-47`, `:519-524`, `:1240-1275` |
| `hook:PlayerCharacter::UpdateVRMeleeData bhkRigidBody_ctor call@0x6B0843` | 0x6B0843 → 0x2AEC80 | всегда | чинит тело ближнего боя игрока: превращает в `bhkRigidBodyT`, переносит смещение узла коллизии | `src/hooks.cpp:54-56`, `:651-696`, `:1277-1317` |
| `hook:bhkWorld update-physics-times call@0x5BBAEF` | 0x5BBAEF → 0xDFB3C0 | `EnableHavokFix=1` | точка D0 | `src/hooks.cpp:64-66`, `:607-625`, `:1319-1356` |
| `hook:bhkCollisionFilter::CompareFilterInfo entry@0xE2BA10` | 0xE2BA10, **вход функции**, 6 байт (Write6Branch) | всегда | перед движковой логикой опрашивает колбэки API: `Collide`→вернуть 1, `Ignore`→0, иначе движок | `src/hooks.cpp:68`, `:698-702`, `:1358-1411`, `src/pluginapi.cpp:208-218` |

### 3.2. Замена вызова (Write5Call, `call rel32`)

| Ресурс | Адрес | Условие | Что делает HIGGS | Доказательство |
|---|---|---|---|---|
| `hook:PlayerCharacter::Update 3rd-person NiNode::Update call@0x6C6A7D` | 0x6C6A7D | всегда | точка B5; сам вызывает `NiAVObject_UpdateNode` корня 3-го лица | `src/hooks.cpp:49`, `:527-548`, `:1413-1416` |
| `hook:bhkWorld::Update hkpWorld::stepDeltaTime call@0xDFB722` | 0xDFB722 | всегда | точка D1 | `src/hooks.cpp:89`, `:704-717`, `:1418-1422` |
| `hook:GetRefFromCollidable GetNodeFromCollidable call@0x3B495B` | 0x3B495B | `treatHandCollisionAsBelongingToPlayer=1` (сборка 1) | тела рук HIGGS отвечают «принадлежу `PlayerWorldNode`» → движок считает руку игроком | `src/hooks.cpp:91`, `:208-218`, `:1436-1439`, `higgs_vr.ini:469` |
| `hook:TriggerEntry::RegisterOverlap Actor::IsInRagdollState call@0x3B6E23` | 0x3B6E23 | `allowAllPlayerCollisionForTriggers=1` (сборка 1) | для игрока всегда `true` → триггеры срабатывают от рук/оружия, а не только от капсулы | `src/hooks.cpp:93`, `:196-205`, `:1441-1444`, `higgs_vr.ini:471` |
| `hook:ArrowProjectile::AddImpact bhkCollisionObject::GetRigidBody call@0x75CF10` | 0x75CF10 | всегда | подставляет тело ближнего боя при «ложном блоке» снаряда | `src/hooks.cpp:95`, `:187-193`, `:1463-1466` |
| `hook:Actor::Jump bhkCharacterController::SetJumping call@0x5DA6FB` | 0x5DA6FB | всегда | уменьшает высоту прыжка по массе удерживаемого | `src/hooks.cpp:747-768`, `:1468-1472` |
| `hook:PlayerCharacter::VRUpdate UpdateHands call@0x6ABCAF` | 0x6ABCAF | всегда | точка B1 | `src/hooks.cpp:770-779`, `:1474-1478` |
| `hook:Actor::UpdateMovement GetCharacterController call@0x5E08C2` | 0x5E08C2 | всегда (работа только для игрока) | точка C | `src/hooks.cpp:781-791`, `:1480-1484` |
| `hook:bhkLinearCaster::linearCast hkpWorld::linearCast call@0x3AFD84` | 0x3AFD84 | всегда | для группы игрока выбрасывает из коллектора попадания по удерживаемым/притягиваемым телам и лёгкому хламу | `src/hooks.cpp:820-842`, `:1486-1490`, `src/main.cpp:839-898` |
| `hook:UpdateHands UpdateClavicleToTransformHand call@0x6AC765` | 0x6AC765 (левая) | всегда | точка B2 | `src/hooks.cpp:845`, `:847-863`, `:1492-1495` |
| `hook:UpdateHands UpdateClavicleToTransformHand call@0x6AC88C` | 0x6AC88C (правая) | всегда | точка B2 | `src/hooks.cpp:846`, `:1497-1500` |
| `hook:UpdateShadows call@0x1321DA7` | 0x1321DA7 | всегда | точка E1 | `src/hooks.cpp:571-579`, `:1507-1510` |
| `hook:DoColorPass NiCamera::FinishAccumulatingPostResolveDepth call@0x1323E3E` | 0x1323E3E | **только `_DEBUG`** | отладочная отрисовка сфер | `src/hooks.cpp:866-894`, `:1512-1518` |

**Общий адрес с PLANCK:** 0x3AFD84 перехватывает и PLANCK тем же способом (`PLANCK src/main.cpp:7345-7349`,
`:7687`). Write5Call считывает текущее смещение вызова как «оригинал» (`src/hooks.cpp:908-917`),
поэтому второй загруженный плагин обернёт первого — цепочка работает, но порядок фильтрации зависит
от порядка загрузки DLL (вывод).

### 3.3. Таблицы виртуальных функций (SafeWrite64)

| Ресурс | Адрес | Что делает HIGGS | Доказательство |
|---|---|---|---|
| `hook:vtbl PlayerCharacter::Update (slot 0xAF)@0x16E27A8` | 0x16E27A8 = 0x16E2230 + 0xAF·8 | после оригинала — только отладочные контроллеры | `src/hooks.cpp:719-744`, `:1452-1455` |
| `hook:vtbl MissileProjectile::UpdateImpactFromCollector@0x16FE4F0` | 0x16FE4F0 | попадание снаряда в тело руки/оружия HIGGS переадресуется на тело ближнего боя движка; при `spoofProjectileWeaponHitsAsIfBlocked=1` — «ложный блок» | `src/hooks.cpp:101-184`, `:1457-1458`, `higgs_vr.ini:579` |
| `hook:vtbl ArrowProjectile::UpdateImpactFromCollector@0x16F99A0` | 0x16F99A0 | то же для стрел | `src/hooks.cpp:105`, `:181-184`, `:1460-1461` |
| `hook:vtbl hkpCachingShapePhantom::setPositionAndLinearCast@0x17B6890` | 0x17B6890 | общий для **всех** кэширующих фантомов (капсулы всех актёров); обрабатывает только фантом со свойством `Character == игрок` | `src/hooks.cpp:794-818`, `:1502-1505` |

### 3.4. Подмена таблицы у отдельных объектов

| Ресурс | Что | Доказательство |
|---|---|---|
| `hook:instance vtbl ShaderReferenceEffect dtor` | копия таблицы (0x40 слотов с typeinfo) с заменённым деструктором; ставится каждому эффекту выделения HIGGS, чтобы вычищать `g_shaderNodes` | `src/effects.cpp:127-143`, `:181-205` |
| `patch:runtime vtbl bhkRigidBodyT@0x182BA80 on new melee body` | новому телу ближнего боя игрока ставится таблица `bhkRigidBodyT` (память расширена патчем 0x6B0828) | `src/hooks.cpp:650`, `:667-681` |

---

## 4. Байтовые патчи (patch:) и мёртвые адреса

| Ресурс | Адрес | Байты | Условие | Смысл | Доказательство |
|---|---|---|---|---|---|
| `patch:0x6AC012 7xNOP SpellOrigin hide` | 0x6AC012 | 7 × `0x90` | `DisableSelectionBeam=0` (сборка 0 → патч стоит) | движок больше не прячет `SpellOrigin`; видимостью управляет HIGGS | `src/hooks.cpp:58`, `:1424-1428`, `higgs_vr.ini:443` |
| `patch:0x6CC000 ret PlayerCharacter::StartGrabObject` | 0x6CC000 | `0xC3` | `DisableVanillaGrab=1` (сборка 1) | отключён ванильный захват-левитация | `src/hooks.cpp:60`, `:1430-1434`, `higgs_vr.ini:466` |
| `patch:0x6B0828 mov ecx,0x60 melee rigidbody alloc` | 0x6B0828 | `B9 60 00 00 00` | всегда | выделение под тело ближнего боя 0x40 → 0x60 байт (место под `bhkRigidBodyT`) | `src/hooks.cpp:62`, `:1446-1450` |

Объявлены, но **не устанавливаются** (кандидаты на удаление): 0x6D2E7F/0x3BA0C0 `pickHook`
(`src/hooks.cpp:31-32`), 0x5BABB5/0xDFE470 `preVRIKMainThreadHook` (`:42-43`), 0x649FD3/0x6C6910
`playerCharacterUpdateHook` (`:51-52`), 0x3BA787 `pickRayCastHook` (`:74`), 0x271EF9 `worldUpdateHook`
(`:76`), 0xDFB554 `physicsUpdateHook` (`:78`), 0xAFAC34 и 0xAFB511 `hkpCharacterProxy integrate`
(`:97-98`). Ни один не упомянут в `PerformHooks` (`src/hooks.cpp:919-1519`, grep по `src/`).

---

## 5. Узлы сцены (node:)

### 5.1. Запись

| Ресурс | Когда | Что пишется | Доказательство |
|---|---|---|---|
| `node:NPC L/R Hand [LHnd]/[RHnd] 1st-person (kNode_Left/RightHandBone) world` | B3 каждый кадр при задержке руки, двуручии, физическом хвате; до/после Update при `handleVrikOffsetting=1` | мировая трансформа руки через `UpdateClavicleToTransformHand` (двигает ключицу + руку) | `src/hand.cpp:2235-2259`, `:2449`, `:3673-3676`, `:3915`, `:4102-4106`, `:4123-4127`, `higgs_vr.ini:584` |
| `node:NPC L/R Clavicle 1st-person (kNode_LeftCavicle/RightCavicle) local` | вместе с рукой; и в B2 вместо движка | локальная трансформа ключицы | `src/hand.cpp:2242-2256`, `src/hooks.cpp:847-863` |
| `node:NPC L/R Finger00..42 local (3rd-person if VRIK else 1st-person)` | B5 при анимации пальцев | `m_localTransform` всех 15 фаланг руки; без VRIK ещё `UpdateNode` | `src/finger_animator.cpp:37-53`, `:81-114` |
| `node:WEAPON/SHIELD 1st-person world` | B3: в `PreUpdate` при `doDoublePrecision=1` (сборка 1); при двуручии | `NiMathDouble::UpdateTransform` = трансформа узла смещения оружия | `src/hand.cpp:4086-4098`, `:3685`, `:3175-3177`, `:3735-3737`, `higgs_vr.ini:582` |
| `node:WEAPON/SHIELD 3rd-person local (temporary)` | B3 в момент начала двуручия с VRIK | временная трансформа «где будет оружие после VRIK», затем восстановление | `src/hand.cpp:2764-2783` |
| `node:weapon offset nodes (Melee/Crossbow/Staff/Shield OffsetNode, BowRotationNode) local` | B3 при двуручии | трансформа оружия; при отпускании — восстановление сохранённой | `src/hand.cpp:3700-3703`, `:1872-1876`, `:3162-3168`, `:3722-3728`, `:2093-2150` |
| `node:weapon collision offset node local` | B3 при двуручии | та же трансформа + вызов движкового `UpdateVRMeleeCollision` для тела ближнего боя другой руки | `src/hand.cpp:3688-3697`, `:2153-2171` |
| `node:Left/RightWandNode local` | B3 при двуручии (не арбалет/посох) | трансформа «жезла», чтобы ванильный блок видел двуручный хват; восстановление | `src/hand.cpp:3705-3711`, `:3170-3172`, `:3730-3732` |
| `node:Primary/SecondaryMagicOffsetNode, MagicAimNode local` | B3 при удержании/двуручии с заклинанием в руке | уводит узлы на −10000 по Z (прячет эффект заклинания в руке) | `src/hand.cpp:4050-4072`, `include/hand.h:237-238` |
| `node:SpellOrigin local + flags; children tempArc/noGoArc flags` | E каждый кадр | позиция/поворот/растяжение луча, флаг видимости (бит 0) у узла и двух дочерних дуг; в остальное время прячет | `src/hand.cpp:4852-4898`, `src/hooks.cpp:487-501` |
| `node:WSActivateRollover (child of RoomNode) local/scale + geometry alpha` | E каждый кадр | трансформа плашки у руки, масштаб 1e-6 для скрытия, альфа материала `BSEffectShaderMaterial+0x54` | `src/hooks.cpp:360-368`, `:370-471`, `src/hand.cpp:4772-4850`, `src/utils.cpp:992-1014` |
| `node:Left/RightControllerNode scale` | E, **только без VRIK** | масштаб 0.0001 узла второй руки при двуручном оружии, 1.0 в меню | `src/hooks.cpp:503-515` |
| `node:Left/RightControllerNode pos/flags (debug)` | B6 при `debugDrawControllers=1` (в сборке 0) | позиция 0, масштаб 1, снятие скрытия | `src/hooks.cpp:726-743`, `higgs_vr.ini:576` |
| `node:held object collidable node local (+skin bone matrices, ShadowSceneNode list)` | B3 в состояниях HeldInit/Held (ключевой кадр) | локальная трансформа узла удерживаемого объекта, скорость его тела, флаг тела бит 6, матрицы костей скина, список теней; `TESObjectREFR_SetPosition` для корневого узла | `src/utils.cpp:280-320`, `src/hand.cpp:3770-3789`, `:2058` |
| `node:grabbed collidable node UpdateNode` | при `forceGrabbedNodeUpdate=1` (сборка 1) | принудительный `NiAVObject_UpdateNode` узла объекта каждый кадр удержания | `src/hand.cpp:1310-1313`, `:3750-3753`, `:3813-3816`, `higgs_vr.ini:358` |
| `node:3rd-person root NiNode::Update` | B5 | HIGGS сам вызывает обновление корня 3-го лица вместо движка | `src/hooks.cpp:541-542` |

### 5.2. Чтение (важное для синергии)

| Ресурс | Зачем | Доказательство |
|---|---|---|
| `node:NPC COM [COM ] 3rd-person rigid body filterInfo` | группа коллизий игрока — каждый кадр для каждой руки, поиск по имени | `src/hand.cpp:2323-2336` |
| `node:NPC L/R Hand 3rd-person world scale` | «размер руки VRIK» → масштаб тела оружия HIGGS | `src/hand.cpp:2194-2199`, `:747-750`, `:4453` |
| `node:WEAPON/SHIELD 3rd-person world (after VRIK)` | запоминает смещение «рука 3-го лица → оружие» для тел оружия и двуручия | `src/hand.cpp:4427-4454`, `:538-556` |
| `node:RoomNode, FollowNode` | пространство игрока, предсказанный сдвиг | `src/main.cpp:379-395` |
| `node:HmdNode` | зоны рта/плеч, скорости OpenVR | `src/hand.cpp:2315-2321`, `src/main.cpp:715` |
| `node:PlayerWorldNode` | ответ перехвата GetRefFromCollidable | `src/hooks.cpp:214` |
| `node:VRMeleeData.collisionNode (player+0x710)` | источник формы для тела оружия HIGGS | `include/utils.h:62`, `src/hand.cpp:726-735` |

---

## 6. Havok: мир, тела, свойства (havok:)

| Ресурс | Доступ | Что | Доказательство |
|---|---|---|---|
| `havok:bhkWorld contact listener (PhysicsListener)` | own | слушатель контактов всего мира; добавляется при смене мира | `src/main.cpp:594`, `src/physics.cpp:660-719` |
| `havok:bhkWorld post-simulation listener` | own | учёт активных касаний, хаптика, чистка | `src/main.cpp:595`, `src/physics.cpp:721-779` |
| `havok:bhkWorld island activation listener` | own | фикс мерцания теней (`EnableShadowUpdateFix=1`) + снятие метки «брошен HIGGS» | `src/main.cpp:597-599`, `src/physics.cpp:309-358`, `:384-395`, `higgs_vr.ini:341` |
| `havok:player hkpCharacterProxy listener` | own | `processConstraintsCallback`: обнуляет скорость поверхностей подвижных не-bipeds — игрока не толкает движущийся хлам | `src/main.cpp:547-560`, `src/physics.cpp:534-551` |
| `havok:HIGGS hand rigid body (per hand)` | own | keyframed box, `SOLVER_DEACTIVATION_OFF`, качество KEYFRAMED, maxAngularVelocity 500, добавляется в мир игрока | `src/hand.cpp:559-614` |
| `havok:HIGGS weapon rigid body (per hand)` | own | keyframed клон формы `VRMeleeData`, масштаб `1/fMeleeWeaponHavokScale × weaponCollisionScale × handSize(VRIK)` | `src/hand.cpp:720-791`, `higgs_vr.ini:291` |
| `havok:hand/weapon body transform (hard keyframe)` | write | каждый кадр в C; с ограничением скорости → `setPosition/Rotation` | `src/hand.cpp:690-717`, `src/physics.cpp:865-886` |
| `havok:g_pickSphere phantom filterInfo+translation (temporary)` | write | **общий движковый фантом прицела**: на время запросов ставится фильтр 0x2C и позиция ладони, затем восстанавливается | `src/RE/offsets.cpp:12`, `src/hand.cpp:223-265`, `:287-314`, `:329-419`, `:449-516` |
| `havok:held body motion type` | write | keyframed→dynamic при физическом хвате; dynamic→keyframed при «ключевом» хвате и обратно при отпускании | `src/hand.cpp:1350-1352`, `:1672-1677`, `:3069`, `:3249-3251` |
| `havok:held body quality type` | write | debris→moving (`convertDebrisToMoving=1`); сохранение/восстановление для keyframed-хвата | `src/hand.cpp:1354-1358`, `:1674`, `:3068`, `higgs_vr.ini:439` |
| `havok:held bhkRigidBody flags bit6` | write | ставится в `UpdateKeyframedNode`; сохраняется/восстанавливается | `src/utils.cpp:294`, `src/hand.cpp:1675`, `:3061` |
| `havok:held/connected bodies inverse inertia (m_inertiaAndMassInv)` | write | ограничение анизотропии инерции на время хвата, восстановление при отпускании (с учётом второй руки) | `src/hand.cpp:1648-1669`, `:3049-3054` |
| `havok:connected bodies contactPointCallbackDelay` | write | 0 на время хвата, восстановление | `src/hand.cpp:1639-1646`, `:3042-3047` |
| `havok:entity contact listeners (EntityCollisionListener L/R)` | write | вешаются на все связанные тела удерживаемого объекта | `src/hand.cpp:1637`, `:3041`, `src/physics.cpp:782-829` |
| `havok:entity contact listener HeldObjectCollisionListener + property 056614 (HIGGS_DROPPED)` | write | брошенному телу ставится свойство (восьмеричное 056614 = 23948 = 0x5D8C) и слушатель; снимается при деактивации острова или новом хвате | `src/physics.cpp:360-382`, `src/hand.cpp:3145-3148`, `:1681` |
| `havok:grab constraint bhkGroupConstraint (PRIORITY_TOI) hand↔object` | own | создаётся при физическом хвате, моторы: `maxForce/minForce/tau/damping/recovery` переписываются **каждый кадр**; группа ограничения = группа объекта | `src/hand.cpp:1616-1619`, `:3917-4000`, `src/RE/havok.cpp:126-149` |
| `havok:nearby bodies linear/angular damping` | write | при хвате у соседних лёгких тел демпфирование на `grabFreezeNearbyVelocityTime`, потом восстановление | `src/hand.cpp:155-201`, `src/main.cpp:282-336` |
| `havok:pulled body angular damping` | write | при притягивании «гравитационными перчатками», восстановление в `EndPull` | `src/hand.cpp:3235-3236`, `:3432-3433`, `:4474-4485` |
| `havok:body linear/angular velocity` | write | бросок, притягивание, пространство игрока (добавить/вычесть скорость комнаты) | `src/hand.cpp:3032-3033`, `:3080-3082`, `:3498-3507`, `src/main.cpp:432-440`, `:489-493` |
| `havok:body position/rotation (warp)` | write | `warpToHand` при внешнем хвате; перенос тел пространства игрока при повороте + `reintegrateAndRecollideEntities` | `src/hand.cpp:1561-1590`, `src/main.cpp:442-478` |
| `havok:contact point CONTACT_IS_DISABLED` | write | удерживаемое ↔ группа игрока; рука ↔ только что брошенное (временно) | `src/physics.cpp:677-684`, `:640-658`, `src/hand.cpp:3013-3016`, `:3063-3066` |
| `havok:contactMgr m_contactPointCallbackDelay=0` | write | для любого контакта с телом HIGGS | `src/physics.cpp:692` |
| `havok:player char-proxy cast collector hits` | write | удаление попаданий из коллектора линейного каста капсулы игрока | `src/main.cpp:839-898` |
| `havok:player-space body registry` | own | список тел, которые двигаются вместе с игроком (руки, оружие, удерживаемое, «содержимое контейнера», соседние кости удерживаемого актёра) | `src/main.cpp:339-351`, `src/hand.cpp:3860-3881` |
| `havok:hkpCharacterProxy velocity` | read | ускорение игрока | `src/hand.cpp:2361-2368` |
| `havok:world lock (BSWriteLocker/BSReadLocker)` | call | берётся точечно вокруг изменений; `StartNearbyDamping` держит read-lock на время `GetClosestPoints` | `src/main.cpp:413`, `src/hand.cpp:161-200`, `src/physics.cpp:1026` |

---

## 7. Коллизии: слои и биты (collision:)

Разметка `filterInfo` (по коду HIGGS): биты 0–6 — слой (`include/utils.h:161`), 8–12 — номер части /
«ragdoll layer» (`include/utils.h:169`), 14 — «коллизия выключена», 15 — «сталкиваться со своей
группой, если бит стоит у обоих» (`src/hand.cpp:577`), 16–31 — группа (`src/hand.cpp:679-680`).
Имена слоёв — `SKSE VR Source/sksevr_2_00_12/src/sksevr/skse64/GameForms.h:562-616` (слой N на строке 562+N).

| Ресурс | Доступ | Что | Доказательство |
|---|---|---|---|
| `collision:layer 56 L_HIGGSCOLLISION` | own | HIGGS занимает «первый неиспользуемый ванильный слой»: пишет `layerBitfields[56]`, имя, и бит 56 во **всех 64** масках слоёв | `src/physics.cpp:831-839`, `:852-863` |
| `collision:layer 56 bitfield enforce per frame` | write | каждый кадр сравнивает `layerBitfields[56]` с `higgsLayerBitfield` из API и перезаписывает; прямую правку маски другим модом HIGGS откатит | `src/main.cpp:611`, `src/physics.cpp:841-850` |
| `collision:higgsLayerBitfield default 0x01053343161b7fff` | own | сталкивается со слоями 0–14, 16, 17, 19, 20, 25, 26, 28, 32 (DeadBip), 33 (BipedNoCC), 38, 40, 41, 44, 45, 48, 50, 56; **не** с 30 (CharController); 8 (Biped) уже включён | `include/pluginapi.h:102`, `GameForms.h:570`, `:592`, `:594-595` |
| `collision:bit14 disable on HIGGS hand body` | write | включается при физическом хвате и двуручии, снимается через `handWeaponCollisionEnableDelay` | `src/hand.cpp:656-676` |
| `collision:bit14 disable on HIGGS weapon body` | write | оружие не вынуто / выключено API / недавний удар (`WeaponCollisionDisableOnHitTime`) | `src/hand.cpp:863-887`, `src/main.cpp:770-792` |
| `collision:group bits16-31 on HIGGS bodies = player group` | write | каждый кадр переписывается (группа игрока меняется, например, на коне) | `src/hand.cpp:679-680`, `:890-891` |
| `collision:bit15 + part bits8-12 RagdollLayer (RightHand=3, LeftHand=5)` | own | протокол «какая рука»: тела рагдолла с `SkipRight=2/SkipBoth=4/SkipLeft=6` не сталкиваются с соответствующей рукой | `src/hand.cpp:577-580`, `:758-761`, `include/physics.h:211-222` |
| `collision:impacted projectile → layer 5 (Weapon) + player group` | write | застрявшая стрела перед хватом/притягиванием: слой, группа, `MOTION_DYNAMIC` | `src/hand.cpp:1339-1348`, `:3238-3246` |
| `collision:part number 18 (shield) on melee body (temporary)` | write | на время обработки попадания снаряда при «ложном блоке» | `src/hooks.cpp:154-155`, `:170` |
| `collision:CompareFilterInfo result override` | hook | исход сравнения фильтров решают колбэки API (первый не-`Continue` выигрывает) | `src/hooks.cpp:1358-1411`, `src/pluginapi.cpp:208-218` |
| `collision:query filter 0x2C (layer 44 CustomPick2, group 0)` | own | фильтр фантома ладони при поиске объектов | `src/hand.cpp:229`, `:291`, `:333`, `:454`, `GameForms.h:606` |
| `collision:query filter (playerGroup<<16)|0x28 (layer 40 ItemPicker)` | own | фильтр луча дальнего выбора | `src/hand.cpp:434`, `GameForms.h:602` |

Замечание: комментарий API говорит, что слой HIGGS используется «для рук, оружия и удерживаемых
объектов» (`include/higgsinterface001.h:89`), но в коде на слой 56 переводятся только тела рук и оружия
(`src/hand.cpp:576`, `:757`); удерживаемые узнаются по указателю (`src/physics.cpp:426-444`). Расхождение
документации с кодом.

---

## 8. Настройки игры и ini (setting:)

| Ресурс | Доступ | Когда | Что | Доказательство |
|---|---|---|---|---|
| `setting:fActivateRumbleIntensity:VRInput` | write | загрузка (`DisableRolloverRumble=1`) и каждый кадр показа плашки | 0; сохранение/восстановление в `RolloverHandler` | `src/main.cpp:1053-1057`, `src/hooks.cpp:397-402`, `:430-433`, `higgs_vr.ini:460` |
| `setting:bAlwaysShowHands:VR` | write | загрузка (`AlwaysShowHands=1`) | 1 | `src/main.cpp:1059-1063`, `higgs_vr.ini:463` |
| `setting:uMaxNumPhysicsStepsPerUpdate@0x1EC8418` / `Complex@0x1EC8430` | write | загрузка (`EnableHavokFix=1`) | 3 / 3 из ini HIGGS, **поверх** ini игры | `src/main.cpp:1065-1069`, `src/RE/offsets.cpp:19-20`, `higgs_vr.ini:337-338` |
| `setting:fMaxTime@0x1EC82B0` / `fMaxTimeComplex@0x1EC8448` | write | D0 каждый кадр | шаг физики = время кадра, делённое на 2..N при `minPhysicsFrameRate` | `src/hooks.cpp:607-625`, `src/RE/offsets.cpp:24-25`, `higgs_vr.ini:335` |
| `setting:fMoveLimitMass@0x1EC84A8` | write | загрузка | = `minCollideClutterMass` (69) | `src/main.cpp:1071-1075`, `higgs_vr.ini:196` |
| `setting:iShadowUpdateFrameDelay@0x1ED4130` + `nextShadowUpdateFrameCount@0x3485798` | write | E1 после деактивации острова | задержка 1 на `numShadowUpdates` кадров, затем прежнее значение | `src/main.cpp:207-222`, `src/physics.cpp:318-321` |
| `setting:fMeleeWeaponHavokScale@0x1EAD900` | read | создание тела оружия | масштаб формы | `src/hand.cpp:744`, `src/RE/offsets.cpp:44` |
| `setting:fActivateRolloverWand{X,Y,Z,RotateX..Z,Scale}` | read | сброс плашки | исходная трансформа | `src/hooks.cpp:360-368`, `src/RE/offsets.cpp:54-60` |
| `setting:higgs_vr.ini (own)` | own | загрузка; перечитывание каждый кадр при `reloadConfigIfModified=1` (в сборке 0) | ~590 строк настроек | `src/config.cpp:550-574`, `:583`, `src/main.cpp:538-545`, `higgs_vr.ini:587` |

---

## 9. Экипировка, инвентарь, формы (equip:, form:)

HIGGS **не меняет экипировку игрока** (оружие/щит/заклинание) — только читает её. Пишет экипировку
NPC (снятие брони при мародёрстве) и инвентарь игрока (стэш/поедание).

| Ресурс | Доступ | Что | Доказательство |
|---|---|---|---|
| `equip:player equipped object per hand (GetEquippedObject/Weapon/Shield/Light/Spell)` | read | может ли рука хватать, двуручие, какое тело оружия, какой узел смещения | `src/hand.cpp:4306-4424`, `:2093-2150`, `:843` |
| `equip:player weapon drawn (actorState.IsWeaponDrawn)` | read | выключение тела оружия, условия хвата/двуручия | `src/hand.cpp:863`, `:4310`, `:4378` |
| `equip:NPC worn item removal (Actor::RemoveItem vfunc 0x56)` | write | мародёрство: броня/оружие с тела падает в мир и притягивается/хватается | `src/hand.cpp:1980-2017`, `:1886-1905`, `:3198-3216` |
| `equip:player EquipItemEx (consume)` | write | зелье/еда, поднесённые ко рту | `src/hand.cpp:3085-3107` |
| `equip:EquipManager_EquipEntryData + TESObjectBOOK_LearnSpell + RemoveItem (spellbook)` | write | книга заклинаний у рта | `src/utils.cpp:1095-1111`, `src/hand.cpp:3099-3103` |
| `form:TESObjectREFR_Activate / Actor::PickUpObject vfunc 0xCE (stash)` | write | предмет у плеча → в инвентарь | `src/hand.cpp:3108-3130` |
| `form:TESObjectREFR_SetActorOwner (loot ownership)` | write | вытянутому из NPC предмету владелец = игрок (не кража) | `src/hand.cpp:3383`, `:3418` |
| `form:TESObjectREFR_SetPosition (held ref)` | write | позиция ссылки удерживаемого keyframed-объекта | `src/hand.cpp:3785-3789` |
| `form:higgs_vr.esp TESEffectShader 0x6F00/0x6F01` | own | шейдеры выделения (обычный/чужое) | `src/main.cpp:904-935` |
| `form:BGSSoundDescriptor dbAttenuation (temporary 0)` | write | «тихие» звуки хвата временно без ослабления (правка общей формы) | `src/hand.cpp:972-978` |
| `form:Projectile flags bit27 (blocked)` | write | «ложный блок» снаряда | `src/hooks.cpp:157` |
| `form:BSLightingShaderProperty effect data (unk68) save/restore` | write | сохранить/вернуть чужой эффект-шейдер на узле при выделении | `src/effects.cpp:35-121` |
| `event:BSTaskPool damage object task` | call | урон разрушаемым объектам при броске | `src/physics.cpp:501-523` |
| `event:CreateDetectionEvent (noise)` | call | шум от брошенного предмета для ИИ | `src/physics.cpp:280-306`, `:491-499` |

---

## 10. Ввод, хаптика, UI, анимационный граф, события

| Ресурс | Доступ | Что | Доказательство |
|---|---|---|---|
| `input:OpenVR k_EButton_SteamVR_Trigger (pressed)` | read/write | чтение; **подавление** на время окна захвата и блокировки; **форсирование** нажатия после неудачного захвата | `src/hand.cpp:4137-4141`, `:4223-4230`, `:4253-4255`, `:4262-4286` |
| `input:OpenVR k_EButton_Grip (pressed/touched)` | read/write | то же для хвата; для Index — «касание» | `src/hand.cpp:4138`, `:4143-4148`, `:4264-4303`, `include/hand.h:182-185`, `src/utils.cpp:1305-1325` |
| `input:InputManager activate mapping` | read | если активация на хвате — иконка «grip» | `src/main.cpp:1042-1051` |
| `input:CrosshairPickData left/rightHandle1..3` | write | выбранный HIGGS объект подставляется как цель активации | `src/utils.cpp:957-979`, `src/hooks.cpp:255-262` |
| `haptics:BSOpenVR::TriggerHapticPulse per hand` | own | два потока (по руке), очередь событий, тик 5 мс | `src/haptics.cpp:8-74`, `include/haptics.h:20-28` |
| `anim:IsBlocking (player, temporary true)` | write | на время обработки снаряда при «ложном блоке» | `src/hooks.cpp:143-152`, `:168-171` |
| `event:MenuOpenCloseEvent sink` | own | список открытых меню → `isGameStopped` | `src/main.cpp:937-940`, `src/menu_checker.cpp:92-117` |
| `event:TESHitEvent sink` | own | удар игрока оружием → временно выключить тело оружия этой руки | `src/main.cpp:770-793`, `:942-949`, `src/hand.cpp:864-868` |
| `event:SKSE messaging (PostLoad/PostPostLoad/DataLoaded)` | own | своё API, получение API VRIK, инициализация | `src/main.cpp:1087-1116` |
| `event:SKSE task interface (UI task, detection task)` | call | `RefreshActivateButtonArt` на UI-потоке; шум из колбэка контакта | `src/hooks.cpp:329-341`, `:413`, `:444`, `src/physics.cpp:498` |
| `event:WSActivateRollover menu (text, button art)` | write | текст и кнопка плашки активации | `src/hooks.cpp:551-604`, `src/hand.cpp:4620-4769` |

---

## 11. Что HIGGS читает у других модов

| Мод | Ресурс | Как | Доказательство |
|---|---|---|---|
| VRIK | `api:IVrikInterface001::getBuildNumber` | PostPostLoad; `< 80400` → аварийный выход | `src/main.cpp:1100-1114` |
| VRIK | `api:IVrikInterface001::getFinalCameraOffsettingAmount` / `getFinalSmoothingOffsettingAmount` | каждый кадр в C (Z-смещение камеры) и в B3 через `g_prevVrikOffset` | `src/main.cpp:400-401`, `:510-511`, `src/hand.cpp:4102-4127` |
| VRIK | `api:IVrikInterface001::getSettingDouble("handSize")` | только инструмент генерации кривых пальцев, вызов закомментирован | `src/finger_curves.cpp:70-75`, `src/hand.cpp:2349-2351` |
| VRIK | присутствие DLL | `GetModuleHandle("vrik")`, отдельно от API | `src/main.cpp:951` |
| VRIK | трансформы 3-го лица после VRIK (рука, WEAPON/SHIELD, масштаб руки) | B5 | `src/hand.cpp:4427-4454` |
| VRIK | копия интерфейса в HIGGS **старая** — без функций кобур сборки 80700 | сравнение с актуальным контрактом | `include/vrikinterface001.h:12-50` против `local/vrik-re/api/vrikinterface001.h:54-74` |
| PLANCK | колбэки через API HIGGS: `CollisionFilterComparison`, `PrePhysicsStep`, `PreVrikPreHiggs`, `Grabbed` | HIGGS исполняет их в своих перехватах | `PLANCK src/main.cpp:8252-8255`, `src/hooks.cpp:521`, `:708`, `:701` |
| PLANCK | `SetHiggsLayerBitfield` (+Biped, +BipedNoCC) | HIGGS применяет в `EnsureHiggsCollisionLayer`. Оба бита уже стоят в умолчании HIGGS — правка PLANCK ничего не меняет (вывод из `include/pluginapi.h:102`) | `PLANCK src/main.cpp:8257-8260` |
| PLANCK | `ForceWeaponCollisionEnabled(false/true)` | флаг навсегда: в API нет функции снятия | `PLANCK src/main.cpp:8262-8263`, `src/pluginapi.cpp:351-354`, `include/pluginapi.h:104` |
| PLANCK | `DisableHand/EnableHand` | вызовы PLANCK сбрасывают/ставят общий флаг руки без счётчика владельцев | `PLANCK src/main.cpp:3768-3780`, `src/pluginapi.cpp:291-319` |

---

## 12. API HIGGS (кратко; полный разбор — задача описи API)

C++: `IHiggsInterface001` через SKSE-сообщение `0xF9279A57` (`src/pluginapi.cpp:11-35`,
`include/higgsinterface001.h:13-131`). Papyrus: скрипт `HiggsVR` — `GetSetting/SetSetting`, `GrabObject`,
`GetGrabbedObject`, `GetGrabbedNodeName`, `CanGrabObject`, `Disable/EnableHand`, `IsDisabled`,
`Disable/EnableWeaponCollision`, `IsWeaponCollisionDisabled`, `IsTwoHanding`, регистрация на 7 событий
`OnObjectPulled/Grabbed/Dropped/Stashed/Consumed`, `OnStartTwoHanding/OnStopTwoHanding`
(`src/papyrusapi.cpp:215-322`).

Наблюдения, влияющие на синергию:
- Колбэки — голые указатели на функции без `userdata`, без удаления и без приоритета; порядок = порядок
  регистрации (`src/pluginapi.cpp:61-143`). Для фильтра коллизий «первый не-Continue выигрывает»
  (`src/pluginapi.cpp:208-218`) — при двух потребителях исход зависит от порядка загрузки.
- `Disable/EnableHand`, `Disable/EnableWeaponCollision` — булевы флаги без счётчика: два мода, выключающие
  руку по разным причинам, снимают блокировку друг друга (`src/pluginapi.cpp:291-349`).
- `ForceWeaponCollisionEnabled` без обратной функции (`src/pluginapi.cpp:351-354`).
- Нет событий: «рука/оружие коснулись тела X» с указанием тела (есть только масса и скорость,
  `include/higgsinterface001.h:37`), «начало/конец притягивания», «смена состояния руки».
- Нет доступа на запись к пальцам (есть только `GetFingerValues`, `src/pluginapi.cpp:416-422`), тогда как
  у VRIK запись есть (`local/vrik-re/api/vrikinterface001.h:41-43`).
- Papyrus-регистрации не сохраняются в сейв: HIGGS не запрашивает интерфейс сериализации SKSE
  (`src/main.cpp:1144-1197` — только messaging, VR, papyrus, task, trampoline) — вывод: после загрузки
  сейва скрипты должны регистрироваться заново.

---

## 13. Механики HIGGS и их пересечения

| Механика | Что делает HIGGS | Пересечение | Доказательство |
|---|---|---|---|
| Физическая рука | keyframed-тело ладони на слое 56, следует за рукой 1-го лица | PLANCK использует его для ударов (`GetHandRigidBody`) | `src/hand.cpp:559-717`, `PLANCK src/main.cpp:4263-4264` |
| Тело оружия | клон формы движкового `VRMeleeData` на слой 56 | дубль тела ближнего боя движка; PLANCK берёт через `GetWeaponRigidBody` | `src/hand.cpp:720-896`, `PLANCK src/main.cpp:4268-4269` |
| Хват, бросок, «гравитационные перчатки» | ограничение рука↔объект или ключевой кадр; притягивание скоростью | PLANCK получает `Grabbed` и дальше работает с удерживаемым актёром | `src/hand.cpp:1301-1709`, `:3186-3515` |
| Хват живых актёров | сила ограничения по FPS, отдельные `tau` для рагдолла/не рагдолла | тряпичные куклы живых NPC создаёт PLANCK (вывод: иначе у живого NPC нет Biped-тел) | `src/hand.cpp:3825-3832`, `:3937-3963` |
| Двуручие | HIGGS двигает руки 1-го лица, узлы смещения оружия, «жезл», тело ближнего боя | у VRIK нет публичного API двуручия; порядок решён адресами | `src/hand.cpp:3524-3718` |
| Сгибание пальцев при хвате | запись фаланг после VRIK, плавный возврат к позе VRIK | **дубль** пальцев VRIK; готовый `setFingerRange/restoreFingers` не используется | `src/finger_animator.cpp:55-176`, `local/vrik-re/api/vrikinterface001.h:38-43` |
| Рука держится за объект | при физическом хвате рука 1-го лица ставится по объекту, VRIK тянет тело за ней | зависит от того, что хук HIGGS стоит до VRIK | `src/hand.cpp:3884-3915`, `src/hooks.cpp:46` |
| Пространство игрока | тела, которые несёт игрок, двигаются вместе с ним, с учётом смещения камеры VRIK | использует API VRIK | `src/main.cpp:371-517` |
| Столкновение капсулы игрока с хламом | лёгкий хлам не останавливает игрока, удерживаемое не толкает | **дубль**: PLANCK фильтрует тот же каст | `src/main.cpp:839-898`, `PLANCK src/main.cpp:7347-7359` |
| Стэш у плеча / еда у рта | сферы в пространстве HMD | вывод: пересечение по месту с кобурами VRIK 11/12 (плечи) без взаимного арбитража; у VRIK 80700 есть `HolsterAttemptCallback` | `src/hand.cpp:1908-1977`, `higgs_vr.ini:71-86`, `vrikslots.ini:5`, `:352-378` |
| Мародёрство брони с тела | снятие через `RemoveItem`, притягивание | — | `src/hand.cpp:2513-2596`, `:1980-2017` |
| Замедление игрока/прыжка по массе | `SpeedMult` (временный модификатор) + цикл `CarryWeight`, высота прыжка | любой мод, меняющий `SpeedMult`, делит с HIGGS одно значение (вывод) | `src/main.cpp:249-269`, `src/utils.cpp:1084-1093`, `src/hooks.cpp:747-768` |
| Фикс шага физики (Havok Fix) | `fMaxTime` по кадру | общий ресурс для любой физики | `src/hooks.cpp:607-625` |
| Фикс мерцания теней | обновление теней при деактивации острова | — | `src/physics.cpp:309-358` |
| Фикс тела ближнего боя | `bhkRigidBodyT` + размер выделения | влияет на тело, которым движок и PLANCK определяют удар (вывод) | `src/hooks.cpp:651-696`, `:1446-1450` |
| Выделение объектов | свои эффект-шейдеры, фильтр по геометрии | — | `src/effects.cpp`, `src/hooks.cpp:221-248` |
| Снаряды в руку/оружие | попадание переадресуется на тело ближнего боя; «ложный блок» | — | `src/hooks.cpp:109-193` |

---

## 14. Цена в кадре (perf)

| Где | Что стоит | Оценка | Доказательство |
|---|---|---|---|
| `src/hand.cpp:2323-2324` | поиск узла `NPC COM [COM ]` по имени в дереве 3-го лица — каждый кадр × 2 руки | обход дерева со сравнением строк | `src/hand.cpp:2323` |
| `src/finger_animator.cpp:37-53` | 15 `GetObjectByName` на руку каждый кадр, пока пальцы анимируются | 30 поисков в кадр при хвате двумя руками | `:40-48`, `:59-60` |
| `src/hand.cpp:2081-2090`, `:2174-2191` | `GetThirdPersonHandNode`, `GetWeaponNode` (`WEAPON`/`SHIELD`), `GetMagicNode` — поиск по имени, вызываются в B1, B3, B5 несколько раз на руку | ~6–10 поисков в кадр (вывод по вызовам) | `src/hand.cpp:4086-4090`, `:4429-4432`, `:4468` |
| `src/hooks.cpp:374-376` | поиск `WSActivateRollover` под `RoomNode` каждый кадр | обход дерева комнаты | — |
| `src/hand.cpp:4691`, `:4753` | **`std::regex` создаётся заново** при каждом вызове `GetActivateText`; вызывается каждый кадр показа плашки (`OverrideActivateText=1`) | компиляция регулярного выражения в кадре — десятки микросекунд (вывод) | `src/hooks.cpp:404-405`, `higgs_vr.ini:486` |
| `src/menu_checker.cpp:109-117` | 30 построений `std::string` + поиск в `unordered_map` на каждый вызов `isGameStopped`; вызывается на каждое чтение состояния контроллера | сотни аллокаций в секунду | `src/hand.cpp:4159`, `src/hooks.cpp:505` |
| `src/haptics.cpp:36-74` | два постоянных потока, пробуждение каждые 5 мс, захват мьютекса даже без событий | ~400 пробуждений/с | `include/haptics.h:20-23` |
| `src/hooks.cpp:1358-1411` | каждый вызов `CompareFilterInfo` в мире (сотни/тысячи за шаг) проходит через HIGGS и вектор колбэков | сохранение 3 регистров + вызов + цикл | `src/pluginapi.cpp:208-218`, `include/higgsinterface001.h:79` |
| `src/physics.cpp:660-719` | слушатель контактов всего мира: на каждое событие контакта 2× `IsHandOrWeaponOrHeld` | дёшево на событие, но на все контакты мира | `src/physics.cpp:436-444` |
| `src/hooks.cpp:797-818` | vtable-хук фантома срабатывает для капсул **всех** актёров, `getProperty(Character)` на каждый вызов | линейный поиск свойства (вывод) | — |
| `src/main.cpp:371-517` | `SimulatePlayerSpace`: `std::set<NiPointer>` вставки/очистки каждый кадр; при повороте — `reintegrateAndRecollideEntities` (узкая фаза) | всплеск при снап-повороте | `src/main.cpp:416-418`, `:477` |
| `src/physics.cpp:1000-1111` | `GetContainedRigidBodies` при удержании (`doContainerPhysics=1`): запрос AABB широкой фазы + линейные касты на каждое тело | растёт с числом предметов в «контейнере» | `src/hand.cpp:3847-3857`, `higgs_vr.ini:198` |
| `src/utils.cpp:1221-1245` | `CollectAllGrabbedRigidBodies` (обход ограничений) каждый кадр физического хвата, в трёх местах | — | `src/hand.cpp:3834-3836`, `:1549-1551`, `:3019-3021` |
| `src/hand.cpp:647`, `:693`, `src/hooks.cpp:532` | `TESRace_IsBeast(Actor_GetRace())` многократно в кадр | мелко, но не кэшируется | — |
| `src/hand.cpp:320-520` | поиск объектов: линейный каст фантома ладони, луч + каст для дальнего — каждый кадр на свободную руку | основная постоянная цена HIGGS (вывод) | `src/hand.cpp:2452-2476` |
| `src/utils.cpp:280-320` | `UpdateKeyframedNode`: `UpdateNode` + матрицы костей скина всего объекта + `ShadowSceneNode_UpdateNodeList` каждый кадр keyframed-хвата | зависит от объекта | — |
| `src/hand.cpp:1393-1415` | скиннинг треугольников при хвате (не в кадре) — время пишется в лог | разово, мс | `src/hand.cpp:1400`, `:1405` |
| `src/config.cpp:550-574` | `fs::last_write_time` каждый кадр при `reloadConfigIfModified=1` (в сборке выключено) | — | `higgs_vr.ini:587` |

---

## 15. Что из этого следует для «тройки как целого» (выводы)

1. **Порядок кадра нужно сделать явным контрактом.** Сейчас HIGGS — фактический планировщик для
   PLANCK (три точки) и «угадывает» место VRIK адресами. Естественный шаг — объявить в API HIGGS
   фазы кадра (`PreVrik`, `PostVrik`, `PrePhysicsStep`, `PostPhysicsStep`, `LateMain`) с `userdata` и
   приоритетом, чтобы VRIK/PLANCK/другие моды подписывались, а не ставили соседние перехваты.
2. **Пальцы — одно место владения.** Либо HIGGS пишет пальцы через `setFingerRange/restoreFingers`
   VRIK, либо VRIK получает от HIGGS «запрос позы» — но не оба в одни узлы.
3. **Фильтрация капсулы игрока — одна реализация.** HIGGS и PLANCK обёртывают один вызов 0x3AFD84;
   логично оставить один перехват и дать второму колбэк.
4. **Флаги API — со счётчиком владельцев** (`DisableHand`, `DisableWeaponCollision`,
   `ForceWeaponCollisionEnabled`) и с обратными операциями.
5. **Слой 56 и протокол `RagdollLayer`** — уже де-факто общий язык HIGGS↔PLANCK; его стоит описать в
   публичном заголовке как контракт (сейчас он только в `include/physics.h:211-222`, не в API).
6. **Оптимизации без смены поведения:** кэш узлов по имени (COM, пальцы, WEAPON/SHIELD, плашка),
   статический `std::regex`, `isGameStopped` без строк, хаптика на условной переменной вместо опроса,
   удаление 8 мёртвых адресов перехватов.
